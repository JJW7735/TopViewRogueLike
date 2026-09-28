#pragma once

#include "CoreMinimal.h"
#include "RLRewardTypes.h"
#include "RLItemTypes.generated.h"

class URLItemDataAsset;

/**
 * 아이템 대분류 종류
 */
UENUM(BlueprintType)
enum class ERLItemType : uint8
{
	Passive      UMETA(DisplayName = "패시브 유물"), // 소지 시 상시 효과/스탯 부여
	Consumable   UMETA(DisplayName = "소비 아이템"), // 사용 시 1개 소모되어 즉각 발동 (포션, 스크롤 등)
	Equipment    UMETA(DisplayName = "장비"),        // 장착 슬롯에 장착하는 장비
	Material     UMETA(DisplayName = "재료/열쇠"),   // 던전 내 특정 문/상자 개방 또는 조합용
	Currency     UMETA(DisplayName = "재화")         // 골드, 소울 등
};

/**
 * 인벤토리 단일 슬롯 데이터
 */
USTRUCT(BlueprintType)
struct FRLInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<URLItemDataAsset> ItemData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Quantity = 0;

	bool IsValid() const
	{
		return ItemData != nullptr && Quantity > 0;
	}

	void Clear()
	{
		ItemData = nullptr;
		Quantity = 0;
	}
};
