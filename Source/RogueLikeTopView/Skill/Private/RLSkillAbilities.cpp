#include "RLSkillAbilities.h"
#include "RLCharacter.h"
#include "RLSkillComponent.h"
#include "RLStatComponent.h"
#include "RLProjectile.h"
#include "RLGroundAreaEffect.h"
#include "RLDecalIndicator.h"
#include "RLAttributeSet.h"
#include "RLGameplayEffects.h"
#include "RLGameplayTags.h"
#include "RLCombatFeedbackSubsystem.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

// =====================================================================
// 즉발 근접 (Instant)
// =====================================================================
void URLGA_Skill_Instant::ActivateSkill()
{
	PrepareCaster(true);

	UAnimMontage* MontageToPlay = Skill->SkillMontage;

	// 기본 공격(LMB) 콤보: 0번째는 SkillMontage, 이후 ComboMontages 순서대로, 일정 시간 입력이 없으면 처음으로
	if (Slot == ESkillSlot::Slot_LMB && Skill->bUseCombo && Skill->ComboMontages.Num() > 0)
	{
		if (ComboCount > 0 && ComboCount - 1 < Skill->ComboMontages.Num())
		{
			MontageToPlay = Skill->ComboMontages[ComboCount - 1];
		}
		ComboCount = (ComboCount + 1) % (Skill->ComboMontages.Num() + 1);

		FTimerManager& TimerManager = Caster->GetWorldTimerManager();
		TimerManager.ClearTimer(ComboResetTimerHandle);
		TimerManager.SetTimer(ComboResetTimerHandle, this, &URLGA_Skill_Instant::ResetCombo, Skill->ComboResetTime, false);
	}

	PlaySkillMontage(MontageToPlay);

	if (Skill->ReleaseVFX)
	{
		ExecuteSkillCue(RLGameplayTags::GameplayCue_Skill_Release, Caster->GetActorLocation());
	}

	// bHitOnAnimNotify면 히트는 ABP의 TriggerActiveSkillHit 노티파이가 HandleAnimHitNotify로 처리
	if (!Skill->bHitOnAnimNotify)
	{
		CreateSkillBoxHitbox();
	}

	CommitSkillCooldown();
	FinishSkill();
}

// =====================================================================
// 투사체 (Projectile)
// =====================================================================
void URLGA_Skill_Projectile::ActivateSkill()
{
	PrepareCaster(true);
	PlaySkillMontage(Skill->SkillMontage);

	if (Skill->ProjectileClass)
	{
		const FVector SpawnLoc = Caster->GetActorLocation() + (Caster->GetActorForwardVector() * Skill->ProjectileSpawnForwardOffset);
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Caster;
		SpawnParams.Instigator = Caster;

		if (ARLProjectile* Projectile = Caster->GetWorld()->SpawnActor<ARLProjectile>(Skill->ProjectileClass, SpawnLoc, Caster->GetActorRotation(), SpawnParams))
		{
			Projectile->DamageData = BuildDamageData(Caster, Skill);
			Projectile->SourceSkill = Skill;
			Projectile->MovementComp->Velocity = Caster->GetActorRotation().Vector() * Skill->ProjectileSpeed;

			if (URLCombatFeedbackSubsystem* FeedbackSub = URLCombatFeedbackSubsystem::Get(Caster))
			{
				FeedbackSub->RegisterDamageSourceActor(Projectile, Caster, Projectile->DamageData, Skill);
			}
		}
	}

	CommitSkillCooldown();
	FinishSkill();
}

// =====================================================================
// 돌진 (Dash)
// =====================================================================
void URLGA_Skill_Dash::ActivateSkill()
{
	// 돌진은 이동 중에도 바로 나가도록 이동 정지 없이 방향만 맞춤
	PrepareCaster(false);
	PlaySkillMontage(Skill->SkillMontage);

	Caster->StartDashMovement(Caster->GetActorLocation() + Caster->GetActorForwardVector() * (Skill->DashPower * 0.3f), 0.15f);
	CreateSkillBoxHitbox();

	CommitSkillCooldown();
	FinishSkill();
}

// =====================================================================
// 버프 (Buff)
// =====================================================================
void URLGA_Skill_Buff::ActivateSkill()
{
	PrepareCaster(true);
	PlaySkillMontage(Skill->SkillMontage);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	URLStatComponent* StatComp = Caster->GetStatComponent();

	// 재시전: 이전 버프를 먼저 정리해 배율이 중첩되지 않게 함 (GE가 빠지면서 오라 큐도 같이 꺼짐)
	if (ASC && BuffEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(BuffEffectHandle);
		BuffEffectHandle.Invalidate();
	}

	if (ASC && StatComp)
	{
		// 현재 이동속도 기준 배율만큼 Duration GE로 더함 → BuffDuration이 지나면 GAS가 자동 해제
		// SourceObject = 스킬 데이터: GameplayCue.Skill.Buff가 여기서 ReleaseVFX를 꺼내 오라로 띄움
		const float SpeedBonus = StatComp->MoveSpeed * (Skill->SpeedMultiplier - 1.0f);
		BuffEffectHandle = RLGameplayEffectUtils::ApplyStatBonusEffect(ASC, URLGE_SkillBuff::StaticClass(),
			{ { URLAttributeSet::GetMoveSpeedAttribute(), SpeedBonus } }, Skill->BuffDuration, Skill);
	}

	CommitSkillCooldown();
	FinishSkill();
}

// =====================================================================
// 캐스팅 (Cast)
// =====================================================================
void URLGA_Skill_Cast::ActivateSkill()
{
	PrepareCaster(true);
	BeginCastPhase();
}

void URLGA_Skill_Cast::BeginCastPhase()
{
	bInCastPhase = true;
	bHasTriggeredPreCastMotion = false;
	CastElapsed = 0.0f;
	SetSkillStateTag(RLGameplayTags::State_Casting);

	if (URLSkillComponent* SkillComp = GetSkillComponent())
	{
		SkillComp->StopPlayerMovement();
		SkillComp->OnCastStateChanged.Broadcast(Slot, true);
	}
	PlaySkillMontage(Skill->SkillMontage);
	StartTickTask();
}

void URLGA_Skill_Cast::TickSkill(float DeltaTime)
{
	if (!bInCastPhase)
	{
		return;
	}

	CastElapsed += DeltaTime;

	// 캐스팅 종료 0.5초 전에 몽타주를 마무리(End) 섹션으로 넘김
	const float MotionTriggerTime = FMath::Max(0.0f, Skill->CastDuration - 0.5f);
	if (!bHasTriggeredPreCastMotion && CastElapsed >= MotionTriggerTime)
	{
		bHasTriggeredPreCastMotion = true;
		JumpSkillMontageToSection(FName("End"));
	}

	if (URLSkillComponent* SkillComp = GetSkillComponent())
	{
		const float Ratio = FMath::Clamp(CastElapsed / FMath::Max(0.01f, Skill->CastDuration), 0.0f, 1.0f);
		SkillComp->OnCastProgressChanged.Broadcast(Slot, CastElapsed, Skill->CastDuration, Ratio);
	}

	if (Skill->CastType != ESkillCastType::GroundTarget)
	{
		RotateToCursor();
	}

	if (CastElapsed >= Skill->CastDuration)
	{
		CompleteCast();
	}
}

void URLGA_Skill_Cast::ReleaseCast()
{
	CreateSkillBoxHitbox();
}

void URLGA_Skill_Cast::CompleteCast()
{
	ReleaseCast();
	CommitSkillCooldown();

	bInCastPhase = false;
	if (URLSkillComponent* SkillComp = GetSkillComponent())
	{
		if (SkillComp->GetDecalIndicator())
		{
			SkillComp->GetDecalIndicator()->SetIndicatorVisibility(false);
		}
		SkillComp->OnCastStateChanged.Broadcast(Slot, false);
	}
	FinishSkill();
}

void URLGA_Skill_Cast::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 시전 도중 취소(피격/회피/이동 명령): 몽타주·데칼 정리 후 UI에 시전 종료 알림
	// (충전 이펙트 큐는 AddSkillCue로 붙였으므로 어빌리티 종료 시 GAS가 자동 제거)
	if (bWasCancelled && bInCastPhase)
	{
		StopSkillMontage();
		if (URLSkillComponent* SkillComp = GetSkillComponent())
		{
			if (SkillComp->GetDecalIndicator())
			{
				SkillComp->GetDecalIndicator()->SetIndicatorVisibility(false);
			}
			SkillComp->OnCastStateChanged.Broadcast(Slot, false);
		}
	}
	bInCastPhase = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =====================================================================
// 장판 (GroundTarget)
// =====================================================================
void URLGA_Skill_GroundTarget::ActivateSkill()
{
	// 조준 단계: 이동은 막지 않고 데칼만 띄움
	bAiming = true;
	SetSkillStateTag(RLGameplayTags::State_Skill_Aiming);

	if (ARLDecalIndicator* Decal = GetSkillComponent() ? GetSkillComponent()->GetDecalIndicator() : nullptr)
	{
		Decal->SetupIndicator(EIndicatorType::Circle, Skill->IndicatorRadius, Skill->MaxRange);
		Decal->SetIndicatorVisibility(true);
	}
	StartTickTask();
}

void URLGA_Skill_GroundTarget::TickSkill(float DeltaTime)
{
	if (bAiming)
	{
		URLSkillComponent* SkillComp = GetSkillComponent();
		if (ARLDecalIndicator* Decal = SkillComp ? SkillComp->GetDecalIndicator() : nullptr)
		{
			Decal->UpdateIndicatorTransform(Caster->GetActorLocation(), SkillComp->GetMouseGroundLocation(), Skill->MaxRange);
		}
		return;
	}
	Super::TickSkill(DeltaTime);
}

void URLGA_Skill_GroundTarget::ConfirmTarget()
{
	URLSkillComponent* SkillComp = GetSkillComponent();
	ARLDecalIndicator* Decal = SkillComp ? SkillComp->GetDecalIndicator() : nullptr;
	if (!bAiming || !Decal)
	{
		return;
	}

	bAiming = false;
	TargetLocation = Decal->GetTargetLocation();

	const FVector Dir = (TargetLocation - Caster->GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		Caster->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	}
	SkillComp->StopPlayerMovement();

	if (Skill->CastDuration > 0.0f)
	{
		// 캐스팅형 장판: 위치를 고정하고 게이지 진행 (틱 태스크는 조준 때 시작한 것을 그대로 사용)
		bInCastPhase = true;
		bHasTriggeredPreCastMotion = false;
		CastElapsed = 0.0f;
		SetSkillStateTag(RLGameplayTags::State_Casting);

		Decal->LockTargetLocation(TargetLocation);
		PlaySkillMontage(Skill->SkillMontage);
		if (Skill->ChargeVFX)
		{
			AddSkillCue(RLGameplayTags::GameplayCue_Skill_Charge, TargetLocation);
		}
		SkillComp->OnCastStateChanged.Broadcast(Slot, true);
		return;
	}

	// 즉발형 장판
	PlaySkillMontage(Skill->SkillMontage);
	if (Skill->ReleaseVFX)
	{
		ExecuteSkillCue(RLGameplayTags::GameplayCue_Skill_Release, TargetLocation);
	}
	Detonate();
	CommitSkillCooldown();
	Decal->SetIndicatorVisibility(false);
	FinishSkill();
}

void URLGA_Skill_GroundTarget::ReleaseCast()
{
	// 충전 이펙트를 끄고 바로 발동 이펙트로 교체
	K2_RemoveGameplayCue(RLGameplayTags::GameplayCue_Skill_Charge);
	if (!bHasTriggeredPreCastMotion)
	{
		JumpSkillMontageToSection(FName("End"));
	}
	if (Skill->ReleaseVFX)
	{
		ExecuteSkillCue(RLGameplayTags::GameplayCue_Skill_Release, TargetLocation);
	}
	Detonate();
}

void URLGA_Skill_GroundTarget::Detonate()
{
	if (Skill->GroundActorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Caster;
		SpawnParams.Instigator = Caster;
		if (AActor* GroundActor = Caster->GetWorld()->SpawnActor<AActor>(Skill->GroundActorClass, TargetLocation, FRotator::ZeroRotator, SpawnParams))
		{
			const FRLDamageData EvaluatedDamage = BuildDamageData(Caster, Skill);
			if (ARLGroundAreaEffect* GroundEffect = Cast<ARLGroundAreaEffect>(GroundActor))
			{
				GroundEffect->DamageData = EvaluatedDamage;
				GroundEffect->SourceSkill = Skill;
			}
			if (URLCombatFeedbackSubsystem* FeedbackSub = URLCombatFeedbackSubsystem::Get(Caster))
			{
				FeedbackSub->RegisterDamageSourceActor(GroundActor, Caster, EvaluatedDamage, Skill);
			}
		}
	}
	else
	{
		Caster->CreateLocationHitbox(TargetLocation, Skill->IndicatorRadius, BuildDamageData(Caster, Skill), true);
	}
}

void URLGA_Skill_GroundTarget::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 조준 중 취소(우클릭/이동/다른 스킬): 데칼만 끔
	if (bAiming)
	{
		if (URLSkillComponent* SkillComp = GetSkillComponent())
		{
			if (SkillComp->GetDecalIndicator())
			{
				SkillComp->GetDecalIndicator()->SetIndicatorVisibility(false);
			}
		}
	}
	bAiming = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =====================================================================
// 차지 (Charge / ChargeDash)
// =====================================================================
void URLGA_Skill_Charge::ActivateSkill()
{
	PrepareCaster(true);
	ChargeElapsed = 0.0f;
	SetSkillStateTag(RLGameplayTags::State_Skill_Charging);
	PlaySkillMontage(Skill->SkillMontage);
	StartTickTask();
}

void URLGA_Skill_Charge::TickSkill(float DeltaTime)
{
	RotateToCursor();
	ChargeElapsed = FMath::Min(ChargeElapsed + DeltaTime, Skill->CastDuration);

	// 최대치까지 모이면 자동 발동
	if (ChargeElapsed >= Skill->CastDuration)
	{
		Release();
	}
}

void URLGA_Skill_Charge::Release()
{
	if (!IsActive())
	{
		return;
	}

	// 최소 차지 시간 이후부터 최대 배율까지 선형 증가
	float Multiplier = 1.0f;
	if (ChargeElapsed >= Skill->MinChargeTime)
	{
		const float Denominator = FMath::Max(0.01f, Skill->CastDuration - Skill->MinChargeTime);
		const float Alpha = FMath::Clamp((ChargeElapsed - Skill->MinChargeTime) / Denominator, 0.0f, 1.0f);
		Multiplier = FMath::Lerp(1.0f, Skill->MaxDamageMultiplier, Alpha);
	}

	if (Skill->CastType == ESkillCastType::ChargeDash && ChargeElapsed >= Skill->MinChargeTime)
	{
		Caster->LaunchClampedToNav(Caster->GetActorForwardVector() * Skill->DashPower, true, true);
	}

	CreateSkillBoxHitbox(Multiplier);
	CommitSkillCooldown();
	FinishSkill();
}

// =====================================================================
// 홀드 (Hold)
// =====================================================================
void URLGA_Skill_Hold::ActivateSkill()
{
	PrepareCaster(true);
	HoldElapsed = 0.0f;
	LastHitTime = 0.0f;
	SetSkillStateTag(RLGameplayTags::State_Skill_Holding);
	PlaySkillMontage(Skill->SkillMontage);
	StartTickTask();
}

void URLGA_Skill_Hold::TickSkill(float DeltaTime)
{
	RotateToCursor();
	HoldElapsed += DeltaTime;

	if (HoldElapsed - LastHitTime >= Skill->TickDamageInterval)
	{
		LastHitTime = HoldElapsed;
		CreateSkillBoxHitbox(1.0f, false);
	}

	if (HoldElapsed >= Skill->CastDuration)
	{
		Release();
	}
}

void URLGA_Skill_Hold::Release()
{
	if (!IsActive())
	{
		return;
	}
	StopSkillMontage();
	CommitSkillCooldown();
	FinishSkill();
}
