#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLRewardTypes.h"
#include "RLItemPoolDataAsset.generated.h"

class URLItemDataAsset;
class URLRewardDataAsset;

/**
 * 상점 아이템 / 보상 카드 / 상점 축복 풀을 에디터에서 관리하기 위한 컨테이너 DataAsset.
 *
 * URLShopSubsystem, URLRewardSubsystem이 초기화될 때 DefaultAssetPath(DA_ItemPool)를 읽어 풀을 만들고,
 * 에셋이 없거나 비어 있으면 기존 C++ 하드코딩 풀(BuildDefaultPools / BuildDefaultRewardPool)을 폴백으로 사용합니다.
 * 아이콘/수치를 바꿀 때는 개별 DA_Item_* / DA_Reward_* / DA_Blessing_* 에셋을 수정하면 됩니다.
 */
UCLASS(BlueprintType)
class ROGUELIKETOPVIEW_API URLItemPoolDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 상점에서 판매되는 인벤토리 아이템 (DA_Item_*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TArray<TObjectPtr<URLItemDataAsset>> Items;

	/** 상자/레벨업 3지선다 보상 카드 (DA_Reward_*, 에셋당 카드 1장) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TArray<TObjectPtr<URLRewardDataAsset>> RewardCards;

	/** 상점 스탯 강화 상품 (DA_Blessing_*, 에셋당 축복 1개) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TArray<TObjectPtr<URLRewardDataAsset>> ShopBlessings;

	/** 서브시스템들이 읽는 기본 풀 에셋 경로 */
	static const TCHAR* DefaultAssetPath;

	/** DefaultAssetPath의 풀 에셋을 로드합니다. 없으면 nullptr (호출 측은 하드코딩 풀로 폴백) */
	static URLItemPoolDataAsset* LoadDefault();

	/** Items 중 유효한 에셋만 OutItems에 추가 */
	void CollectItems(TArray<TObjectPtr<URLItemDataAsset>>& OutItems) const;

	/** 각 URLRewardDataAsset의 Rewards를 순서대로 OutRewards에 추가 */
	static void CollectRewards(const TArray<TObjectPtr<URLRewardDataAsset>>& Sources, TArray<FRLRewardData>& OutRewards);
};
