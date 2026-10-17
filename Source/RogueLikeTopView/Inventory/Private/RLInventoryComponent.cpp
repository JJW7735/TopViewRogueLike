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
			RefreshPassiveItemEffects();
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
		RefreshPassiveItemEffects();
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

void URLInventoryComponent::RefreshPassiveItemEffects()
{
	ARLCharacter* OwnerChar = Cast<ARLCharacter>(GetOwner());
	URLStatComponent* StatComp = OwnerChar ? OwnerChar->GetStatComponent() : nullptr;
	if (!StatComp)
	{
		return;
	}

	URLHealthComponent* HealthComp = OwnerChar->FindComponentByClass<URLHealthComponent>();
	const float OldMaxHealth = HealthComp ? HealthComp->MaxHealth : 0.0f;

	// 새 효과를 먼저 걸고 옛 효과를 나중에 지웁니다.
	// 반대로 하면 최대 체력이 잠깐 기본값까지 내려가면서 현재 체력이 clamp되어 깎여버립니다.
	TArray<FActiveGameplayEffectHandle> NewHandles;
	for (const FRLInventorySlot& Slot : Slots)
	{
		if (Slot.IsValid() && Slot.ItemData->ItemType == ERLItemType::Passive && Slot.ItemData->PassiveStatBonuses.Num() > 0)
		{
			// HealPercent(일회성 회복)는 ApplyRemovableStatBonuses가 무시합니다.
			const FActiveGameplayEffectHandle Handle = StatComp->ApplyRemovableStatBonuses(Slot.ItemData->PassiveStatBonuses, Slot.Quantity);
			if (Handle.IsValid())
			{
				NewHandles.Add(Handle);
			}
		}
	}

	for (FActiveGameplayEffectHandle& OldHandle : PassiveItemEffectHandles)
	{
		StatComp->RemoveStatBonusEffect(OldHandle);
	}
	PassiveItemEffectHandles = MoveTemp(NewHandles);

	// 기존 동작 유지: 최대 체력 아이템을 얻으면 늘어난 만큼 현재 체력도 채워줌 (해제 시에는 clamp만)
	if (HealthComp && HealthComp->MaxHealth > OldMaxHealth)
	{
		HealthComp->FillHealthForMaxHealthIncrease(HealthComp->MaxHealth - OldMaxHealth);
	}

	StatComp->NotifyStatsChanged();
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

	// 복원된 슬롯 기준으로 아이템 GE를 다시 겁니다 (시작 지급 아이템의 GE는 여기서 정리됨).
	// 세이브에는 아이템을 뺀 기본 스탯만 저장되므로 이중 적용되지 않습니다.
	RefreshPassiveItemEffects();

	OnInventoryUpdated.Broadcast();
	OnGoldChanged.Broadcast(Gold);
}
