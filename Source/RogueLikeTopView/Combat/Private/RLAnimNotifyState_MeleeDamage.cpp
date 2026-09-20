#include "RLAnimNotifyState_MeleeDamage.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

// TODO: 실제 데미지 처리 컴포넌트 헤더로 교체하세요. (예: RLHealthComponent.h)
// #include "RLHealthComponent.h"

URLAnimNotifyState_MeleeDamage::URLAnimNotifyState_MeleeDamage()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 60, 60);
#endif
}

void URLAnimNotifyState_MeleeDamage::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	HitActorsThisWindow.Reset();
}

void URLAnimNotifyState_MeleeDamage::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	PerformSweepAndApplyDamage(MeshComp);
}

void URLAnimNotifyState_MeleeDamage::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	HitActorsThisWindow.Reset();
}

void URLAnimNotifyState_MeleeDamage::PerformSweepAndApplyDamage(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp || !MeshComp->DoesSocketExist(StartSocketName) || !MeshComp->DoesSocketExist(EndSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RLAnimNotifyState_MeleeDamage] 소켓 '%s' 또는 '%s'을(를) 찾을 수 없습니다. 스켈레톤에 실제 존재하는 소켓 이름인지 확인하세요."),
			*StartSocketName.ToString(), *EndSocketName.ToString());
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor || !OwnerActor->GetWorld())
	{
		return;
	}

	const FVector Start = MeshComp->GetSocketLocation(StartSocketName);
	const FVector End = MeshComp->GetSocketLocation(EndSocketName);

	TArray<FHitResult> HitResults;
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(TraceRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerActor);

	// 주의: ECC_Pawn 채널로 스윕합니다. 프로젝트에 데미지 판정 전용 커스텀 콜리전 채널(예: ECC_GameTraceChannel1 등)이
	// 이미 정의되어 있다면 그쪽을 쓰는 게 더 안전합니다. 실제 프로젝트의 콜리전 세팅을 확인해서 교체해주세요.
	const bool bHit = OwnerActor->GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity, ECC_Pawn, Sphere, QueryParams);

	if (!bHit)
	{
		return;
	}

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == OwnerActor)
		{
			continue;
		}

		if (bOnlyHitOnce && HitActorsThisWindow.Contains(HitActor))
		{
			continue;
		}

		// TODO: 실제 데미지 적용 로직으로 교체하세요. 아래는 두 가지 대표적인 예시입니다.
		//
		// 예시 1) 언리얼 기본 데미지 이벤트를 쓰는 경우 (AActor::TakeDamage를 오버라이드해서 받는 방식):
		//     UGameplayStatics::ApplyDamage(HitActor, Damage, OwnerActor->GetInstigatorController(), OwnerActor, nullptr);
		//     (이 방식을 쓰려면 .cpp 상단에 "#include \"Kismet/GameplayStatics.h\"" 를 추가해야 합니다.)
		//
		// 예시 2) RLHealthComponent에 자체 데미지 함수가 있는 경우 (실제 함수 시그니처는
		//         RLHealthComponent.h를 직접 열어서 확인한 뒤 아래 함수 이름/인자를 맞춰주세요):
		//     if (URLHealthComponent* HealthComp = HitActor->FindComponentByClass<URLHealthComponent>())
		//     {
		//         HealthComp->TakeDamage(Damage, OwnerActor);
		//     }

		HitActorsThisWindow.Add(HitActor);
	}
}
