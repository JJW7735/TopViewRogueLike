#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLRewardTypes.h"
#include "RLRewardDataAsset.generated.h"

/**
 * 에디터에서 보상 카드 목록을 관리하고 튜닝할 수 있는 데이터 에셋
 */
UCLASS(BlueprintType)
class ROGUELIKETOPVIEW_API URLRewardDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<FRLRewardData> Rewards;
};
