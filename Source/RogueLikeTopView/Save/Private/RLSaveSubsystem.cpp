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

		UE_LOG(LogTemp, Log, TEXT("[SaveSubsystem] 몬스터가 존재하는 전투 중 저장되었습니다. 라운드 처음(PlayerStart)부터 시작하도록 위치와 체력이 초기화됩니다."));
	}
	else
	{
		if (!InRespawnLocation.IsZero())
		{
			CurrentRunData.PlayerLocation = InRespawnLocation;
		}

		CurrentRunData.bIsStageCleared = IsArenaCleared(World);
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
	bPendingRunApply = false;

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
	const int32 NextStageNumber = CurrentRunData.bIsRunActive ? (CurrentRunData.StageNumber + 1) : 1;
	FillRunData(PlayerCharacter, NewLevelName, NextStageNumber, PlayerCharacter->GetActorLocation());

	// ★ 새 스테이지로 진입했으므로 아레나 클리어 플래그는 무조건 false로 초기화
	CurrentRunData.bIsStageCleared = false;

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
				if (Arena->GetArenaState() == ERLArenaState::Cleared)
				{
					return true;
				}
			}
		}
		return false;
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
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
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
	if (Amount <= 0) return;
	CurrentRunData.RunSouls += Amount;
}

void URLSaveSubsystem::RecordStageClear(int32 StageNumber)
{
	if (StageNumber > CurrentMetaData.HighestStageCleared)
	{
		CurrentMetaData.HighestStageCleared = StageNumber;
		SaveMetaProgression();
	}
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

	// 1. 체력 수집
	if (URLHealthComponent* HealthComp = PlayerCharacter->FindComponentByClass<URLHealthComponent>())
	{
		OutData.Health.CurrentHealth = HealthComp->CurrentHealth;
		OutData.Health.MaxHealth = HealthComp->MaxHealth;
	}

	// 2. 스탯 수집
	if (URLStatComponent* StatComp = PlayerCharacter->GetStatComponent())
	{
		OutData.Stats.CurrentLevel = StatComp->CurrentLevel;
		OutData.Stats.CurrentExp = StatComp->CurrentExp;
		OutData.Stats.MaxExp = StatComp->MaxExp;
		OutData.Stats.ExpGrowthMultiplier = StatComp->ExpGrowthMultiplier;
		OutData.Stats.AttackPower = StatComp->AttackPower;
		OutData.Stats.AbilityPower = StatComp->AbilityPower;
		OutData.Stats.CritChance = StatComp->CritChance;
		OutData.Stats.CritDamageMultiplier = StatComp->CritDamageMultiplier;
		OutData.Stats.MoveSpeed = StatComp->MoveSpeed;
		OutData.Stats.AttackSpeed = StatComp->AttackSpeed;
		OutData.Stats.Defense = StatComp->Defense;
		OutData.Stats.DamageReduction = StatComp->DamageReduction;
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

	// 1. 체력 복원
	if (URLHealthComponent* HealthComp = PlayerCharacter->FindComponentByClass<URLHealthComponent>())
	{
		HealthComp->RestoreHealthDirect(InData.Health.CurrentHealth, InData.Health.MaxHealth);
	}

	// 2. 스탯 복원
	if (URLStatComponent* StatComp = PlayerCharacter->GetStatComponent())
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

	// 3. 인벤토리 복원
	if (URLInventoryComponent* InvComp = PlayerCharacter->GetInventoryComponent())
	{
		TArray<FRLInventorySlot> RestoredSlots;
		RestoredSlots.SetNum(FMath::Max(1, InvComp->GetMaxSlots()));

		for (const FRLInventorySlotSaveData& SlotSave : InData.Inventory.Slots)
		{
			if (RestoredSlots.IsValidIndex(SlotSave.SlotIndex) && SlotSave.ItemAssetPath.IsValid())
			{
				URLItemDataAsset* LoadedItem = Cast<URLItemDataAsset>(SlotSave.ItemAssetPath.TryLoad());
				if (LoadedItem)
				{
					RestoredSlots[SlotSave.SlotIndex].ItemData = LoadedItem;
					RestoredSlots[SlotSave.SlotIndex].Quantity = SlotSave.Quantity;
				}
			}
		}

		InvComp->RestoreInventoryDirect(InData.Inventory.Gold, RestoredSlots);
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
}
