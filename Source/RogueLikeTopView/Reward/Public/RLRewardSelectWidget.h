#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLRewardTypes.h"
#include "RLRewardSelectWidget.generated.h"

class ARLCharacter;
class ARLRewardChest;

/**
 * 3지선다 보상 카드 선택 팝업 UI의 C++ 베이스 클래스
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLRewardSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 위젯 생성 직후 호출되어 플레이어 정보와 3개의 카드 데이터를 설정하고 게임을 일시정지합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	void InitializeWidget(ARLCharacter* InPlayer, const TArray<FRLRewardData>& InRewards, ARLRewardChest* InSourceChest = nullptr);

	/**
	 * 플레이어가 특정 카드(0, 1, 2번)를 클릭했을 때 호출됩니다.
	 * 보상을 적용하고 UI를 닫으며 게임을 재개합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	void SelectReward(int32 OptionIndex);

	/**
	 * 위젯을 닫고 게임 일시정지를 해제하며 입력 모드를 원복합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	void CloseWidget();

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FRLRewardData> RewardOptions;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TWeakObjectPtr<ARLCharacter> TargetPlayer;

	/** 보상을 제공한 원본 상자 (선택 완료 후 소멸 연출용) */
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TWeakObjectPtr<ARLRewardChest> SourceChest;

protected:
	/**
	 * C++에서 데이터 준비 완료 시 블루프린트로 알려주는 이벤트.
	 * 블루프린트에서 각 카드 위젯(이름, 설명, 아이콘, 희귀도 색상 등)을 갱신할 때 이 이벤트를 사용합니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Reward")
	void OnRewardsReady(const TArray<FRLRewardData>& Rewards);

	/**
	 * 특정 카드가 선택되었을 때 블루프린트 연출(선택 애니메이션, 사운드 등)용 이벤트
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Reward")
	void OnRewardChosen(int32 ChosenIndex);
};
