#include "RLShopWidget.h"
#include "RLShopSubsystem.h"
#include "RLCharacter.h"
#include "RLInventoryComponent.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void URLShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 플레이어 캐릭터 자동 감지
	if (!TargetPlayer.IsValid())
	{
		TargetPlayer = Cast<ARLCharacter>(GetOwningPlayerPawn());
	}

	// 2. 버튼 클릭 이벤트 바인딩
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddDynamic(this, &URLShopWidget::OnExitButtonClicked);
	}
	if (Btn_Reroll)
	{
		Btn_Reroll->OnClicked.AddDynamic(this, &URLShopWidget::OnRerollButtonClicked);
	}

	if (Btn_Card1) Btn_Card1->OnClicked.AddDynamic(this, &URLShopWidget::OnCard1Clicked);
	if (Btn_Card2) Btn_Card2->OnClicked.AddDynamic(this, &URLShopWidget::OnCard2Clicked);
	if (Btn_Card3) Btn_Card3->OnClicked.AddDynamic(this, &URLShopWidget::OnCard3Clicked);
	if (Btn_Card4) Btn_Card4->OnClicked.AddDynamic(this, &URLShopWidget::OnCard4Clicked);

	// 3. 서브시스템 델리게이트 바인딩
	if (URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this))
	{
		ShopSub->OnShopRefreshed.AddDynamic(this, &URLShopWidget::HandleShopRefreshed);
		ShopSub->OnShopItemPurchased.AddDynamic(this, &URLShopWidget::HandleShopItemPurchased);
		ShopSub->OnShopRerollCostChanged.AddDynamic(this, &URLShopWidget::HandleRerollCostChanged);

		HandleRerollCostChanged(ShopSub->GetRerollCost());
	}

	// 4. 플레이어 인벤토리 골드 연동
	if (TargetPlayer.IsValid())
	{
		if (URLInventoryComponent* InvComp = TargetPlayer->GetInventoryComponent())
		{
			InvComp->OnGoldChanged.AddDynamic(this, &URLShopWidget::HandleGoldChanged);
			HandleGoldChanged(InvComp->GetGold());
		}
	}

	// 5. 전체 상품 UI 갱신
	RefreshAllSlotsUI();

	// 6. 마우스 커서 표시 및 UI 입력 모드 전환
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	if (bPauseGameWhenOpen)
	{
		UGameplayStatics::SetGamePaused(GetWorld(), true);
	}

	BP_OnShopOpened();
}

void URLShopWidget::NativeDestruct()
{
	RestorePlayerInput();
	Super::NativeDestruct();
}

FReply URLShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D MousePos = InMouseEvent.GetScreenSpacePosition();

		UBorder* CardBorders[] = { Border_Card1, Border_Card2, Border_Card3, Border_Card4 };
		for (int32 i = 0; i < 4; ++i)
		{
			if (CardBorders[i] && CardBorders[i]->GetCachedGeometry().IsUnderLocation(MousePos))
			{
				BuyItem(i);
				return FReply::Handled();
			}
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URLShopWidget::OpenShop(ARLCharacter* InPlayer)
{
	TargetPlayer = InPlayer;

	if (TargetPlayer.IsValid())
	{
		if (URLInventoryComponent* InvComp = TargetPlayer->GetInventoryComponent())
		{
			InvComp->OnGoldChanged.RemoveDynamic(this, &URLShopWidget::HandleGoldChanged);
			InvComp->OnGoldChanged.AddDynamic(this, &URLShopWidget::HandleGoldChanged);
			HandleGoldChanged(InvComp->GetGold());
		}
	}

	RefreshAllSlotsUI();
}

void URLShopWidget::CloseShop()
{
	RestorePlayerInput();
	BP_OnShopClosed();
	RemoveFromParent();
}

void URLShopWidget::RestorePlayerInput()
{
	if (bPauseGameWhenOpen && GetWorld())
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
}

bool URLShopWidget::BuyItem(int32 SlotIndex)
{
	URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this);
	if (!ShopSub)
	{
		BP_OnPurchaseFailed(SlotIndex, TEXT("상점 서브시스템을 찾을 수 없습니다."));
		return false;
	}

	ARLCharacter* Buyer = TargetPlayer.IsValid() ? TargetPlayer.Get() : Cast<ARLCharacter>(GetOwningPlayerPawn());
	if (!Buyer)
	{
		BP_OnPurchaseFailed(SlotIndex, TEXT("플레이어 캐릭터가 유효하지 않습니다."));
		return false;
	}

	FString FailReason;
	const bool bSuccess = ShopSub->BuyShopItem(Buyer, SlotIndex, FailReason);

	if (bSuccess)
	{
		FRLShopItem PurchasedItem;
		ShopSub->GetShopItemAt(SlotIndex, PurchasedItem);
		BP_OnPurchaseSuccess(SlotIndex, PurchasedItem);
	}
	else
	{
		BP_OnPurchaseFailed(SlotIndex, FailReason);
	}

	return bSuccess;
}

bool URLShopWidget::RequestReroll()
{
	URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this);
	if (!ShopSub)
	{
		BP_OnRerollFailed(TEXT("상점 서브시스템을 찾을 수 없습니다."));
		return false;
	}

	ARLCharacter* Buyer = TargetPlayer.IsValid() ? TargetPlayer.Get() : Cast<ARLCharacter>(GetOwningPlayerPawn());
	if (!Buyer)
	{
		BP_OnRerollFailed(TEXT("플레이어 캐릭터가 유효하지 않습니다."));
		return false;
	}

	FString FailReason;
	const bool bSuccess = ShopSub->RerollShop(Buyer, FailReason);

	if (bSuccess)
	{
		BP_OnRerollSuccess(ShopSub->GetRerollCost());
	}
	else
	{
		BP_OnRerollFailed(FailReason);
	}

	return bSuccess;
}

void URLShopWidget::RefreshAllSlotsUI()
{
	URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this);
	if (!ShopSub)
	{
		return;
	}

	const TArray<FRLShopItem>& Items = ShopSub->GetCurrentShopItems();

	if (Items.IsValidIndex(0)) UpdateSlotUIElements(0, Items[0], Border_Card1, Txt_Name1, Txt_Desc1, Txt_Rarity1, Txt_Price1, Border_Icon1);
	if (Items.IsValidIndex(1)) UpdateSlotUIElements(1, Items[1], Border_Card2, Txt_Name2, Txt_Desc2, Txt_Rarity2, Txt_Price2, Border_Icon2);
	if (Items.IsValidIndex(2)) UpdateSlotUIElements(2, Items[2], Border_Card3, Txt_Name3, Txt_Desc3, Txt_Rarity3, Txt_Price3, Border_Icon3);
	if (Items.IsValidIndex(3)) UpdateSlotUIElements(3, Items[3], Border_Card4, Txt_Name4, Txt_Desc4, Txt_Rarity4, Txt_Price4, Border_Icon4);
}

void URLShopWidget::UpdateSlotUIElements(int32 SlotIndex, const FRLShopItem& Item,
	UBorder* BorderCard, UTextBlock* TxtName, UTextBlock* TxtDesc,
	UTextBlock* TxtRarity, UTextBlock* TxtPrice, UBorder* BorderIcon)
{
	if (TxtName)
	{
		TxtName->SetText(Item.ItemName);
	}

	if (TxtDesc)
	{
		TxtDesc->SetText(Item.Description);
	}

	if (TxtRarity)
	{
		TxtRarity->SetText(Item.GetRarityDisplayName());
		TxtRarity->SetColorAndOpacity(FSlateColor(Item.GetRarityColor()));
	}

	if (TxtPrice)
	{
		if (Item.bIsSoldOut)
		{
			TxtPrice->SetText(NSLOCTEXT("RLShop", "SoldOut", "품절"));
			TxtPrice->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
		}
		else
		{
			TxtPrice->SetText(FText::AsNumber(Item.Price));

			// 현재 골드로 구매 가능한지에 따라 가격 색상 변경
			int32 CurrentGold = 0;
			if (TargetPlayer.IsValid() && TargetPlayer->GetInventoryComponent())
			{
				CurrentGold = TargetPlayer->GetInventoryComponent()->GetGold();
			}

			if (CurrentGold >= Item.Price)
			{
				TxtPrice->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f))); // Gold
			}
			else
			{
				TxtPrice->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f)));  // Red (부족)
			}
		}
	}

	if (BorderCard)
	{
		if (Item.bIsSoldOut)
		{
			BorderCard->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.7f));
		}
		else
		{
			// 희귀도 색상 은은하게 적용
			FLinearColor BorderCol = Item.GetRarityColor();
			BorderCol.A = 0.85f;
			BorderCard->SetBrushColor(BorderCol);
		}
	}

	// 아이콘 표시: DataAsset의 Icon을 로드해 Border_Icon 브러시로 설정
	if (BorderIcon)
	{
		// WBP에 지정된 기본 브러시(반투명 플레이스홀더 박스)를 기준으로 시작 → 새로고침 시 이전 아이콘이 남지 않음
		const UBorder* DefaultBorder = Cast<UBorder>(BorderIcon->GetArchetype());
		FSlateBrush IconBrush = DefaultBorder ? DefaultBorder->Background : BorderIcon->Background;

		if (UTexture2D* IconTex = Item.Icon.LoadSynchronous())
		{
			IconBrush.SetResourceObject(IconTex);
			IconBrush.TintColor = FSlateColor(FLinearColor::White); // 기본 틴트(알파 0.35) 때문에 아이콘이 흐려지는 것 방지
		}

		BorderIcon->SetBrush(IconBrush);
		BorderIcon->SetBrushColor(FLinearColor::White);
	}
}

void URLShopWidget::HandleGoldChanged(int32 NewGold)
{
	if (Txt_Gold)
	{
		Txt_Gold->SetText(FText::AsNumber(NewGold));
	}

	// 가격 색상 갱신
	RefreshAllSlotsUI();
}

void URLShopWidget::HandleShopRefreshed(const TArray<FRLShopItem>& Items)
{
	RefreshAllSlotsUI();
}

void URLShopWidget::HandleShopItemPurchased(int32 SlotIndex, const FRLShopItem& PurchasedItem)
{
	RefreshAllSlotsUI();
}

void URLShopWidget::HandleRerollCostChanged(int32 NewRerollCost)
{
	if (Txt_Reroll)
	{
		Txt_Reroll->SetText(FText::Format(NSLOCTEXT("RLShop", "RerollFmt", "새로고침 ({0} G)"), FText::AsNumber(NewRerollCost)));
	}
}

void URLShopWidget::OnExitButtonClicked()
{
	CloseShop();
}

void URLShopWidget::OnRerollButtonClicked()
{
	RequestReroll();
}
