#include "RLInventoryComponent.h"
#include "RLItemDataAsset.h"
#include "RLCharacter.h"
#include "RLStatComponent.h"
#include "RLHealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

URLInventoryComponent::URLInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URLInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(FMath::Max(1, MaxSlots));

	// 시작 기본 지급 아이템 추가
	for (const FRLInventorySlot& StartingItem : DefaultStartingItems)
	{
		if (StartingItem.IsValid())
		{
			AddItem(StartingItem.ItemData, StartingItem.Quantity);
		}
	}
}

int32 URLInventoryComponent::AddItem(URLItemDataAsset* ItemData, int32 Amount)
{
	if (!ItemData || Amount <= 0)
	{
		return 0;
	}

	int32 Remaining = Amount;

	// 1. 이미 같은 아이템이 있는 슬롯에 스택 합치기 (최대 스택 제한 고려)
	if (ItemData->MaxStackSize > 1)
	{
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].IsValid() && Slots[i].ItemData == ItemData)
			{
				const int32 SpaceLeft = ItemData->MaxStackSize - Slots[i].Quantity;
				if (SpaceLeft > 0)
				{
					const int32 ToAdd = FMath::Min(SpaceLeft, Remaining);
					Slots[i].Quantity += ToAdd;
					Remaining -= ToAdd;

					OnSlotUpdated.Broadcast(i, Slots[i]);

					if (Remaining <= 0)
					{
						break;
					}
				}
			}
		}
	}

	// 2. 남은 수량을 빈 슬롯에 새로 배치
	while (Remaining > 0)
	{
		const int32 EmptyIndex = FindFirstEmptySlotIndex();
		if (EmptyIndex == INDEX_NONE)
		{
			// 인벤토리가 가득 참
			break;
		}

		const int32 ToAdd = FMath::Min(Remaining, ItemData->MaxStackSize);
		Slots[EmptyIndex].ItemData = ItemData;
		Slots[EmptyIndex].Quantity = ToAdd;
		Remaining -= ToAdd;

		OnSlotUpdated.Broadcast(EmptyIndex, Slots[EmptyIndex]);
	}

	const int32 Added = Amount - Remaining;
	if (Added > 0)
	{
		// 패시브 유물 아이템인 경우 획득 즉시 캐릭터 스탯에 반영
		if (ItemData->ItemType == ERLItemType::Passive)
		{
			ApplyPassiveItemBonuses(ItemData, Added, true);
		}

		OnInventoryUpdated.Broadcast();

		if (GEngine)
		{
			const FString Msg = FString::Printf(TEXT("🎒 [아이템 획득] %s x%d"), *ItemData->ItemName.ToString(), Added);
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, Msg);
		}
	}

	return Added;
}

bool URLInventoryComponent::RemoveItemAtIndex(int32 SlotIndex, int32 Amount)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex].IsValid() || Amount <= 0)
	{
		return false;
	}

	URLItemDataAsset* TargetItem = Slots[SlotIndex].ItemData;
	const int32 ActualRemoved = FMath::Min(Amount, Slots[SlotIndex].Quantity);

	Slots[SlotIndex].Quantity -= ActualRemoved;

	if (Slots[SlotIndex].Quantity <= 0)
	{
		Slots[SlotIndex].Clear();
	}

	// 패시브 아이템 제거 시 스탯 차감
	if (TargetItem && TargetItem->ItemType == ERLItemType::Passive)
	{
		ApplyPassiveItemBonuses(TargetItem, ActualRemoved, false);
	}

	OnSlotUpdated.Broadcast(SlotIndex, Slots[SlotIndex]);
	OnInventoryUpdated.Broadcast();
	return true;
}

bool URLInventoryComponent::RemoveItemByID(FName ItemID, int32 Amount)
{
	if (ItemID.IsNone() || Amount <= 0)
	{
		return false;
	}

	int32 Needed = Amount;

	for (int32 i = Slots.Num() - 1; i >= 0 && Needed > 0; --i)
	{
		if (Slots[i].IsValid() && Slots[i].ItemData->ItemID == ItemID)
		{
			const int32 ToRemove = FMath::Min(Needed, Slots[i].Quantity);
			RemoveItemAtIndex(i, ToRemove);
			Needed -= ToRemove;
		}
	}

	return Needed < Amount;
}

bool URLInventoryComponent::UseItemAtIndex(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex].IsValid())
	{
		return false;
	}

	ARLCharacter* OwnerChar = Cast<ARLCharacter>(GetOwner());
	if (!OwnerChar)
	{
		return false;
	}

	URLItemDataAsset* TargetItem = Slots[SlotIndex].ItemData;
	if (!TargetItem)
	{
		return false;
	}

	// 아이템 사용 효과 실행
	const bool bUsed = TargetItem->UseItem(OwnerChar);
	if (bUsed)
	{
		OnItemUsed.Broadcast(TargetItem, SlotIndex);

		if (TargetItem->bConsumeOnUse)
		{
			RemoveItemAtIndex(SlotIndex, 1);
		}
		return true;
	}

	return false;
}

bool URLInventoryComponent::SwapSlots(int32 FromIndex, int32 ToIndex)
{
	if (!Slots.IsValidIndex(FromIndex) || !Slots.IsValidIndex(ToIndex) || FromIndex == ToIndex)
	{
		return false;
	}

	Slots.Swap(FromIndex, ToIndex);

	OnSlotUpdated.Broadcast(FromIndex, Slots[FromIndex]);
	OnSlotUpdated.Broadcast(ToIndex, Slots[ToIndex]);
	OnInventoryUpdated.Broadcast();
	return true;
}

int32 URLInventoryComponent::GetItemTotalCount(FName ItemID) const
{
	if (ItemID.IsNone())
	{
		return 0;
	}

	int32 Total = 0;
	for (const FRLInventorySlot& Slot : Slots)
	{
		if (Slot.IsValid() && Slot.ItemData->ItemID == ItemID)
		{
			Total += Slot.Quantity;
		}
	}
	return Total;
}

bool URLInventoryComponent::HasItem(FName ItemID, int32 RequiredAmount) const
{
	return GetItemTotalCount(ItemID) >= RequiredAmount;
}

bool URLInventoryComponent::GetSlotAt(int32 SlotIndex, FRLInventorySlot& OutSlot) const
{
	if (Slots.IsValidIndex(SlotIndex))
	{
		OutSlot = Slots[SlotIndex];
		return true;
	}
	OutSlot.Clear();
	return false;
}

void URLInventoryComponent::AddGold(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	Gold += Amount;
	OnGoldChanged.Broadcast(Gold);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
			FString::Printf(TEXT("💰 [+%d Gold] 현재 보유: %d Gold"), Amount, Gold));
	}
}

bool URLInventoryComponent::SpendGold(int32 Amount)
{
	if (Amount <= 0 || Gold < Amount)
	{
		return false;
	}

	Gold -= Amount;
	OnGoldChanged.Broadcast(Gold);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver,
			FString::Printf(TEXT("💸 [-%d Gold] 남은 보유: %d Gold"), Amount, Gold));
	}
	return true;
}

int32 URLInventoryComponent::FindFirstEmptySlotIndex() const
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots[i].IsValid())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void URLInventoryComponent::ApplyPassiveItemBonuses(URLItemDataAsset* ItemData, int32 Multiplier, bool bApply)
{
	if (!ItemData || Multiplier <= 0)
	{
		return;
	}

	ARLCharacter* OwnerChar = Cast<ARLCharacter>(GetOwner());
	if (!OwnerChar)
	{
		return;
	}

	URLStatComponent* StatComp = OwnerChar->FindComponentByClass<URLStatComponent>();
	URLHealthComponent* HealthComp = OwnerChar->FindComponentByClass<URLHealthComponent>();

	const float Sign = bApply ? 1.0f : -1.0f;

	for (const FRLStatBonus& Bonus : ItemData->PassiveStatBonuses)
	{
		const float Delta = Bonus.Value * Multiplier * Sign;

		// 전투 스탯 8종은 StatComp가 처리 (클램프/이동속도 반영 포함)
		if (StatComp && StatComp->AddStatBonus(Bonus.TargetStat, Delta))
		{
			continue;
		}

		switch (Bonus.TargetStat)
		{
		case ERLTargetStat::MaxHealth:
			if (HealthComp) HealthComp->IncreaseMaxHealth(Delta, bApply);
			break;
		case ERLTargetStat::HealPercent:
			// 일회성 회복은 패시브로 적용하지 않음
			break;
		default:
			break;
		}
	}

	if (StatComp)
	{
		StatComp->NotifyStatsChanged();
	}
}

void URLInventoryComponent::RestoreInventoryDirect(int32 InGold, const TArray<FRLInventorySlot>& InSlots)
{
	Gold = FMath::Max(0, InGold);
	Slots.SetNum(FMath::Max(1, MaxSlots));

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (InSlots.IsValidIndex(i))
		{
			Slots[i] = InSlots[i];
		}
		else
		{
			Slots[i].Clear();
		}
	}

	OnInventoryUpdated.Broadcast();
	OnGoldChanged.Broadcast(Gold);
}
