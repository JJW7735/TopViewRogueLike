#include "RLSkillComponent.h"
#include "RLCharacter.h"
#include "RLPlayerController.h"
#include "RLDecalIndicator.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RLStatComponent.h"
#include "RLProjectile.h"
#include "RLGroundAreaEffect.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Animation/AnimInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

URLSkillComponent::URLSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URLSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ARLCharacter>(GetOwner());

	if (GetWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = OwnerCharacter;
		DecalIndicator = GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

		if (DecalIndicator)
		{
			DecalIndicator->SetIndicatorVisibility(false);
		}
	}
}

void URLSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 1. 장판 스킬 조준 중 인디케이터 업데이트
	if (CurrentSkillState == EPlayerSkillState::Aiming && DecalIndicator && OwnerCharacter)
	{
		if (URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot))
		{
			DecalIndicator->UpdateIndicatorTransform(OwnerCharacter->GetActorLocation(), GetMouseGroundLocation(), Skill->MaxRange);
		}
	}

	// 2. 캐스팅(시전) 상태 처리
	if (CurrentSkillState == EPlayerSkillState::Casting)
	{
		URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
		if (!Skill)
		{
			InterruptCasting(TEXT("Missing Skill Data"));
			return;
		}

		CurrentAccumulatedTime += DeltaTime;

		// 캐스팅 종료 0.5초 전 선행 애니메이션(End) 점프 처리
		const float PreMotionLeadTime = 0.5f;
		const float MotionTriggerTime = FMath::Max(0.0f, Skill->CastDuration - PreMotionLeadTime);

		if (!bHasTriggeredPreCastMotion && CurrentAccumulatedTime >= MotionTriggerTime)
		{
			bHasTriggeredPreCastMotion = true;
			if (Skill->SkillMontage && OwnerCharacter)
			{
				if (UAnimInstance* AnimInst = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr)
				{
					AnimInst->Montage_JumpToSection(FName("End"), Skill->SkillMontage);
				}
			}
		}

		float Ratio = FMath::Clamp(CurrentAccumulatedTime / FMath::Max(0.01f, Skill->CastDuration), 0.0f, 1.0f);
		OnCastProgressChanged.Broadcast(CurrentActiveSlot, CurrentAccumulatedTime, Skill->CastDuration, Ratio);

		if (Skill->CastType != ESkillCastType::GroundTarget)
		{
			RotateOwnerToMouseCursor();
		}

		if (CurrentAccumulatedTime >= Skill->CastDuration)
		{
			FinishCasting();
		}
	}

	// 3. 쿨타임 UI 브로드캐스트
	for (const auto& Pair : EquippedSkills)
	{
		if (URLSkillData* Skill = Pair.Value)
		{
			OnSkillCooldownChanged.Broadcast(Pair.Key, GetRemainingCooldown(Pair.Key), Skill->Cooldown);
		}
	}
}

// ========================================================
// 스킬 입력 처리 로직
// ========================================================
void URLSkillComponent::OnSkillInputStarted(ESkillSlot Slot)
{
	if (OwnerCharacter && OwnerCharacter->IsDashing()) return;
	if (!EquippedSkills.Contains(Slot) || !EquippedSkills[Slot]) return;
	if (IsOnCooldown(Slot)) return;

	URLSkillData* Skill = EquippedSkills[Slot];

	if (Skill->CastType == ESkillCastType::GroundTarget)
	{
		StartGroundTargetAiming(Slot);
		return;
	}

	if (CurrentSkillState == EPlayerSkillState::Aiming)
	{
		CancelGroundCast();
	}

	if (CurrentSkillState != EPlayerSkillState::Idle) return;

	CurrentActiveSlot = Slot;
	CurrentAccumulatedTime = 0.0f;
	LastHoldTickTime = 0.0f;

	if (Skill->CastType != ESkillCastType::Dash)
	{
		StopPlayerMovement();
	}
	RotateOwnerToMouseCursor();

	switch (Skill->CastType)
	{
	case ESkillCastType::Instant:
	{
		UAnimMontage* MontageToPlay = Skill->SkillMontage;

		// 기본 공격(LMB) 콤보 시스템 처리
		if (Slot == ESkillSlot::Slot_LMB && Skill->bUseCombo && Skill->ComboMontages.Num() > 0)
		{
			if (ComboCount > 0 && ComboCount - 1 < Skill->ComboMontages.Num())
			{
				MontageToPlay = Skill->ComboMontages[ComboCount - 1];
			}
			ComboCount = (ComboCount + 1) % (Skill->ComboMontages.Num() + 1);

			GetWorld()->GetTimerManager().ClearTimer(ComboResetTimerHandle);
			GetWorld()->GetTimerManager().SetTimer(ComboResetTimerHandle, this, &URLSkillComponent::ResetCombo, Skill->ComboResetTime, false);
		}

		if (MontageToPlay) OwnerCharacter->PlayAnimMontage(MontageToPlay);

		if (Skill->ReleaseVFX)
		{
			SpawnSkillVFX(Skill->ReleaseVFX, OwnerCharacter->GetActorLocation(), Skill->bAttachToWeapon, Skill->WeaponSocketName);
		}

		if (!Skill->bHitOnAnimNotify)
		{
			OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, CalculateFinalDamage(Skill), true);
		}

		CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
		CurrentSkillState = EPlayerSkillState::Idle;
		break;
	}

	case ESkillCastType::Projectile:
		if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);

		if (Skill->ProjectileClass)
		{
			FVector SpawnLoc = OwnerCharacter->GetActorLocation() + (OwnerCharacter->GetActorForwardVector() * Skill->ProjectileSpawnForwardOffset);
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = OwnerCharacter;
			SpawnParams.Instigator = OwnerCharacter;

			if (ARLProjectile* SpawnedProjectile = GetWorld()->SpawnActor<ARLProjectile>(Skill->ProjectileClass, SpawnLoc, OwnerCharacter->GetActorRotation(), SpawnParams))
			{
				SpawnedProjectile->DamageData = CalculateFinalDamage(Skill);
				SpawnedProjectile->MovementComp->Velocity = OwnerCharacter->GetActorRotation().Vector() * Skill->ProjectileSpeed;
			}
		}
		CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
		CurrentSkillState = EPlayerSkillState::Idle;
		break;

	case ESkillCastType::Cast:
		StartCasting(Slot);
		break;

	case ESkillCastType::Dash:
		if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);

		OwnerCharacter->StartDashMovement(OwnerCharacter->GetActorLocation() + OwnerCharacter->GetActorForwardVector() * (Skill->DashPower * 0.3f), 0.15f);
		OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, CalculateFinalDamage(Skill), true);

		CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
		CurrentSkillState = EPlayerSkillState::Idle;
		break;

	case ESkillCastType::Charge:
	case ESkillCastType::ChargeDash:
		CurrentSkillState = EPlayerSkillState::Charging;
		if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
		break;

	case ESkillCastType::Buff:
		if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
		if (Skill->ReleaseVFX)
		{
			ActiveBuffVFXComp = SpawnSkillVFX(Skill->ReleaseVFX, OwnerCharacter->GetActorLocation(), true, NAME_None);
		}

		if (URLStatComponent* StatComp = OwnerCharacter->GetStatComponent())
		{
			// 이동속도 스탯에 GE로 보너스를 겁니다 (MaxWalkSpeed는 StatComp가 자동 반영).
			// 예전엔 MaxWalkSpeed를 직접 곱해서 재시전하면 배율이 중첩되고, 끝나면 600으로 고정돼 이속 보너스가 사라졌음.
			StatComp->RemoveStatBonusEffect(SpeedBuffEffectHandle); // 재시전: 이전 버프를 먼저 해제해 중첩 방지

			const float SpeedBonus = StatComp->MoveSpeed * (Skill->SpeedMultiplier - 1.0f);
			SpeedBuffEffectHandle = StatComp->ApplyRemovableStatBonuses({ { ERLTargetStat::MoveSpeed, SpeedBonus } });
			GetWorld()->GetTimerManager().SetTimer(SpeedBuffTimerHandle, this, &URLSkillComponent::EndSpeedBuff, Skill->BuffDuration, false);
		}

		CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
		CurrentSkillState = EPlayerSkillState::Idle;
		break;

	case ESkillCastType::Hold:
		CurrentSkillState = EPlayerSkillState::Holding;
		if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
		break;

	default:
		break;
	}
}

void URLSkillComponent::OnSkillInputTriggered(ESkillSlot Slot)
{
	if (OwnerCharacter && OwnerCharacter->IsDashing()) return;
	if (CurrentActiveSlot != Slot) return;

	URLSkillData* Skill = EquippedSkills.FindRef(Slot);
	if (!Skill) return;

	if (CurrentSkillState == EPlayerSkillState::Holding)
	{
		RotateOwnerToMouseCursor();
		CurrentAccumulatedTime += GetWorld()->GetDeltaSeconds();

		if (CurrentAccumulatedTime - LastHoldTickTime >= Skill->TickDamageInterval)
		{
			LastHoldTickTime = CurrentAccumulatedTime;
			OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, CalculateFinalDamage(Skill), false);
		}

		if (CurrentAccumulatedTime >= Skill->CastDuration)
		{
			OnSkillInputCompleted(Slot);
		}
	}
	else if (CurrentSkillState == EPlayerSkillState::Charging)
	{
		RotateOwnerToMouseCursor();
		CurrentAccumulatedTime = FMath::Min(CurrentAccumulatedTime + GetWorld()->GetDeltaSeconds(), Skill->CastDuration);

		if (CurrentAccumulatedTime >= Skill->CastDuration)
		{
			CurrentAccumulatedTime = Skill->CastDuration;
			OnSkillInputCompleted(Slot);
		}
	}
}

void URLSkillComponent::OnSkillInputCompleted(ESkillSlot Slot)
{
	if (CurrentSkillState == EPlayerSkillState::Casting || CurrentSkillState == EPlayerSkillState::Aiming || CurrentActiveSlot != Slot) return;

	URLSkillData* Skill = EquippedSkills.FindRef(Slot);
	if (!Skill) return;

	if (CurrentSkillState == EPlayerSkillState::Charging)
	{
		float Multiplier = 1.0f;
		if (CurrentAccumulatedTime >= Skill->MinChargeTime)
		{
			float Denominator = FMath::Max(0.01f, Skill->CastDuration - Skill->MinChargeTime);
			float Alpha = FMath::Clamp((CurrentAccumulatedTime - Skill->MinChargeTime) / Denominator, 0.0f, 1.0f);
			Multiplier = FMath::Lerp(1.0f, Skill->MaxDamageMultiplier, Alpha);
		}

		if (Skill->CastType == ESkillCastType::ChargeDash && CurrentAccumulatedTime >= Skill->MinChargeTime)
		{
			OwnerCharacter->LaunchClampedToNav(OwnerCharacter->GetActorForwardVector() * Skill->DashPower, true, true);
		}

		FRLDamageData FinalDamage = CalculateFinalDamage(Skill);
		FinalDamage.BaseDamage *= Multiplier;

		OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, FinalDamage, true);
		CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
	}
	else if (CurrentSkillState == EPlayerSkillState::Holding)
	{
		if (Skill->SkillMontage) OwnerCharacter->StopAnimMontage(Skill->SkillMontage);
		CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
	}

	CurrentSkillState = EPlayerSkillState::Idle;
	CurrentAccumulatedTime = 0.0f;
}

// ========================================================
// 캐스팅 및 상태 관리 함수
// ========================================================
void URLSkillComponent::ConfirmGroundCast()
{
	if (CurrentSkillState != EPlayerSkillState::Aiming || !DecalIndicator || !OwnerCharacter) return;

	URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
	if (!Skill) return;

	PendingExplosionLocation = DecalIndicator->GetTargetLocation();

	FVector Dir = (PendingExplosionLocation - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		OwnerCharacter->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	}
	StopPlayerMovement();

	if (Skill->CastDuration > 0.0f)
	{
		CurrentSkillState = EPlayerSkillState::Casting;
		CurrentAccumulatedTime = 0.0f;
		bHasTriggeredPreCastMotion = false;

		DecalIndicator->LockTargetLocation(PendingExplosionLocation);

		if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);

		if (Skill->ChargeVFX)
		{
			ActiveCastParticleComp = SpawnSkillVFX(Skill->ChargeVFX, PendingExplosionLocation, Skill->bAttachToWeapon, Skill->WeaponSocketName);
		}

		OnCastStateChanged.Broadcast(CurrentActiveSlot, true);
	}
	else
	{
		if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
		if (Skill->ReleaseVFX) SpawnSkillVFX(Skill->ReleaseVFX, PendingExplosionLocation, Skill->bAttachToWeapon, Skill->WeaponSocketName);

		if (Skill->GroundActorClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = OwnerCharacter;
			SpawnParams.Instigator = OwnerCharacter;
			if (AActor* SpawnedGroundActor = GetWorld()->SpawnActor<AActor>(Skill->GroundActorClass, PendingExplosionLocation, FRotator::ZeroRotator, SpawnParams))
			{
				if (ARLGroundAreaEffect* GroundEffect = Cast<ARLGroundAreaEffect>(SpawnedGroundActor))
				{
					GroundEffect->DamageData = CalculateFinalDamage(Skill);
				}
			}
		}
		else
		{
			OwnerCharacter->CreateLocationHitbox(PendingExplosionLocation, Skill->IndicatorRadius, CalculateFinalDamage(Skill), true);
		}

		CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
		DecalIndicator->SetIndicatorVisibility(false);
		CurrentSkillState = EPlayerSkillState::Idle;
	}
}

void URLSkillComponent::FinishCasting()
{
	if (CurrentSkillState != EPlayerSkillState::Casting || !OwnerCharacter) return;

	URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
	if (Skill)
	{
		if (Skill->CastType == ESkillCastType::GroundTarget)
		{
			if (ActiveCastParticleComp)
			{
				ActiveCastParticleComp->DestroyComponent();
				ActiveCastParticleComp = nullptr;
			}

			if (!bHasTriggeredPreCastMotion && Skill->SkillMontage)
			{
				if (UAnimInstance* AnimInst = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr)
				{
					AnimInst->Montage_JumpToSection(FName("End"), Skill->SkillMontage);
				}
			}

			if (Skill->ReleaseVFX)
			{
				SpawnSkillVFX(Skill->ReleaseVFX, PendingExplosionLocation, Skill->bAttachToWeapon, Skill->WeaponSocketName);
			}

			if (Skill->GroundActorClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = OwnerCharacter;
				SpawnParams.Instigator = OwnerCharacter;
				if (AActor* SpawnedGroundActor = GetWorld()->SpawnActor<AActor>(Skill->GroundActorClass, PendingExplosionLocation, FRotator::ZeroRotator, SpawnParams))
				{
					if (ARLGroundAreaEffect* GroundEffect = Cast<ARLGroundAreaEffect>(SpawnedGroundActor))
					{
						GroundEffect->DamageData = CalculateFinalDamage(Skill);
					}
				}
			}
			else
			{
				OwnerCharacter->CreateLocationHitbox(PendingExplosionLocation, Skill->IndicatorRadius, CalculateFinalDamage(Skill), true);
			}
		}
		else
		{
			OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, CalculateFinalDamage(Skill), true);
		}

		CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
	}

	if (DecalIndicator) DecalIndicator->SetIndicatorVisibility(false);

	OnCastStateChanged.Broadcast(CurrentActiveSlot, false);
	CurrentSkillState = EPlayerSkillState::Idle;
	CurrentAccumulatedTime = 0.0f;
	bHasTriggeredPreCastMotion = false;
}

void URLSkillComponent::InterruptCasting(const FString& Reason)
{
	if (CurrentSkillState != EPlayerSkillState::Casting) return;

	URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
	if (OwnerCharacter && Skill && Skill->SkillMontage)
	{
		OwnerCharacter->StopAnimMontage(Skill->SkillMontage);
	}

	if (ActiveCastParticleComp)
	{
		ActiveCastParticleComp->Deactivate();
		ActiveCastParticleComp = nullptr;
	}

	if (DecalIndicator) DecalIndicator->SetIndicatorVisibility(false);

	OnCastStateChanged.Broadcast(CurrentActiveSlot, false);
	CurrentSkillState = EPlayerSkillState::Idle;
	CurrentAccumulatedTime = 0.0f;
	bHasTriggeredPreCastMotion = false;
}

// ========================================================
// 애니메이션 노티파이 및 유틸리티 
// ========================================================
void URLSkillComponent::TriggerActiveSkillHit()
{
	if (!OwnerCharacter) return;
	if (URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot))
	{
		OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, CalculateFinalDamage(Skill), true);
	}
}

void URLSkillComponent::TriggerActiveSkillDash()
{
	if (!OwnerCharacter) return;
	if (URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot))
	{
		if (Skill->CastType == ESkillCastType::Dash || Skill->CastType == ESkillCastType::ChargeDash)
		{
			FVector LaunchVelocity = OwnerCharacter->GetActorForwardVector() * Skill->DashPower;
			OwnerCharacter->LaunchClampedToNav(LaunchVelocity, true, false);
		}
	}
}

void URLSkillComponent::CancelGroundCast()
{
	if (CurrentSkillState != EPlayerSkillState::Aiming) return;
	CurrentSkillState = EPlayerSkillState::Idle;
	if (DecalIndicator) DecalIndicator->SetIndicatorVisibility(false);
}

void URLSkillComponent::StartCasting(ESkillSlot Slot)
{
	URLSkillData* Skill = EquippedSkills.FindRef(Slot);
	if (!Skill || !OwnerCharacter) return;

	CurrentSkillState = EPlayerSkillState::Casting;
	CurrentActiveSlot = Slot;
	CurrentAccumulatedTime = 0.0f;
	bHasTriggeredPreCastMotion = false;

	StopPlayerMovement();
	if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);

	OnCastStateChanged.Broadcast(Slot, true);
}

void URLSkillComponent::StartGroundTargetAiming(ESkillSlot Slot)
{
	URLSkillData* Skill = EquippedSkills.FindRef(Slot);
	if (!Skill || !DecalIndicator) return;

	CurrentSkillState = EPlayerSkillState::Aiming;
	CurrentActiveSlot = Slot;

	DecalIndicator->SetupIndicator(EIndicatorType::Circle, Skill->IndicatorRadius, Skill->MaxRange);
	DecalIndicator->SetIndicatorVisibility(true);
}

void URLSkillComponent::EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill)
{
	EquippedSkills.Add(Slot, NewSkill);
}

bool URLSkillComponent::IsOnCooldown(ESkillSlot Slot) const
{
	if (!CooldownEndTimes.Contains(Slot)) return false;
	return GetWorld()->GetTimeSeconds() < CooldownEndTimes[Slot];
}

float URLSkillComponent::GetRemainingCooldown(ESkillSlot Slot) const
{
	if (!IsOnCooldown(Slot)) return 0.0f;
	return FMath::Max(0.0f, CooldownEndTimes[Slot] - GetWorld()->GetTimeSeconds());
}

void URLSkillComponent::StopPlayerMovement()
{
	if (!OwnerCharacter) return;
	if (OwnerCharacter->GetCharacterMovement()) OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
	if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController())) PC->StopMovement();
}

void URLSkillComponent::RotateOwnerToMouseCursor()
{
	if (!OwnerCharacter) return;
	if (ARLPlayerController* PC = Cast<ARLPlayerController>(OwnerCharacter->GetController()))
	{
		FHitResult Hit;
		if (PC->GetGroundHitUnderCursor(Hit))
		{
			FVector LookDir = (Hit.ImpactPoint - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
			if (!LookDir.IsNearlyZero())
			{
				OwnerCharacter->SetActorRotation(FRotator(0.f, LookDir.Rotation().Yaw, 0.f));
			}
		}
	}
}

FVector URLSkillComponent::GetMouseGroundLocation() const
{
	if (!OwnerCharacter) return FVector::ZeroVector;
	if (ARLPlayerController* PC = Cast<ARLPlayerController>(OwnerCharacter->GetController()))
	{
		FHitResult Hit;
		if (PC->GetGroundHitUnderCursor(Hit)) return Hit.ImpactPoint;
	}
	return OwnerCharacter->GetActorLocation();
}

void URLSkillComponent::EndSpeedBuff()
{
	if (URLStatComponent* StatComp = OwnerCharacter ? OwnerCharacter->GetStatComponent() : nullptr)
	{
		StatComp->RemoveStatBonusEffect(SpeedBuffEffectHandle);
	}

	if (ActiveBuffVFXComp)
	{
		ActiveBuffVFXComp->Deactivate();
		ActiveBuffVFXComp = nullptr;
	}
}

void URLSkillComponent::ResetCombo()
{
	ComboCount = 0;
}

// ========================================================
// 시스템 헬퍼 (VFX 스폰 & 데미지 계산)
// ========================================================
UFXSystemComponent* URLSkillComponent::SpawnSkillVFX(UFXSystemAsset* VFXAsset, FVector Location, bool bAttach, FName SocketName)
{
	if (!VFXAsset || !OwnerCharacter) return nullptr;

	USceneComponent* TargetAttachComp = OwnerCharacter->GetMesh();

	if (bAttach && SocketName != NAME_None)
	{
		TArray<UMeshComponent*> AllMeshes;
		OwnerCharacter->GetComponents<UMeshComponent>(AllMeshes);

		for (UMeshComponent* MeshComp : AllMeshes)
		{
			if (MeshComp->DoesSocketExist(SocketName))
			{
				TargetAttachComp = MeshComp;
				break;
			}
		}
	}

	if (UNiagaraSystem* NiagaraSys = Cast<UNiagaraSystem>(VFXAsset))
	{
		if (bAttach && TargetAttachComp)
		{
			return Cast<UFXSystemComponent>(UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSys, TargetAttachComp, SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true));
		}
		return Cast<UFXSystemComponent>(UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraSys, Location, FRotator::ZeroRotator));
	}
	else if (UParticleSystem* CascadeSys = Cast<UParticleSystem>(VFXAsset))
	{
		if (bAttach && TargetAttachComp)
		{
			return Cast<UFXSystemComponent>(UGameplayStatics::SpawnEmitterAttached(CascadeSys, TargetAttachComp, SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true));
		}
		return Cast<UFXSystemComponent>(UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), CascadeSys, Location, FRotator::ZeroRotator));
	}

	return nullptr;
}

FRLDamageData URLSkillComponent::CalculateFinalDamage(URLSkillData* Skill)
{
	FRLDamageData FinalDamage = Skill->DamageData;
	FinalDamage.DamageCauser = OwnerCharacter;
	FinalDamage.bCanTriggerSoulCrack |= Skill->bCanTriggerSoulCrack;

	if (OwnerCharacter && OwnerCharacter->GetStatComponent())
	{
		URLStatComponent* Stat = OwnerCharacter->GetStatComponent();
		float BonusDamage = (Stat->AttackPower * Skill->ADScaling) + (Stat->AbilityPower * Skill->APScaling);
		float RawDamage = Skill->DamageData.BaseDamage + BonusDamage;

		// ★ 치명타 판정 및 데미지 계산
		FinalDamage.BaseDamage = Stat->EvaluateDamage(RawDamage, FinalDamage.bIsCritical);

		// [Debug] 최종 데미지 로그 출력
		if (FinalDamage.bIsCritical)
		{
			FString Msg = FString::Printf(TEXT("⚡ [CRITICAL HIT!] Base(%.1f) + Bonus(%.1f) * Crit(x%.1f) = Final(%.1f)"),
				Skill->DamageData.BaseDamage, BonusDamage, Stat->CritDamageMultiplier, FinalDamage.BaseDamage);
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange, Msg);
		}
		else
		{
			FString Msg = FString::Printf(TEXT("Skill Base(%.1f) + Bonus(%.1f) = Final Damage(%.1f)"), 
				Skill->DamageData.BaseDamage, BonusDamage, FinalDamage.BaseDamage);
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Msg);
		}
	}

	return FinalDamage;
}