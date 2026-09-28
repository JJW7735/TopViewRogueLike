#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLItemTypes.h"
#include "RLRewardTypes.h"
#include "RLItemDataAsset.generated.h"

class ARLCharacter;

/**
 * 아이템의 기본 데이터와 효과를 정의하는 PrimaryDataAsset
 */
UCLASS(BlueprintType)
class ROGUELIKETOPVIEW_API URLItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URLItemDataAsset();

	// 아이템 고유 식별자 (예: "Item_HealthPotion", "Item_Relic_VampireFang")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Basic")
	FName ItemID = NAME_None;

	// 화면에 표시될 아이템 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Basic")
	FText ItemName;

	// 아이템 설명 텍스트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Basic")
	FText Description;

	// UI 표시용 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	// 아이템 종류
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Basic")
	ERLItemType ItemType = ERLItemType::Consumable;

	// 희귀도 등급
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Basic")
	ERLRewardRarity Rarity = ERLRewardRarity::Common;

	// 한 슬롯에 겹칠 수 있는 최대 개수 (패시브/장비는 보통 1, 소비/재료는 99 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Stack", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	// 상점 구매/판매 기본 가격 (골드)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Economy", meta = (ClampMin = "0"))
	int32 GoldValue = 10;

	// ==========================================
	// 소비/패시브 효과 관련
	// ==========================================

	// 패시브 유물 소지 시 (또는 장비 시) 상시 적용될 스탯 보너스 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Effects")
	TArray<FRLStatBonus> PassiveStatBonuses;

	// 소비 아이템: 고정 체력 회복량
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Effects|Consumable")
	float InstantHealAmount = 0.0f;

	// 소비 아이템: 최대 체력 비례 회복 퍼센트 (0.0f ~ 100.0f)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Effects|Consumable")
	float PercentHealAmount = 0.0f;

	// 사용 시 슬롯에서 1개 소모되는지 여부 (기본 true)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Effects|Consumable")
	bool bConsumeOnUse = true;

	/**
	 * 아이템을 사용했을 때의 로직. 기본적으로 체력 회복 및 블루프린트 훅(BlueprintNativeEvent)을 처리합니다.
	 * @return 사용 성공 여부 (체력이 이미 꽉 찼으면 false 반환 등)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item")
	bool UseItem(ARLCharacter* UserCharacter);
	virtual bool UseItem_Implementation(ARLCharacter* UserCharacter);

	/** Primary Asset Type 식별용 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("RLItem"), GetFName());
	}
};
