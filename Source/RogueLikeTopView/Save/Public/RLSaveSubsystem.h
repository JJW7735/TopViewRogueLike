#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RLSaveGame.h"
#include "RLSaveSubsystem.generated.h"

class ARLCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunSaveCompletedSignature, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunLoadedSignature, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMetaSavedSignature, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoulsChangedSignature, int32, TotalSouls, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRunSoulsChangedSignature, int32, RunSouls, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMetaUpgradePurchasedSignature, FName, UpgradeKey, int32, NewLevel);

/**
 * 2-Track 세이브 시스템을 총괄하는 GameInstance 서브시스템
 * - [Track 1] 영구 메타 데이터 (소울, 해금, 업그레이드 등)
 * - [Track 2] 런 세이브 데이터 (포탈 이동 시 자동저장, 사망 시 초기화)
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 어디서나 쉽게 서브시스템에 접근 가능한 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Save", meta = (WorldContext = "WorldContextObject"))
	static URLSaveSubsystem* Get(const UObject* WorldContextObject);

	// ==========================================
	// [Track 2] 런 세이브/로드/초기화 (Run In-Progress)
	// ==========================================

	/** 현재 진행 중인 런을 저장합니다 (체력, 스탯, 인벤토리, 스킬, 현재 맵 등) */
	UFUNCTION(BlueprintCallable, Category = "Save|Run")
	bool SaveActiveRun(ARLCharacter* PlayerCharacter, FName InCurrentMapName, int32 InStageNumber, FVector InRespawnLocation = FVector::ZeroVector);

	/** 저장된 런 데이터를 디스크에서 메모리로 로드합니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Run")
	bool LoadActiveRun(bool bPrepareForSpawn = true);

	/** 로드된 런 데이터를 실제 플레이어 캐릭터 컴포넌트에 주입/복원합니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Run")
	bool ApplyRunDataToPlayer(ARLCharacter* PlayerCharacter);

	/** 메인 메뉴나 어디서든 활성 런을 이어하기 (저장된 맵으로 이동 및 캐릭터 상태 복원) */
	UFUNCTION(BlueprintCallable, Category = "Save|Run")
	bool ResumeActiveRun();

	/** 진행 중인 런을 종료/초기화합니다 (사망 또는 런 클리어 시 호출) */
	UFUNCTION(BlueprintCallable, Category = "Save|Run")
	void ClearActiveRun(bool bBankSoulsToMeta = true);

	/** 이어할 수 있는 활성 런 세이브가 존재하는지 여부 */
	UFUNCTION(BlueprintPure, Category = "Save|Run")
	bool HasActiveRun() const;

	/** 캐릭터 선택 화면에서 사용: 지금 고르려는 캐릭터가, 저장된 활성 런의 캐릭터와 다른지 확인합니다.
	 *  true면 "새로 시작하면 기존 런이 삭제됩니다" 경고 팝업을 띄우고, 확인 시 ClearActiveRun(true)을
	 *  호출한 뒤 새 캐릭터로 진행하세요. (캐릭터 정보가 없는 옛 저장 파일은 안전하게 false를 반환합니다) */
	UFUNCTION(BlueprintPure, Category = "Save|Run")
	bool HasActiveRunForDifferentCharacter(TSubclassOf<ARLCharacter> NewCharacterClass) const;

	/** 포탈을 타고 다음 스테이지로 진입할 때 자동 호출되는 편의 함수
	 *  bSpawnAtPlayerStart: 맵 자체가 바뀌는 경우(OpenLevel) true — 위치를 비워서 새 맵의 PlayerStart에 스폰되게 한다. */
	UFUNCTION(BlueprintCallable, Category = "Save|Run")
	bool AutoSaveOnStageTransition(ARLCharacter* PlayerCharacter, FName NewLevelName, bool bSpawnAtPlayerStart = false);

	/** 플레이어 사망 시 호출: 런 세이브 삭제 + 수집한 소울 메타 정산 */
	UFUNCTION(BlueprintCallable, Category = "Save|Run")
	void HandlePlayerDeath();

	/** 캐시된 현재 런 세이브 데이터 조회 */
	UFUNCTION(BlueprintPure, Category = "Save|Run")
	const FRLRunSaveData& GetActiveRunData() const { return CurrentRunData; }

	/** 현재 월드에 살아있는 적이 있거나 아레나 전투가 진행 중인지 확인 */
	UFUNCTION(BlueprintPure, Category = "Save|Combat", meta = (WorldContext = "WorldContextObject"))
	bool IsCombatActive(const UObject* WorldContextObject = nullptr) const;

	/** 현재 월드의 아레나가 클리어 완료 상태인지 확인 */
	UFUNCTION(BlueprintPure, Category = "Save|Combat", meta = (WorldContext = "WorldContextObject"))
	bool IsArenaCleared(const UObject* WorldContextObject = nullptr) const;

	// ==========================================
	// [Track 1] 영구 메타 세이브 (Permanent Meta Progression)
	// ==========================================

	/** 영구 메타 데이터를 디스크에 저장합니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta")
	bool SaveMetaProgression();

	/** 영구 메타 데이터를 디스크에서 로드합니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta")
	bool LoadMetaProgression();

	/** 영구 소울(영혼석) 추가 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta|Souls")
	void AddTotalSouls(int32 Amount);

	/** 영구 소울 소비 (부족할 경우 실패하고 false 반환) */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta|Souls")
	bool SpendTotalSouls(int32 Amount);

	/** 영구 누적 소울 반환 */
	UFUNCTION(BlueprintPure, Category = "Save|Meta|Souls")
	int32 GetTotalSouls() const { return CurrentMetaData.TotalSouls; }

	/** 이번 런에서 임시로 획득한 소울 추가 (사망/클리어 시 영구 소울로 이관됨) */
	UFUNCTION(BlueprintCallable, Category = "Save|Run|Souls")
	void AddRunSouls(int32 Amount);

	/** 배율이 적용된 실수 영혼량을 소수점 잔여분까지 누적해 정수로 지급 (예: 2 × 1.15 = 2.3 → 잔여 0.3 누적) */
	UFUNCTION(BlueprintCallable, Category = "Save|Run|Souls")
	int32 AddRunSoulsFloat(float ExactAmount);

	/** 이번 런 임시 소울 반환 */
	UFUNCTION(BlueprintPure, Category = "Save|Run|Souls")
	int32 GetRunSouls() const { return CurrentRunData.RunSouls; }

	/** 가장 최근 런에서 정산되어 메타 금고로 입금된 소울 반환 (사망/종료 후 게임오버 UI 표시용) */
	UFUNCTION(BlueprintPure, Category = "Save|Run|Souls")
	int32 GetLastSettledRunSouls() const { return LastSettledRunSouls; }

	/** 게임오버 후 재도전: 활성 런을 완전 삭제하고 지정된 레벨(기본: 현재 레벨)을 새로 엽니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Run|GameOver")
	void RestartRunAfterDeath(FName RestartLevelName = NAME_None);

	/** 게임오버 후 메인 메뉴로 복귀: 활성 런을 정리하고 메인 메뉴를 엽니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Run|GameOver")
	void QuitToMainMenuAfterDeath(FName MainMenuLevelName = FName("L_MainMenu"));

	/** 최고 도달 스테이지 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta")
	void RecordStageClear(int32 StageNumber);

	/** 특정 ID(스킬/아이템/특성 등) 해금 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta")
	void UnlockID(FName InID);

	/** 특정 ID가 이미 해금되었는지 확인 */
	UFUNCTION(BlueprintPure, Category = "Save|Meta")
	bool IsIDUnlocked(FName InID) const;

	/** 영구 특성 업그레이드 레벨 조회 */
	UFUNCTION(BlueprintPure, Category = "Save|Meta")
	int32 GetUpgradeLevel(FName UpgradeKey) const;

	/** 영구 특성 업그레이드 레벨 설정 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta")
	void SetUpgradeLevel(FName UpgradeKey, int32 Level);

	/** 등록된 모든 메타 특성의 상태 정보(현재 레벨, 비용, 설명 등)를 가져옵니다 (UI 바인딩용) */
	UFUNCTION(BlueprintPure, Category = "Save|Meta|Upgrade")
	TArray<FRLMetaUpgradeInfo> GetAllMetaUpgradeInfos() const;

	/** 특정 메타 특성의 상태 정보를 조회합니다 */
	UFUNCTION(BlueprintPure, Category = "Save|Meta|Upgrade")
	bool GetMetaUpgradeInfo(FName UpgradeKey, FRLMetaUpgradeInfo& OutInfo) const;

	/** 진행 중인 런(이어하기 가능한 세이브)이 있으면 영구 강화/초기화를 막습니다 (다회차 유도) */
	UFUNCTION(BlueprintPure, Category = "Save|Meta|Upgrade")
	bool IsMetaUpgradeLocked() const { return HasActiveRun(); }

	/** 소울을 소비하여 특정 메타 특성을 1단계 업그레이드합니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta|Upgrade")
	bool UpgradeMetaTrait(FName UpgradeKey, FString& OutFailReason);

	/** 모든 메타 특성을 초기화하고 소모된 소울을 전액 환불합니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta|Upgrade")
	void ResetAllMetaUpgrades(bool bRefundSouls = true);

	/** 현재 저장된 영구 특성 보너스를 플레이어 캐릭터에 실제로 가산/주입합니다 */
	UFUNCTION(BlueprintCallable, Category = "Save|Meta|Upgrade")
	bool ApplyMetaUpgradesToPlayer(ARLCharacter* PlayerCharacter);

	/** 캐시된 영구 메타 데이터 조회 */
	UFUNCTION(BlueprintPure, Category = "Save|Meta")
	const FRLMetaSaveData& GetMetaData() const { return CurrentMetaData; }

	// ==========================================
	// 이벤트 델리게이트
	// ==========================================

	UPROPERTY(BlueprintAssignable, Category = "Save|Event")
	FOnRunSaveCompletedSignature OnRunSaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Save|Event")
	FOnRunLoadedSignature OnRunLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Save|Event")
	FOnMetaSavedSignature OnMetaSaved;

	UPROPERTY(BlueprintAssignable, Category = "Save|Event")
	FOnSoulsChangedSignature OnSoulsChanged;

	/** 이번 런 임시 소울(RunSouls)이 바뀔 때 방송 (몬스터 처치 적립, 런 종료 시 0으로 리셋) */
	UPROPERTY(BlueprintAssignable, Category = "Save|Event")
	FOnRunSoulsChangedSignature OnRunSoulsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Save|Event")
	FOnMetaUpgradePurchasedSignature OnMetaUpgradePurchased;

	// ==========================================
	// 설정 및 상태 플래그
	// ==========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save|Config")
	FString RunSlotName = TEXT("RL_RunSave");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save|Config")
	FString MetaSlotName = TEXT("RL_MetaSave");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save|Config")
	int32 UserIndex = 0;

	/** 로드된 런 데이터를 새로 스폰되는 캐릭터에 자동 주입해야 하는지 여부 */
	UPROPERTY(BlueprintReadWrite, Category = "Save|State")
	bool bPendingRunApply = false;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Save|State")
	FRLRunSaveData CurrentRunData;

	/** 가장 최근 정산된 런 소울 (게임오버 UI 표시용) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Save|State")
	int32 LastSettledRunSouls = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Save|State")
	FRLMetaSaveData CurrentMetaData;

	/** 플레이어 상태와 맵/스테이지 정보를 CurrentRunData에 채움 (디스크 기록은 하지 않음) */
	void FillRunData(ARLCharacter* PlayerCharacter, FName InCurrentMapName, int32 InStageNumber, FVector InRespawnLocation);

	/** CurrentRunData를 디스크 슬롯에 기록 */
	bool WriteRunToDisk();

	void GatherPlayerData(ARLCharacter* PlayerCharacter, FRLRunSaveData& OutData);
	void ApplyPlayerData(ARLCharacter* PlayerCharacter, const FRLRunSaveData& InData);
};
