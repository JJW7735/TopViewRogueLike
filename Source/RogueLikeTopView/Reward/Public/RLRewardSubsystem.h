#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RLRewardTypes.h"
#include "RLRewardSubsystem.generated.h"

class ARLCharacter;
class URLRewardDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRewardAppliedSignature, ARLCharacter*, TargetCharacter, const FRLRewardData&, Reward);

/**
 * 로그라이크 보상 풀 관리, 무작위 3지선다 카드 추첨, 보상 효과 적용을 총괄하는 서브시스템
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLRewardSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 외부 DataAsset에 정의된 보상들을 풀에 추가합니다. */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	void RegisterRewardDataAsset(URLRewardDataAsset* DataAsset);

	/**
	 * 보상 풀에서 무작위로 중복 없는 N개(기본 3개)의 보상 카드를 추첨합니다.
	 * 희귀도 확률 가중치(일반 55%, 희귀 28%, 영웅 13%, 전설 4%)가 적용됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	TArray<FRLRewardData> RollRewards(int32 Count = 3);

	/**
	 * 플레이어가 선택한 보상 카드의 스탯/회복 효과를 대상 캐릭터에 즉시 적용합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	bool ApplyReward(ARLCharacter* TargetCharacter, const FRLRewardData& Reward);

	/** 보상이 적용되었을 때 방송되는 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "Reward")
	FOnRewardAppliedSignature OnRewardApplied;

	/** 월드 컨텍스트 객체로부터 서브시스템을 쉽게 가져오는 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Reward", meta = (WorldContext = "WorldContextObject"))
	static URLRewardSubsystem* GetRewardSubsystem(const UObject* WorldContextObject);

	/** 현재 등록된 전체 보상 풀 조회 */
	UFUNCTION(BlueprintPure, Category = "Reward")
	const TArray<FRLRewardData>& GetRewardPool() const { return RewardPool; }

protected:
	UPROPERTY()
	TArray<FRLRewardData> RewardPool;

	/** 기본 제공 보상 풀 구축 (DataAsset이 없어도 즉시 동작하도록 16+종의 풍부한 보상 탑재) */
	void BuildDefaultRewardPool();
};
