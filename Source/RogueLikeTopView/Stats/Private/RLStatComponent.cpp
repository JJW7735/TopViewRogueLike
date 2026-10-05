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

void URLStatComponent::AddExp(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	CurrentExp += Amount;

	// 레벨업 체크 (경험치가 여러 레벨을 넘길 경우 반복)
	bool bLeveledUp = false;
	while (MaxExp > 0.0f && CurrentExp >= MaxExp)
	{
		CurrentExp -= MaxExp;
		CurrentLevel++;
		MaxExp = FMath::RoundToFloat(MaxExp * ExpGrowthMultiplier);
		bLeveledUp = true;

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
				FString::Printf(TEXT("🎉 [LEVEL UP!] 레벨 %d 달성! (다음 필요 경험치: %.0f)"), CurrentLevel, MaxExp));
		}

		OnLevelUp.Broadcast(CurrentLevel);
	}

	const float Percent = MaxExp > 0.0f ? FMath::Clamp(CurrentExp / MaxExp, 0.0f, 1.0f) : 0.0f;
	OnExpChanged.Broadcast(CurrentExp, MaxExp, Percent);
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

void URLStatComponent::NotifyStatsChanged()
{
	OnStatsChanged.Broadcast();
}

void URLStatComponent::RestoreStatsDirect(int32 InLevel, float InCurrentExp, float InMaxExp, float InExpGrowth,
	float InAttackPower, float InAbilityPower, float InCritChance,
	float InCritDamageMultiplier, float InMoveSpeed, float InAttackSpeed,
	float InDefense, float InDamageReduction)
{
	CurrentLevel = FMath::Max(1, InLevel);
	CurrentExp = FMath::Max(0.0f, InCurrentExp);
	MaxExp = FMath::Max(1.0f, InMaxExp);
	ExpGrowthMultiplier = FMath::Max(1.0f, InExpGrowth);

	AttackPower = InAttackPower;
	AbilityPower = InAbilityPower;
	CritChance = FMath::Clamp(InCritChance, 0.0f, 100.0f);
	CritDamageMultiplier = FMath::Max(1.0f, InCritDamageMultiplier);
	MoveSpeed = FMath::Max(0.0f, InMoveSpeed);
	AttackSpeed = FMath::Max(0.1f, InAttackSpeed);
	Defense = InDefense;
	DamageReduction = FMath::Clamp(InDamageReduction, 0.0f, 80.0f);

	NotifyStatsChanged();
	const float Percent = MaxExp > 0.0f ? FMath::Clamp(CurrentExp / MaxExp, 0.0f, 1.0f) : 0.0f;
	OnExpChanged.Broadcast(CurrentExp, MaxExp, Percent);
}
