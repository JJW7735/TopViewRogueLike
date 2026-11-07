#include "RLAudioSubsystem.h"
#include "RLCharacter.h"
#include "RLEnemyCharacter.h"
#include "RLHealthComponent.h"
#include "RLArenaManager.h"
#include "RLArenaMoodController.h"
#include "RLBossTrigger.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"

float URLAudioSubsystem::MasterVolume = 1.0f;
float URLAudioSubsystem::BGMVolume = 0.45f;
float URLAudioSubsystem::SFXVolume = 0.68f;

namespace
{
	/**
	 * 트랙별 원본 마스터링 음압(LUFS) 편차를 보정하기 위한 개별 볼륨 스케일.
	 * - 09번(메인메뉴)과 06/08/10번(앰비언스)은 잔잔한 반면,
	 * - 01~05번 및 07번은 오케스트라 금관악기/타악기 음압이 훨씬 높으므로 대폭 낮춰서 귀가 피로하지 않게 맞춥니다.
	 */
	float GetTrackLoudnessScale(ERLBGMTrack Track)
	{
		switch (Track)
		{
		case ERLBGMTrack::Explore_JourneyBegins:   // 09: 메인 메뉴 / 거점 (기준 볼륨)
			return 0.85f;
		case ERLBGMTrack::Explore_Atmospheres1:    // 06: 잔잔한 던전 앰비언스 I
		case ERLBGMTrack::Explore_Atmospheres2:    // 08: 잔잔한 던전 앰비언스 II
		case ERLBGMTrack::Explore_Atmospheres3:    // 10: 잔잔한 던전 앰비언스 III
			return 0.65f;
		case ERLBGMTrack::Explore_MistyLands:      // 05: 탐험 트랙 (초반 현악/호른 음압 보정)
		case ERLBGMTrack::Explore_MountainHalls:   // 07: 지하묘지 탐험 (음압 보정)
			return 0.36f;
		case ERLBGMTrack::Battle_TheAmbush:        // 02: 일반 아레나 전투 (과도한 웅장함/시끄러움 방지)
		case ERLBGMTrack::Battle_EnemyApproaches:  // 03: 중후반 아레나/중간보스 전투
			return 0.14f;
		case ERLBGMTrack::Battle_SwordsAtMidnight: // 04: 세바로그 보스전
		case ERLBGMTrack::Battle_HornsOfWar:       // 01: 카이메라 보스전 (음압 대폭 하향)
			return 0.13f;
		default:
			return 0.35f;
		}
	}
	static FAutoConsoleCommandWithWorldAndArgs GPlayBGMCmd(
		TEXT("RL.PlayBGM"),
		TEXT("1~10번 ARPGMusicPack 트랙 즉시 크로스페이드 재생 테스트 (예: RL.PlayBGM 1, 0 입력 시 자동 모드 복귀)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() == 0)
			{
				return;
			}
			URLAudioSubsystem* AudioSys = URLAudioSubsystem::Get(World);
			if (!AudioSys)
			{
				return;
			}

			const int32 TrackNum = FCString::Atoi(*Args[0]);
			if (TrackNum <= 0)
			{
				AudioSys->bAutoManageBGM = true;
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Green,
						TEXT("🎵 [RL.PlayBGM] 자동 상황 감지 BGM 모드로 복귀했습니다."));
				}
				return;
			}

			const int32 Clamped = FMath::Clamp(TrackNum, 1, 10);
			AudioSys->bAutoManageBGM = false;
			AudioSys->PlayBGMTrack(static_cast<ERLBGMTrack>(Clamped), 1.2f, 1.0f);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
					FString::Printf(TEXT("🎵 [RL.PlayBGM] 트랙 #%d 수동 크로스페이드 재생! (RL.PlayBGM 0 입력 시 자동 모드 복귀)"), Clamped));
			}
		})
	);

	static FAutoConsoleCommandWithWorldAndArgs GBGMVolCmd(
		TEXT("RL.BGMVolume"),
		TEXT("배경음악(BGM) 볼륨 실시간 설정 (0.0 ~ 2.0, 예: RL.BGMVolume 0.8)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() == 0)
			{
				return;
			}
			if (URLAudioSubsystem* AudioSys = URLAudioSubsystem::Get(World))
			{
				AudioSys->SetBGMVolume(FCString::Atof(*Args[0]));
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
						FString::Printf(TEXT("🔊 [RL.BGMVolume] BGM 볼륨: %.2f"), AudioSys->GetBGMVolume()));
				}
			}
		})
	);

	static FAutoConsoleCommandWithWorldAndArgs GSFXVolCmd(
		TEXT("RL.SFXVolume"),
		TEXT("효과음(SFX) 볼륨 실시간 설정 (0.0 ~ 2.0, 예: RL.SFXVolume 0.9)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() == 0)
			{
				return;
			}
			if (URLAudioSubsystem* AudioSys = URLAudioSubsystem::Get(World))
			{
				AudioSys->SetSFXVolume(FCString::Atof(*Args[0]));
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
						FString::Printf(TEXT("🔊 [RL.SFXVolume] SFX 볼륨: %.2f"), AudioSys->GetSFXVolume()));
				}
			}
		})
	);

	static FAutoConsoleCommandWithWorld GAudioStatusCmd(
		TEXT("RL.AudioStatus"),
		TEXT("현재 재생 중인 BGM 트랙, 음악 상태 및 볼륨 설정 화면 출력"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (!World)
			{
				return;
			}
			if (URLAudioSubsystem* AudioSys = URLAudioSubsystem::Get(World))
			{
				const UEnum* TrackEnum = StaticEnum<ERLBGMTrack>();
				const UEnum* StateEnum = StaticEnum<ERLMusicState>();
				const FString TrackStr = TrackEnum ? TrackEnum->GetDisplayNameTextByValue(static_cast<int64>(AudioSys->GetCurrentBGMTrack())).ToString() : TEXT("Unknown");
				const FString StateStr = StateEnum ? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(AudioSys->GetCurrentMusicState())).ToString() : TEXT("Unknown");

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
						FString::Printf(TEXT("🎶 [AudioStatus] 상태: %s | 현재 트랙: %s | 볼륨(Master: %.2f, BGM: %.2f, SFX: %.2f)"),
							*StateStr, *TrackStr, AudioSys->GetMasterVolume(), AudioSys->GetBGMVolume(), AudioSys->GetSFXVolume()));
				}
			}
		})
	);
}

URLAudioSubsystem* URLAudioSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<URLAudioSubsystem>() : nullptr;
}

bool URLAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId URLAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URLAudioSubsystem, STATGROUP_Tickables);
}

USoundBase* URLAudioSubsystem::LoadLoopingMusicAsset(const TCHAR* WavPath, const TCHAR* CueFallbackPath)
{
	if (WavPath)
	{
		if (USoundWave* Wave = LoadObject<USoundWave>(nullptr, WavPath))
		{
			Wave->bLooping = true;
			return Wave;
		}
	}
	if (CueFallbackPath)
	{
		if (USoundBase* Cue = LoadObject<USoundBase>(nullptr, CueFallbackPath))
		{
			return Cue;
		}
	}
	return nullptr;
}

void URLAudioSubsystem::LoadSoundWaveVariations(const FString& BasePackagePath, int32 MaxCount, TArray<TObjectPtr<USoundBase>>& OutArray)
{
	OutArray.Empty();
	const FString ShortBaseName = FPackageName::GetShortName(BasePackagePath);

	for (int32 i = 1; i <= MaxCount; ++i)
	{
		const FString AssetName = FString::Printf(TEXT("%s_%02d"), *ShortBaseName, i);
		const FString FullObjectPath = FString::Printf(TEXT("%s_%02d.%s"), *BasePackagePath, i, *AssetName);
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FullObjectPath))
		{
			OutArray.Add(Sound);
		}
	}
}

void URLAudioSubsystem::EnsureAudioAssetsLoaded()
{
	if (bAssetsLoaded)
	{
		return;
	}
	bAssetsLoaded = true;

	// 1. ARPGMusicPack 10곡 로드 (무한 루프 보장)
	BGMTracks.Add(ERLBGMTrack::Battle_HornsOfWar, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/01_Horns_Of_War_BattleTrack.01_Horns_Of_War_BattleTrack"),
		TEXT("/Game/ARPGMusicPack/cues/01_Horns_Of_War_BattleTrack_Cue.01_Horns_Of_War_BattleTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Battle_TheAmbush, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/02_The_Ambush_BattleTrack.02_The_Ambush_BattleTrack"),
		TEXT("/Game/ARPGMusicPack/cues/02_The_Ambush_BattleTrack_Cue.02_The_Ambush_BattleTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Battle_EnemyApproaches, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/03_Enemy_Approaches_BattleTrack.03_Enemy_Approaches_BattleTrack"),
		TEXT("/Game/ARPGMusicPack/cues/03_Enemy_Approaches_BattleTrack_Cue.03_Enemy_Approaches_BattleTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Battle_SwordsAtMidnight, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/04_Swords_At_Midnight_BattleTrack.04_Swords_At_Midnight_BattleTrack"),
		TEXT("/Game/ARPGMusicPack/cues/04_Swords_At_Midnight_BattleTrack_Cue.04_Swords_At_Midnight_BattleTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Explore_MistyLands, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/05_Misty_Lands_ExplorationTrack.05_Misty_Lands_ExplorationTrack"),
		TEXT("/Game/ARPGMusicPack/cues/05_Misty_Lands_ExplorationTrack_Cue.05_Misty_Lands_ExplorationTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Explore_Atmospheres1, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/06_Through_The_Lands_-_Atmospheres_Part_I__ExplorationTrack.06_Through_The_Lands_-_Atmospheres_Part_I__ExplorationTrack"),
		TEXT("/Game/ARPGMusicPack/cues/06_Through_The_Lands_-_Atmospheres_Part_I__ExplorationTrack_Cue.06_Through_The_Lands_-_Atmospheres_Part_I__ExplorationTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Explore_MountainHalls, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/07_Mountain_Halls__ExplorationTrack.07_Mountain_Halls__ExplorationTrack"),
		TEXT("/Game/ARPGMusicPack/cues/07_Mountain_Halls__ExplorationTrack_Cue.07_Mountain_Halls__ExplorationTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Explore_Atmospheres2, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/08_Through_The_Lands_-_Atmospheres_Part_II_ExplorationTrack.08_Through_The_Lands_-_Atmospheres_Part_II_ExplorationTrack"),
		TEXT("/Game/ARPGMusicPack/cues/08_Through_The_Lands_-_Atmospheres_Part_II_ExplorationTrack_Cue.08_Through_The_Lands_-_Atmospheres_Part_II_ExplorationTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Explore_JourneyBegins, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/09_The_Journey_Begins__ExplorationTrack.09_The_Journey_Begins__ExplorationTrack"),
		TEXT("/Game/ARPGMusicPack/cues/09_The_Journey_Begins__ExplorationTrack_Cue.09_The_Journey_Begins__ExplorationTrack_Cue")));

	BGMTracks.Add(ERLBGMTrack::Explore_Atmospheres3, LoadLoopingMusicAsset(
		TEXT("/Game/ARPGMusicPack/wav/10_Through_The_Lands_-_Atmospheres_Part_III_ExplorationTrack.10_Through_The_Lands_-_Atmospheres_Part_III_ExplorationTrack"),
		TEXT("/Game/ARPGMusicPack/cues/10_Through_The_Lands_-_Atmospheres_Part_III_ExplorationTrack_Cue.10_Through_The_Lands_-_Atmospheres_Part_III_ExplorationTrack_Cue")));

	// 2. 카이메라 보스(BP_Boss_2) 비언어적 기합/포효/피격/사망 SoundWave 풀 로드 (MOBA 대사 제외, 순수 기합/괴성만 선별)
	const FString KhaimeraBase = TEXT("/Game/ParagonKhaimera/Characters/Heroes/Khaimera/Sounds/SoundWaves/Khaimera_Effort_");
	LoadSoundWaveVariations(KhaimeraBase + TEXT("Attack"), 5, KhaimeraAttackSounds);
	LoadSoundWaveVariations(KhaimeraBase + TEXT("Swing"), 4, KhaimeraSwingSounds);
	LoadSoundWaveVariations(KhaimeraBase + TEXT("JumpHeavy"), 4, KhaimeraJumpSounds);
	LoadSoundWaveVariations(KhaimeraBase + TEXT("LandHeavy"), 4, KhaimeraLandSounds);
	LoadSoundWaveVariations(KhaimeraBase + TEXT("PainHeavy"), 5, KhaimeraPainSounds);
	LoadSoundWaveVariations(KhaimeraBase + TEXT("Ability_Ultimate"), 5, KhaimeraRoarSounds);
	LoadSoundWaveVariations(KhaimeraBase + TEXT("Death"), 4, KhaimeraDeathSounds);

	// 3. 세바로그 보스(BP_Boss) & 디스트로이어 기합/포효/피격/사망 SoundWave 풀 로드
	const FString SevarogBase = TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Sounds/SoundWaves/Sevarog_Effort_");
	LoadSoundWaveVariations(SevarogBase + TEXT("Attack"), 3, SevarogAttackSounds);
	LoadSoundWaveVariations(SevarogBase + TEXT("Swing"), 3, SevarogSwingSounds);
	LoadSoundWaveVariations(SevarogBase + TEXT("JumpHeavy"), 3, SevarogJumpSounds);
	LoadSoundWaveVariations(SevarogBase + TEXT("LandHeavy"), 3, SevarogLandSounds);
	LoadSoundWaveVariations(SevarogBase + TEXT("PainHeavy"), 3, SevarogPainSounds);
	LoadSoundWaveVariations(SevarogBase + TEXT("Ability_Ultimate"), 3, SevarogRoarSounds);
	LoadSoundWaveVariations(SevarogBase + TEXT("Death"), 3, SevarogDeathSounds);
}

void URLAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	EnsureAudioAssetsLoaded();

	LastRealTickTime = FPlatformTime::Seconds();
	LastStateCheckRealTime = 0.0;
	bHadActiveBossInWorld = false;
	CurrentMusicState = ERLMusicState::None;
	CurrentTrack = ERLBGMTrack::None;

	EvaluateWorldMusicState();
}

void URLAudioSubsystem::Deinitialize()
{
	if (ActiveBGMComp)
	{
		ActiveBGMComp->Stop();
		ActiveBGMComp->DestroyComponent();
		ActiveBGMComp = nullptr;
	}

	if (FadingOutBGMComp)
	{
		FadingOutBGMComp->Stop();
		FadingOutBGMComp->DestroyComponent();
		FadingOutBGMComp = nullptr;
	}

	CharacterTrackers.Empty();

	Super::Deinitialize();
}

void URLAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const double NowReal = FPlatformTime::Seconds();
	const float RealDelta = FMath::Clamp(static_cast<float>(NowReal - LastRealTickTime), 0.0f, 0.1f);
	LastRealTickTime = NowReal;

	// 1. 0.2초 간격으로 월드 내 아레나/보스/맵 스트리밍 상태를 점검하여 BGM 자동 전환
	if (bAutoManageBGM && (NowReal - LastStateCheckRealTime >= 0.20))
	{
		LastStateCheckRealTime = NowReal;
		EvaluateWorldMusicState();
	}

	// 2. 실시간 크로스페이드 볼륨 보간, 일시정지 더킹(Ducking), 무한 루프 감시
	UpdateCrossfadeAndDucking(RealDelta);

	// 3. 보스(카이메라/세바로그) 및 디스트로이어의 공격/점프/분노/균열/사망 기합 사운드 자동 재생
	if (!UGameplayStatics::IsGamePaused(GetWorld()))
	{
		UpdateCharacterCombatVoices();
	}
}

FString URLAudioSubsystem::ResolveActiveMapContextName() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return TEXT("");
	}

	// 레벨 스트리밍으로 로드 및 표시된 서브레벨이 있다면 서브레벨 이름을 우선 반영
	for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel && StreamingLevel->IsLevelVisible())
		{
			const FString AssetName = StreamingLevel->GetWorldAssetPackageFName().ToString();
			if (!AssetName.IsEmpty())
			{
				return FPackageName::GetShortName(AssetName);
			}
		}
	}

	return UWorld::RemovePIEPrefix(World->GetMapName());
}

ERLBGMTrack URLAudioSubsystem::SelectExplorationTrackForMap(const FString& MapName, bool bAnyArenaCleared) const
{
	const FString LowerMap = MapName.ToLower();

	// 1. 메인 메뉴 / 타이틀 / 소울 제단 거점
	if (LowerMap.Contains(TEXT("mainmenu")) || LowerMap.Contains(TEXT("title")) || LowerMap.Contains(TEXT("soul")))
	{
		return ERLBGMTrack::Explore_JourneyBegins;
	}

	// 2. 보스 스테이지 진입 전 고요한 긴장감 구간 (VesselsHeart)
	if (LowerMap.Contains(TEXT("vessel")) || LowerMap.Contains(TEXT("heart")))
	{
		return ERLBGMTrack::Explore_Atmospheres3;
	}

	// 3. 전쟁 납골당 (WarCrypt)
	if (LowerMap.Contains(TEXT("warcrypt")) || LowerMap.Contains(TEXT("crypt")))
	{
		return ERLBGMTrack::Explore_Atmospheres2;
	}

	// 4. 지하묘지 (HauntedCatacombs)
	if (LowerMap.Contains(TEXT("haunted")) || LowerMap.Contains(TEXT("catacomb")))
	{
		return ERLBGMTrack::Explore_MountainHalls;
	}

	// 5. 외곽 성채 (OuterWard)
	if (LowerMap.Contains(TEXT("outerward")) || LowerMap.Contains(TEXT("ward")))
	{
		return ERLBGMTrack::Explore_Atmospheres1;
	}

	// 6. 기본 1스테이지 (RuinedTemple / Map_Persistent 등)
	// 맵 진입 직후 귀가 피로하거나 과하게 웅장하지 않도록 은은한 앰비언스(06번)를 기본으로 재생하고,
	// 아레나 클리어 후에는 08번(Atmospheres Part II)으로 자연스럽게 이어지게 구성
	return bAnyArenaCleared ? ERLBGMTrack::Explore_Atmospheres2 : ERLBGMTrack::Explore_Atmospheres1;
}

ERLBGMTrack URLAudioSubsystem::SelectBossTrack(const ARLEnemyCharacter* BossActor) const
{
	if (BossActor)
	{
		const FString BossName = (BossActor->GetClass()->GetName() + TEXT(" ") + BossActor->GetName()).ToLower();
		// 카이메라 (보스 2): 01_Horns_Of_War
		if (BossName.Contains(TEXT("khaimera")) || BossName.Contains(TEXT("boss_2")))
		{
			return ERLBGMTrack::Battle_HornsOfWar;
		}
		// 세바로그 (보스 1): 04_Swords_At_Midnight
		if (BossName.Contains(TEXT("sevarog")))
		{
			return ERLBGMTrack::Battle_SwordsAtMidnight;
		}
	}

	const FString MapContext = ResolveActiveMapContextName().ToLower();
	if (MapContext.Contains(TEXT("vessel")) || MapContext.Contains(TEXT("warcrypt")))
	{
		return ERLBGMTrack::Battle_HornsOfWar;
	}
	return ERLBGMTrack::Battle_SwordsAtMidnight;
}

void URLAudioSubsystem::EvaluateWorldMusicState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString MapName = ResolveActiveMapContextName();
	const FString LowerMap = MapName.ToLower();

	// 0. 타이틀(메인 메뉴)일 때만 잔잔한 메인 테마 재생
	if (LowerMap.Contains(TEXT("mainmenu")) || LowerMap.Contains(TEXT("title")))
	{
		CurrentMusicState = ERLMusicState::TitleOrHub;
		BossEnrageVolumeBoost = 1.0f;
		PlayBGMTrack(ERLBGMTrack::Explore_JourneyBegins, 1.8f, 1.0f);
		return;
	}

	// 1. 플레이어 사망 시 BGM 부드럽게 페이드 아웃 (정적 연출)
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
	{
		if (URLHealthComponent* PlayerHealth = PlayerChar->FindComponentByClass<URLHealthComponent>())
		{
			if (PlayerHealth->bIsDead)
			{
				CurrentMusicState = ERLMusicState::GameOver;
				BossEnrageVolumeBoost = 1.0f;
				StopBGM(2.5f);
				return;
			}
		}
	}

	// 2. 메인 보스전(세바로그 / 카이메라) 진행 여부 확인 (MiniBoss는 제외)
	ARLEnemyCharacter* AliveBoss = nullptr;
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);

	for (TActorIterator<ARLEnemyCharacter> It(World); It; ++It)
	{
		ARLEnemyCharacter* Enemy = *It;
		URLHealthComponent* EnemyHealth = Enemy ? Enemy->FindComponentByClass<URLHealthComponent>() : nullptr;
		if (!Enemy || !EnemyHealth || EnemyHealth->bIsDead)
		{
			continue;
		}

		const FString EnemyId = (Enemy->GetClass()->GetName() + TEXT(" ") + Enemy->GetName()).ToLower();
		const bool bIsMiniBoss = EnemyId.Contains(TEXT("miniboss")) || (Enemy->bIsElite && !Enemy->bIsBoss);
		if (bIsMiniBoss)
		{
			continue;
		}

		if (Enemy->IsBossEnemy())
		{
			AliveBoss = Enemy;
			break;
		}
	}

	bool bBossMoodActive = false;
	for (TActorIterator<ARLArenaMoodController> It(World); It; ++It)
	{
		if (It->IsBossPhase())
		{
			bBossMoodActive = true;
			break;
		}
	}

	if (AliveBoss)
	{
		URLHealthComponent* BossHealth = AliveBoss->FindComponentByClass<URLHealthComponent>();
		const bool bBossDamaged = BossHealth && (BossHealth->CurrentHealth < BossHealth->MaxHealth - 1.0f);
		bool bBossEngaged = bBossMoodActive || bBossDamaged || (AliveBoss->GetCurrentMontage() != nullptr);
		if (!bBossEngaged && PlayerPawn)
		{
			bBossEngaged = FVector::DistSquared2D(PlayerPawn->GetActorLocation(), AliveBoss->GetActorLocation()) <= FMath::Square(1500.0f);
		}

		if (bBossEngaged)
		{
			bHadActiveBossInWorld = true;
			CurrentMusicState = ERLMusicState::BossCombat;

			const bool bEnraged = AliveBoss->bIsEnraged;
			BossEnrageVolumeBoost = bEnraged ? 1.10f : 1.0f;
			const float TargetPitch = bEnraged ? 1.03f : 1.0f;

			// 보스 출현 시 갑작스럽게 시끄럽지 않도록 4초에 걸쳐 아주 서서히 페이드 인
			PlayBGMTrack(SelectBossTrack(AliveBoss), 4.0f, TargetPitch);
			return;
		}
	}

	// 3. 그 외 일반 스테이지 탐험, 일반 아레나 전투, 보스 처치 완료 후에는 다크 소울 스타일로 BGM 무음(Silence) 유지
	CurrentMusicState = bHadActiveBossInWorld ? ERLMusicState::BossVictoryCalm : ERLMusicState::Exploration;
	BossEnrageVolumeBoost = 1.0f;
	StopBGM(2.0f);
}

void URLAudioSubsystem::PlayBGMTrack(ERLBGMTrack Track, float CrossfadeDuration, float TargetPitch)
{
	EnsureAudioAssetsLoaded();

	CurrentDesiredPitch = FMath::Clamp(TargetPitch, 0.5f, 2.0f);

	if (Track == ERLBGMTrack::None)
	{
		StopBGM(CrossfadeDuration);
		return;
	}

	// 이미 같은 트랙이 재생 중이면 피치/볼륨만 부드럽게 갱신
	if (CurrentTrack == Track && ActiveBGMComp && ActiveBGMComp->IsPlaying())
	{
		return;
	}

	USoundBase* NewSound = BGMTracks.FindRef(Track);
	if (!NewSound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 기존에 페이드 아웃 중이던 트랙이 있으면 정리하고, 현재 활성 트랙을 페이드 아웃 슬롯으로 이동
	if (FadingOutBGMComp)
	{
		FadingOutBGMComp->Stop();
		FadingOutBGMComp->DestroyComponent();
		FadingOutBGMComp = nullptr;
	}

	if (ActiveBGMComp && ActiveBGMComp->IsPlaying())
	{
		FadingOutBGMComp = ActiveBGMComp;
		FadingOutAlpha = ActiveFadeAlpha * GetTrackLoudnessScale(CurrentTrack);
		ActiveBGMComp = nullptr;
	}

	CurrentTrack = Track;
	ActiveFadeAlpha = 0.0f;
	CurrentAppliedPitch = CurrentDesiredPitch;
	CurrentCrossfadeRate = (CrossfadeDuration > 0.05f) ? (1.0f / CrossfadeDuration) : 20.0f;

	// 일시정지(보상 창, 상점, 일시정지 메뉴) 중에도 끊기지 않도록 bIsUISound = true 로 생성
	ActiveBGMComp = UGameplayStatics::CreateSound2D(
		World,
		NewSound,
		0.001f,
		CurrentAppliedPitch,
		0.0f,
		nullptr,
		false,
		false);

	if (ActiveBGMComp)
	{
		ActiveBGMComp->bIsUISound = true;
		ActiveBGMComp->bIsMusic = true;
		ActiveBGMComp->bIgnoreForFlushing = false;
		ActiveBGMComp->Play();
	}
}

void URLAudioSubsystem::StopBGM(float FadeOutDuration)
{
	if (FadingOutBGMComp)
	{
		FadingOutBGMComp->Stop();
		FadingOutBGMComp->DestroyComponent();
		FadingOutBGMComp = nullptr;
	}

	if (ActiveBGMComp)
	{
		FadingOutBGMComp = ActiveBGMComp;
		FadingOutAlpha = ActiveFadeAlpha * GetTrackLoudnessScale(CurrentTrack);
		ActiveBGMComp = nullptr;
	}

	CurrentTrack = ERLBGMTrack::None;
	ActiveFadeAlpha = 0.0f;
	CurrentCrossfadeRate = (FadeOutDuration > 0.05f) ? (1.0f / FadeOutDuration) : 20.0f;
}

void URLAudioSubsystem::UpdateCrossfadeAndDucking(float RealDeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 게임이 일시정지(보상 카드 선택, 상점, 일시정지 메뉴) 상태일 때는 BGM 볼륨을 65%로 부드럽게 낮춤(Ducking)
	const bool bIsPaused = UGameplayStatics::IsGamePaused(World);
	const float TargetDuck = bIsPaused ? 0.65f : 1.0f;
	CurrentDuckMultiplier = FMath::FInterpTo(CurrentDuckMultiplier, TargetDuck, RealDeltaTime, 6.0f);

	const float BaseVolume = FMath::Clamp(MasterVolume * BGMVolume * CurrentDuckMultiplier * BossEnrageVolumeBoost, 0.0f, 2.0f);
	const float TrackScale = GetTrackLoudnessScale(CurrentTrack);

	// 1. 활성 BGM 페이드 인 및 무한 루프 감시
	if (ActiveBGMComp)
	{
		// 혹시라도 트랙이 끝났다면 즉시 재시작하여 무한 루프 보장
		if (!ActiveBGMComp->IsPlaying() && CurrentTrack != ERLBGMTrack::None)
		{
			ActiveBGMComp->Play();
		}

		ActiveFadeAlpha = FMath::Min(1.0f, ActiveFadeAlpha + CurrentCrossfadeRate * RealDeltaTime);
		ActiveBGMComp->SetVolumeMultiplier(FMath::Max(0.0001f, ActiveFadeAlpha * TrackScale * BaseVolume));

		if (!FMath::IsNearlyEqual(CurrentAppliedPitch, CurrentDesiredPitch, 0.002f))
		{
			CurrentAppliedPitch = FMath::FInterpTo(CurrentAppliedPitch, CurrentDesiredPitch, RealDeltaTime, 3.0f);
			ActiveBGMComp->SetPitchMultiplier(CurrentAppliedPitch);
		}
	}

	// 2. 이전 BGM 페이드 아웃 및 소멸
	if (FadingOutBGMComp)
	{
		FadingOutAlpha = FMath::Max(0.0f, FadingOutAlpha - CurrentCrossfadeRate * RealDeltaTime);
		FadingOutBGMComp->SetVolumeMultiplier(FMath::Max(0.0001f, FadingOutAlpha * BaseVolume));

		if (FadingOutAlpha <= 0.005f || !FadingOutBGMComp->IsPlaying())
		{
			FadingOutBGMComp->Stop();
			FadingOutBGMComp->DestroyComponent();
			FadingOutBGMComp = nullptr;
		}
	}
}

void URLAudioSubsystem::UpdateCharacterCombatVoices()
{
	UWorld* World = GetWorld();
	if (!World || MasterVolume <= 0.0f || SFXVolume <= 0.0f)
	{
		return;
	}

	const double NowReal = FPlatformTime::Seconds();

	for (TActorIterator<ARLCharacter> It(World); It; ++It)
	{
		ARLCharacter* Char = *It;
		if (!Char)
		{
			continue;
		}

		const FString ClassAndObjName = (Char->GetClass()->GetName() + TEXT(" ") + Char->GetName()).ToLower();
		const bool bIsKhaimera = ClassAndObjName.Contains(TEXT("khaimera")) || ClassAndObjName.Contains(TEXT("boss_2"));
		const bool bIsSevarogBoss = !bIsKhaimera && (ClassAndObjName.Contains(TEXT("sevarog")) || ClassAndObjName.Contains(TEXT("bp_boss")));
		const bool bIsDestroyerPlayer = ClassAndObjName.Contains(TEXT("destroyer"));

		if (!bIsKhaimera && !bIsSevarogBoss && !bIsDestroyerPlayer)
		{
			continue;
		}

		FRLCharacterAudioTracker& Tracker = CharacterTrackers.FindOrAdd(Char);
		const FVector Loc = Char->GetActorLocation();

		const TArray<TObjectPtr<USoundBase>>& AttackPool = bIsKhaimera ? KhaimeraAttackSounds : SevarogAttackSounds;
		const TArray<TObjectPtr<USoundBase>>& SwingPool  = bIsKhaimera ? KhaimeraSwingSounds  : SevarogSwingSounds;
		const TArray<TObjectPtr<USoundBase>>& JumpPool   = bIsKhaimera ? KhaimeraJumpSounds   : SevarogJumpSounds;
		const TArray<TObjectPtr<USoundBase>>& PainPool   = bIsKhaimera ? KhaimeraPainSounds   : SevarogPainSounds;
		const TArray<TObjectPtr<USoundBase>>& RoarPool   = bIsKhaimera ? KhaimeraRoarSounds   : SevarogRoarSounds;
		const TArray<TObjectPtr<USoundBase>>& DeathPool  = bIsKhaimera ? KhaimeraDeathSounds  : SevarogDeathSounds;

		// 1. 사망 시 보이스 (최우선 1회)
		const URLHealthComponent* CharHealth = Char->FindComponentByClass<URLHealthComponent>();
		const bool bIsDeadNow = CharHealth && CharHealth->bIsDead;
		if (bIsDeadNow && !Tracker.bWasDead)
		{
			Tracker.bWasDead = true;
			PlayRandomVoiceFromPool(DeathPool, Loc, 0.38f, 0.94f, 1.02f);
			continue;
		}
		if (bIsDeadNow)
		{
			continue;
		}

		// 2. 보스 전용 상태 변화 (광폭화 포효, 영혼 균열 타격/대균열 고통 보이스)
		if (ARLEnemyCharacter* EnemyChar = Cast<ARLEnemyCharacter>(Char))
		{
			if (EnemyChar->bIsEnraged && !Tracker.bWasEnraged)
			{
				Tracker.bWasEnraged = true;
				Tracker.LastVoicePlayRealTime = NowReal;
				PlayRandomVoiceFromPool(RoarPool, Loc, 0.42f, 0.92f, 1.0f);
			}

			if (EnemyChar->bHasShatteredThisFight && !Tracker.bWasShattered)
			{
				Tracker.bWasShattered = true;
				Tracker.LastVoicePlayRealTime = NowReal;
				PlayRandomVoiceFromPool(PainPool, Loc, 0.40f, 0.88f, 0.96f);
			}
			else if (EnemyChar->SoulCrackStackCount > Tracker.LastSoulCrackStack)
			{
				Tracker.LastSoulCrackStack = EnemyChar->SoulCrackStackCount;
				Tracker.LastVoicePlayRealTime = NowReal;
				PlayRandomVoiceFromPool(PainPool, Loc, 0.32f, 0.95f, 1.04f);
			}
		}

		// 3. 새 공격/스킬 애니메이션 몽타주 재생 감지
		UAnimMontage* CurrentMontage = Char->GetCurrentMontage();
		if (CurrentMontage != Tracker.LastPlayedMontage.Get())
		{
			Tracker.LastPlayedMontage = CurrentMontage;

			// 너무 빈번한 기합 도배 방지를 위해 최소 0.85초 쿨타임 적용
			if (CurrentMontage && (NowReal - Tracker.LastVoicePlayRealTime >= 0.85))
			{
				const FString MontageName = CurrentMontage->GetName().ToLower();
				if (MontageName.Contains(TEXT("death")) || MontageName.Contains(TEXT("hitreact")) || MontageName.Contains(TEXT("stun")) ||
					MontageName.Contains(TEXT("intro")) || MontageName.Contains(TEXT("spawn")) || MontageName.Contains(TEXT("levelstart")))
				{
					continue;
				}

				Tracker.LastVoicePlayRealTime = NowReal;
				const float PitchMin = bIsDestroyerPlayer ? 0.88f : 0.94f;
				const float PitchMax = bIsDestroyerPlayer ? 0.98f : 1.06f;

				if (MontageName.Contains(TEXT("howl")) || MontageName.Contains(TEXT("roar")) || MontageName.Contains(TEXT("frenzy")) || MontageName.Contains(TEXT("ultimate")))
				{
					PlayRandomVoiceFromPool(RoarPool, Loc, 0.36f, PitchMin, PitchMax);
				}
				else if (MontageName.Contains(TEXT("jump")) || MontageName.Contains(TEXT("leap")) || MontageName.Contains(TEXT("pounce")))
				{
					PlayRandomVoiceFromPool(JumpPool, Loc, 0.30f, PitchMin, PitchMax);
				}
				else if (MontageName.Contains(TEXT("swing")) || MontageName.Contains(TEXT("Combo")) || FMath::RandBool())
				{
					PlayRandomVoiceFromPool(SwingPool.Num() > 0 ? SwingPool : AttackPool, Loc, 0.25f, PitchMin, PitchMax);
				}
				else
				{
					PlayRandomVoiceFromPool(AttackPool.Num() > 0 ? AttackPool : SwingPool, Loc, 0.25f, PitchMin, PitchMax);
				}
			}
		}
	}
}

void URLAudioSubsystem::PlayRandomVoiceFromPool(const TArray<TObjectPtr<USoundBase>>& Pool, const FVector& Location, float VolumeScale, float PitchMin, float PitchMax)
{
	if (Pool.Num() == 0)
	{
		return;
	}

	const int32 Idx = FMath::RandHelper(Pool.Num());
	if (USoundBase* Chosen = Pool[Idx].Get())
	{
		PlaySFXAtLocation(Chosen, Location, VolumeScale, PitchMin, PitchMax);
	}
}

void URLAudioSubsystem::PlaySFXAtLocation(USoundBase* Sound, const FVector& Location, float VolumeScale, float PitchMin, float PitchMax)
{
	UWorld* World = GetWorld();
	if (!World || !Sound || MasterVolume <= 0.0f || SFXVolume <= 0.0f)
	{
		return;
	}

	// 플레이어와의 거리에 따른 자연스러운 볼륨 감쇄 적용 (Attenuation 없는 원본 SoundWave가 2D 최대 볼륨으로 터지는 현상 방지)
	float DistanceAttenuation = 1.0f;
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		const float Dist2D = FVector::Dist2D(PlayerPawn->GetActorLocation(), Location);
		DistanceAttenuation = FMath::GetMappedRangeValueClamped(FVector2D(350.0f, 2400.0f), FVector2D(1.0f, 0.20f), Dist2D);
	}

	const float FinalVolume = FMath::Clamp(MasterVolume * SFXVolume * VolumeScale * DistanceAttenuation, 0.0f, 2.0f);
	const float FinalPitch = FMath::FRandRange(FMath::Min(PitchMin, PitchMax), FMath::Max(PitchMin, PitchMax));

	UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FinalVolume, FinalPitch);
}

void URLAudioSubsystem::SetMasterVolume(float NewVolume)
{
	MasterVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
}

void URLAudioSubsystem::SetBGMVolume(float NewVolume)
{
	BGMVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
}

void URLAudioSubsystem::SetSFXVolume(float NewVolume)
{
	SFXVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
}
