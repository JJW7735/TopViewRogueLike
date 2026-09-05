#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLTypes.h"
#include "RLMonsterPatternDataAsset.generated.h"

UCLASS(BlueprintType)
class ROGUELIKETOPVIEW_API URLMonsterPatternDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patterns")
	TArray<FRLMonsterPatternData> Patterns;
};