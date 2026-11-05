#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RLTypes.h"
#include "RLCombatFeedbackSubsystem.generated.h"

class ARLCharacter;
class URLSkillData;
class UFXSystemAsset;
class UMaterialInterface;
class USpringArmComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UDamageType;
class AController;

/**
 * 공격자가 방금 계산/발동한 스킬 데미지 컨텍스트.
 * RLCharacter.cpp / RLEnemyCharacter.cpp를 수정하지 않고도
 * 치명타 여부, 역경직(HitStop) 시간, 스킬 VFX/SFX 정보를 피격 시점에 정확히 연결합니다.
 */
USTRUCT()
struct FRLPendingDamageContext
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Attacker;
	TWeakObjectPtr<const URLSkillData> SkillData;
	float EvaluatedDamage = 0.0f;
	float HitStopDuration = 0.055f;
	bool bIsCritical = false;
	double Timestamp = 0.0;
};

/**
 * 액터별 역경직(Hit-Stop) 상태 추적
 */
struct FRLActiveHitStop
{
	TWeakObjectPtr<AActor> TargetActor;
	double EndRealTime = 0.0;
};

/**
 * 피격 시 메시 탄성 스케일 펀치(Squash & Stretch) 및 머티리얼 피격 플래시 상태
 */
struct FRLActiveMeshImpact
{
	TWeakObjectPtr<ACharacter> Character;
	FVector BaseScale = FVector::OneVector;
	FVector PunchScale = FVector::OneVector;
	float Elapsed = 0.0f;
	float Duration = 0.14f;
	bool bIsCritical = false;
	bool bAppliedOverlay = false;

	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
};

/**
 * 타격 지점 순간 발광(Impact Point Light Flash) 상태
 */
struct FRLActiveImpactFlashLight
{
	TWeakObjectPtr<UPointLightComponent> LightComp;
	float InitialIntensity = 45000.0f;
	float Elapsed = 0.0f;
	float Duration = 0.08f;
};

/**
 * 각 캐릭터의 OnTakeAnyDamage 이벤트를 서브시스템으로 중계하는 리스너
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLCombatFeedbackListener : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<ARLCharacter> WatchedCharacter;
	TWeakObjectPtr<class URLCombatFeedbackSubsystem> OwnerSubsystem;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleCharacterDeath();
};

/**
 * 타격감(Game Feel) 및 전투/스킬 이펙트(VFX) 통합 서브시스템.
 *
 * RLEnemyCharacter.cpp / RLCharacter.cpp와 충돌하지 않도록 월드 내 캐릭터들을 자동 구독하여:
 *  1) 역경직 (Hit-Stop: CustomTimeDilation 미세 정지 후 자동 복원)
 *  2) 카메라 쉐이크 & 줌 펀치 (어지럽지 않은 절제된 진동 + 치명타/처치 시 살짝 줌인)
 *  3) 피격 탄성 (Mesh Squash & Stretch) & 피격 발광 플래시 (오버레이/머티리얼 + 속성별 순간 라이트)
 *  4) 스킬 시전 이펙트(검기/대시/마법진/폭발) 및 적중 시 속성별 고유 타격 이펙트(화염/번개/비전/참격/강타) 자동 재생
 * 을 처리합니다.
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLCombatFeedbackSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static URLCombatFeedbackSubsystem* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** 스킬/투사체/장판이 데미지를 가하기 직전에 컨텍스트(치명타 여부, 스킬 데이터 등)를 기록합니다. */
	void PushDamageContext(AActor* Attacker, const FRLDamageData& DamageData, const URLSkillData* SkillData = nullptr);

	/** 장판 액터(ARLGroundAreaEffect 등)에 원본 스킬 및 데미지 컨텍스트를 바인딩합니다. */
	void RegisterDamageSourceActor(AActor* SourceActor, AActor* Attacker, const FRLDamageData& DamageData, const URLSkillData* SkillData = nullptr);

	/** 데미지 숫자 서브시스템(URLDamageNumberSubsystem)에서 이번 타격이 치명타인지 즉시 조회합니다. */
	bool IsCurrentHitCritical(AActor* DamageCauser = nullptr) const;

	/** 스킬 발동/베기/돌진/폭발 시점에 스킬 성격에 맞는 시전·검기·대시 이펙트를 자동 재생합니다. */
	void NotifySkillVFX(ARLCharacter* Caster, const URLSkillData* SkillData, const FVector& TargetLocation = FVector::ZeroVector, int32 ComboIndex = 0);

	/** 수동으로 카메라 펀치/쉐이크를 발생시킵니다 (스킬 시전 연출, 콘솔 테스트 등). */
	UFUNCTION(BlueprintCallable, Category = "CombatFeedback")
	void TriggerCameraFeedback(float ShakeAmplitude = 10.0f, float ShakeDuration = 0.10f, float ZoomPunchAmount = 14.0f);

	/** 수동으로 특정 액터에 역경직(Hit-Stop)을 부여합니다. */
	UFUNCTION(BlueprintCallable, Category = "CombatFeedback")
	void ApplyHitStop(AActor* TargetActor, float Duration, float TimeDilation = 0.03f);

	/** 전체 타격감 강도 배율 (콘솔 명령어 RL.HitFeelIntensity 로 실시간 조절 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatFeedback")
	float GlobalFeedbackIntensity = 1.0f;

	/** 자동 스킬/속성 타격 이펙트 활성화 여부 (콘솔 명령어 RL.AutoSkillVFX 0 또는 1 로 실시간 ON/OFF 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatFeedback")
	bool bEnableAutoSkillVFX = true;

	/** 리스너 및 데미지 서브시스템 전용 내부 콜백 */
	void NotifyCharacterDamaged(ARLCharacter* Victim, float Damage, AActor* DamageCauser);
	void NotifyCharacterDied(ARLCharacter* DeadCharacter);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureDefaultAssetsLoaded();
	void TryRegisterCharacter(AActor* Actor);
	void HandleActorSpawned(AActor* SpawnedActor);

	const FRLPendingDamageContext* FindMatchingContext(AActor* DamageCauser) const;
	UFXSystemAsset* ResolveHitVFXAndColor(const URLSkillData* SkillData, bool bIsCritical, FLinearColor& OutLightColor) const;
	void SpawnFXAtTransform(UFXSystemAsset* FXAsset, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, const FVector& Scale = FVector::OneVector);

	void TriggerMeshImpactAndFlash(ACharacter* Victim, const FVector& ImpactLocation, bool bIsCritical, const FLinearColor& ElementLightColor);
	void SpawnImpactFlashLight(const FVector& Location, bool bIsCritical, const FLinearColor& ElementLightColor);

	void UpdateHitStops(double CurrentRealTime);
	void UpdateMeshImpacts(float DeltaTime);
	void UpdateImpactFlashLights(float DeltaTime);
	void UpdateCameraFeedback(float DeltaTime);

	USpringArmComponent* FindPlayerCameraBoom();

	UPROPERTY()
	TArray<TObjectPtr<URLCombatFeedbackListener>> Listeners;

	// ==========================================
	// 속성/공격 타입별 타격 이펙트 (Hit VFX) 캐시
	// ==========================================
	UPROPERTY()
	TObjectPtr<UFXSystemAsset> DefaultNormalHitVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> DefaultCritHitVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> FireHitVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> FireCritExplosionVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> LightningHitVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> ArcaneNormalHitVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> ArcaneCritHitVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> HeavyBluntHitVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> HeavyShockwaveVFX = nullptr;

	// ==========================================
	// 스킬 시전/검기/대시 이펙트 (Skill Cast VFX) 캐시
	// ==========================================
	UPROPERTY()
	TObjectPtr<UFXSystemAsset> BladeSlashRedVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> MagicSlash1VFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> MagicSlash2VFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> DashTrailVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> DashDustVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> CastCircleVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> LightningAuraVFX = nullptr;

	UPROPERTY()
	TObjectPtr<UFXSystemAsset> FireAuraVFX = nullptr;

	/** 피격 순간 메시 위에 잠깐 씌울 발광 오버레이 머티리얼 */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultHitOverlayMaterial = nullptr;

	bool bTriedLoadingDefaultAssets = false;

	/** 공격자(또는 소스 액터)별 최근 데미지 컨텍스트 */
	TMap<TWeakObjectPtr<AActor>, FRLPendingDamageContext> PendingContexts;

	/** 같은 프레임에 OnHealthChanged 경로와 OnTakeAnyDamage 경로가 중복 실행되지 않도록 방지 */
	TMap<TWeakObjectPtr<AActor>, uint64> LastProcessedHitFrameMap;

	TArray<FRLActiveHitStop> ActiveHitStops;
	TArray<FRLActiveMeshImpact> ActiveMeshImpacts;
	TArray<FRLActiveImpactFlashLight> ActiveFlashLights;

	/** 캐릭터별 원본 메시 스케일 캐시 (연타를 맞아도 원래 크기를 잃어버리지 않도록) */
	TMap<TWeakObjectPtr<ACharacter>, FVector> CachedBaseMeshScales;

	/** 카메라 쉐이크 & 줌 펀치 상태 */
	TWeakObjectPtr<USpringArmComponent> CachedCameraBoom;
	float BaseTargetArmLength = 1300.0f;
	FVector BaseSocketOffset = FVector::ZeroVector;
	bool bHasCachedCameraDefaults = false;

	float ShakeTimeRemaining = 0.0f;
	float ShakeTotalDuration = 0.0f;
	float CurrentShakeAmplitude = 0.0f;
	float ShakePhase = 0.0f;

	float CurrentZoomOffset = 0.0f;
	float TargetZoomPunch = 0.0f;

	FDelegateHandle ActorSpawnedHandle;
};
