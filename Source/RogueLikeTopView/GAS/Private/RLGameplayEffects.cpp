#include "RLGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "RLAttributeSet.h"
#include "RLDamageExecution.h"
#include "RLGameplayTags.h"

namespace
{
	FGameplayModifierInfo MakeSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag,
		EGameplayModOp::Type ModOp = EGameplayModOp::Additive)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = ModOp;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Modifier;
	}
}

URLGE_Damage::URLGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 방어력/피해감소 경감은 ExecutionCalculation이 대상의 어트리뷰트를 캡처해서 계산합니다.
	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = URLDamageExecution::StaticClass();
	Executions.Add(DamageExecution);
}

URLGE_Heal::URLGE_Heal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(URLAttributeSet::GetHealthAttribute(), RLGameplayTags::Data_Heal));
}

URLGE_StatBonusBase::URLGE_StatBonusBase()
{
	for (const TPair<FGameplayAttribute, FGameplayTag>& Channel : RLGameplayEffectUtils::GetStatBonusChannels())
	{
		Modifiers.Add(MakeSetByCallerModifier(Channel.Key, Channel.Value));
	}
}

URLGE_StatBonus_Instant::URLGE_StatBonus_Instant()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
}

URLGE_StatBonus_Infinite::URLGE_StatBonus_Infinite()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

URLGE_SoulCrackShield::URLGE_SoulCrackShield()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	// Override가 아닌 Additive: 다른 피해감소 효과(유물/버프)와 겹쳐도 보호막 몫만 빠지도록
	Modifiers.Add(MakeSetByCallerModifier(URLAttributeSet::GetDamageReductionAttribute(), RLGameplayTags::Data_Stat_DamageReduction));
}

namespace RLGameplayEffectUtils
{
	const TArray<TPair<FGameplayAttribute, FGameplayTag>>& GetStatBonusChannels()
	{
		static const TArray<TPair<FGameplayAttribute, FGameplayTag>> Channels = {
			{ URLAttributeSet::GetMaxHealthAttribute(), RLGameplayTags::Data_Stat_MaxHealth },
			{ URLAttributeSet::GetAttackPowerAttribute(), RLGameplayTags::Data_Stat_AttackPower },
			{ URLAttributeSet::GetAbilityPowerAttribute(), RLGameplayTags::Data_Stat_AbilityPower },
			{ URLAttributeSet::GetCritChanceAttribute(), RLGameplayTags::Data_Stat_CritChance },
			{ URLAttributeSet::GetCritDamageMultiplierAttribute(), RLGameplayTags::Data_Stat_CritDamageMultiplier },
			{ URLAttributeSet::GetMoveSpeedAttribute(), RLGameplayTags::Data_Stat_MoveSpeed },
			{ URLAttributeSet::GetAttackSpeedAttribute(), RLGameplayTags::Data_Stat_AttackSpeed },
			{ URLAttributeSet::GetDefenseAttribute(), RLGameplayTags::Data_Stat_Defense },
			{ URLAttributeSet::GetDamageReductionAttribute(), RLGameplayTags::Data_Stat_DamageReduction }
		};
		return Channels;
	}

	FActiveGameplayEffectHandle ApplyStatBonusEffect(UAbilitySystemComponent* ASC,
		TSubclassOf<URLGE_StatBonusBase> EffectClass, const TMap<FGameplayAttribute, float>& Values)
	{
		if (!ASC || !EffectClass || Values.IsEmpty())
		{
			return FActiveGameplayEffectHandle();
		}

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, ASC->MakeEffectContext());
		if (!Spec.IsValid())
		{
			return FActiveGameplayEffectHandle();
		}

		// SetByCaller 값이 하나라도 비어 있으면 GAS가 에러 로그를 남기므로 모든 채널을 채웁니다.
		for (const TPair<FGameplayAttribute, FGameplayTag>& Channel : GetStatBonusChannels())
		{
			const float* Value = Values.Find(Channel.Key);
			Spec.Data->SetSetByCallerMagnitude(Channel.Value, Value ? *Value : 0.0f);
		}

		return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}
