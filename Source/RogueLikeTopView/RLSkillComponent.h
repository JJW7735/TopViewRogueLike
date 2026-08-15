#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLSkillData.h"
#include "RLSkillComponent.generated.h"

class ARLCharacter;

// 쿨타임 변경 시 UI에 알리는 델리게이트 (팀원의 UI와 연동)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillCooldownChanged, ESkillSlot, Slot, float, RemainingTime, float, TotalCooldown);

// 플레이어의 현재 스킬 상태
UENUM(BlueprintType)
enum class EPlayerSkillState : uint8
{
    Idle,
    Casting,
    Charging,
    Holding
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLSkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URLSkillComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // [수정] 기존 TryCastSkill을 3개의 상태로 완벽 분리
    UFUNCTION(BlueprintCallable, Category = "Skill|Input")
    void OnSkillInputStarted(ESkillSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Skill|Input")
    void OnSkillInputTriggered(ESkillSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Skill|Input")
    void OnSkillInputCompleted(ESkillSlot Slot);

    // 슬롯에 스킬 등록
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill);

    // 쿨타임 조회
    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsOnCooldown(ESkillSlot Slot) const;

    UFUNCTION(BlueprintPure, Category = "Skill")
    float GetRemainingCooldown(ESkillSlot Slot) const;

    // 마우스 커서 쪽으로 캐릭터를 즉시 회전
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void RotateOwnerToMouseCursor();

    UPROPERTY(BlueprintAssignable, Category = "Skill|Events")
    FOnSkillCooldownChanged OnSkillCooldownChanged;

protected:
    virtual void BeginPlay() override;

    // 슬롯별 장착된 스킬 데이터
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Equipped")
    TMap<ESkillSlot, URLSkillData*> EquippedSkills;

    // 슬롯별 쿨타임 종료 시각 기록 (GetWorld()->GetTimeSeconds() 기준)
    UPROPERTY()
    TMap<ESkillSlot, float> CooldownEndTimes;

    // [추가] 상태 추적 변수들
    UPROPERTY(BlueprintReadOnly, Category = "Skill|State")
    EPlayerSkillState CurrentSkillState = EPlayerSkillState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Skill|State")
    float CurrentAccumulatedTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Skill|State")
    ESkillSlot CurrentActiveSlot;

private:
    UPROPERTY()
    ARLCharacter* OwnerCharacter;
};