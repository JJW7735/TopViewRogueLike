#include "RLStatComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RLAttributeSet.h"
#include "RLGameplayEffects.h"
#include "RLDamageExecution.h"

URLStatComponent::URLStatComponent()
{
	// 경험치/스탯 모두 이벤트 기반이라 틱이 필요 없습니다.
	PrimaryComponentTick.bCanEverTick = false;
}

const TArray<ERLTargetStat>& URLStatComponent::GetCombatStats()
{
	static const TArray<ERLTargetStat> CombatStats = {
		ERLTargetStat::AttackPower,
		ERLTargetStat::AbilityPower,
		ERLTargetStat::CritChance,
		ERLTargetStat::CritDamageMultiplier,
		ERLTargetStat::MoveSpeed,
		ERLTargetStat::AttackSpeed,
		ERLTargetStat::Defense,
		ERLTargetStat::DamageReduction
	};
	return CombatStats;
}

FGameplayAttribute URLStatComponent::GetAttributeForStat(ERLTargetStat Stat)
{
	switch (Stat)
	{
	case ERLTargetStat::AttackPower:          return URLAttributeSet::GetAttackPowerAttribute();
	case ERLTargetStat::AbilityPower:         return URLAttributeSet::GetAbilityPowerAttribute();
	case ERLTargetStat::CritChance:           return URLAttributeSet::GetCritChanceAttribute();
	case ERLTargetStat::CritDamageMultiplier: return URLAttributeSet::GetCritDamageMultiplierAttribute();
	case ERLTargetStat::MoveSpeed:            return URLAttributeSet::GetMoveSpeedAttribute();
	case ERLTargetStat::AttackSpeed:          return URLAttributeSet::GetAttackSpeedAttribute();
	case ERLTargetStat::Defense:              return URLAttributeSet::GetDefenseAttribute();
	case ERLTargetStat::DamageReduction:      return URLAttributeSet::GetDamageReductionAttribute();
	default:                                  return FGameplayAttribute(); // MaxHealth / HealPercent는 RLHealthComponent 담당
	}
}

void URLStatComponent::OnRegister()
{
	Super::OnRegister();

	// BeginPlay가 아닌 여기서 찾아두는 이유: 컴포넌트 BeginPlay 순서는 보장되지 않아서,
	// 인벤토리의 시작 지급 아이템이 이 컴포넌트 BeginPlay보다 먼저 스탯 GE를 걸 수 있기 때문입니다.
	AbilitySystemComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void URLStatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!AbilitySystemComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[RLStatComponent] %s에 AbilitySystemComponent가 없습니다. ARLCharacter 파생 클래스에서만 사용하세요."), *GetNameSafe(GetOwner()));
		return;
	}

	// BP 클래스 디폴트에서 정한 스탯을 시작값으로 AttributeSet에 넣습니다.
	// (세이브 복원 / 영구 강화는 이보다 나중인 ARLCharacter::BeginPlay 후반부에서 적용됨)
	const TMap<ERLTargetStat, float> DesignerDefaults = {
		{ ERLTargetStat::AttackPower, AttackPower },
		{ ERLTargetStat::AbilityPower, AbilityPower },
		{ ERLTargetStat::CritChance, CritChance },
		{ ERLTargetStat::CritDamageMultiplier, CritDamageMultiplier },
		{ ERLTargetStat::MoveSpeed, MoveSpeed },
		{ ERLTargetStat::AttackSpeed, AttackSpeed },
		{ ERLTargetStat::Defense, Defense },
		{ ERLTargetStat::DamageReduction, DamageReduction }
	};

	bSuppressBroadcast = true;
	for (const TPair<ERLTargetStat, float>& Pair : DesignerDefaults)
	{
		AbilitySystemComp->SetNumericAttributeBase(GetAttributeForStat(Pair.Key), Pair.Value);
	}
	bSuppressBroadcast = false;

	for (const ERLTargetStat Stat : GetCombatStats())
	{
		AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(GetAttributeForStat(Stat))
			.AddUObject(this, &URLStatComponent::HandleStatAttributeChanged);
	}

	SyncFromAttributes();
}

void URLStatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComp)
	{
		for (const ERLTargetStat Stat : GetCombatStats())
		{
			AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(GetAttributeForStat(Stat)).RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void URLStatComponent::SyncFromAttributes()
{
	if (!AbilitySystemComp)
	{
		return;
	}

	AttackPower = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetAttackPowerAttribute());
	AbilityPower = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetAbilityPowerAttribute());
	CritChance = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetCritChanceAttribute());
	CritDamageMultiplier = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetCritDamageMultiplierAttribute());
	MoveSpeed = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetMoveSpeedAttribute());
	AttackSpeed = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetAttackSpeedAttribute());
	Defense = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetDefenseAttribute());
	DamageReduction = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetDamageReductionAttribute());
}

void URLStatComponent::ApplyMoveSpeedToMovement() const
{
	// 기존에는 MoveSpeed를 바꾸는 곳(Inventory/Reward/Shop/Save)마다 MaxWalkSpeed를 따로 맞춰줘야 했습니다.
	if (const ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = MoveSpeed;
		}
	}
}

void URLStatComponent::HandleStatAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	SyncFromAttributes();

	if (ChangeData.Attribute == URLAttributeSet::GetMoveSpeedAttribute())
	{
		ApplyMoveSpeedToMovement();
	}

	// 버프 GE 등 외부에서 바뀐 경우에도 스탯창이 갱신되도록 방송 (일괄 변경 중에는 호출 측이 1회 방송)
	if (!bSuppressBroadcast)
	{
		OnStatsChanged.Broadcast();
	}
}

bool URLStatComponent::AddStatBonus(ERLTargetStat Stat, float Delta)
{
	const FGameplayAttribute Attribute = GetAttributeForStat(Stat);
	if (!Attribute.IsValid() || !AbilitySystemComp)
	{
		return false;
	}

	if (!FMath::IsNearlyZero(Delta))
	{
		TGuardValue<bool> SuppressGuard(bSuppressBroadcast, true);
		RLGameplayEffectUtils::ApplyStatBonusEffect(AbilitySystemComp, URLGE_StatBonus_Instant::StaticClass(), { { Attribute, Delta } });
	}
	return true;
}

FActiveGameplayEffectHandle URLStatComponent::ApplyRemovableStatBonuses(const TArray<FRLStatBonus>& Bonuses, float Multiplier)
{
	if (!AbilitySystemComp)
	{
		return FActiveGameplayEffectHandle();
	}

	// 같은 스탯이 여러 번 나와도 합산해서 GE 하나로 적용
	TMap<FGameplayAttribute, float> Values;
	for (const FRLStatBonus& Bonus : Bonuses)
	{
		const FGameplayAttribute Attribute = Bonus.TargetStat == ERLTargetStat::MaxHealth
			? URLAttributeSet::GetMaxHealthAttribute()
			: GetAttributeForStat(Bonus.TargetStat);

		if (Attribute.IsValid())
		{
			Values.FindOrAdd(Attribute) += Bonus.Value * Multiplier;
		}
	}

	return RLGameplayEffectUtils::ApplyStatBonusEffect(AbilitySystemComp, URLGE_StatBonus_Infinite::StaticClass(), Values);
}

void URLStatComponent::RemoveStatBonusEffect(FActiveGameplayEffectHandle& Handle)
{
	if (AbilitySystemComp && Handle.IsValid())
	{
		AbilitySystemComp->RemoveActiveGameplayEffect(Handle);
	}
	Handle.Invalidate();
}

float URLStatComponent::GetStatBase(ERLTargetStat Stat) const
{
	const FGameplayAttribute Attribute = GetAttributeForStat(Stat);
	if (!Attribute.IsValid() || !AbilitySystemComp)
	{
		return 0.0f;
	}
	return AbilitySystemComp->GetNumericAttributeBase(Attribute);
}

bool URLStatComponent::SetStatBase(ERLTargetStat Stat, float NewBaseValue)
{
	const FGameplayAttribute Attribute = GetAttributeForStat(Stat);
	if (!Attribute.IsValid() || !AbilitySystemComp)
	{
		return false;
	}

	TGuardValue<bool> SuppressGuard(bSuppressBroadcast, true);
	AbilitySystemComp->SetNumericAttributeBase(Attribute, NewBaseValue);
	return true;
}

float URLStatComponent::GetStatValue(ERLTargetStat Stat) const
{
	const FGameplayAttribute Attribute = GetAttributeForStat(Stat);
	if (!Attribute.IsValid() || !AbilitySystemComp)
	{
		return 0.0f;
	}
	return AbilitySystemComp->GetNumericAttribute(Attribute);
}

void URLStatComponent::AddExp(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	CurrentExp += Amount;

	// 레벨업 체크 (경험치가 여러 레벨을 넘길 경우 반복)
	bool bLeveledUp = false;
	while (MaxExp > 0.0f && CurrentExp >= MaxExp)
	{
		CurrentExp -= MaxExp;
		CurrentLevel++;
		MaxExp = FMath::RoundToFloat(MaxExp * ExpGrowthMultiplier);
		bLeveledUp = true;

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
				FString::Printf(TEXT("🎉 [LEVEL UP!] 레벨 %d 달성! (다음 필요 경험치: %.0f)"), CurrentLevel, MaxExp));
		}

		OnLevelUp.Broadcast(CurrentLevel);
	}

	const float Percent = MaxExp > 0.0f ? FMath::Clamp(CurrentExp / MaxExp, 0.0f, 1.0f) : 0.0f;
	OnExpChanged.Broadcast(CurrentExp, MaxExp, Percent);
}

// 치명타는 공격자 쪽에서 스킬 시전 시 한 번 굴립니다 (URLSkillComponent::CalculateFinalDamage).
// 방어력/피해감소 경감은 URLDamageExecution이 피격자 쪽에서 처리합니다.
float URLStatComponent::EvaluateDamage(float InRawDamage, bool& bOutIsCritical) const
{
	bOutIsCritical = false;

	if (CritChance > 0.0f)
	{
		const float Roll = FMath::FRandRange(0.0f, 100.0f);
		if (Roll <= CritChance)
		{
			bOutIsCritical = true;
			return InRawDamage * CritDamageMultiplier;
		}
	}

	return InRawDamage;
}

float URLStatComponent::CalculateDefendedDamage(float InRawDamage, float& OutReducedDamage) const
{
	// 실제 피해 적용은 URLDamageExecution이 하고, 이 함수는 BP/UI에서 "예상 피해"를 볼 때 쓰는 미리보기입니다.
	// 공식은 한 곳(URLDamageExecution::CalculateMitigatedDamage)만 씁니다.
	return URLDamageExecution::CalculateMitigatedDamage(InRawDamage, Defense, DamageReduction, OutReducedDamage);
}

void URLStatComponent::NotifyStatsChanged()
{
	OnStatsChanged.Broadcast();
}

void URLStatComponent::RestoreStatsDirect(int32 InLevel, float InCurrentExp, float InMaxExp, float InExpGrowth,
	float InAttackPower, float InAbilityPower, float InCritChance,
	float InCritDamageMultiplier, float InMoveSpeed, float InAttackSpeed,
	float InDefense, float InDamageReduction)
{
	CurrentLevel = FMath::Max(1, InLevel);
	CurrentExp = FMath::Max(0.0f, InCurrentExp);
	MaxExp = FMath::Max(1.0f, InMaxExp);
	ExpGrowthMultiplier = FMath::Max(1.0f, InExpGrowth);

	// 범위 제한은 URLAttributeSet::ClampAttribute가 처리합니다.
	SetStatBase(ERLTargetStat::AttackPower, InAttackPower);
	SetStatBase(ERLTargetStat::AbilityPower, InAbilityPower);
	SetStatBase(ERLTargetStat::CritChance, InCritChance);
	SetStatBase(ERLTargetStat::CritDamageMultiplier, InCritDamageMultiplier);
	SetStatBase(ERLTargetStat::MoveSpeed, InMoveSpeed);
	SetStatBase(ERLTargetStat::AttackSpeed, InAttackSpeed);
	SetStatBase(ERLTargetStat::Defense, InDefense);
	SetStatBase(ERLTargetStat::DamageReduction, InDamageReduction);

	NotifyStatsChanged();
	const float Percent = MaxExp > 0.0f ? FMath::Clamp(CurrentExp / MaxExp, 0.0f, 1.0f) : 0.0f;
	OnExpChanged.Broadcast(CurrentExp, MaxExp, Percent);
}
