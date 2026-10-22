#include "RLShopSubsystem.h"
#include "RLCharacter.h"
#include "RLInventoryComponent.h"
#include "RLStatComponent.h"
#include "RLHealthComponent.h"
#include "RLItemDataAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void URLShopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildDefaultPools();
	GenerateShopItems(4, true);
}

URLShopSubsystem* URLShopSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URLShopSubsystem>() : nullptr;
}

void URLShopSubsystem::GenerateShopItems(int32 Count, bool bResetReroll)
{
	if (bResetReroll)
	{
		RerollCount = 0;
		OnShopRerollCostChanged.Broadcast(GetRerollCost());
	}

	if (ItemPool.Num() == 0 || StatPool.Num() == 0)
	{
		BuildDefaultPools();
	}

	CurrentShopItems.Reset();
	TArray<FName> UsedIDs;

	const int32 FinalCount = FMath::Clamp(Count, 1, 8);
	for (int32 Slot = 0; Slot < FinalCount; ++Slot)
	{
		FRLShopItem NewItem = RollRandomShopItem(Slot, UsedIDs);
		NewItem.SlotIndex = Slot;
		NewItem.bIsSoldOut = false;

		if (!NewItem.ItemName.IsEmpty())
		{
			UsedIDs.Add(FName(*NewItem.ItemName.ToString()));
		}

		CurrentShopItems.Add(NewItem);
	}

	OnShopRefreshed.Broadcast(CurrentShopItems);
}

FRLShopItem URLShopSubsystem::RollRandomShopItem(int32 SlotIndex, const TArray<FName>& ExcludedIDs)
{
	FRLShopItem OutItem;
	OutItem.SlotIndex = SlotIndex;
	OutItem.bIsSoldOut = false;

	// 슬롯별 테마 구성:
	// 슬롯 0: 소비 아이템 (물약/치료제 등 생존 수단 확정 진열)
	// 슬롯 1: 인벤토리 아이템 (패시브 유물 또는 재료)
	// 슬롯 2: 스탯 강화 / 축복
	// 슬롯 3: 고급 유물 또는 강력한 스탯 축복
	ERLShopItemCategory ChosenCategory = ERLShopItemCategory::Item;
	if (SlotIndex == 0)
	{
		ChosenCategory = ERLShopItemCategory::Item; // 소비 아이템 우선
	}
	else if (SlotIndex == 2)
	{
		ChosenCategory = ERLShopItemCategory::StatUpgrade;
	}
	else
	{
		ChosenCategory = (FMath::RandRange(0, 1) == 0) ? ERLShopItemCategory::Item : ERLShopItemCategory::StatUpgrade;
	}

	// 1. 희귀도 주사위 (일반 50%, 희귀 30%, 영웅 15%, 전설 5%)
	const float RarityRoll = FMath::FRandRange(0.0f, 100.0f);
	ERLRewardRarity TargetRarity = ERLRewardRarity::Common;

	if (SlotIndex == 3) // 3번 슬롯은 고급 상품 우대
	{
		if (RarityRoll < 15.0f)
		{
			TargetRarity = ERLRewardRarity::Legendary;
		}
		else if (RarityRoll < 55.0f)
		{
			TargetRarity = ERLRewardRarity::Epic;
		}
		else
		{
			TargetRarity = ERLRewardRarity::Rare;
		}
	}
	else
	{
		if (RarityRoll < 5.0f)
		{
			TargetRarity = ERLRewardRarity::Legendary;
		}
		else if (RarityRoll < 20.0f)
		{
			TargetRarity = ERLRewardRarity::Epic;
		}
		else if (RarityRoll < 50.0f)
		{
			TargetRarity = ERLRewardRarity::Rare;
		}
		else
		{
			TargetRarity = ERLRewardRarity::Common;
		}
	}

	// 2. 카테고리별 상품 선정
	if (ChosenCategory == ERLShopItemCategory::Item && ItemPool.Num() > 0)
	{
		TArray<URLItemDataAsset*> Candidates;
		for (URLItemDataAsset* ItemAsset : ItemPool)
		{
			if (!ItemAsset) continue;
			if (ExcludedIDs.Contains(FName(*ItemAsset->ItemName.ToString()))) continue;

			// 슬롯 0은 소비 아이템 우선
			if (SlotIndex == 0 && ItemAsset->ItemType != ERLItemType::Consumable)
			{
				continue;
			}

			if (ItemAsset->Rarity == TargetRarity)
			{
				Candidates.Add(ItemAsset);
			}
		}

		// 해당 희귀도가 없으면 아무 아이템이나 후보에 추가
		if (Candidates.Num() == 0)
		{
			for (URLItemDataAsset* ItemAsset : ItemPool)
			{
				if (!ItemAsset) continue;
				if (!ExcludedIDs.Contains(FName(*ItemAsset->ItemName.ToString())))
				{
					Candidates.Add(ItemAsset);
				}
			}
		}

		if (Candidates.Num() > 0)
		{
			URLItemDataAsset* SelectedAsset = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
			OutItem.Category = ERLShopItemCategory::Item;
			OutItem.ItemData = SelectedAsset;
			OutItem.ItemName = SelectedAsset->ItemName;
			OutItem.Description = SelectedAsset->Description;
			OutItem.Icon = SelectedAsset->Icon;
			OutItem.Rarity = SelectedAsset->Rarity;
			OutItem.Quantity = (SelectedAsset->ItemType == ERLItemType::Consumable) ? 3 : 1;

			// 가격 책정 (에셋의 GoldValue 기반 또는 희귀도 기본 가격)
			int32 BasePrice = (SelectedAsset->GoldValue > 0) ? SelectedAsset->GoldValue : 50;
			switch (SelectedAsset->Rarity)
			{
			case ERLRewardRarity::Common:    BasePrice = FMath::Clamp(BasePrice, 30, 60); break;
			case ERLRewardRarity::Rare:      BasePrice = FMath::Clamp(BasePrice, 70, 110); break;
			case ERLRewardRarity::Epic:      BasePrice = FMath::Clamp(BasePrice, 130, 190); break;
			case ERLRewardRarity::Legendary: BasePrice = FMath::Clamp(BasePrice, 220, 320); break;
			}
			OutItem.Price = BasePrice;
			return OutItem;
		}
	}

	// 3. 스탯/축복 카테고리 선정
	if (StatPool.Num() > 0)
	{
		TArray<FRLRewardData> Candidates;
		for (const FRLRewardData& StatData : StatPool)
		{
			if (ExcludedIDs.Contains(FName(*StatData.RewardName.ToString()))) continue;
			if (StatData.Rarity == TargetRarity)
			{
				Candidates.Add(StatData);
			}
		}

		if (Candidates.Num() == 0)
		{
			for (const FRLRewardData& StatData : StatPool)
			{
				if (!ExcludedIDs.Contains(FName(*StatData.RewardName.ToString())))
				{
					Candidates.Add(StatData);
				}
			}
		}

		if (Candidates.Num() > 0)
		{
			const FRLRewardData& SelectedStat = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
			OutItem.Category = ERLShopItemCategory::StatUpgrade;
			OutItem.ItemName = SelectedStat.RewardName;
			OutItem.Description = SelectedStat.Description;
			OutItem.Icon = SelectedStat.Icon;
			OutItem.Rarity = SelectedStat.Rarity;
			OutItem.StatBonuses = SelectedStat.StatBonuses;
			OutItem.Quantity = 1;

			int32 BasePrice = 50;
			switch (SelectedStat.Rarity)
			{
			case ERLRewardRarity::Common:    BasePrice = 45; break;
			case ERLRewardRarity::Rare:      BasePrice = 85; break;
			case ERLRewardRarity::Epic:      BasePrice = 160; break;
			case ERLRewardRarity::Legendary: BasePrice = 280; break;
			}
			OutItem.Price = BasePrice;
			return OutItem;
		}
	}

	// 기본 Fallback 상품 (응급 체력 회복)
	OutItem.Category = ERLShopItemCategory::Heal;
	OutItem.ItemName = NSLOCTEXT("RLShop", "InstantHeal", "응급 회복");
	OutItem.Description = NSLOCTEXT("RLShop", "InstantHeal_Desc", "체력을 50 즉시 회복합니다.");
	OutItem.Rarity = ERLRewardRarity::Common;
	OutItem.Price = 30;
	OutItem.InstantHealAmount = 50.0f;
	OutItem.Quantity = 1;
	return OutItem;
}

bool URLShopSubsystem::BuyShopItem(ARLCharacter* Buyer, int32 SlotIndex, FString& OutFailReason)
{
	if (!Buyer)
	{
		OutFailReason = TEXT("구매자 캐릭터 정보가 유효하지 않습니다.");
		return false;
	}

	if (!CurrentShopItems.IsValidIndex(SlotIndex))
	{
		OutFailReason = TEXT("해당 상점 슬롯이 유효하지 않습니다.");
		return false;
	}

	FRLShopItem& TargetItem = CurrentShopItems[SlotIndex];

	if (TargetItem.bIsSoldOut)
	{
		OutFailReason = TEXT("이미 품절된 상품입니다.");
		return false;
	}

	URLInventoryComponent* InvComp = Buyer->GetInventoryComponent();
	if (!InvComp)
	{
		OutFailReason = TEXT("인벤토리 컴포넌트를 찾을 수 없습니다.");
		return false;
	}

	// 1. 골드 충분 여부 확인
	if (InvComp->GetGold() < TargetItem.Price)
	{
		OutFailReason = FString::Printf(TEXT("골드가 부족합니다! (필요: %d G, 보유: %d G)"), TargetItem.Price, InvComp->GetGold());
		return false;
	}

	// 2. 인벤토리 아이템인 경우 공간 확인
	if (TargetItem.Category == ERLShopItemCategory::Item && TargetItem.ItemData)
	{
		// 겹칠 수 있는지 또는 빈 슬롯이 있는지 확인
		const bool bCanStack = (TargetItem.ItemData->MaxStackSize > 1 && InvComp->HasItem(TargetItem.ItemData->ItemID));
		const bool bHasEmptySlot = (InvComp->FindFirstEmptySlotIndex() != INDEX_NONE);

		if (!bCanStack && !bHasEmptySlot)
		{
			OutFailReason = TEXT("인벤토리가 가득 차서 아이템을 보관할 수 없습니다!");
			return false;
		}
	}

	// 3. 골드 차감
	if (!InvComp->SpendGold(TargetItem.Price))
	{
		OutFailReason = TEXT("골드 차감에 실패했습니다.");
		return false;
	}

	// 4. 상품 지급
	switch (TargetItem.Category)
	{
	case ERLShopItemCategory::Item:
		if (TargetItem.ItemData)
		{
			InvComp->AddItem(TargetItem.ItemData, TargetItem.Quantity);
		}
		break;

	case ERLShopItemCategory::StatUpgrade:
	{
		URLStatComponent* StatComp = Buyer->GetStatComponent();
		URLHealthComponent* HealthComp = Buyer->FindComponentByClass<URLHealthComponent>();

		for (const FRLStatBonus& Bonus : TargetItem.StatBonuses)
		{
			// 전투 스탯 8종은 StatComp가 처리 (클램프/이동속도 반영 포함)
			if (StatComp && StatComp->AddStatBonus(Bonus.TargetStat, Bonus.Value))
			{
				continue;
			}

			switch (Bonus.TargetStat)
			{
			case ERLTargetStat::MaxHealth:
				if (HealthComp) HealthComp->IncreaseMaxHealth(Bonus.Value, true);
				break;
			case ERLTargetStat::HealPercent:
				if (HealthComp)
				{
					const float HealAmount = HealthComp->MaxHealth * (Bonus.Value / 100.0f);
					HealthComp->Heal(HealAmount);
				}
				break;
			default:
				break;
			}
		}

		if (StatComp)
		{
			StatComp->NotifyStatsChanged();
		}
		break;
	}

	case ERLShopItemCategory::Heal:
	{
		if (URLHealthComponent* HealthComp = Buyer->FindComponentByClass<URLHealthComponent>())
		{
			HealthComp->Heal(TargetItem.InstantHealAmount);
		}
		break;
	}
	}

	// 5. 품절 처리 및 방송
	TargetItem.bIsSoldOut = true;
	OnShopItemPurchased.Broadcast(SlotIndex, TargetItem);

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("🛒 [구매 완료] %s (-%d Gold)"), *TargetItem.ItemName.ToString(), TargetItem.Price);
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Green, Msg);
	}

	OutFailReason.Empty();
	return true;
}

bool URLShopSubsystem::RerollShop(ARLCharacter* Buyer, FString& OutFailReason)
{
	if (!Buyer)
	{
		OutFailReason = TEXT("구매자 캐릭터 정보가 유효하지 않습니다.");
		return false;
	}

	URLInventoryComponent* InvComp = Buyer->GetInventoryComponent();
	if (!InvComp)
	{
		OutFailReason = TEXT("인벤토리 컴포넌트를 찾을 수 없습니다.");
		return false;
	}

	const int32 Cost = GetRerollCost();
	if (InvComp->GetGold() < Cost)
	{
		OutFailReason = FString::Printf(TEXT("새로고침 골드가 부족합니다! (필요: %d G, 보유: %d G)"), Cost, InvComp->GetGold());
		return false;
	}

	if (!InvComp->SpendGold(Cost))
	{
		OutFailReason = TEXT("골드 차감에 실패했습니다.");
		return false;
	}

	RerollCount++;
	OnShopRerollCostChanged.Broadcast(GetRerollCost());

	GenerateShopItems(4, false);

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("🔄 [상점 새로고침] (-%d Gold, 다음 비용: %d G)"), Cost, GetRerollCost());
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, Msg);
	}

	OutFailReason.Empty();
	return true;
}

int32 URLShopSubsystem::GetRerollCost() const
{
	return BaseRerollCost + (RerollCount * RerollCostIncrement);
}

bool URLShopSubsystem::GetShopItemAt(int32 SlotIndex, FRLShopItem& OutItem) const
{
	if (CurrentShopItems.IsValidIndex(SlotIndex))
	{
		OutItem = CurrentShopItems[SlotIndex];
		return true;
	}
	return false;
}

void URLShopSubsystem::ResetShop()
{
	RerollCount = 0;
	GenerateShopItems(4, true);
}

void URLShopSubsystem::RegisterItemToPool(URLItemDataAsset* ItemAsset)
{
	if (ItemAsset && !ItemPool.Contains(ItemAsset))
	{
		ItemPool.Add(ItemAsset);
	}
}

URLItemDataAsset* URLShopSubsystem::FindItemByID(FName ItemID) const
{
	if (ItemID.IsNone())
	{
		return nullptr;
	}

	for (URLItemDataAsset* ItemAsset : ItemPool)
	{
		if (ItemAsset && ItemAsset->ItemID == ItemID)
		{
			return ItemAsset;
		}
	}
	return nullptr;
}

void URLShopSubsystem::BuildDefaultPools()
{
	ItemPool.Reset();
	StatPool.Reset();

	// ==========================================
	// 1. 기본 아이템 풀 구축
	// ==========================================
	{
		// 체력 회복 물약
		URLItemDataAsset* Potion = NewObject<URLItemDataAsset>(this, TEXT("Shop_HealthPotion"));
		Potion->ItemID = FName("Item_HealthPotion");
		Potion->ItemName = NSLOCTEXT("RLShop", "Item_HealthPotion", "체력 회복 물약");
		Potion->Description = NSLOCTEXT("RLShop", "Item_HealthPotion_Desc", "사용 시 체력을 40 즉시 회복합니다.");
		Potion->ItemType = ERLItemType::Consumable;
		Potion->Rarity = ERLRewardRarity::Common;
		Potion->MaxStackSize = 10;
		Potion->InstantHealAmount = 40.0f;
		Potion->bConsumeOnUse = true;
		Potion->GoldValue = 35;
		Potion->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Regeneration.T_Regeneration")));
		ItemPool.Add(Potion);
	}
	{
		// 상급 활력 비약
		URLItemDataAsset* GreaterPotion = NewObject<URLItemDataAsset>(this, TEXT("Shop_GreaterPotion"));
		GreaterPotion->ItemID = FName("Item_GreaterPotion");
		GreaterPotion->ItemName = NSLOCTEXT("RLShop", "Item_GreaterPotion", "상급 활력 비약");
		GreaterPotion->Description = NSLOCTEXT("RLShop", "Item_GreaterPotion_Desc", "사용 시 최대 체력의 50%를 회복합니다.");
		GreaterPotion->ItemType = ERLItemType::Consumable;
		GreaterPotion->Rarity = ERLRewardRarity::Rare;
		GreaterPotion->MaxStackSize = 5;
		GreaterPotion->PercentHealAmount = 50.0f;
		GreaterPotion->bConsumeOnUse = true;
		GreaterPotion->GoldValue = 75;
		GreaterPotion->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Regeneration.T_Regeneration")));
		ItemPool.Add(GreaterPotion);
	}
	{
		// 전사의 완력 반지
		URLItemDataAsset* Ring = NewObject<URLItemDataAsset>(this, TEXT("Shop_WarriorRing"));
		Ring->ItemID = FName("Item_WarriorRing");
		Ring->ItemName = NSLOCTEXT("RLShop", "Item_WarriorRing", "전사의 완력 반지");
		Ring->Description = NSLOCTEXT("RLShop", "Item_WarriorRing_Desc", "소지 시 공격력 +15, 치명타 확률 +10%를 부여합니다.");
		Ring->ItemType = ERLItemType::Passive;
		Ring->Rarity = ERLRewardRarity::Rare;
		Ring->MaxStackSize = 1;
		Ring->GoldValue = 95;
		Ring->PassiveStatBonuses.Add({ ERLTargetStat::AttackPower, 15.0f });
		Ring->PassiveStatBonuses.Add({ ERLTargetStat::CritChance, 10.0f });
		Ring->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_DeepCut.T_DeepCut")));
		ItemPool.Add(Ring);
	}
	{
		// 바람의 신속 장화
		URLItemDataAsset* Boots = NewObject<URLItemDataAsset>(this, TEXT("Shop_WindBoots"));
		Boots->ItemID = FName("Item_WindBoots");
		Boots->ItemName = NSLOCTEXT("RLShop", "Item_WindBoots", "바람의 신속 장화");
		Boots->Description = NSLOCTEXT("RLShop", "Item_WindBoots_Desc", "소지 시 이동 속도 +100을 상시 부여합니다.");
		Boots->ItemType = ERLItemType::Passive;
		Boots->Rarity = ERLRewardRarity::Epic;
		Boots->MaxStackSize = 1;
		Boots->GoldValue = 150;
		Boots->PassiveStatBonuses.Add({ ERLTargetStat::MoveSpeed, 100.0f });
		Boots->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Evade.T_Evade")));
		ItemPool.Add(Boots);
	}
	{
		// 수호자의 철갑 방패
		URLItemDataAsset* Shield = NewObject<URLItemDataAsset>(this, TEXT("Shop_GuardianShield"));
		Shield->ItemID = FName("Item_GuardianShield");
		Shield->ItemName = NSLOCTEXT("RLShop", "Item_GuardianShield", "수호자의 철갑 방패");
		Shield->Description = NSLOCTEXT("RLShop", "Item_GuardianShield_Desc", "소지 시 방어력 +15, 피해 감소율 +5%를 부여합니다.");
		Shield->ItemType = ERLItemType::Passive;
		Shield->Rarity = ERLRewardRarity::Rare;
		Shield->MaxStackSize = 1;
		Shield->GoldValue = 90;
		Shield->PassiveStatBonuses.Add({ ERLTargetStat::Defense, 15.0f });
		Shield->PassiveStatBonuses.Add({ ERLTargetStat::DamageReduction, 5.0f });
		Shield->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_EnergyRecharge.T_EnergyRecharge")));
		ItemPool.Add(Shield);
	}
	{
		// 광전사의 투혼 인장
		URLItemDataAsset* BerserkCrest = NewObject<URLItemDataAsset>(this, TEXT("Shop_BerserkCrest"));
		BerserkCrest->ItemID = FName("Item_BerserkCrest");
		BerserkCrest->ItemName = NSLOCTEXT("RLShop", "Item_BerserkCrest", "광전사의 투혼 인장");
		BerserkCrest->Description = NSLOCTEXT("RLShop", "Item_BerserkCrest_Desc", "소지 시 공격 속도 +25%, 공격력 +20을 부여합니다.");
		BerserkCrest->ItemType = ERLItemType::Passive;
		BerserkCrest->Rarity = ERLRewardRarity::Epic;
		BerserkCrest->MaxStackSize = 1;
		BerserkCrest->GoldValue = 175;
		BerserkCrest->PassiveStatBonuses.Add({ ERLTargetStat::AttackSpeed, 0.25f });
		BerserkCrest->PassiveStatBonuses.Add({ ERLTargetStat::AttackPower, 20.0f });
		BerserkCrest->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Berserk.T_Berserk")));
		ItemPool.Add(BerserkCrest);
	}
	{
		// 빛나는 루비 원석
		URLItemDataAsset* Ruby = NewObject<URLItemDataAsset>(this, TEXT("Shop_Ruby"));
		Ruby->ItemID = FName("Item_Ruby");
		Ruby->ItemName = NSLOCTEXT("RLShop", "Item_Ruby", "빛나는 루비 원석");
		Ruby->Description = NSLOCTEXT("RLShop", "Item_Ruby_Desc", "신비로운 붉은 보석입니다. 소지 시 최대 체력 +80을 부여합니다.");
		Ruby->ItemType = ERLItemType::Passive;
		Ruby->Rarity = ERLRewardRarity::Legendary;
		Ruby->MaxStackSize = 1;
		Ruby->GoldValue = 260;
		Ruby->PassiveStatBonuses.Add({ ERLTargetStat::MaxHealth, 80.0f });
		Ruby->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_UltimatePower.T_UltimatePower")));
		ItemPool.Add(Ruby);
	}
	// ------------------------------------------
	// [물리(AD) 빌드 패시브 유물 추가 라인업 (AP와 5:5 대칭)]
	// ------------------------------------------
	{
		// 용병의 전투 단검 (일반)
		URLItemDataAsset* MercenaryDagger = NewObject<URLItemDataAsset>(this, TEXT("Shop_MercenaryDagger"));
		MercenaryDagger->ItemID = FName("Item_MercenaryDagger");
		MercenaryDagger->ItemName = NSLOCTEXT("RLShop", "Item_MercenaryDagger", "용병의 전투 단검");
		MercenaryDagger->Description = NSLOCTEXT("RLShop", "Item_MercenaryDagger_Desc", "소지 시 공격력 +12를 부여합니다.");
		MercenaryDagger->ItemType = ERLItemType::Passive;
		MercenaryDagger->Rarity = ERLRewardRarity::Common;
		MercenaryDagger->MaxStackSize = 1;
		MercenaryDagger->GoldValue = 50;
		MercenaryDagger->PassiveStatBonuses.Add({ ERLTargetStat::AttackPower, 12.0f });
		MercenaryDagger->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_DeepCut.T_DeepCut")));
		ItemPool.Add(MercenaryDagger);
	}
	{
		// 추격자의 사냥 각반 (희귀)
		URLItemDataAsset* HunterGreaves = NewObject<URLItemDataAsset>(this, TEXT("Shop_HunterGreaves"));
		HunterGreaves->ItemID = FName("Item_HunterGreaves");
		HunterGreaves->ItemName = NSLOCTEXT("RLShop", "Item_HunterGreaves", "추격자의 사냥 각반");
		HunterGreaves->Description = NSLOCTEXT("RLShop", "Item_HunterGreaves_Desc", "소지 시 공격력 +15, 이동 속도 +40을 부여합니다.");
		HunterGreaves->ItemType = ERLItemType::Passive;
		HunterGreaves->Rarity = ERLRewardRarity::Rare;
		HunterGreaves->MaxStackSize = 1;
		HunterGreaves->GoldValue = 90;
		HunterGreaves->PassiveStatBonuses.Add({ ERLTargetStat::AttackPower, 15.0f });
		HunterGreaves->PassiveStatBonuses.Add({ ERLTargetStat::MoveSpeed, 40.0f });
		HunterGreaves->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_RapidSpear_Attack.T_RapidSpear_Attack")));
		ItemPool.Add(HunterGreaves);
	}
	{
		// 파멸의 군주 대검 (전설)
		URLItemDataAsset* DoombringerBlade = NewObject<URLItemDataAsset>(this, TEXT("Shop_DoombringerBlade"));
		DoombringerBlade->ItemID = FName("Item_DoombringerBlade");
		DoombringerBlade->ItemName = NSLOCTEXT("RLShop", "Item_DoombringerBlade", "파멸의 군주 대검");
		DoombringerBlade->Description = NSLOCTEXT("RLShop", "Item_DoombringerBlade_Desc", "소지 시 공격력 +55, 치명타 확률 +12%, 치명타 피해 +30%를 부여합니다!");
		DoombringerBlade->ItemType = ERLItemType::Passive;
		DoombringerBlade->Rarity = ERLRewardRarity::Legendary;
		DoombringerBlade->MaxStackSize = 1;
		DoombringerBlade->GoldValue = 280;
		DoombringerBlade->PassiveStatBonuses.Add({ ERLTargetStat::AttackPower, 55.0f });
		DoombringerBlade->PassiveStatBonuses.Add({ ERLTargetStat::CritChance, 12.0f });
		DoombringerBlade->PassiveStatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.30f });
		DoombringerBlade->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_FuryAttack.T_FuryAttack")));
		ItemPool.Add(DoombringerBlade);
	}
	// ------------------------------------------
	// [주문력(AP) 빌드 패시브 유물 라인업]
	// ------------------------------------------
	{
		// 견습 마도사의 고서 (일반)
		URLItemDataAsset* ApprenticeTome = NewObject<URLItemDataAsset>(this, TEXT("Shop_ApprenticeTome"));
		ApprenticeTome->ItemID = FName("Item_ApprenticeTome");
		ApprenticeTome->ItemName = NSLOCTEXT("RLShop", "Item_ApprenticeTome", "견습 마도사의 고서");
		ApprenticeTome->Description = NSLOCTEXT("RLShop", "Item_ApprenticeTome_Desc", "소지 시 주문력 +12를 부여합니다.");
		ApprenticeTome->ItemType = ERLItemType::Passive;
		ApprenticeTome->Rarity = ERLRewardRarity::Common;
		ApprenticeTome->MaxStackSize = 1;
		ApprenticeTome->GoldValue = 50;
		ApprenticeTome->PassiveStatBonuses.Add({ ERLTargetStat::AbilityPower, 12.0f });
		ApprenticeTome->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_StormAttack.T_StormAttack")));
		ItemPool.Add(ApprenticeTome);
	}
	{
		// 비전 마력 반지 (희귀)
		URLItemDataAsset* MageRing = NewObject<URLItemDataAsset>(this, TEXT("Shop_MageRing"));
		MageRing->ItemID = FName("Item_MageRing");
		MageRing->ItemName = NSLOCTEXT("RLShop", "Item_MageRing", "비전 마력 반지");
		MageRing->Description = NSLOCTEXT("RLShop", "Item_MageRing_Desc", "소지 시 주문력 +18, 치명타 확률 +10%를 부여합니다.");
		MageRing->ItemType = ERLItemType::Passive;
		MageRing->Rarity = ERLRewardRarity::Rare;
		MageRing->MaxStackSize = 1;
		MageRing->GoldValue = 95;
		MageRing->PassiveStatBonuses.Add({ ERLTargetStat::AbilityPower, 18.0f });
		MageRing->PassiveStatBonuses.Add({ ERLTargetStat::CritChance, 10.0f });
		MageRing->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_StormAttack.T_StormAttack")));
		ItemPool.Add(MageRing);
	}
	{
		// 마력 증폭 수정구 (희귀)
		URLItemDataAsset* ManaCrystal = NewObject<URLItemDataAsset>(this, TEXT("Shop_ManaCrystal"));
		ManaCrystal->ItemID = FName("Item_ManaCrystal");
		ManaCrystal->ItemName = NSLOCTEXT("RLShop", "Item_ManaCrystal", "마력 증폭 수정구");
		ManaCrystal->Description = NSLOCTEXT("RLShop", "Item_ManaCrystal_Desc", "소지 시 주문력 +15, 이동 속도 +40을 부여합니다.");
		ManaCrystal->ItemType = ERLItemType::Passive;
		ManaCrystal->Rarity = ERLRewardRarity::Rare;
		ManaCrystal->MaxStackSize = 1;
		ManaCrystal->GoldValue = 90;
		ManaCrystal->PassiveStatBonuses.Add({ ERLTargetStat::AbilityPower, 15.0f });
		ManaCrystal->PassiveStatBonuses.Add({ ERLTargetStat::MoveSpeed, 40.0f });
		ManaCrystal->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_EnergyRecharge.T_EnergyRecharge")));
		ItemPool.Add(ManaCrystal);
	}
	{
		// 대마법사의 비전 보주 (영웅)
		URLItemDataAsset* ArcaneOrb = NewObject<URLItemDataAsset>(this, TEXT("Shop_ArcaneOrb"));
		ArcaneOrb->ItemID = FName("Item_ArcaneOrb");
		ArcaneOrb->ItemName = NSLOCTEXT("RLShop", "Item_ArcaneOrb", "대마법사의 비전 보주");
		ArcaneOrb->Description = NSLOCTEXT("RLShop", "Item_ArcaneOrb_Desc", "소지 시 주문력 +30, 치명타 피해 +25%를 부여합니다.");
		ArcaneOrb->ItemType = ERLItemType::Passive;
		ArcaneOrb->Rarity = ERLRewardRarity::Epic;
		ArcaneOrb->MaxStackSize = 1;
		ArcaneOrb->GoldValue = 175;
		ArcaneOrb->PassiveStatBonuses.Add({ ERLTargetStat::AbilityPower, 30.0f });
		ArcaneOrb->PassiveStatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.25f });
		ArcaneOrb->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_CursedFury.T_CursedFury")));
		ItemPool.Add(ArcaneOrb);
	}
	{
		// 공허의 마력 왕관 (전설)
		URLItemDataAsset* ArchmageCrown = NewObject<URLItemDataAsset>(this, TEXT("Shop_ArchmageCrown"));
		ArchmageCrown->ItemID = FName("Item_ArchmageCrown");
		ArchmageCrown->ItemName = NSLOCTEXT("RLShop", "Item_ArchmageCrown", "공허의 마력 왕관");
		ArchmageCrown->Description = NSLOCTEXT("RLShop", "Item_ArchmageCrown_Desc", "소지 시 주문력 +55, 치명타 확률 +12%, 치명타 피해 +30%를 부여합니다!");
		ArchmageCrown->ItemType = ERLItemType::Passive;
		ArchmageCrown->Rarity = ERLRewardRarity::Legendary;
		ArchmageCrown->MaxStackSize = 1;
		ArchmageCrown->GoldValue = 280;
		ArchmageCrown->PassiveStatBonuses.Add({ ERLTargetStat::AbilityPower, 55.0f });
		ArchmageCrown->PassiveStatBonuses.Add({ ERLTargetStat::CritChance, 12.0f });
		ArchmageCrown->PassiveStatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.30f });
		ArchmageCrown->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_UltimatePower.T_UltimatePower")));
		ItemPool.Add(ArchmageCrown);
	}

	// ==========================================
	// 2. 기본 스탯/축복 상품 풀 구축
	// ==========================================
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_Atk");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_Atk", "용맹의 축복");
		S.Description = NSLOCTEXT("RLShop", "Blessing_Atk_Desc", "공격력이 10 증가합니다.");
		S.Rarity = ERLRewardRarity::Common;
		S.StatBonuses.Add({ ERLTargetStat::AttackPower, 10.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_DeepCut.T_DeepCut")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_AP");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_AP", "비전의 마력");
		S.Description = NSLOCTEXT("RLShop", "Blessing_AP_Desc", "주문력이 10 증가합니다.");
		S.Rarity = ERLRewardRarity::Common;
		S.StatBonuses.Add({ ERLTargetStat::AbilityPower, 10.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_StormAttack.T_StormAttack")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_Crit");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_Crit", "암살자의 감각");
		S.Description = NSLOCTEXT("RLShop", "Blessing_Crit_Desc", "치명타 확률 +8%, 치명타 피해 +15%가 증가합니다.");
		S.Rarity = ERLRewardRarity::Rare;
		S.StatBonuses.Add({ ERLTargetStat::CritChance, 8.0f });
		S.StatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.15f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_PiercingAttack.T_PiercingAttack")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_Health");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_Health", "거인의 생명력");
		S.Description = NSLOCTEXT("RLShop", "Blessing_Health_Desc", "최대 체력이 60 증가하고 즉시 회복됩니다.");
		S.Rarity = ERLRewardRarity::Rare;
		S.StatBonuses.Add({ ERLTargetStat::MaxHealth, 60.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Regeneration.T_Regeneration")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_IronWill");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_IronWill", "강철 의지");
		S.Description = NSLOCTEXT("RLShop", "Blessing_IronWill_Desc", "방어력 +20, 피해 감소율 +6%가 증가합니다.");
		S.Rarity = ERLRewardRarity::Epic;
		S.StatBonuses.Add({ ERLTargetStat::Defense, 20.0f });
		S.StatBonuses.Add({ ERLTargetStat::DamageReduction, 6.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_EnergyRecharge.T_EnergyRecharge")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_GodlyPower");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_GodlyPower", "전신의 가호");
		S.Description = NSLOCTEXT("RLShop", "Blessing_GodlyPower_Desc", "공격력 +40, 공격 속도 +20%, 이동 속도 +50이 대폭 증가합니다!");
		S.Rarity = ERLRewardRarity::Legendary;
		S.StatBonuses.Add({ ERLTargetStat::AttackPower, 40.0f });
		S.StatBonuses.Add({ ERLTargetStat::AttackSpeed, 0.20f });
		S.StatBonuses.Add({ ERLTargetStat::MoveSpeed, 50.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_UltimatePower.T_UltimatePower")));
		StatPool.Add(S);
	}
	// ------------------------------------------
	// [물리(AD) 빌드 상점 축복 추가 라인업 (AP와 4:4 대칭)]
	// ------------------------------------------
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_FighterSpirit");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_FighterSpirit", "투사의 투지");
		S.Description = NSLOCTEXT("RLShop", "Blessing_FighterSpirit_Desc", "공격력 +18, 최대 체력 +30이 증가합니다.");
		S.Rarity = ERLRewardRarity::Rare;
		S.StatBonuses.Add({ ERLTargetStat::AttackPower, 18.0f });
		S.StatBonuses.Add({ ERLTargetStat::MaxHealth, 30.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_FuryAttack.T_FuryAttack")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_WarlordMight");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_WarlordMight", "전장의 지배자");
		S.Description = NSLOCTEXT("RLShop", "Blessing_WarlordMight_Desc", "공격력 +30, 치명타 확률 +10%가 증가합니다.");
		S.Rarity = ERLRewardRarity::Epic;
		S.StatBonuses.Add({ ERLTargetStat::AttackPower, 30.0f });
		S.StatBonuses.Add({ ERLTargetStat::CritChance, 10.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_BloodyGrip.T_BloodyGrip")));
		StatPool.Add(S);
	}
	// ------------------------------------------
	// [주문력(AP) 빌드 상점 축복 라인업]
	// ------------------------------------------
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_SageWisdom");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_SageWisdom", "현자의 깨달음");
		S.Description = NSLOCTEXT("RLShop", "Blessing_SageWisdom_Desc", "주문력 +18, 최대 체력 +30이 증가합니다.");
		S.Rarity = ERLRewardRarity::Rare;
		S.StatBonuses.Add({ ERLTargetStat::AbilityPower, 18.0f });
		S.StatBonuses.Add({ ERLTargetStat::MaxHealth, 30.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_StormAttack.T_StormAttack")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_ArcaneSurge");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_ArcaneSurge", "비전 폭주");
		S.Description = NSLOCTEXT("RLShop", "Blessing_ArcaneSurge_Desc", "주문력 +30, 치명타 확률 +10%가 증가합니다.");
		S.Rarity = ERLRewardRarity::Epic;
		S.StatBonuses.Add({ ERLTargetStat::AbilityPower, 30.0f });
		S.StatBonuses.Add({ ERLTargetStat::CritChance, 10.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_CursedFury.T_CursedFury")));
		StatPool.Add(S);
	}
	{
		FRLRewardData S;
		S.RewardID = FName("Shop_Blessing_ArchmageGrace");
		S.RewardName = NSLOCTEXT("RLShop", "Blessing_ArchmageGrace", "마법신의 가호");
		S.Description = NSLOCTEXT("RLShop", "Blessing_ArchmageGrace_Desc", "주문력 +45, 치명타 피해 +35%, 이동 속도 +50이 대폭 증가합니다!");
		S.Rarity = ERLRewardRarity::Legendary;
		S.StatBonuses.Add({ ERLTargetStat::AbilityPower, 45.0f });
		S.StatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.35f });
		S.StatBonuses.Add({ ERLTargetStat::MoveSpeed, 50.0f });
		S.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_UltimatePower.T_UltimatePower")));
		StatPool.Add(S);
	}
}
