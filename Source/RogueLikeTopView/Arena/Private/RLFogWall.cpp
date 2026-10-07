#include "RLFogWall.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName FadeParam(TEXT("FadeAlpha"));
	const FName ColorParam(TEXT("FogColor"));
	const FName DensityParam(TEXT("Density"));
	const FName SeedParam(TEXT("Seed"));
	constexpr float DissolveTickInterval = 1.0f / 30.0f;
}

ARLFogWall::ARLFogWall()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Pawn만 막는다. 탑뷰 바닥 클릭(Ground)/카메라/가시성 트레이스는 통과시켜야 조작에 방해가 없음
	BlockVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockVolume"));
	BlockVolume->SetupAttachment(SceneRoot);
	BlockVolume->SetBoxExtent(FVector(40.0f, 300.0f, 200.0f));
	BlockVolume->SetCollisionProfileName(TEXT("Custom"));
	BlockVolume->SetCollisionObjectType(ECC_WorldStatic);
	BlockVolume->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BlockVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	BlockVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BlockVolume->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultFogMaterial(TEXT("/Game/Effect/FogWall/M_FogWall.M_FogWall"));
	if (DefaultFogMaterial.Succeeded())
	{
		FogMaterial = DefaultFogMaterial.Object;
	}

	UStaticMesh* LayerMesh = PlaneMesh.Object;
	auto SetupLayer = [this, LayerMesh](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Layer = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Layer->SetupAttachment(SceneRoot);
		if (LayerMesh)
		{
			Layer->SetStaticMesh(LayerMesh);
		}
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->SetCanEverAffectNavigation(false);
		return Layer;
	};
	FogLayerFront = SetupLayer(TEXT("FogLayerFront"));
	FogLayerBack = SetupLayer(TEXT("FogLayerBack"));
}

void ARLFogWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateLayerLayout();

	if (FogMaterial)
	{
		FogLayerFront->SetMaterial(0, FogMaterial);
		FogLayerBack->SetMaterial(0, FogMaterial);
	}
}

void ARLFogWall::UpdateLayerLayout()
{
	// 액터 원점 = 바닥. 박스는 바닥 위로 세우고, 안개 평면은 바닥 아래로 SinkBelowFloor만큼 묻어서 아래 가장자리 페이드를 숨김
	const FVector Extent = BlockVolume->GetUnscaledBoxExtent();
	BlockVolume->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Z));

	const float Height = Extent.Z * 2.0f + SinkBelowFloor;
	const float Width = Extent.Y * 2.0f;
	const float CenterZ = Extent.Z - SinkBelowFloor * 0.5f;

	// 기본 Plane(100x100, +Z 법선)을 Pitch 90으로 세우면 로컬 X가 높이, 로컬 Y가 폭이 됨
	const FRotator Upright(90.0f, 0.0f, 0.0f);
	const FVector LayerScale(Height / 100.0f, Width / 100.0f, 1.0f);

	FogLayerFront->SetRelativeLocationAndRotation(FVector(-LayerSpacing * 0.5f, 0.0f, CenterZ), Upright);
	FogLayerFront->SetRelativeScale3D(LayerScale);
	FogLayerBack->SetRelativeLocationAndRotation(FVector(LayerSpacing * 0.5f, 0.0f, CenterZ), Upright);
	FogLayerBack->SetRelativeScale3D(LayerScale);
}

void ARLFogWall::BeginPlay()
{
	Super::BeginPlay();

	FogMIDs.Reset();
	int32 LayerIndex = 0;
	for (UStaticMeshComponent* Layer : { FogLayerFront.Get(), FogLayerBack.Get() })
	{
		if (UMaterialInstanceDynamic* MID = Layer->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(ColorParam, FogColor);
			MID->SetScalarParameterValue(DensityParam, FogDensity);
			// 두 장이 같은 무늬로 겹치지 않도록 노이즈 위상을 다르게
			MID->SetScalarParameterValue(SeedParam, 137.0f * LayerIndex);
			FogMIDs.Add(MID);
		}
		++LayerIndex;
	}
	ApplyFade(1.0f);
}

void ARLFogWall::Dissolve()
{
	if (!bBlocking)
	{
		return;
	}
	bBlocking = false;

	// 길은 즉시 열고, 안개만 서서히 걷어냄
	BlockVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DissolveElapsed = 0.0f;
	GetWorldTimerManager().SetTimer(DissolveTimerHandle, this, &ARLFogWall::TickDissolve, DissolveTickInterval, true);
	BP_OnDissolveStarted();
}

void ARLFogWall::TickDissolve()
{
	DissolveElapsed += DissolveTickInterval;
	const float Alpha = 1.0f - FMath::Clamp(DissolveElapsed / DissolveDuration, 0.0f, 1.0f);
	ApplyFade(FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f));

	if (Alpha <= 0.0f)
	{
		GetWorldTimerManager().ClearTimer(DissolveTimerHandle);
		SetActorHiddenInGame(true);
	}
}

void ARLFogWall::Restore()
{
	GetWorldTimerManager().ClearTimer(DissolveTimerHandle);
	bBlocking = true;
	SetActorHiddenInGame(false);
	BlockVolume->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ApplyFade(1.0f);
}

void ARLFogWall::ApplyFade(float Alpha)
{
	for (UMaterialInstanceDynamic* MID : FogMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(FadeParam, Alpha);
		}
	}
}
