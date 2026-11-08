#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLRewardTypes.h"
#include "RLBuffEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UBorder;

/**
 * 인벤토리 "버프" 탭의 한 줄 (WBP_BuffEntry)의 C++ 베이스 위젯 — 읽기 전용 표시 전용
 * - 아이콘 / 이름(같은 버프를 여러 번 얻었으면 "×N") / 희귀도 테두리를 채운다
 * - 희귀도 색상은 FRLRewardData::GetRarityColor (WBP_InventorySlot 테두리와 같은 색) 를 그대로 사용
 * - 설명은 툴팁으로 표시
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLBuffEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 획득 기록 1건으로 이 줄을 채웁니다. WBP_Inventory의 RefreshBuffList에서 호출 */
	UFUNCTION(BlueprintCallable, Category = "Buff")
	void SetupEntry(const FRLAcquiredReward& Entry);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Buff", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, Category = "Buff", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	// 희귀도 색으로 외곽선을 칠할 테두리 (브러시는 디자이너에서 RoundedBox로 설정)
	UPROPERTY(BlueprintReadOnly, Category = "Buff", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> EntryBorder;

	/** SetupEntry 끝에 호출됩니다. BP에서 추가 연출이 필요할 때 사용 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Buff")
	void BP_OnEntrySetup(const FRLAcquiredReward& Entry);
};
