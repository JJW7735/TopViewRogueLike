#include "RLAnimNotifyState_Spin.h"
#include "Components/SkeletalMeshComponent.h"
#include "RLCharacter.h"

URLAnimNotifyState_Spin::URLAnimNotifyState_Spin()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(120, 255, 60);
#endif
}

void URLAnimNotifyState_Spin::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp)
	{
		return;
	}

	FSpinState& State = SpinStates.FindOrAdd(MeshComp);
	State.OriginalRelativeRotation = MeshComp->GetRelativeRotation();
	State.Elapsed = 0.0f;
	State.TotalDuration = TotalDuration;
	State.HitTimer = 0.0f;
}

void URLAnimNotifyState_Spin::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	FSpinState* State = MeshComp ? SpinStates.Find(MeshComp) : nullptr;
	if (!State)
	{
		return;
	}

	State->Elapsed += FrameDeltaTime;

	// 시작/끝 RampTime 구간에서 속도를 0→1→0으로 보간
	float SpeedScale = 1.0f;
	if (RampTime > KINDA_SMALL_NUMBER)
	{
		const float Remaining = State->TotalDuration - State->Elapsed;
		SpeedScale = FMath::Clamp(FMath::Min(State->Elapsed, Remaining) / RampTime, 0.0f, 1.0f);
	}

	MeshComp->AddRelativeRotation(FRotator(0.0f, YawSpeed * SpeedScale * FrameDeltaTime, 0.0f));

	if (!bDealDamage)
	{
		return;
	}

	State->HitTimer += FrameDeltaTime;
	if (State->HitTimer >= HitInterval)
	{
		State->HitTimer -= HitInterval;
		// 몽타주 에디터 미리보기에서는 오너가 ARLCharacter가 아니므로 판정 없이 회전만 합니다.
		if (ARLCharacter* OwnerCharacter = Cast<ARLCharacter>(MeshComp->GetOwner()))
		{
			OwnerCharacter->CreateSectorHitbox(HitRadius, 0.0f, DamageData, bDrawDebug);
		}
	}
}

void URLAnimNotifyState_Spin::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (const FSpinState* State = MeshComp ? SpinStates.Find(MeshComp) : nullptr)
	{
		MeshComp->SetRelativeRotation(State->OriginalRelativeRotation);
		SpinStates.Remove(MeshComp);
	}
}
