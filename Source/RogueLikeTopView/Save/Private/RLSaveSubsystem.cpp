#include "RLSaveSubsystem.h"
#include "RLCharacter.h"
#include "RLHealthComponent.h"
#include "RLStatComponent.h"
#include "RLInventoryComponent.h"
#include "RLSkillComponent.h"
#include "RLItemDataAsset.h"
#include "RLSkillData.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RLEnemyCharacter.h"
#include "RLArenaManager.h"
#include "RLBossTrigger.h"
#include "RLShopSubsystem.h"
#include "RLRewardChest.h"

void URLSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 게임 인스턴스 시작 시 영구 메타 데이터 로드
	LoadMetaProgression();

	// 이전에 진행 중이던 활성 런이 있는지 점검
	if (UGameplayStatics::DoesSaveGameExist(RunSlotName, UserIndex))
	{
		if (URLSaveGame* LoadedRun = Cast<URLSaveGame>(UGameplayStatics::LoadGameFromSlot(RunSlotName, UserIndex)))
		{
			CurrentRunData = LoadedRun->RunData;
		}
	}
}

URLSaveSubsystem* URLSaveSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World)
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<URLSaveSubsystem>();
		}
	}

	UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GI ? GI->GetSubsystem<URLSaveSubsystem>() : nullptr;
}

// ==========================================
// [Track 2] 런 세이브/로드/초기화 (Run In-Progress)
// ==========================================

bool URLSaveSubsystem::SaveActiveRun(ARLCharacter* PlayerCharacter, FName InCurrentMapName, int32 InStageNumber, FVector InRespawnLocation)
{
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SaveSubsystem] SaveActiveRun failed: PlayerCharacter is null"));
		return false;
	}

	FillRunData(PlayerCharacter, InCurrentMapName, InStageNumber, InRespawnLocation);
	return WriteRunToDisk();
}

void URLSaveSubsystem::FillRunData(ARLCharacter* PlayerCharacter, FName InCurrentMapName, int32 InStageNumber, FVector InRespawnLocation)
{
	CurrentRunData.bIsRunActive = true;
	const FString CleanMap = UWorld::RemovePIEPrefix(InCurrentMapName.ToString());
	CurrentRunData.CurrentMapName = FName(*CleanMap);
	CurrentRunData.StageNumber = FMath::Max(1, InStageNumber);
	CurrentRunData.SaveTimestamp = FDateTime::Now();

	GatherPlayerData(PlayerCharacter, CurrentRunData);

	UWorld* World = PlayerCharacter ? PlayerCharacter->GetWorld() : GetWorld();
	const bool bInCombat = IsCombatActive(World);

	if (bInCombat)
	{
		// ★ [유저 요청] 몬스터가 있을 때(전투 중) 나가면 라운드 처음부터 진행하도록 초기화:
		// 1. 플레이어 위치를 ZeroVector로 리셋 -> 레벨 로드 시 기본 PlayerStart 위치에서 스폰/유지됨
		CurrentRunData.PlayerLocation = FVector::ZeroVector;
		CurrentRunData.PlayerRotation = FRotator::ZeroRotator;

		// 2. 전투 중 소모된 체력을 최대 체력으로 회복하여 라운드 시작 컨디션 제공
		CurrentRunData.Health.CurrentHealth = CurrentRunData.Health.MaxHealth;

		// 3. 전투 중이었으므로 해당 스테이지는 미클리어 상태
		CurrentRunData.bIsStageCleared = false;
		CurrentRunData.UnopenedChests.Empty();

		UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 몬스터가 존재하는 전투 중 저장되었습니다. 라운드 처음(PlayerStart)부터 시작하도록 위치와 체력이 초기화됩니다."));
	}
	else
	{
		if (!InRespawnLocation.IsZero())
		{
			CurrentRunData.PlayerLocation = InRespawnLocation;
		}

		CurrentRunData.bIsStageCleared = IsArenaCleared(World);

		// 필드에 아직 열지 않은 보상 상자 목록 저장 (이어하기 시 그대로 복원)
		CurrentRunData.UnopenedChests.Empty();
		if (World)
		{
			TArray<AActor*> FoundChests;
			UGameplayStatics::GetAllActorsOfClass(World, ARLRewardChest::StaticClass(), FoundChests);
			for (AActor* Actor : FoundChests)
			{
				if (ARLRewardChest* Chest = Cast<ARLRewardChest>(Actor))
				{
					if (IsValid(Chest) && !Chest->IsOpen())
					{
						FRLUnopenedChestSaveData ChestSave;
						ChestSave.ChestClassPath = FSoftClassPath(Chest->GetClass());
						ChestSave.Location = Chest->GetActorLocation();
						ChestSave.Rotation = Chest->GetActorRotation();
						CurrentRunData.UnopenedChests.Add(ChestSave);
					}
				}
			}
		}
	}
}

bool URLSaveSubsystem::WriteRunToDisk()
{
	URLSaveGame* SaveObj = Cast<URLSaveGame>(UGameplayStatics::CreateSaveGameObject(URLSaveGame::StaticClass()));
	if (!SaveObj)
	{
		OnRunSaveCompleted.Broadcast(false);
		return false;
	}

	SaveObj->SaveSlotName = RunSlotName;
	SaveObj->UserIndex = UserIndex;
	SaveObj->RunData = CurrentRunData;

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveObj, RunSlotName, UserIndex);
	OnRunSaveCompleted.Broadcast(bSuccess);

	if (bSuccess && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("💾 [런 세이브 완료] 스테이지 %d (%s) 진행 상태가 저장되었습니다."),
			CurrentRunData.StageNumber, *CurrentRunData.CurrentMapName.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, Msg);
	}

	return bSuccess;
}

bool URLSaveSubsystem::LoadActiveRun(bool bPrepareForSpawn)
{
	if (!UGameplayStatics::DoesSaveGameExist(RunSlotName, UserIndex))
	{
		OnRunLoaded.Broadcast(false);
		return false;
	}

	URLSaveGame* LoadedSave = Cast<URLSaveGame>(UGameplayStatics::LoadGameFromSlot(RunSlotName, UserIndex));
	if (!LoadedSave || !LoadedSave->RunData.bIsRunActive)
	{
		OnRunLoaded.Broadcast(false);
		return false;
	}

	CurrentRunData = LoadedSave->RunData;
	bPendingRunApply = bPrepareForSpawn;
	OnRunLoaded.Broadcast(true);

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("📂 [런 로드 완료] 스테이지 %d (%s) 이어하기 준비 완료"),
			CurrentRunData.StageNumber, *CurrentRunData.CurrentMapName.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
	}

	return true;
}

bool URLSaveSubsystem::ApplyRunDataToPlayer(ARLCharacter* PlayerCharacter)
{
	if (!PlayerCharacter || !CurrentRunData.bIsRunActive)
	{
		return false;
	}

	ApplyPlayerData(PlayerCharacter, CurrentRunData);
	bPendingRunApply = false;
	PlayerCharacter->Tags.AddUnique(TEXT("RunResumed"));

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("✨ [캐릭터 복원 완료] 위치(%.0f, %.0f, %.0f) / HP / 스탯 / 인벤토리 / 스킬 복원됨"),
			CurrentRunData.PlayerLocation.X, CurrentRunData.PlayerLocation.Y, CurrentRunData.PlayerLocation.Z);
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Yellow, Msg);
	}

	return true;
}

void URLSaveSubsystem::ClearActiveRun(bool bBankSoulsToMeta)
{
	// 이번 런 정산 영혼 기록 (게임오버 UI 결과창 조회용)
	LastSettledRunSouls = CurrentRunData.RunSouls;

	if (bBankSoulsToMeta && CurrentRunData.RunSouls > 0)
	{
		AddTotalSouls(CurrentRunData.RunSouls);
	}

	CurrentRunData.bIsRunActive = false;
	CurrentRunData.bIsStageCleared = false;
	CurrentRunData.RunSouls = 0;
	CurrentRunData.RunSoulsFraction = 0.0f;
	CurrentRunData.UnopenedChests.Empty();
	CurrentRunData.PendingLevelUpRewardCount = 0;
	CurrentRunData.PendingLevelUpRewards.Empty();
	bPendingRunApply = false;
	if (LastSettledRunSouls > 0)
	{
		OnRunSoulsChanged.Broadcast(0, -LastSettledRunSouls);
	}

	if (URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this))
	{
		ShopSub->ResetShop();
	}

	URLSaveGame* SaveObj = Cast<URLSaveGame>(UGameplayStatics::CreateSaveGameObject(URLSaveGame::StaticClass()));
	if (SaveObj)
	{
		SaveObj->SaveSlotName = RunSlotName;
		SaveObj->UserIndex = UserIndex;
		SaveObj->RunData = CurrentRunData;
		UGameplayStatics::SaveGameToSlot(SaveObj, RunSlotName, UserIndex);
	}
}

bool URLSaveSubsystem::ResumeActiveRun()
{
	if (!LoadActiveRun(true))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString CurrentMap = UWorld::RemovePIEPrefix(World->GetMapName());
	const FString SavedMap = UWorld::RemovePIEPrefix(CurrentRunData.CurrentMapName.ToString());

	if (SavedMap.IsEmpty() || SavedMap == TEXT("None") || CurrentMap.Equals(SavedMap, ESearchCase::IgnoreCase))
	{
		// 이미 저장된 맵과 같은 맵인 경우 현재 플레이어에게 즉시 적용 (위치 텔레포트 포함)
		if (ARLCharacter* Player = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		{
			return ApplyRunDataToPlayer(Player);
		}
		return true;
	}

	// 다른 맵인 경우 저장된 맵 열기 (새 맵 로드 후 ARLCharacter::BeginPlay에서 bPendingRunApply로 자동 복원)
	UGameplayStatics::OpenLevel(this, FName(*SavedMap));
	return true;
}

bool URLSaveSubsystem::HasActiveRun() const
{
	return CurrentRunData.bIsRunActive;
}

bool URLSaveSubsystem::HasActiveRunForDifferentCharacter(TSubclassOf<ARLCharacter> NewCharacterClass) const
{
	if (!HasActiveRun() || !NewCharacterClass)
	{
		return false;
	}

	const FString SavedClassPath = CurrentRunData.CharacterClass.ToString();
	if (SavedClassPath.IsEmpty())
	{
		// 캐릭터 정보가 없는 옛 저장 파일은 "다른 캐릭터"로 간주하지 않고 그냥 통과시킴
		return false;
	}

	return SavedClassPath != NewCharacterClass->GetPathName();
}

bool URLSaveSubsystem::AutoSaveOnStageTransition(ARLCharacter* PlayerCharacter, FName NewLevelName, bool bSpawnAtPlayerStart)
{
	if (!PlayerCharacter)
	{
		return false;
	}

	// 스테이지 번호 1 증가
	// 첫 맵에서는 아직 런 세이브가 없어 bIsRunActive가 false지만, 그 맵이 1스테이지이므로 다음은 2스테이지
	const int32 CurrentStageNumber = CurrentRunData.bIsRunActive ? CurrentRunData.StageNumber : 1;
	const int32 NextStageNumber = CurrentStageNumber + 1;
	FillRunData(PlayerCharacter, NewLevelName, NextStageNumber, PlayerCharacter->GetActorLocation());

	// ★ 새 스테이지로 진입했으므로 아레나 클리어 플래그 및 이전 스테이지 미개봉 상자 초기화
	CurrentRunData.bIsStageCleared = false;
	CurrentRunData.UnopenedChests.Empty();

	// ★ 다음 스테이지 진입 시 상점 진열 상품 및 리롤 비용을 새로 갱신 (이전 스테이지에서 산 품절 상태 초기화)
	if (URLShopSubsystem* ShopSub = URLShopSubsystem::Get(PlayerCharacter))
	{
		ShopSub->ResetShop();
	}

	// 맵이 바뀌면 이전 맵 좌표는 의미가 없으므로 비워둔다 (ApplyPlayerData는 영벡터면 위치 복원을 건너뜀 → PlayerStart 유지)
	if (bSpawnAtPlayerStart)
	{
		CurrentRunData.PlayerLocation = FVector::ZeroVector;
		CurrentRunData.PlayerRotation = FRotator::ZeroRotator;
	}

	const bool bSaved = WriteRunToDisk();

	if (bSaved)
	{
		// 이전 스테이지 클리어 기록 메타에 반영
		RecordStageClear(NextStageNumber - 1);
	}

	return bSaved;
}

bool URLSaveSubsystem::IsCombatActive(const UObject* WorldContextObject) const
{
	const UWorld* World = WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : GetWorld();
	if (!World)
	{
		return false;
	}

	// 1. 월드에 살아있는 적(ARLEnemyCharacter)이 존재하는지 확인
	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(World, ARLEnemyCharacter::StaticClass(), FoundEnemies);
	for (AActor* Actor : FoundEnemies)
	{
		if (ARLEnemyCharacter* Enemy = Cast<ARLEnemyCharacter>(Actor))
		{
			if (URLHealthComponent* Health = Enemy->FindComponentByClass<URLHealthComponent>())
			{
				if (!Health->bIsDead && Health->CurrentHealth > 0.0f)
				{
					return true;
				}
			}
			else if (IsValid(Enemy))
			{
				return true;
			}
		}
	}

	// 2. 아레나 매니저가 전투 진행 중인지 확인 (웨이브 진행 중 또는 웨이브 사이 대기 중)
	TArray<AActor*> FoundArenas;
	UGameplayStatics::GetAllActorsOfClass(World, ARLArenaManager::StaticClass(), FoundArenas);
	for (AActor* Actor : FoundArenas)
	{
		if (ARLArenaManager* Arena = Cast<ARLArenaManager>(Actor))
		{
			const ERLArenaState State = Arena->GetArenaState();
			if (State == ERLArenaState::InProgress || State == ERLArenaState::BetweenWaves)
			{
				return true;
			}
			if (Arena->GetRemainingEnemies() > 0)
			{
				return true;
			}
		}
	}

	return false;
}

bool URLSaveSubsystem::IsArenaCleared(const UObject* WorldContextObject) const
{
	const UWorld* World = WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<AActor*> FoundArenas;
	UGameplayStatics::GetAllActorsOfClass(World, ARLArenaManager::StaticClass(), FoundArenas);
	if (FoundArenas.Num() > 0)
	{
		for (AActor* Actor : FoundArenas)
		{
			if (ARLArenaManager* Arena = Cast<ARLArenaManager>(Actor))
			{
				if (Arena->GetArenaState() != ERLArenaState::Cleared)
				{
					return false;
				}
			}
		}
		return true;
	}

	// 보스 맵 (ARLArenaManager가 없고 ARLBossTrigger가 있는 경우)
	TArray<AActor*> FoundBossTriggers;
	UGameplayStatics::GetAllActorsOfClass(World, ARLBossTrigger::StaticClass(), FoundBossTriggers);
	if (FoundBossTriggers.Num() > 0)
	{
		for (AActor* Actor : FoundBossTriggers)
		{
			if (Actor && Actor->ActorHasTag(TEXT("BossDefeated")))
			{
				return true;
			}
		}
		return false;
	}

	return false;
}

void URLSaveSubsystem::HandlePlayerDeath()
{
	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("💀 [영구 사망] 런이 종료되었습니다. (정산된 영혼: +%d)"), CurrentRunData.RunSouls);
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, Msg);
	}

	// 런 세이브 삭제 및 이번 런에서 모은 영혼 메타 계정으로 입금
	ClearActiveRun(true);
}

void URLSaveSubsystem::RestartRunAfterDeath(FName RestartLevelName)
{
	ClearActiveRun(true);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
	UGameplayStatics::SetGamePaused(this, false);

	FName TargetMap = RestartLevelName;
	if (TargetMap == NAME_None && GetWorld())
	{
		TargetMap = FName(*UWorld::RemovePIEPrefix(GetWorld()->GetMapName()));
	}

	if (TargetMap != NAME_None)
	{
		UGameplayStatics::OpenLevel(this, TargetMap);
	}
}

void URLSaveSubsystem::QuitToMainMenuAfterDeath(FName MainMenuLevelName)
{
	ClearActiveRun(true);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
	}
	UGameplayStatics::SetGamePaused(this, false);

	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}

// ==========================================
// [Track 1] 영구 메타 세이브 (Permanent Meta Progression)
// ==========================================

bool URLSaveSubsystem::SaveMetaProgression()
{
	URLSaveGame* SaveObj = Cast<URLSaveGame>(UGameplayStatics::CreateSaveGameObject(URLSaveGame::StaticClass()));
	if (!SaveObj)
	{
		OnMetaSaved.Broadcast(false);
		return false;
	}

	SaveObj->SaveSlotName = MetaSlotName;
	SaveObj->UserIndex = UserIndex;
	SaveObj->MetaData = CurrentMetaData;

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveObj, MetaSlotName, UserIndex);
	OnMetaSaved.Broadcast(bSuccess);
	return bSuccess;
}

bool URLSaveSubsystem::LoadMetaProgression()
{
	if (!UGameplayStatics::DoesSaveGameExist(MetaSlotName, UserIndex))
	{
		// 메타 파일이 없으면 새 메타 데이터 생성 후 저장
		CurrentMetaData = FRLMetaSaveData();
		SaveMetaProgression();
		return true;
	}

	URLSaveGame* LoadedSave = Cast<URLSaveGame>(UGameplayStatics::LoadGameFromSlot(MetaSlotName, UserIndex));
	if (!LoadedSave)
	{
		return false;
	}

	CurrentMetaData = LoadedSave->MetaData;
	return true;
}

void URLSaveSubsystem::AddTotalSouls(int32 Amount)
{
	if (Amount <= 0) return;

	CurrentMetaData.TotalSouls += Amount;
	SaveMetaProgression();
	OnSoulsChanged.Broadcast(CurrentMetaData.TotalSouls, Amount);

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("🔮 [영혼 획득] +%d (보유: %d)"), Amount, CurrentMetaData.TotalSouls);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Purple, Msg);
	}
}

bool URLSaveSubsystem::SpendTotalSouls(int32 Amount)
{
	if (Amount <= 0 || CurrentMetaData.TotalSouls < Amount)
	{
		return false;
	}

	CurrentMetaData.TotalSouls -= Amount;
	SaveMetaProgression();
	OnSoulsChanged.Broadcast(CurrentMetaData.TotalSouls, -Amount);
	return true;
}

void URLSaveSubsystem::AddRunSouls(int32 Amount)
{
	if (Amount == 0) return;
	CurrentRunData.RunSouls = FMath::Max(0, CurrentRunData.RunSouls + Amount);
	OnRunSoulsChanged.Broadcast(CurrentRunData.RunSouls, Amount);
}

int32 URLSaveSubsystem::AddRunSoulsFloat(float ExactAmount)
{
	if (ExactAmount <= 0.0f) return 0;

	const float Total = ExactAmount + CurrentRunData.RunSoulsFraction;
	const int32 WholeSouls = FMath::FloorToInt(Total);
	CurrentRunData.RunSoulsFraction = FMath::Max(0.0f, Total - static_cast<float>(WholeSouls));

	if (WholeSouls > 0)
	{
		AddRunSouls(WholeSouls);
	}
	return WholeSouls;
}

void URLSaveSubsystem::ResetRunSouls()
{
	const int32 PrevSouls = CurrentRunData.RunSouls;
	CurrentRunData.RunSouls = 0;
	CurrentRunData.RunSoulsFraction = 0.0f;

	if (PrevSouls != 0)
	{
		OnRunSoulsChanged.Broadcast(0, -PrevSouls);
	}
}

void URLSaveSubsystem::RecordStageClear(int32 StageNumber)
{
	if (StageNumber > CurrentMetaData.HighestStageCleared)
	{
		CurrentMetaData.HighestStageCleared = StageNumber;
		SaveMetaProgression();
	}
}

void URLSaveSubsystem::RecordRunAttempt()
{
	CurrentMetaData.TotalRunsAttempted++;
	SaveMetaProgression();
}

void URLSaveSubsystem::RecordVictory(int32 FinalStageNumber)
{
	CurrentMetaData.TotalVictories++;
	if (FinalStageNumber > CurrentMetaData.HighestStageCleared)
	{
		CurrentMetaData.HighestStageCleared = FinalStageNumber;
	}
	SaveMetaProgression();
}

void URLSaveSubsystem::UnlockID(FName InID)
{
	if (InID.IsNone()) return;

	if (!CurrentMetaData.UnlockedIDs.Contains(InID))
	{
		CurrentMetaData.UnlockedIDs.Add(InID);
		SaveMetaProgression();
	}
}

bool URLSaveSubsystem::IsIDUnlocked(FName InID) const
{
	return CurrentMetaData.UnlockedIDs.Contains(InID);
}

int32 URLSaveSubsystem::GetUpgradeLevel(FName UpgradeKey) const
{
	if (const int32* Found = CurrentMetaData.PermanentUpgrades.Find(UpgradeKey))
	{
		return *Found;
	}
	return 0;
}

void URLSaveSubsystem::SetUpgradeLevel(FName UpgradeKey, int32 Level)
{
	if (UpgradeKey.IsNone()) return;

	CurrentMetaData.PermanentUpgrades.Add(UpgradeKey, FMath::Max(0, Level));
	SaveMetaProgression();
}

namespace
{
	struct FRLMetaUpgradeDefinition
	{
		FName Key;
		FText DisplayName;
		FText DescriptionFormat;
		int32 MaxLevel;
		int32 BaseCost;
		int32 CostIncrement;
		float ValuePerLevel;
	};

	static const TArray<FRLMetaUpgradeDefinition>& GetMetaDefinitions()
	{
		static const TArray<FRLMetaUpgradeDefinition> Definitions = {
			{
				FName("MaxHealth_Up"),
				NSLOCTEXT("RLMeta", "HealthName", "거인의 체질"),
				NSLOCTEXT("RLMeta", "HealthDesc", "던전 시작 시 최대 체력이 단계당 +20 증가합니다."),
				5, 30, 20, 20.0f
			},
			{
				FName("Atk_Up"),
				NSLOCTEXT("RLMeta", "AtkName", "전사의 완력"),
				NSLOCTEXT("RLMeta", "AtkDesc", "기본 공격력이 단계당 +3 증가합니다."),
				5, 40, 25, 3.0f
			},
			{
				FName("AP_Up"),
				NSLOCTEXT("RLMeta", "APName", "비전의 마력"),
				NSLOCTEXT("RLMeta", "APDesc", "기본 주문력이 단계당 +3 증가합니다."),
				5, 40, 25, 3.0f
			},
			{
				FName("MoveSpeed_Up"),
				NSLOCTEXT("RLMeta", "SpeedName", "바람의 발걸음"),
				NSLOCTEXT("RLMeta", "SpeedDesc", "기본 이동 속도가 단계당 +15 증가합니다."),
				5, 35, 20, 15.0f
			},
			{
				FName("Crit_Up"),
				NSLOCTEXT("RLMeta", "CritName", "암살자의 예리함"),
				NSLOCTEXT("RLMeta", "CritDesc", "치명타 확률이 단계당 +2% 증가합니다."),
				5, 50, 30, 2.0f
			},
			{
				FName("Defense_Up"),
				NSLOCTEXT("RLMeta", "DefName", "강철 피부"),
				NSLOCTEXT("RLMeta", "DefDesc", "기본 방어력이 단계당 +3 증가합니다."),
				5, 35, 20, 3.0f
			},
			{
				FName("StartGold_Up"),
				NSLOCTEXT("RLMeta", "GoldName", "부유한 시작"),
				NSLOCTEXT("RLMeta", "GoldDesc", "던전 시작 시 소지 골드가 단계당 +50G 증가합니다."),
				5, 45, 25, 50.0f
			},
			{
				FName("SoulGain_Up"),
				NSLOCTEXT("RLMeta", "SoulName", "영혼 수확자"),
				NSLOCTEXT("RLMeta", "SoulDesc", "적 처치 시 영혼 획득량이 단계당 +15% 증가합니다."),
				5, 60, 40, 0.15f
			}
		};
		return Definitions;
	}

	int32 CalculateUpgradeCost(int32 CurrentLevel, int32 BaseCost, int32 Increment)
	{
		return BaseCost + (CurrentLevel * Increment);
	}

	FRLMetaUpgradeInfo BuildMetaUpgradeInfo(const FRLMetaUpgradeDefinition& Def, int32 CurrentLevel, int32 TotalSouls)
	{
		FRLMetaUpgradeInfo Info;
		Info.UpgradeKey = Def.Key;
		Info.DisplayName = Def.DisplayName;
		Info.Description = Def.DescriptionFormat;
		Info.MaxLevel = Def.MaxLevel;
		Info.CurrentLevel = FMath::Clamp(CurrentLevel, 0, Def.MaxLevel);
		Info.bIsMaxLevel = (Info.CurrentLevel >= Def.MaxLevel);

		if (!Info.bIsMaxLevel)
		{
			Info.NextLevelCost = CalculateUpgradeCost(Info.CurrentLevel, Def.BaseCost, Def.CostIncrement);
			Info.bCanAfford = (TotalSouls >= Info.NextLevelCost);
		}
		else
		{
			Info.NextLevelCost = 0;
			Info.bCanAfford = false;
		}

		Info.CurrentBonusValue = Info.CurrentLevel * Def.ValuePerLevel;
		Info.NextBonusValue = (Info.CurrentLevel + 1) * Def.ValuePerLevel;
		return Info;
	}
}

TArray<FRLMetaUpgradeInfo> URLSaveSubsystem::GetAllMetaUpgradeInfos() const
{
	TArray<FRLMetaUpgradeInfo> Results;
	const TArray<FRLMetaUpgradeDefinition>& Defs = GetMetaDefinitions();

	for (const FRLMetaUpgradeDefinition& Def : Defs)
	{
		Results.Add(BuildMetaUpgradeInfo(Def, GetUpgradeLevel(Def.Key), CurrentMetaData.TotalSouls));
	}

	return Results;
}

bool URLSaveSubsystem::GetMetaUpgradeInfo(FName UpgradeKey, FRLMetaUpgradeInfo& OutInfo) const
{
	const TArray<FRLMetaUpgradeDefinition>& Defs = GetMetaDefinitions();
	for (const FRLMetaUpgradeDefinition& Def : Defs)
	{
		if (Def.Key == UpgradeKey)
		{
			OutInfo = BuildMetaUpgradeInfo(Def, GetUpgradeLevel(Def.Key), CurrentMetaData.TotalSouls);
			return true;
		}
	}
	return false;
}

bool URLSaveSubsystem::UpgradeMetaTrait(FName UpgradeKey, FString& OutFailReason)
{
	if (IsMetaUpgradeLocked())
	{
		OutFailReason = TEXT("게임 진행 중에는 영혼 강화를 할 수 없습니다.");
		return false;
	}

	FRLMetaUpgradeInfo Info;
	if (!GetMetaUpgradeInfo(UpgradeKey, Info))
	{
		OutFailReason = TEXT("존재하지 않는 특성 키입니다.");
		return false;
	}

	if (Info.bIsMaxLevel)
	{
		OutFailReason = TEXT("이미 최대 레벨에 도달한 특성입니다.");
		return false;
	}

	if (CurrentMetaData.TotalSouls < Info.NextLevelCost)
	{
		OutFailReason = FString::Printf(TEXT("영혼이 부족합니다! (필요: %d, 보유: %d)"), Info.NextLevelCost, CurrentMetaData.TotalSouls);
		return false;
	}

	if (!SpendTotalSouls(Info.NextLevelCost))
	{
		OutFailReason = TEXT("영혼 차감에 실패했습니다.");
		return false;
	}

	const int32 NewLevel = Info.CurrentLevel + 1;
	SetUpgradeLevel(UpgradeKey, NewLevel);

	OnMetaUpgradePurchased.Broadcast(UpgradeKey, NewLevel);

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("🔮 [영구 특성 강화] %s %d단계 달성! (-%d Souls)"),
			*Info.DisplayName.ToString(), NewLevel, Info.NextLevelCost);
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor(180, 100, 255), Msg);
	}

	OutFailReason.Empty();
	return true;
}

void URLSaveSubsystem::ResetAllMetaUpgrades(bool bRefundSouls)
{
	if (bRefundSouls)
	{
		int32 RefundTotal = 0;
		const TArray<FRLMetaUpgradeDefinition>& Defs = GetMetaDefinitions();
		for (const FRLMetaUpgradeDefinition& Def : Defs)
		{
			const int32 Lvl = GetUpgradeLevel(Def.Key);
			for (int32 i = 0; i < Lvl; ++i)
			{
				RefundTotal += CalculateUpgradeCost(i, Def.BaseCost, Def.CostIncrement);
			}
		}

		if (RefundTotal > 0)
		{
			AddTotalSouls(RefundTotal);
		}
	}

	CurrentMetaData.PermanentUpgrades.Reset();
	SaveMetaProgression();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("🔄 모든 영구 특성이 초기화되었습니다."));
	}
}

bool URLSaveSubsystem::ApplyMetaUpgradesToPlayer(ARLCharacter* PlayerCharacter)
{
	if (!PlayerCharacter)
	{
		return false;
	}

	URLStatComponent* StatComp = PlayerCharacter->GetStatComponent();
	URLHealthComponent* HealthComp = PlayerCharacter->FindComponentByClass<URLHealthComponent>();
	URLInventoryComponent* InvComp = PlayerCharacter->GetInventoryComponent();

	int32 UpgradesAppliedCount = 0;

	// 1. MaxHealth_Up (최대 체력)
	const int32 HealthLvl = GetUpgradeLevel(FName("MaxHealth_Up"));
	if (HealthLvl > 0 && HealthComp)
	{
		const float BonusHealth = HealthLvl * 20.0f;
		HealthComp->IncreaseMaxHealth(BonusHealth, true);
		UpgradesAppliedCount++;
	}

	// 2. Atk_Up (공격력)
	const int32 AtkLvl = GetUpgradeLevel(FName("Atk_Up"));
	if (AtkLvl > 0 && StatComp)
	{
		StatComp->AddStatBonus(ERLTargetStat::AttackPower, AtkLvl * 3.0f);
		UpgradesAppliedCount++;
	}

	// 3. AP_Up (주문력)
	const int32 APLvl = GetUpgradeLevel(FName("AP_Up"));
	if (APLvl > 0 && StatComp)
	{
		StatComp->AddStatBonus(ERLTargetStat::AbilityPower, APLvl * 3.0f);
		UpgradesAppliedCount++;
	}

	// 4. MoveSpeed_Up (이동 속도)
	const int32 SpeedLvl = GetUpgradeLevel(FName("MoveSpeed_Up"));
	if (SpeedLvl > 0 && StatComp)
	{
		StatComp->AddStatBonus(ERLTargetStat::MoveSpeed, SpeedLvl * 15.0f);
		UpgradesAppliedCount++;
	}

	// 5. Crit_Up (치명타 확률)
	const int32 CritLvl = GetUpgradeLevel(FName("Crit_Up"));
	if (CritLvl > 0 && StatComp)
	{
		StatComp->AddStatBonus(ERLTargetStat::CritChance, CritLvl * 2.0f);
		UpgradesAppliedCount++;
	}

	// 6. Defense_Up (방어력)
	const int32 DefLvl = GetUpgradeLevel(FName("Defense_Up"));
	if (DefLvl > 0 && StatComp)
	{
		StatComp->AddStatBonus(ERLTargetStat::Defense, DefLvl * 3.0f);
		UpgradesAppliedCount++;
	}

	// 7. StartGold_Up (시작 골드)
	const int32 GoldLvl = GetUpgradeLevel(FName("StartGold_Up"));
	if (GoldLvl > 0 && InvComp)
	{
		InvComp->AddGold(GoldLvl * 50);
		UpgradesAppliedCount++;
	}

	if (StatComp)
	{
		StatComp->NotifyStatsChanged();
	}

	if (GEngine && UpgradesAppliedCount > 0)
	{
		const FString Msg = FString::Printf(TEXT("✨ [영구 메타 강화 적용] %d종의 영구 특성 스탯이 캐릭터에 반영되었습니다."), UpgradesAppliedCount);
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor(180, 100, 255), Msg);
	}

	return true;
}

// ==========================================
// 내부 데이터 수집 및 주입 헬퍼
// ==========================================

void URLSaveSubsystem::GatherPlayerData(ARLCharacter* PlayerCharacter, FRLRunSaveData& OutData)
{
	if (!PlayerCharacter) return;

	// ★ 어느 캐릭터로 저장된 런인지 기록 (블레이드로 저장한 런이 소서리스한테 복원되는 것 방지)
	OutData.CharacterClass = PlayerCharacter->GetClass();
	OutData.SavedCharacterDisplayName = PlayerCharacter->CharacterDisplayName;

	OutData.PlayerLocation = PlayerCharacter->GetActorLocation();
	OutData.PlayerRotation = PlayerCharacter->GetActorRotation();

	// ★ 체력/스탯은 "패시브 아이템을 뺀 기본값"으로 저장합니다 (버전 1).
	//   아이템 보너스는 로드 시 인벤토리가 GE로 다시 걸어주므로, 최종값을 저장하면 이중 적용됩니다.
	//   보상/상점/메타 강화는 Instant GE로 기본값에 이미 들어가 있어서 그대로 저장됩니다.
	OutData.StatSaveVersion = 1;

	// 1. 체력 수집
	if (URLHealthComponent* HealthComp = PlayerCharacter->FindComponentByClass<URLHealthComponent>())
	{
		OutData.Health.CurrentHealth = HealthComp->CurrentHealth;
		OutData.Health.MaxHealth = HealthComp->GetMaxHealthBase();
	}

	// 2. 스탯 수집
	if (URLStatComponent* StatComp = PlayerCharacter->GetStatComponent())
	{
		OutData.Stats.CurrentLevel = StatComp->CurrentLevel;
		OutData.Stats.CurrentExp = StatComp->CurrentExp;
		OutData.Stats.MaxExp = StatComp->MaxExp;
		OutData.Stats.ExpGrowthMultiplier = StatComp->ExpGrowthMultiplier;
		OutData.Stats.AttackPower = StatComp->GetStatBase(ERLTargetStat::AttackPower);
		OutData.Stats.AbilityPower = StatComp->GetStatBase(ERLTargetStat::AbilityPower);
		OutData.Stats.CritChance = StatComp->GetStatBase(ERLTargetStat::CritChance);
		OutData.Stats.CritDamageMultiplier = StatComp->GetStatBase(ERLTargetStat::CritDamageMultiplier);
		OutData.Stats.MoveSpeed = StatComp->GetStatBase(ERLTargetStat::MoveSpeed);
		OutData.Stats.AttackSpeed = StatComp->GetStatBase(ERLTargetStat::AttackSpeed);
		OutData.Stats.Defense = StatComp->GetStatBase(ERLTargetStat::Defense);
		OutData.Stats.DamageReduction = StatComp->GetStatBase(ERLTargetStat::DamageReduction);
	}

	// 3. 인벤토리 수집
	if (URLInventoryComponent* InvComp = PlayerCharacter->GetInventoryComponent())
	{
		OutData.Inventory.Gold = InvComp->GetGold();
		OutData.Inventory.Slots.Empty();

		const TArray<FRLInventorySlot>& Slots = InvComp->GetSlots();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].IsValid())
			{
				FRLInventorySlotSaveData SlotSave;
				SlotSave.SlotIndex = i;
				SlotSave.Quantity = Slots[i].Quantity;
				SlotSave.ItemID = Slots[i].ItemData->ItemID;
				SlotSave.ItemAssetPath = FSoftObjectPath(Slots[i].ItemData);
				OutData.Inventory.Slots.Add(SlotSave);
			}
		}
	}

	// 4. 스킬 수집
	if (URLSkillComponent* SkillComp = PlayerCharacter->GetSkillComponent())
	{
		OutData.Skills.EquippedSkills.Empty();
		for (const auto& Pair : SkillComp->GetEquippedSkills())
		{
			if (Pair.Value)
			{
				FRLSkillSlotSaveData SkillSlotSave;
				SkillSlotSave.Slot = Pair.Key;
				SkillSlotSave.SkillAssetPath = FSoftObjectPath(Pair.Value);
				OutData.Skills.EquippedSkills.Add(SkillSlotSave);
			}
		}
	}

	// 5. 미선택 레벨업 보상 카드 수집
	OutData.PendingLevelUpRewardCount = PlayerCharacter->GetPendingLevelUpRewardCount();
	OutData.PendingLevelUpRewards = PlayerCharacter->GetActiveLevelUpRewardOptions();
}

void URLSaveSubsystem::ApplyPlayerData(ARLCharacter* PlayerCharacter, const FRLRunSaveData& InData)
{
	if (!PlayerCharacter) return;

	// 0. 위치 및 회전 복원 (저장된 좌표가 유효한 경우)
	if (!InData.PlayerLocation.IsZero())
	{
		if (AController* Controller = PlayerCharacter->GetController())
		{
			Controller->StopMovement();
		}
		if (UCharacterMovementComponent* MoveComp = PlayerCharacter->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
		const bool bTeleported = PlayerCharacter->TeleportTo(InData.PlayerLocation, InData.PlayerRotation, false, true);
		if (!bTeleported)
		{
			PlayerCharacter->SetActorLocationAndRotation(InData.PlayerLocation, InData.PlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// ★ 복원 순서: 스탯 기본값 → 인벤토리(아이템 GE 재적용) → 체력
	//   체력을 마지막에 하는 이유: 저장된 현재 체력이 아이템 최대 체력 보너스를 포함할 수 있어서,
	//   아이템 GE가 걸리기 전에 복원하면 기본 최대 체력에 clamp되어 깎입니다.
	URLStatComponent* StatComp = PlayerCharacter->GetStatComponent();
	URLHealthComponent* HealthComp = PlayerCharacter->FindComponentByClass<URLHealthComponent>();

	// 1. 스탯 복원
	if (StatComp)
	{
		StatComp->RestoreStatsDirect(
			InData.Stats.CurrentLevel,
			InData.Stats.CurrentExp,
			InData.Stats.MaxExp,
			InData.Stats.ExpGrowthMultiplier,
			InData.Stats.AttackPower,
			InData.Stats.AbilityPower,
			InData.Stats.CritChance,
			InData.Stats.CritDamageMultiplier,
			InData.Stats.MoveSpeed,
			InData.Stats.AttackSpeed,
			InData.Stats.Defense,
			InData.Stats.DamageReduction
		);
	}

	// 2. 인벤토리 복원 (패시브 아이템 GE가 여기서 다시 걸림)
	if (URLInventoryComponent* InvComp = PlayerCharacter->GetInventoryComponent())
	{
		TArray<FRLInventorySlot> RestoredSlots;
		RestoredSlots.SetNum(FMath::Max(1, InvComp->GetMaxSlots()));

		URLShopSubsystem* ShopSub = URLShopSubsystem::Get(PlayerCharacter);

		for (const FRLInventorySlotSaveData& SlotSave : InData.Inventory.Slots)
		{
			if (!RestoredSlots.IsValidIndex(SlotSave.SlotIndex))
			{
				continue;
			}

			URLItemDataAsset* LoadedItem = nullptr;
			if (SlotSave.ItemAssetPath.IsValid())
			{
				LoadedItem = Cast<URLItemDataAsset>(SlotSave.ItemAssetPath.TryLoad());
			}

			// 런타임 생성 상점 아이템 등 세션 재시작 시 경로 역직렬화가 실패한 경우 ItemID로 풀에서 복원
			if (!LoadedItem && ShopSub && !SlotSave.ItemID.IsNone())
			{
				LoadedItem = ShopSub->FindItemByID(SlotSave.ItemID);
			}

			if (LoadedItem)
			{
				RestoredSlots[SlotSave.SlotIndex].ItemData = LoadedItem;
				RestoredSlots[SlotSave.SlotIndex].Quantity = SlotSave.Quantity;
			}
		}

		InvComp->RestoreInventoryDirect(InData.Inventory.Gold, RestoredSlots);
	}

	// 버전 0(GAS 전환 전) 저장 파일: 저장값이 아이템 보너스를 포함한 최종값이므로,
	// 방금 다시 걸린 아이템 보너스만큼 기본값을 빼서 최종값이 저장 당시와 같아지게 맞춥니다.
	const bool bLegacyFinalValues = InData.StatSaveVersion < 1;
	if (bLegacyFinalValues && StatComp)
	{
		const TArray<TPair<ERLTargetStat, float>> SavedFinals = {
			{ ERLTargetStat::AttackPower, InData.Stats.AttackPower },
			{ ERLTargetStat::AbilityPower, InData.Stats.AbilityPower },
			{ ERLTargetStat::CritChance, InData.Stats.CritChance },
			{ ERLTargetStat::CritDamageMultiplier, InData.Stats.CritDamageMultiplier },
			{ ERLTargetStat::MoveSpeed, InData.Stats.MoveSpeed },
			{ ERLTargetStat::AttackSpeed, InData.Stats.AttackSpeed },
			{ ERLTargetStat::Defense, InData.Stats.Defense },
			{ ERLTargetStat::DamageReduction, InData.Stats.DamageReduction }
		};
		for (const TPair<ERLTargetStat, float>& Pair : SavedFinals)
		{
			const float ItemBonus = StatComp->GetStatValue(Pair.Key) - StatComp->GetStatBase(Pair.Key);
			StatComp->SetStatBase(Pair.Key, Pair.Value - ItemBonus);
		}
		StatComp->NotifyStatsChanged();
	}

	// 3. 체력 복원
	if (HealthComp)
	{
		float MaxHealthBase = InData.Health.MaxHealth;
		if (bLegacyFinalValues)
		{
			MaxHealthBase -= HealthComp->MaxHealth - HealthComp->GetMaxHealthBase();
		}
		HealthComp->RestoreHealthDirect(InData.Health.CurrentHealth, MaxHealthBase);
	}

	// 4. 스킬 복원
	if (URLSkillComponent* SkillComp = PlayerCharacter->GetSkillComponent())
	{
		for (const FRLSkillSlotSaveData& SkillSlotSave : InData.Skills.EquippedSkills)
		{
			if (SkillSlotSave.SkillAssetPath.IsValid())
			{
				URLSkillData* LoadedSkill = Cast<URLSkillData>(SkillSlotSave.SkillAssetPath.TryLoad());
				if (LoadedSkill)
				{
					SkillComp->EquipSkill(SkillSlotSave.Slot, LoadedSkill);
				}
			}
		}
	}

	// 5. 미선택 레벨업 보상 카드 복원
	if (InData.PendingLevelUpRewardCount > 0)
	{
		PlayerCharacter->RestorePendingLevelUpRewards(InData.PendingLevelUpRewardCount, InData.PendingLevelUpRewards);
	}

	// 6. 미개봉 보상 상자 복원 (이미 같은 위치에 미개봉 상자가 있으면 중복 스폰 방지)
	if (InData.UnopenedChests.Num() > 0)
	{
		if (UWorld* World = PlayerCharacter->GetWorld())
		{
			TArray<AActor*> ExistingChests;
			UGameplayStatics::GetAllActorsOfClass(World, ARLRewardChest::StaticClass(), ExistingChests);

			for (const FRLUnopenedChestSaveData& SavedChest : InData.UnopenedChests)
			{
				bool bAlreadyExists = false;
				for (AActor* ExistingActor : ExistingChests)
				{
					if (ARLRewardChest* ExistingChest = Cast<ARLRewardChest>(ExistingActor))
					{
						if (IsValid(ExistingChest) && !ExistingChest->IsOpen()
							&& FVector::DistSquared(ExistingChest->GetActorLocation(), SavedChest.Location) < FMath::Square(50.0f))
						{
							bAlreadyExists = true;
							break;
						}
					}
				}

				if (bAlreadyExists)
				{
					continue;
				}

				TSubclassOf<ARLRewardChest> TargetChestClass = SavedChest.ChestClassPath.TryLoadClass<ARLRewardChest>();
				if (!TargetChestClass)
				{
					TargetChestClass = LoadClass<ARLRewardChest>(nullptr, TEXT("/Game/Blueprint/MyRLRewardChest.MyRLRewardChest_C"));
				}
				if (!TargetChestClass)
				{
					TargetChestClass = ARLRewardChest::StaticClass();
				}

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				World->SpawnActor<ARLRewardChest>(TargetChestClass, SavedChest.Location, SavedChest.Rotation, SpawnParams);
			}
		}
	}
}
