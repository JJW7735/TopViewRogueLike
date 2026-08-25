#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLSkillData.h"
#include "TimerManager.h"
#include "RLSkillComponent.generated.h"

class ARLCharacter;
class ARLDecalIndicator;
class UParticleSystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillCooldownChanged, ESkillSlot, Slot, float, RemainingTime, float, MaxCooldown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnCastProgressChanged, ESkillSlot, Slot, float, CurrentTime, float, MaxDuration, float, ProgressRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCastStateChanged, ESkillSlot, Slot, bool, bIsCasting);

UENUM(BlueprintType)
enum class EPlayerSkillState : uint8
{
    Idle,
    Casting,      // 캐스팅 게이지 채우는 중 (100% 도달 시에만 발동)
    Charging,     // 차징 힘 모으는 중
    Holding,      // 홀딩 유지 중
    Aiming        // 장판 위치 조준 중 (이동 가능)
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLSkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URLSkillComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void OnSkillInputStarted(ESkillSlot Slot);
    void OnSkillInputTriggered(ESkillSlot Slot);
    void OnSkillInputCompleted(ESkillSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void ConfirmGroundCast();

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void CancelGroundCast();

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void TriggerActiveSkillHit();

    void StartCasting(ESkillSlot Slot);
    void FinishCasting();
    void InterruptCasting(const FString& Reason = TEXT("Interrupted"));

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill);

    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsOnCooldown(ESkillSlot Slot) const;

    UFUNCTION(BlueprintPure, Category = "Skill")
    float GetRemainingCooldown(ESkillSlot Slot) const;

    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsAiming() const { return CurrentSkillState == EPlayerSkillState::Aiming; }

    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsCasting() const { return CurrentSkillState == EPlayerSkillState::Casting; }

    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsCharging() const { return CurrentSkillState == EPlayerSkillState::Charging; }

    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsHolding() const { return CurrentSkillState == EPlayerSkillState::Holding; }

    UFUNCTION(BlueprintPure, Category = "Skill")
    EPlayerSkillState GetSkillState() const { return CurrentSkillState; }

    UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
    FOnSkillCooldownChanged OnSkillCooldownChanged;

    UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
    FOnCastProgressChanged OnCastProgressChanged;

    UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
    FOnCastStateChanged OnCastStateChanged;

protected:
    virtual void BeginPlay() override;

    UPROPERTY()
    ARLCharacter* OwnerCharacter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    TMap<ESkillSlot, URLSkillData*> EquippedSkills;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    TMap<ESkillSlot, float> CooldownEndTimes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    EPlayerSkillState CurrentSkillState = EPlayerSkillState::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    ESkillSlot CurrentActiveSlot = ESkillSlot::Slot_Q;

    UPROPERTY()
    float CurrentAccumulatedTime = 0.0f;

    UPROPERTY()
    float LastHoldTickTime = 0.0f;

    // 캐스팅 성공 시 폭발할 목표 지점 좌표
    UPROPERTY()
    FVector PendingExplosionLocation = FVector::ZeroVector;

    // 캐스팅 중 바닥에 유지되는 지속 번개 파티클 컴포넌트
    UPROPERTY()
    UParticleSystemComponent* ActiveCastParticleComp = nullptr;

    UPROPERTY()
    ARLDecalIndicator* DecalIndicator;

    void StartGroundTargetAiming(ESkillSlot Slot);
    void StopPlayerMovement();
    void RotateOwnerToMouseCursor();
    FVector GetMouseGroundLocation() const;

    // ===== 이속 버프 전용 =====
    UPROPERTY()
    FTimerHandle SpeedBuffTimerHandle;

    UFUNCTION()
    void EndSpeedBuff();
};