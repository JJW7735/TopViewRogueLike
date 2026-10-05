#include "RLArenaManager.h"
#include "RLEnemyCharacter.h"
#include "RLRewardChest.h"
#include "RLPortal.h"
#include "RLCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

ARLArenaManager::ARLArenaManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(RootComponent);
	TriggerVolume->SetBoxExtent(FVector(1000.0f, 1000.0f, 300.0f));
	TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
	TriggerVolume->SetCollisionResponseToChannel(ECC_Ground, ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	TriggerVolume->SetGenerateOverlapEvents(true);

	PortalSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PortalSpawnPoint"));
	PortalSpawnPoint->SetupAttachment(RootComponent);
	PortalSpawnPoint->SetRelativeLocation(FVector(0.0f, 400.0f, 0.0f));

	ChestSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ChestSpawnPoint"));
	ChestSpawnPoint->SetupAttachment(RootComponent);
	ChestSpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

	EntranceBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("EntranceBarrier"));
	EntranceBarrier->SetupAttachment(RootComponent);
	EntranceBarrier->SetRelativeLocation(FVector(0.0f, -600.0f, 100.0f));
	EntranceBarrier->SetBoxExtent(FVector(50.0f, 250.0f, 150.0f));
	EntranceBarrier->SetCollisionProfileName(TEXT("Custom"));
	EntranceBarrier->SetCollisionObjectType(ECC_WorldStatic);
	EntranceBarrier->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntranceBarrier->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	EntranceBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EntranceBarrier->SetHiddenInGame(true);

	// 사방 4면 테두리 자동 결계 벽 (TriggerVolume 둘레를 감싸 플레이어 이탈 방지)
	BoundaryWall_North = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundaryWall_North"));
	BoundaryWall_North->SetupAttachment(TriggerVolume);
	BoundaryWall_North->SetCollisionProfileName(TEXT("Custom"));
	BoundaryWall_North->SetCollisionObjectType(ECC_WorldStatic);
	BoundaryWall_North->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundaryWall_North->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BoundaryWall_North->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryWall_North->SetHiddenInGame(true);

	BoundaryWall_South = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundaryWall_South"));
	BoundaryWall_South->SetupAttachment(TriggerVolume);
	BoundaryWall_South->SetCollisionProfileName(TEXT("Custom"));
	BoundaryWall_South->SetCollisionObjectType(ECC_WorldStatic);
	BoundaryWall_South->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundaryWall_South->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BoundaryWall_South->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryWall_South->SetHiddenInGame(true);

	BoundaryWall_East = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundaryWall_East"));
	BoundaryWall_East->SetupAttachment(TriggerVolume);
	BoundaryWall_East->SetCollisionProfileName(TEXT("Custom"));
	BoundaryWall_East->SetCollisionObjectType(ECC_WorldStatic);
	BoundaryWall_East->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundaryWall_East->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BoundaryWall_East->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryWall_East->SetHiddenInGame(true);

	BoundaryWall_West = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundaryWall_West"));
	BoundaryWall_West->SetupAttachment(TriggerVolume);
	BoundaryWall_West->SetCollisionProfileName(TEXT("Custom"));
	BoundaryWall_West->SetCollisionObjectType(ECC_WorldStatic);
	BoundaryWall_West->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundaryWall_West->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BoundaryWall_West->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryWall_West->SetHiddenInGame(true);

	// 기본 클래스 폴백 세팅
	RewardChestClass = ARLRewardChest::StaticClass();
	PortalClass = ARLPortal::StaticClass();
}

void ARLArenaManager::BeginPlay()
{
	Super::BeginPlay();

	// 전투 영역 트리거가 마우스 바닥 클릭(Ground)이나 카메라를 방해하지 않도록 보장
	if (TriggerVolume)
	{
		TriggerVolume->SetCollisionResponseToChannel(ECC_Ground, ECR_Ignore);
		TriggerVolume->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		TriggerVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}

	// 입구 차단벽 평상시(전투 전) 비활성화
	if (EntranceBarrier)
	{
		EntranceBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EntranceBarrier->SetHiddenInGame(true);
	}

	// 4면 결계 벽 위치 초기화 및 비활성화
	UpdateBoundaryWalls();
	UBoxComponent* InitWalls[] = { BoundaryWall_North, BoundaryWall_South, BoundaryWall_East, BoundaryWall_West };
	for (UBoxComponent* Wall : InitWalls)
	{
		if (Wall)
		{
			Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Wall->SetHiddenInGame(true);
		}
	}

	// 1. 블루프린트 내부에 배치된 스폰 포인트 화살표(SpawnPoint_Enemy1~5 등) 자동 수집
	if (InternalSpawnPoints.IsEmpty())
	{
		TArray<USceneComponent*> AllComps;
		GetComponents<USceneComponent>(AllComps);
		for (USceneComponent* Comp : AllComps)
		{
			if (Comp && Comp != SceneRoot && Comp != PortalSpawnPoint && Comp != ChestSpawnPoint && Comp != TriggerVolume)
			{
				if (Comp->GetName().Contains(TEXT("SpawnPoint_Enemy")) || Comp->IsA<UArrowComponent>())
				{
					// SpawnPoint_Portal은 제외
					if (!Comp->GetName().Contains(TEXT("Portal")))
					{
						InternalSpawnPoints.Add(Comp);
					}
				}
			}
		}

		// 이름 순 정렬 (SpawnPoint_Enemy1, SpawnPoint_Enemy2 ...)
		InternalSpawnPoints.Sort([](const USceneComponent& A, const USceneComponent& B)
		{
			return A.GetName() < B.GetName();
		});
	}

	// 2. 블루프린트에 기존 SpawnPoint_Portal 화살표가 있다면 PortalSpawnPoint 위치로 연동
	TArray<USceneComponent*> SceneComps;
	GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* Comp : SceneComps)
	{
		if (Comp && Comp != PortalSpawnPoint && Comp->GetName().Contains(TEXT("SpawnPoint_Portal")) && PortalSpawnPoint)
		{
			PortalSpawnPoint->SetWorldLocationAndRotation(Comp->GetComponentLocation(), Comp->GetComponentRotation());
			break;
		}
	}

	if (TriggerVolume)
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ARLArenaManager::OnTriggerVolumeOverlap);
	}

	// 기존 포탈이 지정되어 있다면 전투 전에는 숨겨두기
	if (ExistingPortalActor)
	{
		ExistingPortalActor->SetActorHiddenInGame(true);
		ExistingPortalActor->SetActorEnableCollision(false);
	}
}

void ARLArenaManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveDelayTimerHandle);
		World->GetTimerManager().ClearTimer(SpawnIntervalTimerHandle);
		World->GetTimerManager().ClearTimer(BarrierCloseTimerHandle);
	}

	CleanupArenaSpawnedActors();

	Super::EndPlay(EndPlayReason);
}

void ARLArenaManager::OnTriggerVolumeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bAutoStartOnOverlap || CurrentState != ERLArenaState::Inactive)
	{
		return;
	}

	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		if (PlayerChar->IsPlayerControlled())
		{
			StartArena();
		}
	}
}

void ARLArenaManager::StartArena()
{
	if (CurrentState != ERLArenaState::Inactive)
	{
		return;
	}

	if (Waves.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ArenaManager] '%s': 등록된 웨이브(Waves)가 없어 아레나를 시작할 수 없습니다."), *GetName());
		return;
	}

	CurrentState = ERLArenaState::InProgress;
	OnArenaStateChanged.Broadcast(CurrentState);

	BP_OnArenaStarted();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, FString::Printf(TEXT("[Arena] %s: 전투 시작! 총 %d개 웨이브"), *GetName(), Waves.Num()));
	}

	// 플레이어가 방 안으로 안전하게 진입한 후 결계 닫기 (벽 끼임 방지)
	if (BarrierCloseDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(BarrierCloseTimerHandle, this, &ARLArenaManager::LockBarriers, BarrierCloseDelay, false);
	}
	else
	{
		LockBarriers();
	}

	StartWave(0);
}

void ARLArenaManager::StartWave(int32 WaveIndex)
{
	if (!Waves.IsValidIndex(WaveIndex))
	{
		CompleteArena();
		return;
	}

	CurrentWaveIndex = WaveIndex;
	CurrentState = ERLArenaState::InProgress;
	OnArenaStateChanged.Broadcast(CurrentState);

	const FRLWaveData& WaveData = Waves[CurrentWaveIndex];

	// 웨이브 스폰 큐 조립
	SpawnQueue.Empty();
	TotalWaveEnemies = 0;

	for (const FRLMonsterSpawnEntry& Entry : WaveData.Spawns)
	{
		if (!Entry.EnemyClass || Entry.Count <= 0)
		{
			continue;
		}

		for (int32 i = 0; i < Entry.Count; ++i)
		{
			FPendingSpawn Pending;
			Pending.EnemyClass = Entry.EnemyClass;
			Pending.PreferredPointIndex = Entry.SpecificSpawnPointIndex;
			SpawnQueue.Add(Pending);
			TotalWaveEnemies++;
		}
	}

	RemainingEnemies = TotalWaveEnemies;

	OnWaveStarted.Broadcast(CurrentWaveIndex, Waves.Num());
	OnEnemyCountChanged.Broadcast(RemainingEnemies, TotalWaveEnemies);
	BP_OnWaveStarted(CurrentWaveIndex, Waves.Num());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			FString::Printf(TEXT("[Arena] 웨이브 %d/%d (%s) 시작! (적: %d마리)"),
				CurrentWaveIndex + 1, Waves.Num(), *WaveData.WaveName, TotalWaveEnemies));
	}

	if (TotalWaveEnemies == 0)
	{
		CompleteCurrentWave();
		return;
	}

	// 몬스터 순차 스폰 타이머 설정
	if (WaveData.SpawnInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(SpawnIntervalTimerHandle, this, &ARLArenaManager::ProcessSpawnQueue, WaveData.SpawnInterval, true, 0.0f);
	}
	else
	{
		while (!SpawnQueue.IsEmpty())
		{
			ProcessSpawnQueue();
		}
	}
}

void ARLArenaManager::ProcessSpawnQueue()
{
	if (SpawnQueue.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SpawnIntervalTimerHandle);
		return;
	}

	FPendingSpawn Next = SpawnQueue[0];
	SpawnQueue.RemoveAt(0);

	SpawnSingleEnemy(Next.EnemyClass, Next.PreferredPointIndex);

	if (SpawnQueue.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SpawnIntervalTimerHandle);
	}
}

void ARLArenaManager::SpawnSingleEnemy(TSubclassOf<ARLEnemyCharacter> EnemyClass, int32 PreferredPointIndex)
{
	UWorld* World = GetWorld();
	if (!World || !EnemyClass)
	{
		RemainingEnemies = FMath::Max(0, RemainingEnemies - 1);
		OnEnemyCountChanged.Broadcast(RemainingEnemies, TotalWaveEnemies);
		if (RemainingEnemies <= 0 && SpawnQueue.IsEmpty())
		{
			CompleteCurrentWave();
		}
		return;
	}

	FRotator SpawnRot = FRotator::ZeroRotator;
	const FVector SpawnLoc = GetSpawnLocation(PreferredPointIndex, SpawnRot);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARLEnemyCharacter* Enemy = World->SpawnActor<ARLEnemyCharacter>(EnemyClass, SpawnLoc, SpawnRot, SpawnParams);
	if (Enemy)
	{
		ActiveEnemies.Add(Enemy);
		Enemy->OnEnemyDied.AddDynamic(this, &ARLArenaManager::HandleEnemyDied);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ArenaManager] '%s' 몬스터 스폰 실패!"), *EnemyClass->GetName());
		RemainingEnemies = FMath::Max(0, RemainingEnemies - 1);
		OnEnemyCountChanged.Broadcast(RemainingEnemies, TotalWaveEnemies);

		if (RemainingEnemies <= 0 && SpawnQueue.IsEmpty())
		{
			CompleteCurrentWave();
		}
	}
}

FVector ARLArenaManager::GetSpawnLocation(int32 PointIndex, FRotator& OutRotation) const
{
	UWorld* World = GetWorld();

	// 플레이어 위치 파악 (회전 방향 및 최소 거리 판정용)
	FVector PlayerLoc = GetActorLocation();
	if (ARLCharacter* Player = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		PlayerLoc = Player->GetActorLocation();
	}

	// 1. 고정 스폰 포인트 모드이거나, 특정 인덱스가 명시적으로 지정된 경우
	if (SpawnLocationMode == ERLSpawnLocationMode::FixedSpawnPoints || PointIndex >= 0)
	{
		// 1-1. 특정 인덱스 지정 시 내부 화살표 확인
		if (PointIndex >= 0 && InternalSpawnPoints.IsValidIndex(PointIndex) && InternalSpawnPoints[PointIndex])
		{
			OutRotation = InternalSpawnPoints[PointIndex]->GetComponentRotation();
			return InternalSpawnPoints[PointIndex]->GetComponentLocation();
		}
		// 1-2. 특정 인덱스 지정 시 외부 액터 확인
		if (PointIndex >= 0 && SpawnPointActors.IsValidIndex(PointIndex) && SpawnPointActors[PointIndex])
		{
			OutRotation = SpawnPointActors[PointIndex]->GetActorRotation();
			return SpawnPointActors[PointIndex]->GetActorLocation();
		}

		// 1-3. 내부 화살표 컴포넌트 풀에서 순환 분배
		if (InternalSpawnPoints.Num() > 0)
		{
			int32 ValidIndex = (const_cast<ARLArenaManager*>(this)->SpawnPointRoundRobin++) % InternalSpawnPoints.Num();
			if (USceneComponent* ChosenComp = InternalSpawnPoints[ValidIndex])
			{
				OutRotation = ChosenComp->GetComponentRotation();
				return ChosenComp->GetComponentLocation();
			}
		}

		// 1-4. 외부 스폰 액터 풀에서 순환 분배
		if (SpawnPointActors.Num() > 0)
		{
			int32 ValidIndex = (const_cast<ARLArenaManager*>(this)->SpawnPointRoundRobin++) % SpawnPointActors.Num();
			if (AActor* Chosen = SpawnPointActors[ValidIndex])
			{
				OutRotation = Chosen->GetActorRotation();
				return Chosen->GetActorLocation();
			}
		}
	}

	// 2. [기본 모드] 전투 영역(TriggerVolume) 내 완전 무작위 스폰
	if (SpawnLocationMode == ERLSpawnLocationMode::RandomInTriggerVolume && TriggerVolume)
	{
		const FBoxSphereBounds Bounds = TriggerVolume->CalcBounds(TriggerVolume->GetComponentTransform());
		const FVector Center = Bounds.Origin;
		const FVector Extent = Bounds.BoxExtent * 0.85f; // 테두리 벽에 끼지 않도록 85% 범위 내 선정

		FVector CandidateLoc = Center;
		bool bFoundValidSpot = false;

		// 플레이어 코앞을 피하면서 최대 15회 후보 좌표 무작위 추출
		for (int32 Attempt = 0; Attempt < 15; ++Attempt)
		{
			CandidateLoc = FVector(
				FMath::FRandRange(Center.X - Extent.X, Center.X + Extent.X),
				FMath::FRandRange(Center.Y - Extent.Y, Center.Y + Extent.Y),
				Center.Z
			);

			if (FVector::Dist2D(CandidateLoc, PlayerLoc) >= MinDistanceFromPlayer)
			{
				bFoundValidSpot = true;
				break;
			}
		}

		// 네비메시(NavMesh) 위에 안전하게 안착
		if (bProjectToNavMesh && World)
		{
			if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				FNavLocation NavLoc;
				if (NavSys->ProjectPointToNavigation(CandidateLoc, NavLoc, FVector(500.0f, 500.0f, 800.0f)))
				{
					CandidateLoc = NavLoc.Location + FVector(0.0f, 0.0f, 20.0f);
				}
			}
		}

		// 스폰 즉시 플레이어 방향을 바라보도록 설정
		OutRotation = (PlayerLoc - CandidateLoc).Rotation();
		OutRotation.Pitch = 0.0f;
		OutRotation.Roll = 0.0f;
		return CandidateLoc;
	}

	// 3. 반경 기반 무작위 스폰 (RandomInRadius)
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Dist = FMath::FRandRange(FMath::Max(150.0f, MinDistanceFromPlayer), DefaultSpawnRadius);
	FVector CandidateLoc = GetActorLocation() + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 20.0f);

	if (bProjectToNavMesh && World)
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation NavLoc;
			if (NavSys->ProjectPointToNavigation(CandidateLoc, NavLoc, FVector(500.0f, 500.0f, 800.0f)))
			{
				CandidateLoc = NavLoc.Location + FVector(0.0f, 0.0f, 20.0f);
			}
		}
	}

	OutRotation = (PlayerLoc - CandidateLoc).Rotation();
	OutRotation.Pitch = 0.0f;
	OutRotation.Roll = 0.0f;
	return CandidateLoc;
}

void ARLArenaManager::HandleEnemyDied(ARLEnemyCharacter* DeadEnemy)
{
	ActiveEnemies.Remove(DeadEnemy);

	RemainingEnemies = FMath::Max(0, RemainingEnemies - 1);
	OnEnemyCountChanged.Broadcast(RemainingEnemies, TotalWaveEnemies);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("[Arena] 적 처치! 남은 적: %d / %d"), RemainingEnemies, TotalWaveEnemies));
	}

	if (RemainingEnemies <= 0 && SpawnQueue.IsEmpty())
	{
		CompleteCurrentWave();
	}
}

void ARLArenaManager::CompleteCurrentWave()
{
	GetWorldTimerManager().ClearTimer(SpawnIntervalTimerHandle);

	OnWaveCleared.Broadcast(CurrentWaveIndex);
	BP_OnWaveCleared(CurrentWaveIndex);

	const int32 NextWaveIndex = CurrentWaveIndex + 1;
	if (Waves.IsValidIndex(NextWaveIndex))
	{
		CurrentState = ERLArenaState::BetweenWaves;
		OnArenaStateChanged.Broadcast(CurrentState);

		const float Delay = Waves[NextWaveIndex].DelayBeforeWave;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow,
				FString::Printf(TEXT("[Arena] 웨이브 %d 클리어! %.1f초 후 다음 웨이브 시작..."), CurrentWaveIndex + 1, Delay));
		}

		FTimerDelegate TimerDel;
		TimerDel.BindUObject(this, &ARLArenaManager::StartWave, NextWaveIndex);
		GetWorldTimerManager().SetTimer(WaveDelayTimerHandle, TimerDel, FMath::Max(0.1f, Delay), false);
	}
	else
	{
		CompleteArena();
	}
}

void ARLArenaManager::CompleteArena()
{
	CurrentState = ERLArenaState::Cleared;
	OnArenaStateChanged.Broadcast(CurrentState);

	UnlockBarriers();

	UWorld* World = GetWorld();
	if (World)
	{
		// 1. 보상 상자 스폰
		if (bSpawnRewardChestOnClear)
		{
			TSubclassOf<ARLRewardChest> TargetChestClass = RewardChestClass;
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
				const FVector ChestLoc = ChestSpawnPoint ? ChestSpawnPoint->GetComponentLocation() : GetActorLocation();
				const FRotator ChestRot = ChestSpawnPoint ? ChestSpawnPoint->GetComponentRotation() : GetActorRotation();

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				SpawnedRewardChest = World->SpawnActor<ARLRewardChest>(TargetChestClass, ChestLoc, ChestRot, SpawnParams);
			}
		}

		// 2. 출구 포탈 활성화 또는 스폰
		if (ExistingPortalActor)
		{
			ExistingPortalActor->SetActorHiddenInGame(false);
			ExistingPortalActor->SetActorEnableCollision(true);
			if (ARLPortal* Portal = Cast<ARLPortal>(ExistingPortalActor))
			{
				if (SpawnedRewardChest.IsValid())
				{
					Portal->SetLinkedChestToClean(SpawnedRewardChest.Get());
				}
			}
		}
		else if (bSpawnPortalOnClear)
		{
			TSubclassOf<ARLPortal> TargetPortalClass = PortalClass;
			if (!TargetPortalClass || TargetPortalClass == ARLPortal::StaticClass())
			{
				TargetPortalClass = LoadClass<ARLPortal>(nullptr, TEXT("/Game/Blueprint/BP_Portal.BP_Portal_C"));
				if (!TargetPortalClass)
				{
					TargetPortalClass = ARLPortal::StaticClass();
				}
			}

			if (TargetPortalClass)
			{
				const FVector PortalLoc = PortalSpawnPoint ? PortalSpawnPoint->GetComponentLocation() : (GetActorLocation() + FVector(0.0f, 400.0f, 0.0f));
				const FRotator PortalRot = PortalSpawnPoint ? PortalSpawnPoint->GetComponentRotation() : GetActorRotation();

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				ARLPortal* NewPortal = World->SpawnActor<ARLPortal>(TargetPortalClass, PortalLoc, PortalRot, SpawnParams);
				if (NewPortal)
				{
					SpawnedPortal = NewPortal;
					if (SpawnedRewardChest.IsValid())
					{
						NewPortal->SetLinkedChestToClean(SpawnedRewardChest.Get());
					}
				}
			}
		}
	}

	OnArenaCleared.Broadcast();
	BP_OnArenaCleared();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[Arena] ★★★ %s: 아레나 모든 웨이브 완료! 보상 상자 및 포탈 개방! ★★★"), *GetName()));
	}
}

void ARLArenaManager::UpdateBoundaryWalls()
{
	if (!TriggerVolume)
	{
		return;
	}

	const FVector Extent = TriggerVolume->GetUnscaledBoxExtent();
	const float WallThickness = 50.0f;
	const float WallHeight = Extent.Z;

	// +X (북쪽) - TriggerVolume 외곽에 배치
	if (BoundaryWall_North)
	{
		BoundaryWall_North->SetRelativeLocation(FVector(Extent.X + WallThickness, 0.0f, 0.0f));
		BoundaryWall_North->SetBoxExtent(FVector(WallThickness, Extent.Y + WallThickness * 2.0f, WallHeight));
	}

	// -X (남쪽)
	if (BoundaryWall_South)
	{
		BoundaryWall_South->SetRelativeLocation(FVector(-(Extent.X + WallThickness), 0.0f, 0.0f));
		BoundaryWall_South->SetBoxExtent(FVector(WallThickness, Extent.Y + WallThickness * 2.0f, WallHeight));
	}

	// +Y (동쪽)
	if (BoundaryWall_East)
	{
		BoundaryWall_East->SetRelativeLocation(FVector(0.0f, Extent.Y + WallThickness, 0.0f));
		BoundaryWall_East->SetBoxExtent(FVector(Extent.X + WallThickness * 2.0f, WallThickness, WallHeight));
	}

	// -Y (서쪽)
	if (BoundaryWall_West)
	{
		BoundaryWall_West->SetRelativeLocation(FVector(0.0f, -(Extent.Y + WallThickness), 0.0f));
		BoundaryWall_West->SetBoxExtent(FVector(Extent.X + WallThickness * 2.0f, WallThickness, WallHeight));
	}
}

void ARLArenaManager::LockBarriers()
{
	// 0. 결계 닫기 전 안전 검사: 플레이어가 결계/벽 근처에 있으면 끼이지 않도록 잠시 대기
	ARLCharacter* Player = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (Player)
	{
		const FVector PlayerLoc = Player->GetActorLocation();

		// 플레이어가 경계 벽 근처(150cm 이내)에 머물러 있다면 끼이지 않도록 0.5초 더 대기
		bool bTooCloseToWall = false;
		UBoxComponent* AllCheckWalls[] = { BoundaryWall_North, BoundaryWall_South, BoundaryWall_East, BoundaryWall_West, EntranceBarrier };
		for (UBoxComponent* CheckWall : AllCheckWalls)
		{
			if (CheckWall)
			{
				const FBox WallBox = CheckWall->Bounds.GetBox().ExpandBy(FVector(150.0f, 150.0f, 50.0f));
				if (WallBox.IsInsideOrOn(PlayerLoc))
				{
					bTooCloseToWall = true;
					break;
				}
			}
		}

		if (bTooCloseToWall)
		{
			// 아직 입구에 걸쳐 있으므로 0.5초 후 다시 시도
			GetWorldTimerManager().SetTimer(BarrierCloseTimerHandle, this, &ARLArenaManager::LockBarriers, 0.5f, false);
			return;
		}
	}

	// 1. 사방 4면 전체 결계 활성화 (Pawn 채널만 Block, Ground/Camera/Visibility는 Ignore)
	if (bEncloseEntireArena)
	{
		UpdateBoundaryWalls();
		UBoxComponent* ActiveWalls[] = { BoundaryWall_North, BoundaryWall_South, BoundaryWall_East, BoundaryWall_West };
		for (UBoxComponent* Wall : ActiveWalls)
		{
			if (Wall)
			{
				Wall->SetCollisionProfileName(TEXT("Custom"));
				Wall->SetCollisionObjectType(ECC_WorldStatic);
				Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
				Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				Wall->SetHiddenInGame(false);
			}
		}
	}

	// 2. 자체 내장 입구 차단벽 활성화 (실내 룸 문 차단, 4면 결계와 중복 활성화 방지)
	if (EntranceBarrier && bUseEntranceBarrier && !bEncloseEntireArena)
	{
		EntranceBarrier->SetCollisionProfileName(TEXT("Custom"));
		EntranceBarrier->SetCollisionObjectType(ECC_WorldStatic);
		EntranceBarrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		EntranceBarrier->SetCollisionResponseToAllChannels(ECR_Ignore);
		EntranceBarrier->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		EntranceBarrier->SetHiddenInGame(false);
	}

	// 3. 혹시라도 결계 활성화 시점에 플레이어가 벽과 겹쳤다면 아레나 중심 쪽으로 안전하게 즉시 텔레포트 (끼임 완전 방지)
	if (Player)
	{
		const FVector Center = GetActorLocation();
		const FVector PlayerLoc = Player->GetActorLocation();
		const FVector DirToCenter = (Center - PlayerLoc).GetSafeNormal2D();

		UBoxComponent* AllCheckWalls[] = { BoundaryWall_North, BoundaryWall_South, BoundaryWall_East, BoundaryWall_West, EntranceBarrier };
		for (UBoxComponent* CheckWall : AllCheckWalls)
		{
			if (CheckWall && CheckWall->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				const FBox WallBox = CheckWall->Bounds.GetBox().ExpandBy(FVector(50.0f, 50.0f, 50.0f));
				if (WallBox.IsInsideOrOn(PlayerLoc))
				{
					Player->TeleportTo(PlayerLoc + DirToCenter * 200.0f, Player->GetActorRotation());
					break;
				}
			}
		}
	}

	// 4. 외부 등록된 문/장애물 액터 활성화
	for (AActor* Barrier : BarrierActors)
	{
		if (Barrier)
		{
			Barrier->SetActorHiddenInGame(false);
			Barrier->SetActorEnableCollision(true);
		}
	}
	BP_OnBarriersLocked();
}

void ARLArenaManager::UnlockBarriers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BarrierCloseTimerHandle);
	}

	// 1. 사방 4면 전체 결계 비활성화
	UBoxComponent* ActiveWalls[] = { BoundaryWall_North, BoundaryWall_South, BoundaryWall_East, BoundaryWall_West };
	for (UBoxComponent* Wall : ActiveWalls)
	{
		if (Wall)
		{
			Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Wall->SetHiddenInGame(true);
		}
	}

	// 2. 자체 내장 입구 차단벽 비활성화 (통과 가능)
	if (EntranceBarrier)
	{
		EntranceBarrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EntranceBarrier->SetHiddenInGame(true);
	}

	// 3. 외부 등록된 문/장애물 액터 비활성화
	for (AActor* Barrier : BarrierActors)
	{
		if (Barrier)
		{
			Barrier->SetActorHiddenInGame(true);
			Barrier->SetActorEnableCollision(false);
		}
	}
	BP_OnBarriersUnlocked();
}

void ARLArenaManager::ResetArena()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveDelayTimerHandle);
		World->GetTimerManager().ClearTimer(SpawnIntervalTimerHandle);
	}

	for (TWeakObjectPtr<ARLEnemyCharacter> EnemyPtr : ActiveEnemies)
	{
		if (EnemyPtr.IsValid())
		{
			EnemyPtr->Destroy();
		}
	}
	ActiveEnemies.Empty();
	SpawnQueue.Empty();

	CleanupArenaSpawnedActors();

	CurrentWaveIndex = -1;
	RemainingEnemies = 0;
	TotalWaveEnemies = 0;
	CurrentState = ERLArenaState::Inactive;

	UnlockBarriers();
	OnArenaStateChanged.Broadcast(CurrentState);
}

void ARLArenaManager::CleanupArenaSpawnedActors()
{
	if (SpawnedRewardChest.IsValid())
	{
		SpawnedRewardChest->Destroy();
		SpawnedRewardChest = nullptr;
	}

	if (SpawnedPortal.IsValid())
	{
		SpawnedPortal->Destroy();
		SpawnedPortal = nullptr;
	}
}

void ARLArenaManager::TestStartArena()
{
	ResetArena();
	StartArena();
}

void ARLArenaManager::TestClearCurrentWave()
{
	// 스폰 큐 비우고 현재 살아있는 모든 몬스터에게 사망 처리
	SpawnQueue.Empty();
	TArray<TWeakObjectPtr<ARLEnemyCharacter>> EnemiesToKill = ActiveEnemies;
	for (TWeakObjectPtr<ARLEnemyCharacter> EnemyPtr : EnemiesToKill)
	{
		if (EnemyPtr.IsValid())
		{
			EnemyPtr->Destroy();
			HandleEnemyDied(EnemyPtr.Get());
		}
	}
}

void ARLArenaManager::TestKillAllEnemies()
{
	TestClearCurrentWave();
}
