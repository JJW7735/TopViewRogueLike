#include "RLBossTrigger.h"
#include "RLArenaMoodController.h"
#include "RLCharacter.h"
#include "RLFogWall.h"
#include "RLSaveSubsystem.h"
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

namespace
{
	bool ShouldSkipClearedBossEncounter(const UObject* WorldContextObject, const ARLCharacter* PlayerChar)
	{
		if (!WorldContextObject || !PlayerChar || !PlayerChar->ActorHasTag(TEXT("RunResumed")))
		{
			return false;
		}

		URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(WorldContextObject);
		if (!SaveSys || !SaveSys->HasActiveRun() || !SaveSys->GetActiveRunData().bIsStageCleared)
		{
			return false;
		}

		const UWorld* World = WorldContextObject->GetWorld();
		const FString CurrentMap = World ? UWorld::RemovePIEPrefix(World->GetMapName()) : TEXT("");
		const FString SavedMap = UWorld::RemovePIEPrefix(SaveSys->GetActiveRunData().CurrentMapName.ToString());
		return !SavedMap.IsEmpty() && SavedMap != TEXT("None") && CurrentMap.Equals(SavedMap, ESearchCase::IgnoreCase);
	}
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

	// 로드 시 플레이어가 이미 클리어 상태로 이어하기했는지 0.2초 후 점검 (ARLCharacter::BeginPlay 완료 대기)
	FTimerHandle CheckTimer;
	GetWorldTimerManager().SetTimer(CheckTimer, [this]()
	{
		if (bTriggered) return;

		ARLCharacter* PlayerChar = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		if (ShouldSkipClearedBossEncounter(this, PlayerChar))
		{
			UE_LOG(LogTemp, Log, TEXT("[BossTrigger] '%s': 저장된 보스전이 이미 클리어된 상태입니다. 보스 소환을 건너뛰고 장벽을 개방합니다."), *GetName());

			bTriggered = true;
			TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UnlockBarriers();
			Tags.AddUnique(TEXT("BossDefeated"));
			if (MoodController)
			{
				MoodController->SetBossPhase(true, true);
			}
		}
	}, 0.2f, false);
}

void ARLBossTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		if (ShouldSkipClearedBossEncounter(this, PlayerChar))
		{
			bTriggered = true;
			TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UnlockBarriers();
			Tags.AddUnique(TEXT("BossDefeated"));
			if (MoodController)
			{
				MoodController->SetBossPhase(true, true);
			}
			return;
		}

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
	Tags.AddUnique(TEXT("BossDefeated"));
	UnlockBarriers();
}
