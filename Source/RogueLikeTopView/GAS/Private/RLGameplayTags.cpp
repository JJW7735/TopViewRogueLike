#include "RLGameplayTags.h"

namespace RLGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "사망 상태. 피해/스킬 사용 불가");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "무적. ReceiveCustomDamage/TakeDamage에서 피해 무시 (기존 bIsInvulnerable 대체)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SuperArmor, "State.SuperArmor", "경직 면역 (기존 CanBeStaggered() == false 대체)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "기절. 이동/스킬 사용 불가");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Casting, "State.Casting", "시전 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_IgnoreDefense, "State.IgnoreDefense", "받는 피해에 방어력/피해감소 미적용 (보스 대경직 등)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_LMB, "InputTag.Skill.LMB", "기본 공격 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_Q, "InputTag.Skill.Q", "Q 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_W, "InputTag.Skill.W", "W 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_E, "InputTag.Skill.E", "E 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_R, "InputTag.Skill.R", "R 스킬 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Dodge, "InputTag.Dodge", "회피 입력");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller: 피해량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Heal, "Data.Heal", "SetByCaller: 회복량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_TrueDamage, "Data.TrueDamage", "SetByCaller: 1이면 방어력/피해감소 무시 (고정 피해)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_MaxHealth, "Data.Stat.MaxHealth", "SetByCaller: 최대 체력 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_AttackPower, "Data.Stat.AttackPower", "SetByCaller: 공격력 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_AbilityPower, "Data.Stat.AbilityPower", "SetByCaller: 주문력 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_CritChance, "Data.Stat.CritChance", "SetByCaller: 치명타 확률 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_CritDamageMultiplier, "Data.Stat.CritDamageMultiplier", "SetByCaller: 치명타 피해 배율 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_MoveSpeed, "Data.Stat.MoveSpeed", "SetByCaller: 이동 속도 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_AttackSpeed, "Data.Stat.AttackSpeed", "SetByCaller: 공격 속도 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_Defense, "Data.Stat.Defense", "SetByCaller: 방어력 증감량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stat_DamageReduction, "Data.Stat.DamageReduction", "SetByCaller: 피해 감소율 증감량(덮어쓰기 GE에서는 고정값)");
}
