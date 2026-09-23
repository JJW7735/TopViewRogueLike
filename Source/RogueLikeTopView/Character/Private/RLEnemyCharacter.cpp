#include "RLEnemyCharacter.h"
#include "RLHealthComponent.h"
#include "RLRewardChest.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

ARLEnemyCharacter::ARLEnemyCharacter()
{
	DropChestClass = ARLRewardChest::StaticClass();
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

	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("EnemyDied"));
		}
	}

	// 몬스터 사망 델리게이트 방송 (방 관리자 / 웨이브 시스템 연동)
	OnEnemyDied.Broadcast(this);

	// ★ 사망 시 보상 상자 드롭
	if (bDropChestOnDeath && DropChestClass)
	{
		if (FMath::FRand() <= DropChance)
		{
			FVector SpawnLoc = GetActorLocation() + ChestSpawnOffset;
			if (UCapsuleComponent* Capsule = GetCapsuleComponent())
			{
				SpawnLoc.Z -= Capsule->GetScaledCapsuleHalfHeight();
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			GetWorld()->SpawnActor<ARLRewardChest>(DropChestClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Yellow,
					FString::Printf(TEXT("🎁 [%s] 처치! 보상 상자가 생성되었습니다!"), *GetName()));
			}
		}
	}
}
