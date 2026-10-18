#include "RLSkillData.h"
#include "RLGameplayAbility_Skill.h"

TSubclassOf<URLGameplayAbility_Skill> URLSkillData::GetAbilityClass() const
{
	return AbilityClassOverride ? AbilityClassOverride : URLGameplayAbility_Skill::GetDefaultAbilityClass(CastType);
}
