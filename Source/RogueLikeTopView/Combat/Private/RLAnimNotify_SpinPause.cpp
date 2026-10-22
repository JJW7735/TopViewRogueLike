#include "RLAnimNotify_SpinPause.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RLCharacter.h"

URLAnimNotify_SpinPause::URLAnimNotify_SpinPause()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(120, 255, 60);
#endif
}

void URLAnimNotify_SpinPause::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!Montage || !AnimInstance || !MeshComp->GetWorld() || SpinStates.Contains(MeshComp))
	{
		return;
	}

	FSpinState& State = SpinStates.Add(MeshComp);
	State.Montage = Montage;
	State.OriginalRelativeRotation = MeshComp->GetRelativeRotation();
	State.OriginalPlayRate = AnimInstance->Montage_GetPlayRate(Montage);
	AnimInstance->Montage_SetPlayRate(Montage, State.OriginalPlayRate * GetPlayRateScale(0.0f));

	MeshComp->GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &URLAnimNotify_SpinPause::TickSpin, TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)));
}

void URLAnimNotify_SpinPause::TickSpin(TWeakObjectPtr<USkeletalMeshComponent> WeakMesh)
{
	USkeletalMeshComponent* MeshComp = WeakMesh.Get();
	FSpinState* State = SpinStates.Find(WeakMesh);
	if (!MeshComp || !State)
	{
		SpinStates.Remove(WeakMesh);
		return;
	}

	// 회전 도중 사망 등으로 몽타주가 다른 걸로 끊겼으면 재개하지 않고 방향만 원복
	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	UWorld* World = MeshComp->GetWorld();
	if (!AnimInstance || !World || !AnimInstance->Montage_IsActive(State->Montage.Get()))
	{
		FinishSpin(MeshComp, false);
		return;
	}

	const float DeltaTime = World->GetDeltaSeconds();
	State->Elapsed += DeltaTime;

	const float Alpha = FMath::Clamp(State->Elapsed / SpinDuration, 0.0f, 1.0f);
	const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, EaseExponent);
	const float Direction = bCounterClockwise ? -1.0f : 1.0f;
	MeshComp->SetRelativeRotation(State->OriginalRelativeRotation + FRotator(0.0f, 360.0f * SpinTurns * EasedAlpha * Direction, 0.0f));
	AnimInstance->Montage_SetPlayRate(State->Montage.Get(), State->OriginalPlayRate * GetPlayRateScale(State->Elapsed));

	if (bDealDamage)
	{
		State->HitTimer += DeltaTime;
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

	if (Alpha >= 1.0f)
	{
		FinishSpin(MeshComp, true);
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &URLAnimNotify_SpinPause::TickSpin, WeakMesh));
}

void URLAnimNotify_SpinPause::FinishSpin(USkeletalMeshComponent* MeshComp, bool bRestorePlayRate)
{
	const FSpinState* State = SpinStates.Find(MeshComp);
	if (!State)
	{
		return;
	}

	MeshComp->SetRelativeRotation(State->OriginalRelativeRotation);

	if (bRestorePlayRate)
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_SetPlayRate(State->Montage.Get(), State->OriginalPlayRate);
		}
	}

	SpinStates.Remove(MeshComp);
}

float URLAnimNotify_SpinPause::GetPlayRateScale(float Elapsed) const
{
	// 시작 SlowDownTime 동안 1 → 0, 마지막 SpeedUpTime 동안 0 → 1 (SmoothStep으로 부드럽게)
	const float Remaining = SpinDuration - Elapsed;
	if (SpeedUpTime > 0.0f && Remaining < SpeedUpTime)
	{
		return FMath::SmoothStep(0.0f, 1.0f, 1.0f - Remaining / SpeedUpTime);
	}
	if (SlowDownTime > 0.0f && Elapsed < SlowDownTime)
	{
		return 1.0f - FMath::SmoothStep(0.0f, 1.0f, Elapsed / SlowDownTime);
	}
	return 0.0f;
}
