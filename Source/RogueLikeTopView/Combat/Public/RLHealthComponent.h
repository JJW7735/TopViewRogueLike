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

    /** 체력 회복 함수 */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void Heal(float HealAmount);

    /** 최대 체력 영구 증가 함수 (선택적으로 현재 체력도 증가) */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void IncreaseMaxHealth(float Amount, bool bHealCurrent = true);
};