#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RLSkillData.h"
#include "RLSaveGame.generated.h"

class ARLCharacter;

/**
 * 체력 저장 데이터
 */
USTRUCT(BlueprintType)
struct FRLHealthSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Health")
	float MaxHealth = 100.0f;
};

/**
 * 스탯 및 레벨 저장 데이터
 */
USTRUCT(BlueprintType)
struct FRLStatSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	int32 CurrentLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float CurrentExp = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float MaxExp = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float ExpGrowthMultiplier = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float AttackPower = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float AbilityPower = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float CritChance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float CritDamageMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float AttackSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float Defense = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Stats")
	float DamageReduction = 0.0f;
};

/**
 * 인벤토리 단일 슬롯 저장 데이터
 */
USTRUCT(BlueprintType)
struct FRLInventorySlotSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Inventory")
	int32 SlotIndex = 0;

	// 아이템 데이터에셋 소프트 경로 (경로 기반 역직렬화로 애셋 유실 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Inventory")
	FSoftObjectPath ItemAssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Inventory")
	int32 Quantity = 0;
};

/**
 * 인벤토리 전체 저장 데이터
 */
USTRUCT(BlueprintType)
struct FRLInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Inventory")
	int32 Gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Inventory")
	TArray<FRLInventorySlotSaveData> Slots;
};

/**
 * 장착 스킬 슬롯 저장 데이터
 */
USTRUCT(BlueprintType)
struct FRLSkillSlotSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Skill")
	ESkillSlot Slot = ESkillSlot::Slot_Q;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Skill")
	FSoftObjectPath SkillAssetPath;
};

/**
 * 스킬 전체 저장 데이터
 */
USTRUCT(BlueprintType)
struct FRLSkillSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Skill")
	TArray<FRLSkillSlotSaveData> EquippedSkills;
};

/**
 * [Track 2] 런 세이브 데이터 (진행 중인 런 / 일시중단 이어하기용)
 * 사망 시 즉시 초기화/삭제되는 정통 로그라이크 규칙 적용
 */
USTRUCT(BlueprintType)
struct FRLRunSaveData
{
	GENERATED_BODY()

	// 현재 런이 진행 중인지 여부 (사망 시 false)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	bool bIsRunActive = false;

	// ★ 이 런이 어느 캐릭터(블레이드/소서리스/디스트로이어 등)로 저장된 것인지 식별.
	//   저장 시점의 캐릭터와 지금 스폰되는 캐릭터가 다르면 복원하지 않기 위한 안전장치.
	//   (이 필드가 추가되기 전 저장 파일은 비어있으므로, 비어있을 땐 구분 없이 복원 허용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	TSoftClassPtr<ARLCharacter> CharacterClass;

	// ★ "새 게임 시작 시 기존 런 삭제" 경고 팝업에 표시할 캐릭터 이름 (ARLCharacter::CharacterDisplayName 복사본).
	//   BP에서 클래스를 다시 로드하지 않고 바로 텍스트로 쓸 수 있게 저장 시점에 같이 박아둠.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FText SavedCharacterDisplayName;

	// 현재 진행 중인 스트리밍 맵 이름 (예: L_Haunted, L_Soul 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FName CurrentMapName = NAME_None;

	// 현재 도달 스테이지 번호
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	int32 StageNumber = 1;

	// 현재 스테이지의 아레나 전투가 이미 클리어되었는지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	bool bIsStageCleared = false;

	// 플레이어 위치 및 회전
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FRotator PlayerRotation = FRotator::ZeroRotator;

	// 이번 런에서 획득한 영혼(사망 또는 클리어 시 메타에 합산될 임시 재화)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	int32 RunSouls = 0;

	// 저장된 일시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FDateTime SaveTimestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FRLHealthSaveData Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FRLStatSaveData Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FRLInventorySaveData Inventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Run")
	FRLSkillSaveData Skills;
};

/**
 * [Track 1] 영구 메타 세이브 데이터 (프로필 / 영구 진행도)
 * 런 사망/종료와 관계없이 영구 보존
 */
USTRUCT(BlueprintType)
struct FRLMetaSaveData
{
	GENERATED_BODY()

	// 영구 누적 영혼 (영구 특성 업그레이드 및 해금에 사용하는 메타 재화)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 TotalSouls = 0;

	// 최고 도달 스테이지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 HighestStageCleared = 0;

	// 누적 도전 횟수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 TotalRunsAttempted = 0;

	// 누적 승리(최종 클리어) 횟수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	int32 TotalVictories = 0;

	// 영구 해금된 스킬, 아이템, 기능 ID 집합
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	TSet<FName> UnlockedIDs;

	// 영구 스탯 업그레이드 단계 (예: Key = "MaxHealth_Up", Value = 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save|Meta")
	TMap<FName, int32> PermanentUpgrades;
};

/**
 * 언리얼 엔진의 USaveGame을 상속하는 로그라이크 세이브 게임 클래스
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	URLSaveGame();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	FString SaveSlotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	int32 UserIndex = 0;

	// [Track 2] 런 세이브 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FRLRunSaveData RunData;

	// [Track 1] 영구 메타 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FRLMetaSaveData MetaData;
};
