#include "RLQuickSlotWidget.h"
#include "RLInventoryComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void URLQuickSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (Txt_Key)
	{
		Txt_Key->SetText(KeyText);
	}
}

void URLQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		BoundController = PC;
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &URLQuickSlotWidget::HandlePossessedPawnChanged);
		BindToPawn(PC->GetPawn());
	}
	else
	{
		RefreshQuickSlot();
	}
}

void URLQuickSlotWidget::NativeDestruct()
{
	UnbindInventory();

	if (APlayerController* PC = BoundController.Get())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &URLQuickSlotWidget::HandlePossessedPawnChanged);
	}
	BoundController.Reset();

	Super::NativeDestruct();
}

void URLQuickSlotWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void URLQuickSlotWidget::BindToPawn(APawn* Pawn)
{
	UnbindInventory();

	if (URLInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<URLInventoryComponent>() : nullptr)
	{
		BoundInventory = Inv;
		// OnSlotUpdated가 아닌 OnInventoryUpdated에 연결: 세이브 복원(RestoreInventoryDirect)은 이쪽만 브로드캐스트함
		Inv->OnInventoryUpdated.AddUniqueDynamic(this, &URLQuickSlotWidget::HandleInventoryUpdated);
	}

	RefreshQuickSlot();
}

void URLQuickSlotWidget::UnbindInventory()
{
	if (URLInventoryComponent* Inv = BoundInventory.Get())
	{
		Inv->OnInventoryUpdated.RemoveDynamic(this, &URLQuickSlotWidget::HandleInventoryUpdated);
	}
	BoundInventory.Reset();
}

void URLQuickSlotWidget::HandleInventoryUpdated()
{
	RefreshQuickSlot();
}

void URLQuickSlotWidget::RefreshQuickSlot()
{
	FRLInventorySlot Entry;

	if (const URLInventoryComponent* Inv = BoundInventory.Get())
	{
		const TArray<FRLInventorySlot> Entries = Inv->GetQuickSlotEntries(QuickIndex + 1);
		if (Entries.IsValidIndex(QuickIndex))
		{
			Entry = Entries[QuickIndex];
		}
	}

	const bool bHasItem = Entry.IsValid();

	if (QuickItemSlot)
	{
		QuickItemSlot->SetRenderOpacity(bHasItem ? 1.0f : EmptySlotOpacity);
	}

	BP_OnQuickSlotChanged(Entry, bHasItem);
}
