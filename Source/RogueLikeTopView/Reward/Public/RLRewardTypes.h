#pragma once

#include "CoreMinimal.h"
#include "RLRewardTypes.generated.h"

/**
 * 보상 카드의 희귀도 등급
 */
UENUM(BlueprintType)
enum class ERLRewardRarity : uint8
{
	Common     UMETA(DisplayName = "일반"),
	Rare       UMETA(DisplayName = "희귀"),
	Epic       UMETA(DisplayName = "영웅"),
	Legendary  UMETA(DisplayName = "전설")
};

/**
 * 보상 선택 UI가 어떤 경로로 떴는지 구분하는 값.
 * 상자(Chest)는 게임을 멈추고 신중하게 고르는 연출, 레벨업(LevelUp)은 전투 흐름을 끊지 않는 가벼운 연출로 분기할 때 사용.
 */
UENUM(BlueprintType)
enum class ERLRewardTriggerSource : uint8
{
	Chest     UMETA(DisplayName = "보상 상자"),
	LevelUp   UMETA(DisplayName = "레벨업")
};

/**
 * 강화 대상 스탯/효과 종류
 */
UENUM(BlueprintType)
enum class ERLTargetStat : uint8
{
	AttackPower             UMETA(DisplayName = "공격력"),
	AbilityPower            UMETA(DisplayName = "주문력"),
	CritChance              UMETA(DisplayName = "치명타 확률"),
	CritDamageMultiplier    UMETA(DisplayName = "치명타 피해"),
	MoveSpeed               UMETA(DisplayName = "이동 속도"),
	AttackSpeed             UMETA(DisplayName = "공격 속도"),
	Defense                 UMETA(DisplayName = "방어력"),
	DamageReduction         UMETA(DisplayName = "피해 감소율"),
	MaxHealth               UMETA(DisplayName = "최대 체력"),
	HealPercent             UMETA(DisplayName = "체력 회복")
};

/**
 * 단일 스탯 보너스 수치
 */
USTRUCT(BlueprintType)
struct FRLStatBonus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	ERLTargetStat TargetStat = ERLTargetStat::AttackPower;

	// 증가 수치 (예: 공격력 +10.0, 치명타율 +5.0%, 이속 +50.0 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	float Value = 10.0f;
};

/**
 * 3지선다 보상 카드 1장의 데이터 구조체
 */
USTRUCT(BlueprintType)
struct FRLRewardData
{
	GENERATED_BODY()

	// 보상 고유 식별자
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	FName RewardID = NAME_None;

	// 카드에 표시될 이름 (예: "거인의 완력", "치명적인 일격")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	FText RewardName;

	// 카드 설명 텍스트 (예: "공격력이 15 증가합니다.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	FText Description;

	// 희귀도 등급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	ERLRewardRarity Rarity = ERLRewardRarity::Common;

	// 이 카드가 제공하는 스탯 보너스 목록 (단일 또는 복합 효과)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	TArray<FRLStatBonus> StatBonuses;

	// 카드 아이콘 텍스처
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	TSoftObjectPtr<UTexture2D> Icon;

	// 카드 출현 가중치 (기본 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	float SelectionWeight = 1.0f;

	// 희귀도에 맞는 테마 색상 반환 (UI 테두리/빛 연출용)
	static FLinearColor GetRarityColor(ERLRewardRarity InRarity)
	{
		switch (InRarity)
		{
		case ERLRewardRarity::Common:
			return FLinearColor(0.85f, 0.85f, 0.85f, 1.0f); // Slate White
		case ERLRewardRarity::Rare:
			return FLinearColor(0.18f, 0.58f, 1.0f, 1.0f);  // Bright Blue
		case ERLRewardRarity::Epic:
			return FLinearColor(0.70f, 0.25f, 0.95f, 1.0f); // Royal Violet
		case ERLRewardRarity::Legendary:
			return FLinearColor(1.0f, 0.72f, 0.10f, 1.0f);  // Radiant Gold
		default:
			return FLinearColor::White;
		}
	}

	FLinearColor GetRarityColor() const
	{
		return GetRarityColor(Rarity);
	}

	// 희귀도 표시 문자열
	static FText GetRarityDisplayName(ERLRewardRarity InRarity)
	{
		switch (InRarity)
		{
		case ERLRewardRarity::Common:
			return NSLOCTEXT("RLReward", "Common", "일반");
		case ERLRewardRarity::Rare:
			return NSLOCTEXT("RLReward", "Rare", "희귀");
		case ERLRewardRarity::Epic:
			return NSLOCTEXT("RLReward", "Epic", "영웅");
		case ERLRewardRarity::Legendary:
			return NSLOCTEXT("RLReward", "Legendary", "전설");
		default:
			return FText::GetEmpty();
		}
	}

	FText GetRarityDisplayName() const
	{
		return GetRarityDisplayName(Rarity);
	}
};
