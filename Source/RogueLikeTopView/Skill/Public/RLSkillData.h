#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RLTypes.h"
#include "RLSkillData.generated.h"

class UFXSystemAsset;
class USoundBase;
class UCameraShakeBase;
class URLGameplayAbility_Skill;

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

	// ★ 이 스킬을 실행할 GA 클래스. 비워두면 CastType 기본 GA(URLGA_Skill_*)를 사용합니다.
	// 특수한 스킬은 URLGameplayAbility_Skill을 상속한 BP/C++ GA를 만들어 여기에 지정하세요.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Mechanic")
	TSubclassOf<URLGameplayAbility_Skill> AbilityClassOverride;

	TSubclassOf<URLGameplayAbility_Skill> GetAbilityClass() const;

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

	// ★ 보스 균열 강화판 패턴 중 올바른 타이밍/구역에서 맞히면 영혼의 균열(Soul Crack) 스택을 쌓는 스킬인지.
	// CastType과 무관한 독립 플래그라 기존 스킬 아무거나 지정 가능합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Combat")
	bool bCanTriggerSoulCrack = false;

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
	// 시각 효과 & 사운드 & 타격감 (VFX / SFX / Feel)
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect")
	UFXSystemAsset* ChargeVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect")
	UFXSystemAsset* ReleaseVFX;

	/** 적에게 적중했을 때 타격 위치에 스폰할 피격 이펙트 (나이아가라/캐스케이드 공용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect")
	TObjectPtr<UFXSystemAsset> HitVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect")
	bool bAttachToWeapon = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Effect", meta = (EditCondition = "bAttachToWeapon"))
	FName WeaponSocketName = TEXT("HammerHeadSocket");

	/** 스킬 시전 순간 재생할 효과음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Sound")
	TObjectPtr<USoundBase> CastSound = nullptr;

	/** 스킬이 적에게 적중했을 때 재생할 타격 효과음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Sound")
	TObjectPtr<USoundBase> HitSound = nullptr;

	/** 스킬 적중 시 카메라 흔들림/줌 펀치 강도 배율 (1.0 = 기본, 0 = 흔들림 없음, 궁극기 등은 1.5~2.5 추천) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Feel", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float CameraShakeScale = 1.0f;

	/** 선택적 카메라 쉐이크 에셋 (비워두어도 C++ 프로시저럴 카메라 펀치/쉐이크가 자동 작동합니다) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Feel")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;
};
