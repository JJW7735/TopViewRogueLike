#include "RLThrownAxe.h"
#include "Components/ArrowComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"

ARLThrownAxe::ARLThrownAxe()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	AxeMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("AxeMesh"));
	AxeMesh->SetupAttachment(RootComponent);
	AxeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AxeMesh->SetCastShadow(true);

#if WITH_EDITORONLY_DATA
	ForwardArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ForwardArrow"));
	if (ForwardArrow)
	{
		ForwardArrow->SetupAttachment(RootComponent);
		ForwardArrow->ArrowSize = 3.0f;
	}
#endif

	// 보스가 죽거나 패턴이 끊겨 정리 호출을 못 받아도 남지 않도록 안전장치
	InitialLifeSpan = 20.0f;
}

void ARLThrownAxe::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터 레벨에 놓았을 때만 미리보기 (게임 중에는 InitFromWeapon이 처리)
	if (!PreviewSourceMesh || (GetWorld() && GetWorld()->IsGameWorld()))
	{
		return;
	}

	if (SetupWeaponPose(PreviewSourceMesh, PreviewWeaponBoneName))
	{
		AxeMesh->SetRelativeScale3D(FVector(PreviewScale));
		AxeMesh->SetRelativeRotation(bPreviewLandedPose ? FRotator(LandedPitch, 0.0f, 0.0f) : FRotator::ZeroRotator);
	}
}

bool ARLThrownAxe::InitFromWeapon(USkeletalMeshComponent* SourceMesh, FName WeaponBoneName)
{
	if (!SourceMesh || !SetupWeaponPose(SourceMesh->GetSkinnedAsset(), WeaponBoneName))
	{
		return false;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < SourceMesh->GetNumMaterials(); ++MaterialIndex)
	{
		AxeMesh->SetMaterial(MaterialIndex, SourceMesh->GetMaterial(MaterialIndex));
	}
	AxeMesh->SetRelativeRotation(FRotator::ZeroRotator);
	AxeMesh->SetWorldScale3D(SourceMesh->GetComponentScale());
	return true;
}

bool ARLThrownAxe::SetupWeaponPose(USkinnedAsset* SkinnedAsset, FName WeaponBoneName)
{
	if (!SkinnedAsset)
	{
		return false;
	}

	const FReferenceSkeleton& RefSkeleton = SkinnedAsset->GetRefSkeleton();
	const int32 WeaponIndex = RefSkeleton.FindBoneIndex(WeaponBoneName);
	if (WeaponIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ThrownAxe] 본 '%s'을(를) 찾을 수 없습니다."), *WeaponBoneName.ToString());
		return false;
	}

	if (AxeMesh->GetSkinnedAsset() != SkinnedAsset)
	{
		AxeMesh->SetSkinnedAssetAndUpdate(SkinnedAsset);
	}

	// 본 인덱스 순서 = 부모가 항상 자식보다 앞이므로, 순서대로 컴포넌트 공간 트랜스폼을 정하면 부모 기준 로컬 값이 맞게 계산됩니다.
	// 무기 본의 자식(도끼 장식 끈 등)은 건드리지 않아 원래 상대 위치로 무기 본을 따라갑니다.
	const FTransform CollapsedTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.001f));
	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		if (BoneIndex == WeaponIndex)
		{
			AxeMesh->SetBoneTransformByName(BoneName, FTransform(AxeRotationOffset, AxeLocationOffset), EBoneSpaces::ComponentSpace);
			continue;
		}

		bool bIsWeaponChild = false;
		for (int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex); ParentIndex != INDEX_NONE; ParentIndex = RefSkeleton.GetParentIndex(ParentIndex))
		{
			if (ParentIndex == WeaponIndex)
			{
				bIsWeaponChild = true;
				break;
			}
		}

		if (!bIsWeaponChild)
		{
			AxeMesh->SetBoneTransformByName(BoneName, CollapsedTransform, EBoneSpaces::ComponentSpace);
		}
	}

	// 보통은 컴포넌트 틱에서 반영되지만 에디터 미리보기에선 틱이 돌지 않으므로 즉시 갱신
	AxeMesh->RefreshBoneTransforms();
	return true;
}

void ARLThrownAxe::Launch(const FVector& TargetLocation, float FlightTime, float ArcHeight)
{
	StartLocation = GetActorLocation();
	EndLocation = TargetLocation - FVector(0.0f, 0.0f, EmbedDepth);
	FlightDuration = FMath::Max(FlightTime, 0.05f);
	FlightArcHeight = ArcHeight;
	FlightElapsed = 0.0f;
	FlightYaw = (EndLocation - StartLocation).GetSafeNormal2D().Rotation().Yaw;

	// 공중제비 바퀴 수를 정수로 맞춰서 마지막 각도가 정확히 LandedPitch가 되게 함 (앞으로 구르므로 음수 방향).
	// 아주 가까이 던져서 비행이 짧으면 0바퀴 = 박히는 각도로만 휙 꺾여 바로 꽂힘
	const float Turns = FMath::RoundToFloat(SpinTurnsPerSecond * FlightDuration);
	TotalPitch = -360.0f * Turns + LandedPitch;

	SetActorRotation(FRotator(0.0f, FlightYaw, 0.0f));
	bFlying = true;
}

void ARLThrownAxe::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bFlying)
	{
		return;
	}

	FlightElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(FlightElapsed / FlightDuration, 0.0f, 1.0f);

	FVector NewLocation = FMath::Lerp(StartLocation, EndLocation, Alpha);
	NewLocation.Z += 4.0f * FlightArcHeight * Alpha * (1.0f - Alpha);

	// 비행 방향(Yaw)을 축으로 세로 공중제비. FRotator → FQuat 변환은 Pitch가 ±90을 넘어도 그대로 계산되므로
	// 여러 바퀴 회전도 끊김 없이 표현됩니다
	const FQuat NewRotation = FQuat(FRotator(0.0f, FlightYaw, 0.0f)) * FQuat(FRotator(TotalPitch * Alpha, 0.0f, 0.0f));
	SetActorLocationAndRotation(NewLocation, NewRotation);

	if (Alpha >= 1.0f)
	{
		bFlying = false;
		OnLanded.Broadcast(this);
		OnAxeLanded();
	}
}
