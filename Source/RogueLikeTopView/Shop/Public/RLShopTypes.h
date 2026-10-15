#pragma once

#include "CoreMinimal.h"
#include "RLRewardTypes.h"
#include "RLItemTypes.h"
#include "RLShopTypes.generated.h"

class URLItemDataAsset;

/**
 * 상점 상품의 종류 (인벤토리 아이템 vs 즉시 스탯 강화 vs 체력 회복)
 */
UENUM(BlueprintType)
enum class ERLShopItemCategory : uint8
{
	Item          UMETA(DisplayName = "인벤토리 아이템"),
	StatUpgrade   UMETA(DisplayName = "스탯 강화"),
	Heal          UMETA(DisplayName = "체력 회복")
};

/**
 * 상점에 진열되는 단일 상품 데이터 구조체
 */
USTRUCT(BlueprintType)
struct FRLShopItem
{
	GENERATED_BODY()

	// 상점 슬롯 인덱스 (0, 1, 2, 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 SlotIndex = 0;

	// 상품 분류
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	ERLShopItemCategory Category = ERLShopItemCategory::Item;

	// 인벤토리 아이템 데이터 (Item 카테고리일 때 유효)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TObjectPtr<URLItemDataAsset> ItemData = nullptr;

	// 스탯 보너스 목록 (StatUpgrade 카테고리일 때 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TArray<FRLStatBonus> StatBonuses;

	// 즉시 체력 회복량 (Heal 카테고리일 때 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	float InstantHealAmount = 0.0f;

	// 화면에 표시될 상품 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText ItemName;

	// 상품 설명 텍스트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText Description;

	// UI 표시용 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TSoftObjectPtr<UTexture2D> Icon;

	// 희귀도 등급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	ERLRewardRarity Rarity = ERLRewardRarity::Common;

	// 판매 가격 (골드)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 Price = 50;

	// 지급 수량 (소비 아이템 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 Quantity = 1;

	// 품절(구매 완료) 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	bool bIsSoldOut = false;

	// 유효한 상품인지 확인
	bool IsValid() const
	{
		return !ItemName.IsEmpty() && Price >= 0;
	}

	// 희귀도 테마 색상 반환
	FLinearColor GetRarityColor() const
	{
		switch (Rarity)
		{
		case ERLRewardRarity::Common:
			return FLinearColor(0.85f, 0.85f, 0.85f, 1.0f); // Slate White
		case ERLRewardRarity::Rare:
			return FLinearColor(0.18f, 0.58f, 1.0f, 1.0f);  // Bright Blue
		case ERLRewardRarity::Epic:
			return FLinearColor(0.70f, 0.25f, 0.95f, 1.0f); // Royal Violet
		case ERLRewardRarity::Legendary:
			return FLinearColor(1.0f, 0.72f, 0.10f, 1.0f);  // Radiant Gold
		default:
			return FLinearColor::White;
		}
	}

	// 희귀도 표시 문자열
	FText GetRarityDisplayName() const
	{
		switch (Rarity)
		{
		case ERLRewardRarity::Common:
			return NSLOCTEXT("RLShop", "Common", "일반");
		case ERLRewardRarity::Rare:
			return NSLOCTEXT("RLShop", "Rare", "희귀");
		case ERLRewardRarity::Epic:
			return NSLOCTEXT("RLShop", "Epic", "영웅");
		case ERLRewardRarity::Legendary:
			return NSLOCTEXT("RLShop", "Legendary", "전설");
		default:
			return FText::GetEmpty();
		}
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShopRefreshedSignature, const TArray<FRLShopItem>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShopItemPurchasedSignature, int32, SlotIndex, const FRLShopItem&, PurchasedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShopRerollCostChangedSignature, int32, NewRerollCost);
