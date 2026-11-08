#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RLRewardTypes.h"
#include "RLRewardSubsystem.generated.h"

class ARLCharacter;
class URLRewardDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRewardAppliedSignature, ARLCharacter*, TargetCharacter, const FRLRewardData&, Reward);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAcquiredRewardsChangedSignature);

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
	 * 보상 카드/상점 축복의 스탯/회복 효과를 대상 캐릭터에 즉시 적용합니다. (스탯 보너스 적용 경로는 여기 하나)
	 * 영구 효과가 있는 보상이면 이번 판 획득 기록(AcquiredRewards)에도 남깁니다.
	 * @param bShowPickupMessage 화면 디버그 메시지 출력 여부 (상점처럼 호출 측이 따로 메시지를 띄우면 false)
	 */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	bool ApplyReward(ARLCharacter* TargetCharacter, const FRLRewardData& Reward, bool bShowPickupMessage = true);

	/** 보상이 적용되었을 때 방송되는 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "Reward")
	FOnRewardAppliedSignature OnRewardApplied;

	/** 이번 판에 획득한 영구 버프 목록 (획득 순서, 같은 RewardID는 Count로 합산) */
	UFUNCTION(BlueprintPure, Category = "Reward|History")
	const TArray<FRLAcquiredReward>& GetAcquiredRewards() const { return AcquiredRewards; }

	/** 획득 기록을 비웁니다. 새 런 시작 시 ARLCharacter::BeginPlay에서 호출됩니다. */
	UFUNCTION(BlueprintCallable, Category = "Reward|History")
	void ResetAcquiredRewards();

	/** 획득 기록에 남길 보상인지 (체력 회복처럼 일회성 효과만 있는 보상은 false) */
	UFUNCTION(BlueprintPure, Category = "Reward|History")
	static bool IsPersistentReward(const FRLRewardData& Reward);

	/** 획득 기록이 바뀌었을 때(추가/초기화) 방송. 인벤토리 버프 탭이 여기에 연결해 목록을 갱신합니다. */
	UPROPERTY(BlueprintAssignable, Category = "Reward|History")
	FOnAcquiredRewardsChangedSignature OnAcquiredRewardsChanged;

	/** 월드 컨텍스트 객체로부터 서브시스템을 쉽게 가져오는 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Reward", meta = (WorldContext = "WorldContextObject"))
	static URLRewardSubsystem* GetRewardSubsystem(const UObject* WorldContextObject);

	/** 현재 등록된 전체 보상 풀 조회 */
	UFUNCTION(BlueprintPure, Category = "Reward")
	const TArray<FRLRewardData>& GetRewardPool() const { return RewardPool; }

protected:
	UPROPERTY()
	TArray<FRLRewardData> RewardPool;

	// 이번 판 획득 기록. GameInstance 서브시스템이라 스테이지 이동(OpenLevel)에도 유지된다.
	// 세이브에는 저장하지 않음 (이어하기 시 빈 목록) — FRLRunSaveData 연동은 세이브 시스템 정리 후 진행 예정
	UPROPERTY()
	TArray<FRLAcquiredReward> AcquiredRewards;

	void RecordAcquiredReward(const FRLRewardData& Reward);

	/** 기본 제공 보상 풀 구축 (DataAsset이 없어도 즉시 동작하도록 16+종의 풍부한 보상 탑재) */
	void BuildDefaultRewardPool();
};
