// Fill out your copyright notice in the Description page of Project Settings.


#include "RLStatComponent.h"

// Sets default values for this component's properties
URLStatComponent::URLStatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void URLStatComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

float URLStatComponent::EvaluateDamage(float InRawDamage, bool& bOutIsCritical) const
{
	bOutIsCritical = false;

	if (CritChance > 0.0f)
	{
		const float Roll = FMath::FRandRange(0.0f, 100.0f);
		if (Roll <= CritChance)
		{
			bOutIsCritical = true;
			return InRawDamage * CritDamageMultiplier;
		}
	}

	return InRawDamage;
}

float URLStatComponent::CalculateDefendedDamage(float InRawDamage, float& OutReducedDamage) const
{
	OutReducedDamage = 0.0f;
	if (InRawDamage <= 0.0f)
	{
		return 0.0f;
	}

	// 1. 방어력(Defense) 기반 데미지 배율 계산 (점근적 공식: 100 / (100 + Defense))
	float DefenseMultiplier = 1.0f;
	if (Defense >= 0.0f)
	{
		DefenseMultiplier = 100.0f / (100.0f + Defense);
	}
	else
	{
		// 음수 방어력(방어력 감소 디버프)일 때 추가 피해 적용
		DefenseMultiplier = 2.0f - (100.0f / (100.0f - Defense));
	}

	// 2. 퍼센트 피해 감소율(DamageReduction) 적용 (최대 80% 상한선)
	const float ClampedReduction = FMath::Clamp(DamageReduction, 0.0f, 80.0f);
	const float ReductionMultiplier = 1.0f - (ClampedReduction / 100.0f);

	// 3. 최종 받는 데미지 산출 (최소 1.0f 데미지 보장)
	const float CalculatedDamage = InRawDamage * DefenseMultiplier * ReductionMultiplier;
	const float FinalDamage = FMath::Max(1.0f, CalculatedDamage);

	OutReducedDamage = FMath::Max(0.0f, InRawDamage - FinalDamage);
	return FinalDamage;
}
