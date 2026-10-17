#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "RLRewardTypes.h"
#include "RLStatComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatsChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnExpChangedSignature, float, CurrentExp, float, MaxExp, float, ExpPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUpSignature, int32, NewLevel);

// ★ 레벨/경험치 + 전투 스탯 Facade.
// - 레벨/경험치: 전투 스탯이 아니라서 이 컴포넌트가 직접 관리합니다.
// - 전투 스탯 8종: 실제 값은 Owner ASC의 URLAttributeSet에 있고, 아래 변수들은 그 값을 따라가는 읽기 전용 미러입니다.
//   BP 클래스 디폴트에서 정한 값은 BeginPlay에서 AttributeSet 시작값으로 들어갑니다.
//   값을 바꿀 때는 반드시 AddStatBonus / SetStatBase / RestoreStatsDirect 또는 GE를 사용하세요.
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

	/** 세이브 로드 시 스탯 및 레벨 정보를 직접 복원하고 UI/이동속도를 갱신 (스탯 인자는 GetStatBase 기준 기본값) */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RestoreStatsDirect(int32 InLevel, float InCurrentExp, float InMaxExp, float InExpGrowth,
	                        float InAttackPower, float InAbilityPower, float InCritChance,
	                        float InCritDamageMultiplier, float InMoveSpeed, float InAttackSpeed,
	                        float InDefense, float InDamageReduction);

	// ==========================================
	// ★ 전투 스탯 변경 API (Inventory / Reward / Shop / Save 공용)
	// ==========================================

	/**
	 * 런 동안 영구적인 전투 스탯 보너스 (보상 카드 / 상점 / 메타 강화). Instant GE로 기본값을 바꿉니다.
	 * 범위 제한(치명타 0~100 등)은 URLAttributeSet이 처리합니다.
	 * OnStatsChanged는 방송하지 않으니 여러 개를 바꾼 뒤 NotifyStatsChanged()를 한 번 호출하세요.
	 * @return 전투 스탯이면 true. MaxHealth / HealPercent는 체력 컴포넌트 담당이라 false를 반환합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool AddStatBonus(ERLTargetStat Stat, float Delta);

	/**
	 * 해제 가능한 스탯 보너스 묶음 (패시브 아이템, 버프). Infinite GE 하나로 적용하고 핸들을 돌려줍니다.
	 * MaxHealth도 포함되며 HealPercent는 무시합니다. 현재 체력은 건드리지 않습니다.
	 * 해제는 RemoveStatBonusEffect(핸들).
	 */
	FActiveGameplayEffectHandle ApplyRemovableStatBonuses(const TArray<FRLStatBonus>& Bonuses, float Multiplier = 1.0f);

	/** ApplyRemovableStatBonuses로 건 효과를 해제합니다. */
	void RemoveStatBonusEffect(FActiveGameplayEffectHandle& Handle);

	/** 전투 스탯 기본값을 직접 지정합니다 (세이브 복원용. 아이템/버프 GE 효과는 그대로 유지됨). */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool SetStatBase(ERLTargetStat Stat, float NewBaseValue);

	/** 버프/아이템까지 반영된 현재 스탯 값 */
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetStatValue(ERLTargetStat Stat) const;

	/** 아이템/버프를 뺀 기본값 (보상/상점/메타 강화는 포함). 세이브에는 이 값을 저장합니다. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetStatBase(ERLTargetStat Stat) const;

	/** ERLTargetStat → AttributeSet 어트리뷰트 매핑 (전투 스탯이 아니면 무효한 어트리뷰트) */
	static FGameplayAttribute GetAttributeForStat(ERLTargetStat Stat);

	// ==========================================
	// ★ 핵심 전투 스탯 (AttributeSet 미러, 읽기 전용)
	// ==========================================

	// 1. 공격력 (물리 데미지 기반)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float AttackPower = 10.0f;

	// 2. 주문력 (마법/스킬 데미지 기반)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float AbilityPower = 10.0f;

	// 3. 치명타 확률 (0.0% ~ 100.0%, 예: 15.0 = 15%)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float CritChance = 10.0f;

	// 4. 치명타 피해 배율 (예: 1.5 = 150% 피해, 2.0 = 200% 피해)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float CritDamageMultiplier = 1.5f;

	// 5. 이동 속도 (변경 시 CharacterMovement의 MaxWalkSpeed에 자동 반영)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MoveSpeed = 600.0f;

	// 6. 공격 속도 배율 (1.0 = 기본 속도 100%, 1.5 = 150% 빠름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float AttackSpeed = 1.0f;

	// 7. 방어력 (수치형: 높을수록 받는 피해 감소, 피해 배율: 100 / (100 + Defense))
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float Defense = 10.0f;

	// 8. 받는 피해 감소율 (0.0% ~ 80.0%, 버프/유물/스킬에 의한 추가 퍼센트 피해 감소)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0", ClampMax = "80.0"))
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
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComp;

	// 전투 스탯 8종 (ERLTargetStat 순서)
	static const TArray<ERLTargetStat>& GetCombatStats();

	void HandleStatAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void SyncFromAttributes();
	void ApplyMoveSpeedToMovement() const;

	// AddStatBonus 등 일괄 변경 중에는 OnStatsChanged 자동 방송을 막습니다 (호출 측에서 NotifyStatsChanged 1회)
	bool bSuppressBroadcast = false;
};
