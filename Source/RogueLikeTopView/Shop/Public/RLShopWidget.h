#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RLShopTypes.h"
#include "RLShopWidget.generated.h"

class UTextBlock;
class UButton;
class UBorder;
class ARLCharacter;
class URLShopSubsystem;

/**
 * 인게임 상점 UI (WBP_ShopTest)의 C++ 베이스 위젯
 * - 상점 상품 4종 자동 UI 바인딩 및 품절/희귀도 연출
 * - 골드 표시 및 실시간 동기화
 * - 리롤 및 구매 기능 C++ 처리
 * - 마우스 커서 및 게임 일시정지 안전 관리
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/**
	 * 플레이어 캐릭터를 전달받아 상점을 열고 초기화합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop|UI")
	void OpenShop(ARLCharacter* InPlayer);

	/**
	 * 상점을 닫고 일시정지 및 입력 모드를 안전하게 원복합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop|UI")
	void CloseShop();

	/**
	 * 특정 슬롯(0, 1, 2, 3)의 상품을 구매합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop|UI")
	bool BuyItem(int32 SlotIndex);

	/**
	 * 상점 새로고침(리롤)을 요청합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop|UI")
	bool RequestReroll();

	/**
	 * 현재 상점 상태에 맞춰 전체 UI를 새로고침합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop|UI")
	void RefreshAllSlotsUI();

protected:
	// 상점 오픈 시 게임을 일시정지할지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Config")
	bool bPauseGameWhenOpen = true;

	// 상점 이용 대상 플레이어
	UPROPERTY(BlueprintReadOnly, Category = "Shop|UI")
	TWeakObjectPtr<ARLCharacter> TargetPlayer;

	// ==========================================
	// WBP_ShopTest 자동 바인딩 위젯 (Optional)
	// ==========================================

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Gold;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Reroll;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Reroll;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Exit;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Exit;

	// --- Slot 1 ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card1;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Name1;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Desc1;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Rarity1;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Price1;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Icon1;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Card1;

	// --- Slot 2 ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card2;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Name2;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Desc2;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Rarity2;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Price2;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Icon2;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Card2;

	// --- Slot 3 ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card3;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Name3;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Desc3;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Rarity3;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Price3;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Icon3;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Card3;

	// --- Slot 4 ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Card4;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Name4;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Desc4;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Rarity4;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Price4;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Icon4;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Card4;

	// ==========================================
	// 블루프린트 연출용 이벤트 훅
	// ==========================================

	UFUNCTION(BlueprintImplementableEvent, Category = "Shop|Event")
	void BP_OnShopOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Shop|Event")
	void BP_OnPurchaseSuccess(int32 SlotIndex, const FRLShopItem& PurchasedItem);

	UFUNCTION(BlueprintImplementableEvent, Category = "Shop|Event")
	void BP_OnPurchaseFailed(int32 SlotIndex, const FString& FailReason);

	UFUNCTION(BlueprintImplementableEvent, Category = "Shop|Event")
	void BP_OnRerollSuccess(int32 NewRerollCost);

	UFUNCTION(BlueprintImplementableEvent, Category = "Shop|Event")
	void BP_OnRerollFailed(const FString& FailReason);

	UFUNCTION(BlueprintImplementableEvent, Category = "Shop|Event")
	void BP_OnShopClosed();

private:
	void UpdateSlotUIElements(int32 SlotIndex, const FRLShopItem& Item,
		UBorder* BorderCard, UTextBlock* TxtName, UTextBlock* TxtDesc,
		UTextBlock* TxtRarity, UTextBlock* TxtPrice, UBorder* BorderIcon);

	UFUNCTION()
	void HandleGoldChanged(int32 NewGold);

	UFUNCTION()
	void HandleShopRefreshed(const TArray<FRLShopItem>& Items);

	UFUNCTION()
	void HandleShopItemPurchased(int32 SlotIndex, const FRLShopItem& PurchasedItem);

	UFUNCTION()
	void HandleRerollCostChanged(int32 NewRerollCost);

	UFUNCTION()
	void OnExitButtonClicked();

	UFUNCTION()
	void OnRerollButtonClicked();

	UFUNCTION() void OnCard1Clicked() { BuyItem(0); }
	UFUNCTION() void OnCard2Clicked() { BuyItem(1); }
	UFUNCTION() void OnCard3Clicked() { BuyItem(2); }
	UFUNCTION() void OnCard4Clicked() { BuyItem(3); }

	void RestorePlayerInput();
};
