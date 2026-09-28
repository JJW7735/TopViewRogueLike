#include "RLEnemyCharacter.h"
#include "RLHealthComponent.h"
#include "RLStatComponent.h"
#include "RLRewardChest.h"
#include "RLItemDrop.h"
#include "RLItemDataAsset.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ARLEnemyCharacter::ARLEnemyCharacter()
{
	DropChestClass = ARLRewardChest::StaticClass();
	ItemDropClass = ARLItemDrop::StaticClass();
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

	// 1. 플레이어에게 처치 경험치 지급
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (URLStatComponent* PlayerStats = PlayerChar->GetStatComponent())
		{
			PlayerStats->AddExp(ExpReward);
		}
	}

	// 몬스터 사망 델리게이트 방송 (방 관리자 / 웨이브 시스템 연동)
	OnEnemyDied.Broadcast(this);

	const FVector MyLoc = GetActorLocation();

	// 보스 여부 판정 (bIsBoss 체크 여부 또는 클래스/액터 이름에 "Boss"가 포함되어 있으면 자동 보스 판정)
	const bool bIsActualBoss = bIsBoss || GetClass()->GetName().Contains(TEXT("Boss"), ESearchCase::IgnoreCase) || GetName().Contains(TEXT("Boss"), ESearchCase::IgnoreCase);

	// 2. 보스 처치: 100% 확정 보상 상자 + 대량 골드 드롭
	if (bIsActualBoss)
	{
		// 보상 상자 클래스 확보 (MyRLRewardChest 에셋이 있으면 메시/이펙트가 들어간 블루프린트로 스폰)
		TSubclassOf<ARLRewardChest> TargetChestClass = DropChestClass;
		if (!TargetChestClass || TargetChestClass == ARLRewardChest::StaticClass())
		{
			TargetChestClass = LoadClass<ARLRewardChest>(nullptr, TEXT("/Game/Blueprint/MyRLRewardChest.MyRLRewardChest_C"));
			if (!TargetChestClass)
			{
				TargetChestClass = ARLRewardChest::StaticClass();
			}
		}

		if (TargetChestClass)
		{
			FVector SpawnLoc = MyLoc + ChestSpawnOffset;
			if (UCapsuleComponent* Capsule = GetCapsuleComponent())
			{
				SpawnLoc.Z -= (Capsule->GetScaledCapsuleHalfHeight() - 10.0f);
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			GetWorld()->SpawnActor<ARLRewardChest>(TargetChestClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
					FString::Printf(TEXT("🏆 [보스 처치!] %s 보상 상자가 생성되었습니다!"), *TargetChestClass->GetName()));
			}
		}

		// 보스 주변에 대량의 골드 드롭 3개 스폰
		if (ItemDropClass)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ARLItemDrop* GoldDrop = GetWorld()->SpawnActor<ARLItemDrop>(ItemDropClass, MyLoc, FRotator::ZeroRotator, SpawnParams))
				{
					GoldDrop->InitializeGoldDrop(FMath::RandRange(40, 70));
					const float Angle = (360.0f / 3.0f) * i;
					const FVector Dir = FRotator(0.f, Angle, 0.f).Vector();
					GoldDrop->LaunchDrop(Dir * 200.0f + FVector(0.f, 0.f, 400.f));
				}
			}
		}
	}
	// 3. 일반 몬스터: 보상 상자 X, 35% 확률로 바닥에 필드 아이템/골드만 드롭
	else if (ItemDropClass && FMath::FRand() <= ItemDropChance)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (ARLItemDrop* DropActor = GetWorld()->SpawnActor<ARLItemDrop>(ItemDropClass, MyLoc, FRotator::ZeroRotator, SpawnParams))
		{
			// 아이템 후보 풀이 있고 50% 확률이면 아이템 드롭, 아니면 골드 드롭
			if (PossibleItemDrops.Num() > 0 && FMath::RandBool())
			{
				const int32 RandomIdx = FMath::RandRange(0, PossibleItemDrops.Num() - 1);
				DropActor->InitializeItemDrop(PossibleItemDrops[RandomIdx], 1);
			}
			else
			{
				const int32 RolledGold = FMath::RandRange(MinGoldDrop, MaxGoldDrop);
				DropActor->InitializeGoldDrop(RolledGold);
			}

			// 랜덤한 방향으로 퐁 튀어나오는 포물선 발사
			const FVector RandomDir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
			DropActor->LaunchDrop(RandomDir * FMath::FRandRange(100.f, 200.f) + FVector(0.f, 0.f, FMath::FRandRange(280.f, 380.f)));
		}
	}
}
