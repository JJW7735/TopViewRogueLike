#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RLAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundWave;
class ARLCharacter;
class ARLEnemyCharacter;
class ARLArenaManager;
class UAnimMontage;

/**
 * ARPGMusicPack 10종 트랙 열거형
 */
UENUM(BlueprintType)
enum class ERLBGMTrack : uint8
{
	None                    UMETA(DisplayName = "없음"),
	Battle_HornsOfWar       UMETA(DisplayName = "01. Horns Of War (보스2 카이메라 / 최종 결전)"),
	Battle_TheAmbush        UMETA(DisplayName = "02. The Ambush (일반 아레나 웨이브 전투)"),
	Battle_EnemyApproaches  UMETA(DisplayName = "03. Enemy Approaches (엘리트/중간보스 & 후반 웨이브 전투)"),
	Battle_SwordsAtMidnight UMETA(DisplayName = "04. Swords At Midnight (보스1 세바로그 전투)"),
	Explore_MistyLands      UMETA(DisplayName = "05. Misty Lands (폐허 사원 / 1스테이지 탐험)"),
	Explore_Atmospheres1    UMETA(DisplayName = "06. Atmospheres Part I (외곽 성채 / 아레나 클리어 후)"),
	Explore_MountainHalls   UMETA(DisplayName = "07. Mountain Halls (지하묘지 탐험)"),
	Explore_Atmospheres2    UMETA(DisplayName = "08. Atmospheres Part II (전쟁 납골당 탐험)"),
	Explore_JourneyBegins   UMETA(DisplayName = "09. The Journey Begins (타이틀 / 거점 / 보스 클리어)"),
	Explore_Atmospheres3    UMETA(DisplayName = "10. Atmospheres Part III (보스방 진입 전 긴장감 / 게임오버)")
};

/**
 * 게임플레이 상황에 따른 자동 음악 상태
 */
UENUM(BlueprintType)
enum class ERLMusicState : uint8
{
	None            UMETA(DisplayName = "초기 상태"),
	TitleOrHub      UMETA(DisplayName = "타이틀 / 로비 거점"),
	Exploration     UMETA(DisplayName = "던전 비전투 탐험"),
	ArenaCombat     UMETA(DisplayName = "아레나 웨이브 전투"),
	BossCombat      UMETA(DisplayName = "보스 전투"),
	BossVictoryCalm UMETA(DisplayName = "보스 처치 완료 여운"),
	GameOver        UMETA(DisplayName = "플레이어 사망 (게임오버)")
};

/**
 * 보스 및 캐릭터의 몽타주/상태 변화를 추적하여 기합/포효/피격/사망 보이스를 재생하는 런타임 트래커
 */
struct FRLCharacterAudioTracker
{
	TWeakObjectPtr<UAnimMontage> LastPlayedMontage;
	double LastVoicePlayRealTime = 0.0;
	bool bWasEnraged = false;
	bool bWasDead = false;
	bool bWasShattered = false;
	int32 LastSoulCrackStack = 0;
};

/**
 * 배경음악(BGM) 크로스페이드 전환 및 보스/전투 사운드 자동 연출 서브시스템.
 *
 * - 기존 캐릭터/보스/아레나 블루프린트나 C++ 파일을 건드리지 않고 월드 상태를 자동 감지하여 작동합니다.
 * - 일시정지(보상 카드 선택, 상점, 일시정지 메뉴) 중에도 BGM이 끊기지 않고 자연스러운 볼륨 더킹(Ducking)이 적용됩니다.
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static URLAudioSubsystem* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

	// ==========================================
	// BGM 제어 API
	// ==========================================

	/** 특정 BGM 트랙으로 부드럽게 크로스페이드 전환합니다. */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void PlayBGMTrack(ERLBGMTrack Track, float CrossfadeDuration = 1.6f, float TargetPitch = 1.0f);

	/** 현재 재생 중인 BGM을 부드럽게 페이드 아웃하며 정지합니다. */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void StopBGM(float FadeOutDuration = 1.2f);

	/** 자동 상황 감지 기반 BGM 전환 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|BGM")
	bool bAutoManageBGM = true;

	UFUNCTION(BlueprintPure, Category = "Audio|BGM")
	ERLBGMTrack GetCurrentBGMTrack() const { return CurrentTrack; }

	UFUNCTION(BlueprintPure, Category = "Audio|BGM")
	ERLMusicState GetCurrentMusicState() const { return CurrentMusicState; }

	// ==========================================
	// 볼륨 제어 API (맵 이동 시에도 전역 유지)
	// ==========================================

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetMasterVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "Audio|Volume")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetBGMVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "Audio|Volume")
	float GetBGMVolume() const { return BGMVolume; }

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetSFXVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "Audio|Volume")
	float GetSFXVolume() const { return SFXVolume; }

	/** 위치 기반 효과음을 랜덤 피치/볼륨 변조 및 전역 SFX 볼륨을 적용하여 재생합니다. */
	UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
	void PlaySFXAtLocation(USoundBase* Sound, const FVector& Location, float VolumeScale = 1.0f, float PitchMin = 0.94f, float PitchMax = 1.06f);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureAudioAssetsLoaded();
	USoundBase* LoadLoopingMusicAsset(const TCHAR* WavPath, const TCHAR* CueFallbackPath);
	void LoadSoundWaveVariations(const FString& BasePackagePath, int32 MaxCount, TArray<TObjectPtr<USoundBase>>& OutArray);

	void EvaluateWorldMusicState();
	FString ResolveActiveMapContextName() const;
	ERLBGMTrack SelectBossTrack(const ARLEnemyCharacter* BossActor) const;

	void UpdateCrossfadeAndDucking(float RealDeltaTime);
	void UpdateCharacterCombatVoices();
	void PlayRandomVoiceFromPool(const TArray<TObjectPtr<USoundBase>>& Pool, const FVector& Location, float VolumeScale = 1.0f, float PitchMin = 0.94f, float PitchMax = 1.06f);

	UPROPERTY()
	TMap<ERLBGMTrack, TObjectPtr<USoundBase>> BGMTracks;

	// 카이메라 보스(BP_Boss_2) 내장 보이스/기합 사운드 풀
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> KhaimeraAttackSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> KhaimeraSwingSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> KhaimeraJumpSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> KhaimeraPainSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> KhaimeraRoarSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> KhaimeraDeathSounds;

	// 세바로그 보스(BP_Boss) & 디스트로이어 내장 보이스/기합 사운드 풀
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> SevarogAttackSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> SevarogSwingSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> SevarogJumpSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> SevarogPainSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> SevarogRoarSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> SevarogDeathSounds;

	bool bAssetsLoaded = false;

	// 듀얼 덱 크로스페이더 오디오 컴포넌트
	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveBGMComp = nullptr;

	UPROPERTY()
	TObjectPtr<UAudioComponent> FadingOutBGMComp = nullptr;

	ERLBGMTrack CurrentTrack = ERLBGMTrack::None;
	ERLMusicState CurrentMusicState = ERLMusicState::None;

	float ActiveFadeAlpha = 0.0f;
	float FadingOutAlpha = 0.0f;
	float CurrentCrossfadeRate = 0.65f;
	float CurrentDuckMultiplier = 1.0f;
	float CurrentDesiredPitch = 1.0f;
	float CurrentAppliedPitch = 1.0f;
	float BossEnrageVolumeBoost = 1.0f;

	double LastStateCheckRealTime = 0.0;
	double LastRealTickTime = 0.0;
	bool bHadActiveBossInWorld = false;

	TMap<TWeakObjectPtr<ARLCharacter>, FRLCharacterAudioTracker> CharacterTrackers;

	static float MasterVolume;
	static float BGMVolume;
	static float SFXVolume;
};
