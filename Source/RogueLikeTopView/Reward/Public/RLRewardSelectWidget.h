#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLRewardTypes.h"
#include "RLRewardSelectWidget.generated.h"

class ARLCharacter;
class ARLRewardChest;
class UTexture2D;

/**
 * 3지선다 보상 카드 선택 팝업 UI의 C++ 베이스 클래스
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLRewardSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 위젯 생성 직후 호출되어 플레이어 정보와 3개의 카드 데이터를 설정합니다.
	 * TriggerSource가 Chest면 게임을 일시정지하고, LevelUp이면 전투가 끊기지 않도록 일시정지하지 않습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reward")
	void InitializeWidget(ARLCharacter* InPlayer, const TArray<FRLRewardData>& InRewards, ARLRewardChest* InSourceChest = nullptr, ERLRewardTriggerSource InTriggerSource = ERLRewardTriggerSource::Chest);

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

	/** 이 보상 UI가 상자에서 떴는지, 레벨업으로 자동으로 떴는지 구분. WBP에서 타이틀/배경/연출을 다르게 분기할 때 사용 */
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	ERLRewardTriggerSource TriggerSource = ERLRewardTriggerSource::Chest;

protected:
	/** InitializeWidget에서 이 위젯이 직접 게임을 일시정지시켰는지 여부 (CloseWidget에서 본인이 멈춘 경우에만 해제) */
	bool bPausedGameOnOpen = false;

	/**
	 * 카드 아이콘(TSoftObjectPtr)을 미리 로드해 붙잡아 두는 하드 레퍼런스.
	 * WBP의 "Resolve Soft Reference"는 로드를 하지 않으므로, OnRewardsReady 전에 여기서 로드해 둬야 아이콘이 항상 표시된다.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedIcons;

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
