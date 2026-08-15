#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLTypes.h"
#include "RLSkillData.generated.h"

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Animation")
    UAnimMontage* SkillMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combat")
    FRLDamageData DamageData;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Hitbox")
    FVector HitboxExtent = FVector(120.f, 80.f, 60.f);

    // ★ HitForwardOffset -> HitboxForwardOffset 으로 수정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Hitbox")
    float HitboxForwardOffset = 150.f;
};