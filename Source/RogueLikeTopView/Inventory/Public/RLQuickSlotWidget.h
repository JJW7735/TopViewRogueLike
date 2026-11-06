#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLItemTypes.h"
#include "RLQuickSlotWidget.generated.h"

class UTextBlock;
class URLInventoryComponent;

/**
 * HUD 상시 표시 퀵슬롯 한 칸 (WBP_QuickSlot)의 C++ 베이스 위젯
 * - 플레이어 인벤토리의 OnInventoryUpdated에 연결해 자동 갱신 (세이브 복원 포함)
 * - 폰이 바뀌면(사망/재생성/스테이지 이동 등) 새 폰의 인벤토리로 다시 연결
 * - 실제 아이콘/수량 표시는 BP가 BP_OnQuickSlotChanged에서 WBP_InventorySlot.UpdateSlot으로 처리
 * - 사용 입력은 ARLPlayerController(1/2/3 키)가 담당하며, 이 위젯은 표시 전용
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 현재 인벤토리 상태로 이 칸을 다시 그립니다. */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void RefreshQuickSlot();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 몇 번째 퀵슬롯인지 (0부터). URLInventoryComponent::GetQuickSlotEntries 순서와 같음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot", meta = (ClampMin = "0"))
	int32 QuickIndex = 0;

	// 칸 위에 표시할 키 이름 (예: "1")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot")
	FText KeyText;

	// 빈 칸일 때 아이템 슬롯 부분의 투명도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmptySlotOpacity = 0.4f;

	// 키 표시 텍스트 (선택)
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Key;

	// 재사용 중인 WBP_InventorySlot 인스턴스 (빈 칸 투명도 처리용, 선택)
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> QuickItemSlot;

	/**
	 * 표시할 내용이 바뀔 때마다 호출됩니다. BP에서 QuickItemSlot(WBP_InventorySlot)의 UpdateSlot에 SlotData를 넘기면 됩니다.
	 * @param SlotData 아이템 + 합계 수량 (빈 칸이면 ItemData = None)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "QuickSlot")
	void BP_OnQuickSlotChanged(const FRLInventorySlot& SlotData, bool bHasItem);

private:
	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void BindToPawn(APawn* Pawn);
	void UnbindInventory();

	TWeakObjectPtr<URLInventoryComponent> BoundInventory;
	TWeakObjectPtr<APlayerController> BoundController;
};
