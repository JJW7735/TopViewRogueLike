#include "RLGameplayTags.h"

namespace RLGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "사망 상태. 피해/스킬 사용 불가");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "무적. 피해 GE 적용 차단 (기존 bIsInvulnerable 대체)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SuperArmor, "State.SuperArmor", "경직 면역 (기존 CanBeStaggered() == false 대체)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "기절. 이동/스킬 사용 불가");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Casting, "State.Casting", "시전 중");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_LMB, "InputTag.Skill.LMB", "기본 공격 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_Q, "InputTag.Skill.Q", "Q 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_W, "InputTag.Skill.W", "W 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_E, "InputTag.Skill.E", "E 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_R, "InputTag.Skill.R", "R 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Dodge, "InputTag.Dodge", "회피 입력");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller: 피해량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Heal, "Data.Heal", "SetByCaller: 회복량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StatBonus, "Data.StatBonus", "SetByCaller: 보상/상점/아이템 스탯 증가량");
}
