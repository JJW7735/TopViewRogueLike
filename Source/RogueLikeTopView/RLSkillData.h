#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLTypes.h"
#include "RLSkillData.generated.h"

class UFXSystemAsset;

UENUM(BlueprintType)
enum class ESkillSlot : uint8
{
	Slot_LMB UMETA(DisplayName = "Left Mouse Button (Basic Attack)"),
	Slot_Q   UMETA(DisplayName = "Q Skill"),
	Slot_W   UMETA(DisplayName = "W Skill"),
	Slot_E   UMETA(DisplayName = "E Skill"),
	Slot_R   UMETA(DisplayName = "R Skill")
};

UENUM(BlueprintType)
enum class ESkillCastType : uint8
{
	Instant      UMETA(DisplayName = "Instant"),
	Cast         UMETA(DisplayName = "Casting"),
	Charge       UMETA(DisplayName = "Charge"),
	Hold         UMETA(DisplayName = "Holding"),
	GroundTarget UMETA(DisplayName = "GroundTarget"),
	Projectile   UMETA(DisplayName = "Projectile"),
	Dash         UMETA(DisplayName = "Dash"),
	Buff         UMETA(DisplayName = "Buff"),
	ChargeDash   UMETA(DisplayName = "ChargeDash")
};

UCLASS(BlueprintType)
class ROGUELIKETOPVIEW_API URLSkillData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==========================================
	// 기본 정보 (Basic Info)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Basic")
	FName SkillName = TEXT("DefaultSkill");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Basic")
	FString Description = TEXT("Skill Description");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Basic")
	UTexture2D* SkillIcon;

	// ==========================================
	// 메카닉 & 쿨타임 (Mechanics)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic")
	ESkillCastType CastType = ESkillCastType::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic", meta = (EditCondition = "CastType == ESkillCastType::Instant"))
	bool bHitOnAnimNotify = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic", meta = (ClampMin = "0.0"))
	float Cooldown = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic", meta = (EditCondition = "CastType == ESkillCastType::Cast || CastType == ESkillCastType::Charge || CastType == ESkillCastType::Hold || CastType == ESkillCastType::GroundTarget || CastType == ESkillCastType::ChargeDash"))
	float CastDuration = 1.5f;

	// ==========================================
	// 데미지 & 스탯 계수 (Damage & Scaling)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combat")
	FRLDamageData DamageData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat")
	float ADScaling = 1.0f; // 물리 데미지 계수

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Combat")
	float APScaling = 0.0f; // 마법 데미지 계수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Hitbox")
	FVector HitboxExtent = FVector(120.f, 80.f, 60.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Hitbox")
	float HitboxForwardOffset = 150.f;

	// ==========================================
	// 애니메이션 & 콤보 (Animation & Combo)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Animation")
	UAnimMontage* SkillMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Animation")
	bool bUseCombo = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Animation", meta = (EditCondition = "bUseCombo"))
	TArray<UAnimMontage*> ComboMontages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Animation", meta = (EditCondition = "bUseCombo", ClampMin = "0.1"))
	float ComboResetTime = 1.0f;

	// ==========================================
	// 차지 & 홀드 옵션 (Charge & Hold)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|ChargeAndHold", meta = (EditCondition = "CastType == ESkillCastType::Charge || CastType == ESkillCastType::Hold || CastType == ESkillCastType::ChargeDash"))
	float MinChargeTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|ChargeAndHold", meta = (EditCondition = "CastType == ESkillCastType::Charge || CastType == ESkillCastType::ChargeDash"))
	float MaxDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|ChargeAndHold", meta = (EditCondition = "CastType == ESkillCastType::Hold"))
	float TickDamageInterval = 0.2f;

	// ==========================================
	// 투사체 옵션 (Projectile)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Projectile", meta = (EditCondition = "CastType == ESkillCastType::Projectile"))
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Projectile", meta = (EditCondition = "CastType == ESkillCastType::Projectile"))
	float ProjectileSpawnForwardOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Projectile", meta = (EditCondition = "CastType == ESkillCastType::Projectile"))
	float ProjectileSpeed = 1500.0f;

	// ==========================================
	// 장판 옵션 (Ground Target)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|GroundTarget", meta = (EditCondition = "CastType == ESkillCastType::GroundTarget"))
	float MaxRange = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|GroundTarget", meta = (EditCondition = "CastType == ESkillCastType::GroundTarget"))
	float IndicatorRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|GroundTarget")
	TSubclassOf<AActor> GroundActorClass;

	// ==========================================
	// 대쉬 옵션 (Dash)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Dash", meta = (EditCondition = "CastType == ESkillCastType::Dash || CastType == ESkillCastType::ChargeDash"))
	float DashPower = 1500.f;

	// ==========================================
	// 버프 옵션 (Buff)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Buff", meta = (EditCondition = "CastType == ESkillCastType::Buff"))
	float SpeedMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Buff", meta = (EditCondition = "CastType == ESkillCastType::Buff"))
	float BuffDuration = 3.0f;

	// ==========================================
	// 시각 효과 (VFX)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect")
	UFXSystemAsset* ChargeVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect")
	UFXSystemAsset* ReleaseVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect")
	bool bAttachToWeapon = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect", meta = (EditCondition = "bAttachToWeapon"))
	FName WeaponSocketName = TEXT("HammerHeadSocket");
};