#include "RLHealthComponent.h"
#include "GameFramework/Actor.h"

// 생성자: 기본 체력을 100으로 세팅합니다.
URLHealthComponent::URLHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // 체력 시스템은 매 프레임 돌아갈 필요가 없으므로 끕니다(최적화).

    MaxHealth = 100.0f;
    CurrentHealth = MaxHealth;
    bIsDead = false;
}

void URLHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;

    // [협업의 핵심] 나(컴포넌트)를 껴입고 있는 주인(캐릭터)을 찾습니다.
    AActor* MyOwner = GetOwner();
    if (MyOwner)
    {
        // 주인이 언리얼 기본 데미지를 입으면, 내가 만든 HandleTakeAnyDamage 함수가 자동으로 대신 맞게끔 연결합니다.
        MyOwner->OnTakeAnyDamage.AddDynamic(this, &URLHealthComponent::HandleTakeAnyDamage);
    }
}

// 데미지를 입었을 때 실행되는 핵심 로직
void URLHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
    // 데미지가 없거나 이미 죽었다면 무시합니다.
    if (Damage <= 0.0f || bIsDead)
    {
        return;
    }

    // 체력을 깎되, 0 이하로 내려가거나 최대 체력을 넘지 못하게 꽉 묶어줍니다(Clamp).
    CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);

    // 체력이 변했다고 블루프린트 체력바에 방송(Broadcast)을 보냅니다.
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, Damage);

    // 체력이 0이 되면 사망 처리를 합니다.
    if (CurrentHealth <= 0.0f)
    {
        bIsDead = true;
        OnDeath.Broadcast(); // 사망 방송을 보냅니다.
    }
}

// ★ 체력 회복 (흡혈 등에서 사용). HandleTakeAnyDamage와 대칭되는 형태로 작성했습니다.
void URLHealthComponent::Heal(float Amount)
{
    // 회복량이 없거나 이미 죽었다면 무시합니다 (죽은 뒤에 흡혈로 살아나는 걸 방지).
    if (Amount <= 0.0f || bIsDead)
    {
        return;
    }

    const float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);

    const float ActualHealed = CurrentHealth - OldHealth;
    if (ActualHealed > 0.0f)
    {
        // 데미지 방송(HealthDelta 양수)과 구분하기 위해 회복은 음수로 방송합니다.
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, -ActualHealed);
    }
}

// ★ 최대 체력 증감 (패시브 아이템 장착/해제, 영구 보상 등). 헤더 주석 참고.
void URLHealthComponent::IncreaseMaxHealth(float Delta, bool bApply)
{
    if (FMath::IsNearlyZero(Delta))
    {
        return;
    }

    // 최대 체력이 0 이하로 내려가지 않도록 최소 1은 보장합니다.
    MaxHealth = FMath::Max(1.0f, MaxHealth + Delta);

    if (bApply)
    {
        // 장착/획득으로 최대 체력이 늘어난 경우, 늘어난 만큼 현재 체력도 함께 채워줍니다.
        CurrentHealth = FMath::Clamp(CurrentHealth + Delta, 0.0f, MaxHealth);
    }
    else
    {
        // 해제로 최대 체력이 줄어든 경우, 현재 체력을 직접 깎지 않고 새 최대치를 넘지 않게만 clamp합니다.
        CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
    }

    // 데미지/회복이 아닌 최대치 변경 이벤트이므로 HealthDelta는 0으로 방송합니다 (체력바 갱신용).
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.0f);
}