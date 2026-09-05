#pragma once

#include "CoreMinimal.h"
#include "RLTypes.generated.h"

class UAnimMontage;

// 공격 판정 및 데미지 정보를 담는 공용 구조체
USTRUCT(BlueprintType)
struct FRLDamageData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float KnockbackForce = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float HitStopDuration = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIgnoreInvincibility = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	AActor* DamageCauser = nullptr;
};

// ★ 몬스터 개별 공격 패턴 데이터 구조체
USTRUCT(BlueprintType)
struct FRLMonsterPatternData
{
	GENERATED_BODY()

	// 패턴 식별 이름 (예: "Slash", "Rush", "JumpSmash")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	FName PatternID = NAME_None;

	// 공격 모션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 공격 유효 사거리 (이 거리 이내여야 패턴 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	float AttackRange = 250.0f;

	// 패턴 재사용 대기 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	float Cooldown = 3.0f;

	// 패턴 사용 확률 가중치 (기본 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	float SelectionWeight = 1.0f;

	// 이 패턴 실행 시 적용할 데미지 정보
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	FRLDamageData DamageData;
};