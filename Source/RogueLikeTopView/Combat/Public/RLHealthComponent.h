#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLHealthComponent.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

// [방송국 개설] 체력이 변하거나 죽었을 때 UI나 애니메이션 쪽에 신호를 보내기 위한 선언입니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChangedSignature, float, CurrentHealth, float, MaxHealth, float, HealthDelta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathSignature);

// ★ GAS Facade: 실제 체력은 Owner ASC의 URLAttributeSet(Health/MaxHealth)에 있고,
// 이 컴포넌트는 기존 API(함수/델리게이트/변수)를 그대로 유지한 채 GAS로 위임하는 얇은 래퍼입니다. (Lyra의 LyraHealthComponent와 같은 역할)
// 덕분에 체력바 위젯, BP_EnemyBase, 보스 BTT 등 기존 블루프린트는 수정 없이 동작합니다.
//
// - MaxHealth: BP 클래스 디폴트에서 정하는 "시작 최대 체력". BeginPlay에서 AttributeSet으로 들어간 뒤로는 표시용 미러입니다.
// - CurrentHealth / bIsDead: AttributeSet 값을 그대로 따라가는 읽기 전용 미러입니다.
//   값을 바꾸려면 반드시 Heal / IncreaseMaxHealth / RestoreHealthDirect 또는 GE를 사용하세요.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URLHealthComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 팀원이 만든 히트박스에서 데미지가 들어왔을 때 자동으로 실행될 방어 함수입니다.
    // 내부적으로 URLGE_Damage를 적용합니다.
    UFUNCTION()
    void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

public:
    /* 체력 변수들 (BeginPlay 이후로는 AttributeSet 미러) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float CurrentHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    bool bIsDead;

    /* 체력 변경 및 사망 신호 (블루프린트 체력바에서 이 신호를 받아서 씁니다) */
    // HealthDelta: 데미지 = 양수, 회복 = 음수, 최대치 변경/복원 = 0
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeathSignature OnDeath;

    // ★ 체력 회복 (흡혈 등). 죽은 상태거나 Amount가 0 이하면 무시합니다. 내부적으로 URLGE_Heal을 적용합니다.
    UFUNCTION(BlueprintCallable, Category = "Health")
    void Heal(float Amount);

    // ★ 최대 체력 증감 (패시브 아이템 장착/해제, 영구 보상 등에서 사용).
    // bApply: true면 늘어난 만큼 현재 체력도 채워주고, false면 현재 체력은 새 최대치를 넘지 않게 clamp만 합니다
    //         (장비 해제가 곧바로 피해로 취급되지 않도록 하기 위함).
    // TODO(2단계): Infinite GE(핸들로 해제)로 교체 예정. 지금은 AttributeSet 기본값(Base)을 직접 바꿉니다.
    UFUNCTION(BlueprintCallable, Category = "Health")
    void IncreaseMaxHealth(float Delta, bool bApply);

    /** 세이브 로드 시 체력 상태를 직접 안전하게 복원 (OnDeath는 방송하지 않음) */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void RestoreHealthDirect(float InCurrentHealth, float InMaxHealth);

private:
    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComp;

    // 어트리뷰트 변경 콜백 (ASC → 미러 갱신 → 기존 델리게이트 방송)
    void HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
    void HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);

    void SyncFromAttributes();
    void HandleOutOfHealth();

    // 여러 어트리뷰트를 한 번에 바꿀 때 중간 방송을 막고 마지막에 한 번만 방송하기 위한 플래그
    bool bSuppressBroadcast = false;

    // 기존 동작 유지: 데미지 방송의 HealthDelta는 실제 깎인 양이 아니라 "들어온 피해량"입니다 (막타 데미지 숫자 표시용).
    float PendingDamageForBroadcast = 0.0f;
};
