#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLSkillData.h"
#include "RLSkillComponent.generated.h"

class ARLCharacter;

// 쿨타임 변경 시 UI에 알리는 델리게이트 (팀원의 UI와 연동)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillCooldownChanged, ESkillSlot, Slot, float, RemainingTime, float, TotalCooldown);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLSkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URLSkillComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 스킬 시전 시도 (Q, W, E, R 키 입력 시 호출)
    UFUNCTION(BlueprintCallable, Category = "Skill")
    bool TryCastSkill(ESkillSlot Slot);

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

private:
    UPROPERTY()
    ARLCharacter* OwnerCharacter;
};