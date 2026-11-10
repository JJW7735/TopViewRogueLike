#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLArenaManager.generated.h"

class UBoxComponent;
class USceneComponent;
class ARLEnemyCharacter;
class ARLRewardChest;
class ARLPortal;
class ARLCharacter;

/**
 * 아레나 전투 상태 열거형
 */
UENUM(BlueprintType)
enum class ERLArenaState : uint8
{
	Inactive        UMETA(DisplayName = "비활성화 (대기)"),
	InProgress      UMETA(DisplayName = "전투 진행 중"),
	BetweenWaves    UMETA(DisplayName = "웨이브 대기 중"),
	Cleared         UMETA(DisplayName = "클리어 완료")
};

/**
 * 몬스터 스폰 위치 결정 모드
 */
UENUM(BlueprintType)
enum class ERLSpawnLocationMode : uint8
{
	RandomInTriggerVolume   UMETA(DisplayName = "전투 영역(TriggerVolume) 내 완전 무작위 스폰"),
	RandomInRadius          UMETA(DisplayName = "아레나 중심 기준 반경 내 무작위 스폰"),
	FixedSpawnPoints        UMETA(DisplayName = "고정 스폰 화살표/마커 위치 스폰")
};

/**
 * 웨이브 내 개별 몬스터 스폰 정의
 */
USTRUCT(BlueprintType)
struct FRLMonsterSpawnEntry
{
	GENERATED_BODY()

	// 스폰할 몬스터 클래스 (ARLEnemyCharacter 기반 블루프린트)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<ARLEnemyCharacter> EnemyClass;

	// 스폰할 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 Count = 1;

	// 특정 스폰 포인트 인덱스 (-1이면 사용 가능한 스폰 포인트 중 라운드로빈/순환 분배)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 SpecificSpawnPointIndex = -1;
};

/**
 * 단일 웨이브 데이터 정의
 */
USTRUCT(BlueprintType)
struct FRLWaveData
{
	GENERATED_BODY()

	// 웨이브 이름 (디버깅/UI용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FString WaveName = TEXT("Wave 1");

	// 이 웨이브에서 소환할 몬스터 구성 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FRLMonsterSpawnEntry> Spawns;

	// 이전 웨이브 클리어 후 이 웨이브 시작 전 대기 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float DelayBeforeWave = 1.0f;

	// 몬스터 개별 스폰 간격 (초, 0이면 한 번에 즉시 스폰)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float SpawnInterval = 0.25f;
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArenaStateChanged, ERLArenaState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArenaWaveStarted, int32, CurrentWaveIndex, int32, TotalWaves);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArenaEnemyCountChanged, int32, RemainingEnemies, int32, TotalWaveEnemies);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArenaWaveCleared, int32, ClearedWaveIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArenaCleared);

/**
 * 탑뷰 로그라이크 아레나/전투 구역 총괄 매니저
 * - 플레이어 진입 시 전투 트리거 및 문(Barrier) 폐쇄
 * - 웨이브별 몬스터 순차/동적 스폰
 * - OnEnemyDied 델리게이트 연동 실시간 처치 카운팅
 * - 클리어 시 문 개방 + 보상 상자(ARLRewardChest) + 포탈(ARLPortal) 생성
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLArenaManager : public AActor
{
	GENERATED_BODY()

public:
	ARLArenaManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ==========================================
	// 컴포넌트
	// ==========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 플레이어 진입 감지용 전투 영역 트리거 볼륨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume;

	// 출구 포탈이 생성될 위치 (컴포넌트로 에디터 뷰포트에서 드래그 배치 가능)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> PortalSpawnPoint;

	// 보상 상자가 생성될 위치 (컴포넌트로 에디터 뷰포트에서 드래그 배치 가능)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ChestSpawnPoint;

	// 전투 시작 시 입구를 막아줄 자체 차단벽 박스 (뷰포트에서 방 출입구로 이동/크기 조절)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> EntranceBarrier;

	// 전투 영역 4면 자동 결계 차단벽 (동, 서, 남, 북)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoundaryWall_North;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoundaryWall_South;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoundaryWall_East;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoundaryWall_West;

	// ==========================================
	// 웨이브 & 스폰 설정
	// ==========================================

	// 플레이어가 TriggerVolume에 겹치면 자동으로 전투 시작 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Trigger")
	bool bAutoStartOnOverlap = true;

	// 순차 진행될 웨이브 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	TArray<FRLWaveData> Waves;

	// 몬스터 스폰 위치 결정 모드 (기본값: 전투 영역 TriggerVolume 내 무작위 스폰)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning")
	ERLSpawnLocationMode SpawnLocationMode = ERLSpawnLocationMode::RandomInTriggerVolume;

	// 네비메시(NavMesh) 위에 안전하게 안착시킬지 여부 (벽이나 허공에 스폰 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning")
	bool bProjectToNavMesh = true;

	// 플레이어 코앞에 몬스터가 바로 스폰되지 않도록 하는 최소 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning", meta = (ClampMin = "0.0"))
	float MinDistanceFromPlayer = 350.0f;

	// 블루프린트 내부에 부착된 고정 스폰 포인트 컴포넌트 목록 (ArrowComponent, SceneComponent 등).
	// 비워둘 경우 BeginPlay에서 'SpawnPoint_Enemy' 이름이 포함된 컴포넌트나 ArrowComponent를 자동 수집합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning")
	TArray<TObjectPtr<USceneComponent>> InternalSpawnPoints;

	// 레벨에 배치된 스폰 포인트 액터 목록 (TargetPoint 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning")
	TArray<TObjectPtr<AActor>> SpawnPointActors;

	// 반경 기반 랜덤 스폰(RandomInRadius) 시 사용할 원형 스폰 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning", meta = (ClampMin = "100.0"))
	float DefaultSpawnRadius = 800.0f;

	// ==========================================
	// 문/차단벽 (Barriers)
	// ==========================================

	// 전투 중 TriggerVolume 4면 테두리 전체에 결계를 쳐서 사방 어디로도 못 나가게 막을지 여부 (벽 없는 맵/야외 투기장 필수, 기본: true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Barriers")
	bool bEncloseEntireArena = true;

	// 플레이어 진입 후 결계가 닫히기까지의 딜레이 (초, 기본: 1.5초)
	// 방 안으로 충분히 걸어 들어온 후 등 뒤에서 닫혀 벽에 끼이는 현상을 방지합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Barriers", meta = (ClampMin = "0.0"))
	float BarrierCloseDelay = 1.5f;

	// 자체 EntranceBarrier 박스로 특정 입구를 자동 차단할지 여부 (실내 던전 룸에서 문만 막을 때 사용, 기본: false)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Barriers")
	bool bUseEntranceBarrier = false;

	// 전투 중 잠그고, 클리어 시 열어줄 외부 문/장애물 액터 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Barriers")
	TArray<TObjectPtr<AActor>> BarrierActors;

	// ==========================================
	// 클리어 보상 & 포탈 연동
	// ==========================================

	// 모든 웨이브 클리어 시 보상 상자 스폰 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Rewards")
	bool bSpawnRewardChestOnClear = true;

	// 스폰할 보상 상자 클래스 (비워두면 기본 MyRLRewardChest 또는 ARLRewardChest 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Rewards")
	TSubclassOf<ARLRewardChest> RewardChestClass;

	// 모든 웨이브 클리어 시 출구 포탈 스폰 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Rewards")
	bool bSpawnPortalOnClear = true;

	// 스폰할 포탈 클래스 (비워두면 기본 BP_Portal 또는 ARLPortal 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Rewards")
	TSubclassOf<ARLPortal> PortalClass;

	// 이미 레벨에 배치된 포탈 액터가 있다면 지정 (지정 시 스폰 대신 해당 포탈을 활성화/표시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Rewards")
	TObjectPtr<AActor> ExistingPortalActor;

	// 동적으로 스폰된 보상 상자 참조
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arena|Spawned")
	TWeakObjectPtr<ARLRewardChest> SpawnedRewardChest;

	// 동적으로 스폰된 출구 포탈 참조
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arena|Spawned")
	TWeakObjectPtr<ARLPortal> SpawnedPortal;

	// 아레나에서 동적으로 생성했던 보상 상자 및 포탈을 강제 정리
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void CleanupArenaSpawnedActors();

	// ==========================================
	// 이벤트 델리게이트 (UI/사운드/BP 연동용)
	// ==========================================

	UPROPERTY(BlueprintAssignable, Category = "Arena|Events")
	FOnArenaStateChanged OnArenaStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Events")
	FOnArenaWaveStarted OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Events")
	FOnArenaEnemyCountChanged OnEnemyCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Events")
	FOnArenaWaveCleared OnWaveCleared;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Events")
	FOnArenaCleared OnArenaCleared;

	// ==========================================
	// 블루프린트 연출 오버라이드 훅 (VFX, SFX)
	// ==========================================

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Hooks")
	void BP_OnArenaStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Hooks")
	void BP_OnWaveStarted(int32 CurrentWave, int32 TotalWaves);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Hooks")
	void BP_OnWaveCleared(int32 ClearedWave);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Hooks")
	void BP_OnArenaCleared();

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Hooks")
	void BP_OnBarriersLocked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Hooks")
	void BP_OnBarriersUnlocked();

	// ==========================================
	// 제어 함수
	// ==========================================

	// 아레나 전투 시작
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void StartArena();

	// 특정 웨이브 시작
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void StartWave(int32 WaveIndex);

	// 강제 초기화
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void ResetArena();

	// 현재 상태 조회
	UFUNCTION(BlueprintPure, Category = "Arena")
	ERLArenaState GetArenaState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetCurrentWaveIndex() const { return CurrentWaveIndex; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetTotalWaves() const { return Waves.Num(); }

	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetRemainingEnemies() const { return RemainingEnemies; }

	// ==========================================
	// 테스트 치트 (콘솔 명령어)
	// ==========================================

	UFUNCTION(Exec, BlueprintCallable, Category = "Arena|Test")
	void TestStartArena();

	UFUNCTION(Exec, BlueprintCallable, Category = "Arena|Test")
	void TestClearCurrentWave();

	UFUNCTION(Exec, BlueprintCallable, Category = "Arena|Test")
	void TestKillAllEnemies();

protected:
	UFUNCTION()
	void OnTriggerVolumeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEnemyDied(ARLEnemyCharacter* DeadEnemy);

	void ProcessSpawnQueue();
	void SpawnSingleEnemy(TSubclassOf<ARLEnemyCharacter> EnemyClass, int32 PreferredPointIndex);
	FVector GetSpawnLocation(int32 PointIndex, FRotator& OutRotation) const;

	void LockBarriers();
	void UnlockBarriers();
	void UpdateBoundaryWalls();

	void CompleteCurrentWave();
	void CompleteArena();
	void SpawnOrActivateExitPortal();
	void CheckInitialPlayerOverlap();

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Arena|Runtime")
	ERLArenaState CurrentState = ERLArenaState::Inactive;

	UPROPERTY(VisibleInstanceOnly, Category = "Arena|Runtime")
	int32 CurrentWaveIndex = -1;

	UPROPERTY(VisibleInstanceOnly, Category = "Arena|Runtime")
	int32 RemainingEnemies = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Arena|Runtime")
	int32 TotalWaveEnemies = 0;

	int32 SpawnPointRoundRobin = 0;

	struct FPendingSpawn
	{
		TSubclassOf<ARLEnemyCharacter> EnemyClass;
		int32 PreferredPointIndex = -1;
	};
	TArray<FPendingSpawn> SpawnQueue;

	UPROPERTY()
	TArray<TWeakObjectPtr<ARLEnemyCharacter>> ActiveEnemies;

	FTimerHandle WaveDelayTimerHandle;
	FTimerHandle SpawnIntervalTimerHandle;
	FTimerHandle BarrierCloseTimerHandle;
	FTimerHandle InitialOverlapCheckTimerHandle;
};
