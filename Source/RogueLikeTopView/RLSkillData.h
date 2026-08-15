#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLTypes.h"
#include "RLSkillData.generated.h"

class ARLProjectile;

UENUM(BlueprintType)
enum class ESkillSlot : uint8
{
    Slot_Q UMETA(DisplayName = "Q Skill"),
    Slot_W UMETA(DisplayName = "W Skill"),
    Slot_E UMETA(DisplayName = "E Skill"),
    Slot_R UMETA(DisplayName = "R Skill")
};
UENUM(BlueprintType)
enum class ESkillCastType : uint8
{
    Instant       UMETA(DisplayName = "Instant"),
    Cast          UMETA(DisplayName = "Casting"),
    Charge        UMETA(DisplayName = "Charge"),
    GroundTarget  UMETA(DisplayName = "GroundTarget"),
    Projectile    UMETA(DisplayName = "Projectile")
    Hold          UMETA(DisplayName = "Holding"),
    GroundTarget  UMETA(DisplayName = "GroundTarget")
};
UCLASS(BlueprintType)
class ROGUELIKETOPVIEW_API URLSkillData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Basic")
    FName SkillName = TEXT("DefaultSkill");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Basic")
    FString Description = TEXT("Skill Description");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Basic")
    UTexture2D* SkillIcon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic")
    ESkillCastType CastType = ESkillCastType::Instant;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic", meta = (ClampMin = "0.0"))
    float Cooldown = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic", meta = (EditCondition = "CastType == ESkillCastType::Cast || CastType == ESkillCastType::Charge"))
    float CastDuration = 1.5f;


    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic", meta = (EditCondition = "CastType == ESkillCastType::Cast || CastType == ESkillCastType::Charge || CastType == ESkillCastType::Hold"))
    float CastDuration = 1.5f;

    // 팀원 차징 & 홀딩 옵션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic|ChargeAndHold")
    float MinChargeTime = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic|ChargeAndHold")
    float MaxDamageMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic|ChargeAndHold")
    float TickDamageInterval = 0.2f;

    // ─────────────────────────────────────────────
    // ★ 장판 조준(GroundTarget) 인디케이터 옵션
    // ─────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|GroundTarget", meta = (EditCondition = "CastType == ESkillCastType::GroundTarget"))
    float MaxRange = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|GroundTarget", meta = (EditCondition = "CastType == ESkillCastType::GroundTarget"))
    float IndicatorRadius = 250.0f;


    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Animation")
    UAnimMontage* SkillMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combat")
    FRLDamageData DamageData;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Hitbox")
    FVector HitboxExtent = FVector(120.f, 80.f, 60.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Hitbox")
    float HitboxForwardOffset = 150.f;

    // ===== Projectile 전용 =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Projectile",
        meta = (EditCondition = "CastType == ESkillCastType::Projectile"))
    TSubclassOf<ARLProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Projectile",
        meta = (EditCondition = "CastType == ESkillCastType::Projectile"))
    float ProjectileSpeed = 1500.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Projectile",
        meta = (EditCondition = "CastType == ESkillCastType::Projectile"))
    float ProjectileSpawnForwardOffset = 60.f;
};