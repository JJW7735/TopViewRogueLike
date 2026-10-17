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

public:
    /**
     * ★ 모든 피해의 단일 진입점. 공격자(DamageCauser)의 ASC에서 URLGE_Damage를 만들어 이 캐릭터에게 적용합니다.
     * 방어력/피해감소 경감은 URLDamageExecution이 처리합니다.
     * - ARLCharacter::ReceiveCustomDamage (스킬/근접/투사체/장판)
     * - ARLCharacter::TakeDamage (BP/엔진 ApplyDamage 경로, 기존처럼 경감 없음 = bTrueDamage)
     * @param RawDamage 공격자가 계산한 피해량 (스킬 계수/치명타 반영 완료)
     * @param bTrueDamage true면 방어력/피해감소 무시
     * @return 실제로 들어간 피해량 (경감 후, 체력 clamp 전). 이미 죽었거나 피해가 없으면 0
     */
    float ApplyIncomingDamage(float RawDamage, AActor* DamageCauser, bool bTrueDamage);

    /* 체력 변수들 (BeginPlay 이후로는 AttributeSet 미러) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float CurrentHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    bool bIsDead;

    /* 체력 변경 및 사망 신호 (블루프린트 체력바에서 이 신호를 받아서 씁니다) */
    // HealthDelta: 데미지 = 양수, 회복 = 음수, 최대치 변경/복원/최대치 감소로 인한 clamp = 0
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeathSignature OnDeath;

    // ★ 체력 회복 (흡혈 등). 죽은 상태거나 Amount가 0 이하면 무시합니다. 내부적으로 URLGE_Heal을 적용합니다.
    UFUNCTION(BlueprintCallable, Category = "Health")
    void Heal(float Amount);

    // ★ 런 동안 영구적인 최대 체력 증감 (보상 / 상점 / 메타 강화). Instant GE로 기본값을 바꿉니다.
    // 패시브 아이템은 이 함수가 아니라 URLInventoryComponent가 Infinite GE로 처리합니다.
    // bApply: true면 늘어난 만큼 현재 체력도 채워주고, false면 현재 체력은 새 최대치를 넘지 않게 clamp만 합니다.
    UFUNCTION(BlueprintCallable, Category = "Health")
    void IncreaseMaxHealth(float Delta, bool bApply);

    // ★ 최대 체력이 다른 경로(아이템 GE 등)로 늘어났을 때, 늘어난 만큼 현재 체력을 채웁니다.
    // 회복 연출로 취급되지 않도록 HealthDelta 0으로 한 번만 방송합니다.
    void FillHealthForMaxHealthIncrease(float Amount);

    /** 아이템/버프를 뺀 최대 체력 기본값. 세이브에는 이 값을 저장합니다. */
    UFUNCTION(BlueprintPure, Category = "Health")
    float GetMaxHealthBase() const;

    /** 세이브 로드 시 체력 상태를 직접 안전하게 복원 (InMaxHealth = GetMaxHealthBase 기준 기본값, OnDeath는 방송하지 않음) */
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

    // 피해 GE 적용 중인지. 이 동안의 체력 감소만 "피해"로 방송하고(HealthDelta = 경감 후 피해량, 막타도 clamp 전 값),
    // 그 외의 체력 감소(최대 체력 감소로 인한 clamp)는 HealthDelta 0으로 방송합니다.
    bool bApplyingDamage = false;
};
