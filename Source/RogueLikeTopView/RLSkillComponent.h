#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLSkillData.h"
#include "TimerManager.h"
#include "RLSkillComponent.generated.h"

class ARLCharacter;
class ARLDecalIndicator;
class UParticleSystemComponent;
class UFXSystemComponent;

// ========================================================
// 델리게이트 선언 (UI 연동용)
// ========================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillCooldownChanged, ESkillSlot, Slot, float, RemainingTime, float, MaxCooldown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnCastProgressChanged, ESkillSlot, Slot, float, CurrentTime, float, MaxDuration, float, ProgressRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCastStateChanged, ESkillSlot, Slot, bool, bIsCasting);

// ========================================================
// 스킬 상태 Enum
// ========================================================
UENUM(BlueprintType)
enum class EPlayerSkillState : uint8
{
	Idle,
	Casting,      // 캐스팅 게이지 채우는 중 (100% 도달 시 발동)
	Charging,     // 차징 힘 모으는 중
	Holding,      // 홀딩 유지 중
	Aiming        // 장판 위치 조준 중
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URLSkillComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================
	// 입력 핸들링
	// ========================================================
	void OnSkillInputStarted(ESkillSlot Slot);
	void OnSkillInputTriggered(ESkillSlot Slot);
	void OnSkillInputCompleted(ESkillSlot Slot);

	// ========================================================
	// 스킬 발동 제어 및 애니메이션 노티파이
	// ========================================================
	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void ConfirmGroundCast();

	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void CancelGroundCast();

	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void TriggerActiveSkillHit();

	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void TriggerActiveSkillDash();

	void StartCasting(ESkillSlot Slot);
	void FinishCasting();
	void InterruptCasting(const FString& Reason = TEXT("Interrupted"));

	// ========================================================
	// 유틸리티 및 상태 Getter
	// ========================================================
	UFUNCTION(BlueprintCallable, Category = "Skill|Setup")
	void EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill);

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsOnCooldown(ESkillSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	float GetRemainingCooldown(ESkillSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsAiming() const { return CurrentSkillState == EPlayerSkillState::Aiming; }

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsCasting() const { return CurrentSkillState == EPlayerSkillState::Casting; }

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsCharging() const { return CurrentSkillState == EPlayerSkillState::Charging; }

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsHolding() const { return CurrentSkillState == EPlayerSkillState::Holding; }

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	EPlayerSkillState GetSkillState() const { return CurrentSkillState; }

	FRLDamageData CalculateFinalDamage(URLSkillData* Skill);

	// ========================================================
	// 블루프린트 이벤트 (UI 업데이트용)
	// ========================================================
	UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
	FOnSkillCooldownChanged OnSkillCooldownChanged;

	UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
	FOnCastProgressChanged OnCastProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
	FOnCastStateChanged OnCastStateChanged;

protected:
	virtual void BeginPlay() override;

	// ========================================================
	// 핵심 참조 및 데이터
	// ========================================================
	UPROPERTY()
	ARLCharacter* OwnerCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Data")
	TMap<ESkillSlot, URLSkillData*> EquippedSkills;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|State")
	TMap<ESkillSlot, float> CooldownEndTimes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|State")
	EPlayerSkillState CurrentSkillState = EPlayerSkillState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|State")
	ESkillSlot CurrentActiveSlot = ESkillSlot::Slot_Q;

	// ========================================================
	// 타이머 및 콤보 변수
	// ========================================================
	UPROPERTY()
	float CurrentAccumulatedTime = 0.0f;

	UPROPERTY()
	float LastHoldTickTime = 0.0f;

	UPROPERTY()
	int32 ComboCount = 0;

	UPROPERTY()
	FTimerHandle ComboResetTimerHandle;

	UPROPERTY()
	FTimerHandle SpeedBuffTimerHandle;

	UPROPERTY()
	bool bHasTriggeredPreCastMotion = false;

	// ========================================================
	// VFX 및 시각 효과 참조
	// ========================================================
	UPROPERTY()
	FVector PendingExplosionLocation = FVector::ZeroVector;

	UPROPERTY()
	UFXSystemComponent* ActiveCastParticleComp = nullptr;

	UPROPERTY()
	UFXSystemComponent* ActiveBuffVFXComp = nullptr;

	UPROPERTY()
	ARLDecalIndicator* DecalIndicator;

	// ========================================================
	// 내부 헬퍼 함수
	// ========================================================
	void StartGroundTargetAiming(ESkillSlot Slot);
	void StopPlayerMovement();
	void RotateOwnerToMouseCursor();
	FVector GetMouseGroundLocation() const;

	UFUNCTION()
	void EndSpeedBuff();

	UFUNCTION()
	void ResetCombo();

	UFXSystemComponent* SpawnSkillVFX(UFXSystemAsset* VFXAsset, FVector Location, bool bAttach, FName SocketName);
};