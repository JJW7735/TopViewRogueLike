#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLHealthComponent.generated.h"

// [방송국 개설] 체력이 변하거나 죽었을 때 UI나 애니메이션 쪽에 신호를 보내기 위한 선언입니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChangedSignature, float, CurrentHealth, float, MaxHealth, float, HealthDelta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathSignature);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URLHealthComponent();

protected:
    virtual void BeginPlay() override;

    // 팀원이 만든 히트박스에서 데미지가 들어왔을 때 자동으로 실행될 방어 함수입니다.
    UFUNCTION()
    void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

public:
    /* 체력 변수들 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Health")
    float CurrentHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Health")
    bool bIsDead;

    /* 체력 변경 및 사망 신호 (블루프린트 체력바에서 이 신호를 받아서 씁니다) */
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeathSignature OnDeath;

    // ★ 체력 회복 (흡혈 등). 죽은 상태거나 Amount가 0 이하면 무시합니다.
    // 내부에서 0~MaxHealth로 Clamp하고, 실제로 회복된 양만큼 OnHealthChanged를 방송합니다.
    // 주의: HealthDelta를 "음수"로 방송합니다 (데미지=양수, 회복=음수로 구분하기 위함).
    // 기존에 OnHealthChanged를 듣고 있는 BP가 있다면 델타 부호를 참고하도록 확인이 필요할 수 있습니다.
    UFUNCTION(BlueprintCallable, Category = "Health")
    void Heal(float Amount);

    // ★ 최대 체력 증감 (패시브 아이템 장착/해제, 영구 보상 등에서 사용).
    // Delta: MaxHealth에 더할 값 (장착/획득 시 양수, 해제 시 음수로 호출 측에서 부호를 맞춰서 전달합니다).
    // bApply: true면 "영구 증가/장착 적용" 의미로, 늘어난 MaxHealth만큼 CurrentHealth도 함께 채워줍니다
    //         (예: 영구 최대체력 보상 획득 시 그만큼 바로 회복). false면 "해제/회수" 의미로, CurrentHealth는
    //         추가로 깎지 않고 줄어든 새 MaxHealth를 넘지 않도록 clamp만 합니다 (장비 해제가 곧바로 피해로
    //         취급되지 않도록 하기 위함).
    // 주의: 이 함수는 RLInventoryComponent::ApplyPassiveItemBonuses, RLRewardSubsystem의 호출부 기록을
    // 바탕으로 복원한 구현입니다. 원본 구현과 100% 동일하지 않을 수 있으니, 이상 동작이 보이면 git 이력/
    // Visual Studio 로컬 히스토리로 원본과 대조해 주세요.
    UFUNCTION(BlueprintCallable, Category = "Health")
    void IncreaseMaxHealth(float Delta, bool bApply);

    /** 세이브 로드 시 체력 상태를 직접 안전하게 복원 */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void RestoreHealthDirect(float InCurrentHealth, float InMaxHealth);
};