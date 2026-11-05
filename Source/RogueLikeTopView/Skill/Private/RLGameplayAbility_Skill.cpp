#include "RLGameplayAbility_Skill.h"
#include "RLSkillAbilities.h"
#include "RLCharacter.h"
#include "RLSkillComponent.h"
#include "RLStatComponent.h"
#include "RLGameplayEffects.h"
#include "RLGameplayTags.h"
#include "RLAbilityTask_Tick.h"
#include "RLCombatFeedbackSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"

URLGameplayAbility_Skill::URLGameplayAbility_Skill()
{
	// 스킬마다 진행 상태(콤보 수, 차지 시간 등)를 가지므로 캐릭터당 인스턴스 1개
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	// 사망/기절 중이거나, 다른 스킬을 시전·차지·홀드 중이면 발동 불가 (예전 CurrentSkillState != Idle 체크 대체)
	ActivationBlockedTags.AddTag(RLGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(RLGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(RLGameplayTags::State_Casting);
	ActivationBlockedTags.AddTag(RLGameplayTags::State_Skill_Charging);
	ActivationBlockedTags.AddTag(RLGameplayTags::State_Skill_Holding);
}

TSubclassOf<URLGameplayAbility_Skill> URLGameplayAbility_Skill::GetDefaultAbilityClass(ESkillCastType CastType)
{
	switch (CastType)
	{
	case ESkillCastType::Instant:      return URLGA_Skill_Instant::StaticClass();
	case ESkillCastType::Projectile:   return URLGA_Skill_Projectile::StaticClass();
	case ESkillCastType::Dash:         return URLGA_Skill_Dash::StaticClass();
	case ESkillCastType::Buff:         return URLGA_Skill_Buff::StaticClass();
	case ESkillCastType::Cast:         return URLGA_Skill_Cast::StaticClass();
	case ESkillCastType::GroundTarget: return URLGA_Skill_GroundTarget::StaticClass();
	case ESkillCastType::Charge:
	case ESkillCastType::ChargeDash:   return URLGA_Skill_Charge::StaticClass();
	case ESkillCastType::Hold:         return URLGA_Skill_Hold::StaticClass();
	default:                           return URLGA_Skill_Instant::StaticClass();
	}
}

void URLGameplayAbility_Skill::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	Skill = Spec ? Cast<URLSkillData>(Spec->SourceObject.Get()) : nullptr;
	Caster = ActorInfo ? Cast<ARLCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Spec)
	{
		Slot = static_cast<ESkillSlot>(Spec->InputID);
	}

	if (!Skill || !Caster)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 스킬 시전 효과음(CastSound)이 지정되어 있으면 시전 위치에서 재생
	if (Skill->CastSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Caster, Skill->CastSound, Caster->GetActorLocation());
	}

	ActivateSkill();
}

void URLGameplayAbility_Skill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetSkillStateTag(FGameplayTag());
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ========================================================
// 쿨타임 (URLGE_SkillCooldown + 핸들)
// ========================================================
bool URLGameplayAbility_Skill::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return GetCooldownRemaining() <= 0.0f && Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void URLGameplayAbility_Skill::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!Skill || Skill->Cooldown <= 0.0f)
	{
		return;
	}

	const FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(URLGE_SkillCooldown::StaticClass(), GetAbilityLevel());
	if (CooldownSpec.IsValid())
	{
		CooldownSpec.Data->SetSetByCallerMagnitude(RLGameplayTags::Data_Cooldown, Skill->Cooldown);
		CooldownEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec);
	}
}

float URLGameplayAbility_Skill::GetCooldownRemaining() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !CooldownEffectHandle.IsValid() || !ASC->GetWorld())
	{
		return 0.0f;
	}

	const FActiveGameplayEffect* CooldownEffect = ASC->GetActiveGameplayEffect(CooldownEffectHandle);
	return CooldownEffect ? FMath::Max(0.0f, CooldownEffect->GetTimeRemaining(ASC->GetWorld()->GetTimeSeconds())) : 0.0f;
}

// ========================================================
// 하위 클래스용 헬퍼
// ========================================================
void URLGameplayAbility_Skill::PrepareCaster(bool bStopMovement) const
{
	if (URLSkillComponent* SkillComp = GetSkillComponent())
	{
		if (bStopMovement)
		{
			SkillComp->StopPlayerMovement();
		}
		SkillComp->RotateOwnerToMouseCursor();
	}
}

void URLGameplayAbility_Skill::RotateToCursor() const
{
	if (URLSkillComponent* SkillComp = GetSkillComponent())
	{
		SkillComp->RotateOwnerToMouseCursor();
	}
}

void URLGameplayAbility_Skill::FinishSkill(bool bCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
	}
}

void URLGameplayAbility_Skill::CommitSkillCooldown()
{
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void URLGameplayAbility_Skill::SetSkillStateTag(const FGameplayTag& NewStateTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	if (CurrentStateTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(CurrentStateTag);
	}
	CurrentStateTag = NewStateTag;
	if (CurrentStateTag.IsValid())
	{
		ASC->AddLooseGameplayTag(CurrentStateTag);
	}
}

URLAbilityTask_Tick* URLGameplayAbility_Skill::StartTickTask()
{
	URLAbilityTask_Tick* TickTask = URLAbilityTask_Tick::CreateTickTask(this);
	TickTask->OnTick.AddUObject(this, &URLGameplayAbility_Skill::TickSkill);
	TickTask->ReadyForActivation();
	return TickTask;
}

void URLGameplayAbility_Skill::PlaySkillMontage(UAnimMontage* Montage) const
{
	if (Montage && Caster)
	{
		Caster->PlayAnimMontage(Montage);
	}
}

void URLGameplayAbility_Skill::StopSkillMontage() const
{
	if (Skill && Skill->SkillMontage && Caster)
	{
		Caster->StopAnimMontage(Skill->SkillMontage);
	}
}

void URLGameplayAbility_Skill::JumpSkillMontageToSection(FName SectionName) const
{
	if (!Skill || !Skill->SkillMontage || !Caster)
	{
		return;
	}
	if (UAnimInstance* AnimInst = Caster->GetMesh() ? Caster->GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInst->Montage_JumpToSection(SectionName, Skill->SkillMontage);
	}
}

void URLGameplayAbility_Skill::CreateSkillBoxHitbox(float DamageMultiplier, bool bDrawDebug) const
{
	if (!Skill || !Caster)
	{
		return;
	}

	// 타격 판정 순간 스킬 성격(검기/대시/차지/채널링 등)에 맞는 스킬 시전 이펙트 자동 재생
	if (URLCombatFeedbackSubsystem* FeedbackSub = URLCombatFeedbackSubsystem::Get(Caster))
	{
		static int32 GlobalSwingCounter = 0;
		FeedbackSub->NotifySkillVFX(Caster, Skill, Caster->GetActorLocation(), GlobalSwingCounter++);
	}

	FRLDamageData DamageData = BuildDamageData(Caster, Skill);
	DamageData.BaseDamage *= DamageMultiplier;
	Caster->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, DamageData, bDrawDebug);
}

URLSkillComponent* URLGameplayAbility_Skill::GetSkillComponent() const
{
	return Caster ? Caster->GetSkillComponent() : nullptr;
}

void URLGameplayAbility_Skill::HandleAnimHitNotify()
{
	CreateSkillBoxHitbox();
}

void URLGameplayAbility_Skill::HandleAnimDashNotify()
{
	if (Skill && Caster && (Skill->CastType == ESkillCastType::Dash || Skill->CastType == ESkillCastType::ChargeDash))
	{
		Caster->LaunchClampedToNav(Caster->GetActorForwardVector() * Skill->DashPower, true, false);
	}
}

// ========================================================
// 정적 헬퍼 (데미지 계산 / VFX)
// ========================================================
FRLDamageData URLGameplayAbility_Skill::BuildDamageData(ARLCharacter* InCaster, const URLSkillData* InSkill)
{
	FRLDamageData FinalDamage = InSkill ? InSkill->DamageData : FRLDamageData();
	if (!InSkill)
	{
		return FinalDamage;
	}

	FinalDamage.DamageCauser = InCaster;
	FinalDamage.bCanTriggerSoulCrack |= InSkill->bCanTriggerSoulCrack;

	if (URLStatComponent* Stat = InCaster ? InCaster->GetStatComponent() : nullptr)
	{
		const float BonusDamage = (Stat->AttackPower * InSkill->ADScaling) + (Stat->AbilityPower * InSkill->APScaling);
		const float RawDamage = InSkill->DamageData.BaseDamage + BonusDamage;

		// ★ 치명타 판정 (스킬 1회 시전 = 1회 판정)
		FinalDamage.BaseDamage = Stat->EvaluateDamage(RawDamage, FinalDamage.bIsCritical);

		if (GEngine)
		{
			if (FinalDamage.bIsCritical)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange, FString::Printf(TEXT("⚡ [CRITICAL HIT!] Base(%.1f) + Bonus(%.1f) * Crit(x%.1f) = Final(%.1f)"),
					InSkill->DamageData.BaseDamage, BonusDamage, Stat->CritDamageMultiplier, FinalDamage.BaseDamage));
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("Skill Base(%.1f) + Bonus(%.1f) = Final Damage(%.1f)"),
					InSkill->DamageData.BaseDamage, BonusDamage, FinalDamage.BaseDamage));
			}
		}
	}

	// ★ 타격감 서브시스템에 이번 스킬 시전의 치명타/역경직/VFX·SFX 컨텍스트 등록
	if (InCaster)
	{
		if (URLCombatFeedbackSubsystem* FeedbackSub = URLCombatFeedbackSubsystem::Get(InCaster))
		{
			FeedbackSub->PushDamageContext(InCaster, FinalDamage, InSkill);
		}
	}

	return FinalDamage;
}

FGameplayCueParameters URLGameplayAbility_Skill::MakeSkillCueParams(const FVector& Location) const
{
	FGameplayCueParameters Params;
	Params.SourceObject = Skill;
	Params.Instigator = Caster;
	Params.EffectCauser = Caster;
	Params.Location = Location;
	return Params;
}

void URLGameplayAbility_Skill::ExecuteSkillCue(const FGameplayTag& CueTag, const FVector& Location)
{
	K2_ExecuteGameplayCueWithParams(CueTag, MakeSkillCueParams(Location));
}

void URLGameplayAbility_Skill::AddSkillCue(const FGameplayTag& CueTag, const FVector& Location)
{
	K2_AddGameplayCueWithParams(CueTag, MakeSkillCueParams(Location), true);
}
