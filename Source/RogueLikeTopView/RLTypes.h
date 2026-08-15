#pragma once

#include "CoreMinimal.h"
#include "RLTypes.generated.h"

// 공격 판정 및 데미지 정보를 담는 공용 구조체
USTRUCT(BlueprintType)
struct FRLDamageData
{
    GENERATED_BODY()

    // 기본 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float BaseDamage = 15.0f;

    // 넉백 파워 (0이면 넉백 없음, 500~800 적당)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float KnockbackForce = 600.0f;

    // 타격 시 역경직 시간 (초 단위)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float HitStopDuration = 0.05f;

    // 무적 상태(회피 중) 무시 여부 (특수 패턴용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    bool bIgnoreInvincibility = false;

    // 공격자 액터 포인터
    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    AActor* DamageCauser = nullptr;
};