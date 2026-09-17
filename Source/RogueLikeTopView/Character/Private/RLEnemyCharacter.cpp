// Fill out your copyright notice in the Description page of Project Settings.

#include "RLEnemyCharacter.h"
#include "RLHealthComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"

ARLEnemyCharacter::ARLEnemyCharacter()
{
}

void ARLEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// EnrageHealthPercentThreshold가 음수(기본값)인 일반 몬스터도 델리게이트 바인딩 자체는 걸어두지만,
	// HandleEnemyHealthChanged 내부에서 threshold < 0이면 바로 return하므로 실질적인 비용은 거의 없습니다.
	if (HealthComp)
	{
		HealthComp->OnHealthChanged.AddDynamic(this, &ARLEnemyCharacter::HandleEnemyHealthChanged);
	}
}

float ARLEnemyCharacter::GetHealthPercent() const
{
	if (!HealthComp || FMath::IsNearlyZero(HealthComp->MaxHealth))
	{
		return 0.0f;
	}
	return HealthComp->CurrentHealth / HealthComp->MaxHealth;
}

void ARLEnemyCharacter::HandleEnemyHealthChanged(float NewCurrentHealth, float NewMaxHealth, float HealthDelta)
{
	// ★ 이 몬스터가 분노 기능을 쓰지 않는 개체(일반 몬스터, threshold 기본값 -1)라면 여기서 끝냅니다.
	if (EnrageHealthPercentThreshold < 0.0f)
	{
		return;
	}

	const float Percent = FMath::IsNearlyZero(NewMaxHealth) ? 0.0f : (NewCurrentHealth / NewMaxHealth);

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsFloat(BlackboardKey_HealthPercent, Percent);
		}
	}

	if (!bIsEnraged && Percent <= EnrageHealthPercentThreshold)
	{
		EnterEnrage();
	}
	else if (bIsEnraged && !bEnrageIsOneWay && Percent > EnrageHealthPercentThreshold)
	{
		bIsEnraged = false;
		if (AAIController* AICon = Cast<AAIController>(GetController()))
		{
			if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
			{
				BB->SetValueAsBool(BlackboardKey_IsEnraged, false);
			}
		}
	}
}

void ARLEnemyCharacter::EnterEnrage()
{
	bIsEnraged = true;

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(BlackboardKey_IsEnraged, true);
		}
	}

	OnEnraged();
}

void ARLEnemyCharacter::HandleDeath()
{
	Super::HandleDeath();

	bIsEnraged = false;

	// ★ AAIController::GetBrainComponent() / UBrainComponent::StopLogic()은 AIModule의 안정적인 공개 API입니다.
	// 정확한 시그니처는 IDE 자동완성으로 한 번 더 확인해보시는 걸 권장합니다(제가 엔진 소스를 직접 grep한 건 아니라서요).
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("EnemyDied"));
		}
	}
}
