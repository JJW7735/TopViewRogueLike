#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLSkillData.h"
#include "RLSkillComponent.generated.h"

class ARLCharacter;
class ARLDecalIndicator;

// 쿨타임 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillCooldownChanged, ESkillSlot, Slot, float, RemainingTime, float, TotalCooldown);

// 스킬 상태 (팀원 상태 + Aiming 통합)
UENUM(BlueprintType)
enum class EPlayerSkillState : uint8
{
    Idle,
    Casting,
    Charging,
    Holding,
    Aiming
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLSkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URLSkillComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 입력 처리 함수군 (Enhanced Input 바인딩용)
    UFUNCTION(BlueprintCallable, Category = "Skill|Input")
    void OnSkillInputStarted(ESkillSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Skill|Input")
    void OnSkillInputTriggered(ESkillSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Skill|Input")
    void OnSkillInputCompleted(ESkillSlot Slot);

    // 단일 시전 호환용
    UFUNCTION(BlueprintCallable, Category = "Skill")
    bool TryCastSkill(ESkillSlot Slot);

    // ─────────────────────────────────────────────
    // ★ 장판 조준(GroundTarget) 제어 함수
    // ─────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "Skill|Indicator")
    void ConfirmGroundCast();

    UFUNCTION(BlueprintCallable, Category = "Skill|Indicator")
    void CancelGroundCast();

    UFUNCTION(BlueprintPure, Category = "Skill|Indicator")
    bool IsAiming() const { return CurrentSkillState == EPlayerSkillState::Aiming; }

    // 유틸리티 및 쿨타임 조회
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill);

    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsOnCooldown(ESkillSlot Slot) const;

    UFUNCTION(BlueprintPure, Category = "Skill")
    float GetRemainingCooldown(ESkillSlot Slot) const;

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void RotateOwnerToMouseCursor();

    UFUNCTION(BlueprintPure, Category = "Skill")
    EPlayerSkillState GetCurrentSkillState() const { return CurrentSkillState; }

    UPROPERTY(BlueprintAssignable, Category = "Skill|Events")
    FOnSkillCooldownChanged OnSkillCooldownChanged;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Equipped")
    TMap<ESkillSlot, URLSkillData*> EquippedSkills;

    UPROPERTY()
    TMap<ESkillSlot, float> CooldownEndTimes;

    UPROPERTY(BlueprintReadOnly, Category = "Skill|State")
    EPlayerSkillState CurrentSkillState = EPlayerSkillState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Skill|State")
    float CurrentAccumulatedTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Skill|State")
    ESkillSlot CurrentActiveSlot;

    // 바닥 데칼 인디케이터 액터
    UPROPERTY()
    ARLDecalIndicator* DecalIndicator;

private:
    UPROPERTY()
    ARLCharacter* OwnerCharacter;

    void StartGroundTargetAiming(ESkillSlot Slot);
    FVector GetMouseGroundLocation() const;
};