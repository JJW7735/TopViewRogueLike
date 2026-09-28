#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLStatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatsChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnExpChangedSignature, float, CurrentExp, float, MaxExp, float, ExpPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUpSignature, int32, NewLevel);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URLStatComponent();

	// 스탯이 변경되었을 때(보상 획득, 버프 등) 방송할 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnStatsChangedSignature OnStatsChanged;

	// 경험치 변경 시 호출 (현재 경험치, 최대 경험치, 비율 0.0~1.0)
	UPROPERTY(BlueprintAssignable, Category = "Stats|Level")
	FOnExpChangedSignature OnExpChanged;

	// 레벨업 시 호출 (새로운 레벨)
	UPROPERTY(BlueprintAssignable, Category = "Stats|Level")
	FOnLevelUpSignature OnLevelUp;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void NotifyStatsChanged();

	// ==========================================
	// ★ 레벨 & 경험치
	// ==========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Level", meta = (ClampMin = "1"))
	int32 CurrentLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Level", meta = (ClampMin = "0.0"))
	float CurrentExp = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Level", meta = (ClampMin = "1.0"))
	float MaxExp = 100.0f;

	// 레벨업 시 다음 필요 경험치 증가 배율 (기본 1.25배 = 25% 증가)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Level", meta = (ClampMin = "1.0"))
	float ExpGrowthMultiplier = 1.25f;

	/** 경험치 획득 및 레벨업 판정 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Level")
	void AddExp(float Amount);

	UFUNCTION(BlueprintPure, Category = "Stats|Level")
	int32 GetCurrentLevel() const { return CurrentLevel; }

	UFUNCTION(BlueprintPure, Category = "Stats|Level")
	float GetExpPercent() const { return MaxExp > 0.0f ? FMath::Clamp(CurrentExp / MaxExp, 0.0f, 1.0f) : 0.0f; }

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

	// 7. 방어력 (수치형: 높을수록 받는 피해 감소, 피해 배율: 100 / (100 + Defense))
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Defense = 10.0f;

	// 8. 받는 피해 감소율 (0.0% ~ 80.0%, 버프/유물/스킬에 의한 추가 퍼센트 피해 감소)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float DamageReduction = 0.0f;

	/**
	 * 입력받은 데미지를 바탕으로 치명타 여부를 주사위 굴려 판정하고,
	 * 치명타 발생 시 배율을 곱한 최종 피해량을 반환합니다.
	 * @param InRawDamage 공격력/주문력 계수가 적용된 기본 피해량
	 * @param bOutIsCritical 치명타가 발동했는지 여부 반환
	 * @return 치명타 배율이 적용된 최종 피해량
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	float EvaluateDamage(float InRawDamage, bool& bOutIsCritical) const;

	/**
	 * 들어오는 피해량에 피격자의 방어력(Defense) 및 피해 감소율(DamageReduction)을 적용하여 최종 받는 피해량을 계산합니다.
	 * @param InRawDamage 공격자가 가한 원래 데미지
	 * @param OutReducedDamage 방어력 및 감소율로 인해 경감된 피해량
	 * @return 경감 후 최종 피해량 (최소 1.0f 보장)
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	float CalculateDefendedDamage(float InRawDamage, float& OutReducedDamage) const;

protected:
	virtual void BeginPlay() override;
};