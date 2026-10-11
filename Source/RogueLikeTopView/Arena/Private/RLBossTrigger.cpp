#include "RLBossTrigger.h"
#include "RLArenaMoodController.h"
#include "RLCharacter.h"
#include "RLFogWall.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

ARLBossTrigger::ARLBossTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->InitBoxExtent(FVector(300.f, 300.f, 150.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	BossSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("BossSpawnPoint"));
	BossSpawnPoint->SetupAttachment(RootComponent);
	BossSpawnPoint->ArrowSize = 3.f;
	BossSpawnPoint->SetRelativeLocation(FVector(1000.f, 0.f, 0.f));
}

void ARLBossTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (!MoodController)
	{
		for (TActorIterator<ARLArenaMoodController> It(GetWorld()); It; ++It)
		{
			if (It->GetLevel() == GetLevel())
			{
				MoodController = *It;
				break;
			}
		}
	}

	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLBossTrigger::OnTriggerBeginOverlap);
}

void ARLBossTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<ARLCharacter>(OtherActor))
	{
		TriggerBossEncounter();
	}
}

void ARLBossTrigger::TriggerBossEncounter()
{
	if (bTriggerOnce && bTriggered)
	{
		return;
	}
	bTriggered = true;

	if (bTriggerOnce)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (BossClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// AutoPossessAI(PlacedInWorldOrSpawned)로 AI 컨트롤러가 자동 빙의됨
		SpawnedBoss = GetWorld()->SpawnActor<APawn>(BossClass, BossSpawnPoint->GetComponentLocation(), BossSpawnPoint->GetComponentRotation(), SpawnParams);
	}

	if (MoodController)
	{
		MoodController->SetBossPhase(true);
	}

	if (SpawnedBoss)
	{
		SpawnedBoss->OnDestroyed.AddUniqueDynamic(this, &ARLBossTrigger::HandleBossDestroyed);
		OnBossSpawned(SpawnedBoss);
	}

	// 들어온 길목을 닫아 보스전에서 도망칠 수 없게 함
	if (BarrierCloseDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(BarrierCloseTimerHandle, this, &ARLBossTrigger::LockBarriers, BarrierCloseDelay, false);
	}
	else
	{
		LockBarriers();
	}
}

void ARLBossTrigger::LockBarriers()
{
	// 플레이어가 안개벽 안/근처에 서 있으면 끼이지 않도록 0.5초 뒤 다시 시도
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector PlayerLoc = Player->GetActorLocation();
		for (AActor* Barrier : BarrierActors)
		{
			const ARLFogWall* FogWall = Cast<ARLFogWall>(Barrier);
			if (FogWall && !FogWall->IsBlocking() && FogWall->BlockVolume)
			{
				const FBox FogBox = FogWall->BlockVolume->Bounds.GetBox().ExpandBy(FVector(150.0f, 150.0f, 50.0f));
				if (FogBox.IsInsideOrOn(PlayerLoc))
				{
					GetWorldTimerManager().SetTimer(BarrierCloseTimerHandle, this, &ARLBossTrigger::LockBarriers, 0.5f, false);
					return;
				}
			}
		}
	}

	for (AActor* Barrier : BarrierActors)
	{
		if (ARLFogWall* FogWall = Cast<ARLFogWall>(Barrier))
		{
			FogWall->Restore();
		}
		else if (Barrier)
		{
			Barrier->SetActorHiddenInGame(false);
			Barrier->SetActorEnableCollision(true);
		}
	}
}

void ARLBossTrigger::UnlockBarriers()
{
	GetWorldTimerManager().ClearTimer(BarrierCloseTimerHandle);

	for (AActor* Barrier : BarrierActors)
	{
		if (ARLFogWall* FogWall = Cast<ARLFogWall>(Barrier))
		{
			FogWall->Dissolve();
		}
		else if (Barrier)
		{
			Barrier->SetActorHiddenInGame(true);
			Barrier->SetActorEnableCollision(false);
		}
	}
}

void ARLBossTrigger::HandleBossDestroyed(AActor* DestroyedActor)
{
	UnlockBarriers();
}
