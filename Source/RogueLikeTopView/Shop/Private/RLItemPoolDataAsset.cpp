#include "RLItemPoolDataAsset.h"
#include "RLItemDataAsset.h"
#include "RLRewardDataAsset.h"

const TCHAR* URLItemPoolDataAsset::DefaultAssetPath = TEXT("/Game/DataAsset/DA_ItemPool.DA_ItemPool");

URLItemPoolDataAsset* URLItemPoolDataAsset::LoadDefault()
{
	// 에셋이 없는 것이 정상적인 폴백 상황이므로 로드 실패 경고를 남기지 않음
	return LoadObject<URLItemPoolDataAsset>(nullptr, DefaultAssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void URLItemPoolDataAsset::CollectItems(TArray<TObjectPtr<URLItemDataAsset>>& OutItems) const
{
	for (URLItemDataAsset* Item : Items)
	{
		if (Item)
		{
			OutItems.Add(Item);
		}
	}
}

void URLItemPoolDataAsset::CollectRewards(const TArray<TObjectPtr<URLRewardDataAsset>>& Sources, TArray<FRLRewardData>& OutRewards)
{
	for (const URLRewardDataAsset* Source : Sources)
	{
		if (Source)
		{
			OutRewards.Append(Source->Rewards);
		}
	}
}
