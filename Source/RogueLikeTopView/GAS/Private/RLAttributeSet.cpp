#include "RLAttributeSet.h"
#include "GameplayEffectExtension.h"

URLAttributeSet::URLAttributeSet()
{
	// 기존 URLHealthComponent / URLStatComponent 기본값과 동일
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitAttackPower(10.0f);
	InitAbilityPower(10.0f);
	InitCritChance(10.0f);
	InitCritDamageMultiplier(1.5f);
	InitAttackSpeed(1.0f);
	InitDefense(10.0f);
	InitDamageReduction(0.0f);
	InitMoveSpeed(600.0f);
	InitIncomingDamage(0.0f);
}

void URLAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
	}
	else if (Attribute == GetCritDamageMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetDamageReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 80.0f);
	}
	else if (Attribute == GetAttackSpeedAttribute())
	{
		// 0이 되면 몽타주 재생 속도가 0이 되어 멈추므로 최소값 보장
		NewValue = FMath::Max(NewValue, 0.1f);
	}
	else if (Attribute == GetMoveSpeedAttribute() || Attribute == GetDefenseAttribute()
		|| Attribute == GetAttackPowerAttribute() || Attribute == GetAbilityPowerAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void URLAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 버프/디버프 GE가 걸리거나 풀려서 "현재값"이 다시 계산될 때마다 호출됩니다.
	ClampAttribute(Attribute, NewValue);
}

void URLAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// 최대 체력이 줄었으면 현재 체력이 새 최대치를 넘지 않게 맞춥니다.
	// (기존 URLHealthComponent::IncreaseMaxHealth(bApply = false)와 같은 동작: 장비 해제가 피해로 취급되지 않음)
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			ASC->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
}

void URLAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float LocalDamage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - LocalDamage, 0.0f, GetMaxHealth()));
			// TODO(1단계): 체력 0 도달 시 사망 처리 → URLHealthComponent(Facade)의 OnDeath로 전달
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Instant GE(회복 등)로 Health 기본값이 직접 바뀐 경우의 클램프
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}
