#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "RLDamageExecution.generated.h"

// ★ 피해 경감 계산 (URLGE_Damage가 사용).
// 입력: SetByCaller Data.Damage(공격자가 계산한 피해량, 스킬 계수/치명타 반영 완료), Data.TrueDamage(1이면 방어 무시)
// 캡처: 대상의 Defense, DamageReduction
// 출력: 대상의 IncomingDamage 메타 어트리뷰트 → URLAttributeSet::PostGameplayEffectExecute에서 Health 감소
//
// 대상에게 State.IgnoreDefense 태그가 있으면(보스 대경직 등) 트루 데미지처럼 경감 없이 들어갑니다.
// 치명타는 스킬 시전 시 공격자 쪽(URLSkillComponent::CalculateFinalDamage)에서 한 번 굴립니다
// (한 번의 스킬이 여러 대상을 맞혀도 치명타 연출이 일관되도록).
UCLASS()
class ROGUELIKETOPVIEW_API URLDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	URLDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

	/**
	 * 방어력/피해감소율 경감 공식 (게임 전체에서 이 함수 하나만 사용).
	 * 방어력 배율: 100 / (100 + Defense), 음수 방어력은 2 - 100 / (100 - Defense)
	 * 피해감소율: 0 ~ 80% 상한, 최종 피해는 최소 1 보장
	 */
	static float CalculateMitigatedDamage(float RawDamage, float Defense, float DamageReduction, float& OutReducedDamage);
};
