#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLStatComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URLStatComponent();

	// ==========================================
	// ★ 핵심 스탯 4가지
	// ==========================================

	// 1. 공격력 (물리 데미지 기반)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackPower = 10.0f;

	// 2. 주문력 (마법/스킬 데미지 기반)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AbilityPower = 10.0f;

	// 3. 치명타 확률 (0.0% ~ 100.0%, 예: 15.0 = 15%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float CritChance = 10.0f;

	// 4. 치명타 피해 배율 (예: 1.5 = 150% 피해, 2.0 = 200% 피해)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "1.0"))
	float CritDamageMultiplier = 1.5f;

	// 5. 이동 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MoveSpeed = 600.0f;

	// 6. 공격 속도 배율 (1.0 = 기본 속도 100%, 1.5 = 150% 빠름)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackSpeed = 1.0f;

	/**
	 * 입력받은 데미지를 바탕으로 치명타 여부를 주사위 굴려 판정하고,
	 * 치명타 발생 시 배율을 곱한 최종 피해량을 반환합니다.
	 * @param InRawDamage 공격력/주문력 계수가 적용된 기본 피해량
	 * @param bOutIsCritical 치명타가 발동했는지 여부 반환
	 * @return 치명타 배율이 적용된 최종 피해량
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	float EvaluateDamage(float InRawDamage, bool& bOutIsCritical) const;

protected:
	virtual void BeginPlay() override;
};