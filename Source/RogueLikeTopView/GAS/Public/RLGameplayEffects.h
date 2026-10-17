#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RLGameplayEffects.generated.h"

class UAbilitySystemComponent;

// ★ C++에서 직접 쓰는 공용 GE. 수치는 전부 SetByCaller로 런타임에 넘깁니다.
//
// 스탯 변경은 "얼마나 오래 가는가"에 따라 GE 종류를 나눕니다.
//   - Instant  : 런 동안 영구 (보상 카드, 상점 스탯 구매, 영구 메타 강화) → 기본값(Base)이 바뀌고 세이브에 그대로 저장됨
//   - Infinite : 출처가 사라지면 해제 (패시브 아이템, 버프 스킬, 디버프) → 핸들로 제거, 세이브에는 저장 안 함(아이템이 다시 적용)

// 피해: IncomingDamage 메타 어트리뷰트에 Data.Damage 만큼 더함 → URLAttributeSet::PostGameplayEffectExecute에서 Health 감소
UCLASS()
class ROGUELIKETOPVIEW_API URLGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URLGE_Damage();
};

// 회복: Health에 Data.Heal 만큼 더함 (최대치 클램프는 AttributeSet 담당)
UCLASS()
class ROGUELIKETOPVIEW_API URLGE_Heal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URLGE_Heal();
};

// 스탯 보너스 공통: MaxHealth + 전투 스탯 8종에 Data.Stat.* SetByCaller 값을 더합니다.
// 직접 쓰지 말고 RLGameplayEffectUtils::ApplyStatBonusEffect로 적용하세요 (안 쓰는 채널을 0으로 채워줌).
UCLASS(Abstract)
class ROGUELIKETOPVIEW_API URLGE_StatBonusBase : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URLGE_StatBonusBase();
};

// 런 동안 영구 보너스 (보상 / 상점 / 메타 강화)
UCLASS()
class ROGUELIKETOPVIEW_API URLGE_StatBonus_Instant : public URLGE_StatBonusBase
{
	GENERATED_BODY()

public:
	URLGE_StatBonus_Instant();
};

// 해제 가능한 보너스 (패시브 아이템 / 버프)
UCLASS()
class ROGUELIKETOPVIEW_API URLGE_StatBonus_Infinite : public URLGE_StatBonusBase
{
	GENERATED_BODY()

public:
	URLGE_StatBonus_Infinite();
};

// 소울 셰터 디버프: DamageReduction을 Data.Stat.DamageReduction 값으로 덮어씀 (해제되면 원래 값으로 자동 복귀)
UCLASS()
class ROGUELIKETOPVIEW_API URLGE_SoulShatterDebuff : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URLGE_SoulShatterDebuff();
};

namespace RLGameplayEffectUtils
{
	// 스탯 보너스 GE가 다루는 (어트리뷰트, SetByCaller 태그) 목록
	ROGUELIKETOPVIEW_API const TArray<TPair<FGameplayAttribute, FGameplayTag>>& GetStatBonusChannels();

	/**
	 * 스탯 보너스 GE를 Values 수치로 적용합니다. Values에 없는 채널은 0으로 채웁니다.
	 * @return Infinite GE면 해제용 핸들, Instant GE면 무효 핸들
	 */
	ROGUELIKETOPVIEW_API FActiveGameplayEffectHandle ApplyStatBonusEffect(UAbilitySystemComponent* ASC,
		TSubclassOf<URLGE_StatBonusBase> EffectClass, const TMap<FGameplayAttribute, float>& Values);
}
