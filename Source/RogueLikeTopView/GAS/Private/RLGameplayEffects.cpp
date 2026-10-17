#include "RLGameplayEffects.h"
#include "RLAttributeSet.h"
#include "RLGameplayTags.h"

namespace
{
	FGameplayModifierInfo MakeSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Modifier;
	}
}

URLGE_Damage::URLGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(URLAttributeSet::GetIncomingDamageAttribute(), RLGameplayTags::Data_Damage));
}

URLGE_Heal::URLGE_Heal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(URLAttributeSet::GetHealthAttribute(), RLGameplayTags::Data_Heal));
}
