#include "RLHealthComponent.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "RLAttributeSet.h"
#include "RLGameplayEffects.h"
#include "RLGameplayTags.h"

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

    AActor* MyOwner = GetOwner();
    if (!MyOwner)
    {
        return;
    }

    // ARLCharacter::BeginPlay가 Super 호출 전에 InitAbilityActorInfo를 끝내두므로 여기서는 바로 쓸 수 있습니다.
    AbilitySystemComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MyOwner);
    if (!AbilitySystemComp)
    {
        UE_LOG(LogTemp, Error, TEXT("[RLHealthComponent] %s에 AbilitySystemComponent가 없습니다. ARLCharacter 파생 클래스에서만 사용하세요."), *MyOwner->GetName());
        return;
    }

    // BP 클래스 디폴트에서 정한 MaxHealth를 시작값으로 AttributeSet에 넣습니다.
    // (세이브 복원 / 영구 강화는 이보다 나중인 ARLCharacter::BeginPlay 후반부에서 적용됨)
    // 현재 체력은 아이템 보너스까지 반영된 최대치로 채웁니다 (시작 지급 아이템이 먼저 적용됐을 수도 있음).
    bSuppressBroadcast = true;
    AbilitySystemComp->SetNumericAttributeBase(URLAttributeSet::GetMaxHealthAttribute(), FMath::Max(1.0f, MaxHealth));
    AbilitySystemComp->SetNumericAttributeBase(URLAttributeSet::GetHealthAttribute(),
        AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetMaxHealthAttribute()));
    bSuppressBroadcast = false;

    AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(URLAttributeSet::GetHealthAttribute())
        .AddUObject(this, &URLHealthComponent::HandleHealthAttributeChanged);
    AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(URLAttributeSet::GetMaxHealthAttribute())
        .AddUObject(this, &URLHealthComponent::HandleMaxHealthAttributeChanged);

    SyncFromAttributes();
    bIsDead = false;

    // 엔진 기본 ApplyDamage(OnTakeAnyDamage)는 더 이상 직접 듣지 않습니다.
    // ARLCharacter::TakeDamage가 받아서 ApplyIncomingDamage로 넘겨줍니다 (피해 진입점 일원화).
}

void URLHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AbilitySystemComp)
    {
        AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(URLAttributeSet::GetHealthAttribute()).RemoveAll(this);
        AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(URLAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
}

void URLHealthComponent::SyncFromAttributes()
{
    if (!AbilitySystemComp)
    {
        return;
    }

    CurrentHealth = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetHealthAttribute());
    MaxHealth = AbilitySystemComp->GetNumericAttribute(URLAttributeSet::GetMaxHealthAttribute());
}

void URLHealthComponent::HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
    SyncFromAttributes();

    if (bSuppressBroadcast || FMath::IsNearlyEqual(ChangeData.OldValue, ChangeData.NewValue))
    {
        return;
    }

    // 데미지 = 양수, 회복 = 음수 (기존 규칙 유지)
    // 피해 GE 적용 중이 아닌데 체력이 줄었다면 최대 체력 감소(아이템 해제 등)로 인한 clamp이므로 0
    float HealthDelta = 0.0f;
    if (bApplyingDamage)
    {
        const URLAttributeSet* AttributeSet = AbilitySystemComp->GetSet<URLAttributeSet>();
        HealthDelta = AttributeSet ? AttributeSet->GetLastDamageTaken() : (ChangeData.OldValue - ChangeData.NewValue);
    }
    else if (ChangeData.NewValue > ChangeData.OldValue)
    {
        HealthDelta = ChangeData.OldValue - ChangeData.NewValue;
    }
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, HealthDelta);

    if (CurrentHealth <= 0.0f && !bIsDead)
    {
        HandleOutOfHealth();
    }
}

void URLHealthComponent::HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
    SyncFromAttributes();

    if (bSuppressBroadcast)
    {
        return;
    }

    // 최대치 변경 이벤트이므로 HealthDelta는 0 (체력바 갱신용)
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.0f);
}

void URLHealthComponent::HandleOutOfHealth()
{
    bIsDead = true;
    AbilitySystemComp->AddLooseGameplayTag(RLGameplayTags::State_Dead);
    OnDeath.Broadcast(); // 사망 방송을 보냅니다.
}

float URLHealthComponent::ApplyIncomingDamage(float RawDamage, AActor* DamageCauser, bool bTrueDamage)
{
    // 데미지가 없거나 이미 죽었다면 무시합니다.
    if (RawDamage <= 0.0f || bIsDead || !AbilitySystemComp)
    {
        return 0.0f;
    }

    // 공격자의 ASC가 GE를 "보내는" 쪽(Source)이 됩니다. 환경 피해 등 공격자 ASC가 없으면 자기 자신이 Source.
    UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageCauser);
    if (!SourceASC)
    {
        SourceASC = AbilitySystemComp;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddInstigator(DamageCauser, DamageCauser);

    const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(URLGE_Damage::StaticClass(), 1.0f, Context);
    if (!Spec.IsValid())
    {
        return 0.0f;
    }
    Spec.Data->SetSetByCallerMagnitude(RLGameplayTags::Data_Damage, RawDamage);
    Spec.Data->SetSetByCallerMagnitude(RLGameplayTags::Data_TrueDamage, bTrueDamage ? 1.0f : 0.0f);

    // 경감 계산(URLDamageExecution) → IncomingDamage → Health 감소 → HandleHealthAttributeChanged
    // → OnHealthChanged / OnDeath 까지 전부 이 호출 안에서 동기로 끝납니다.
    {
        TGuardValue<bool> DamageGuard(bApplyingDamage, true);
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), AbilitySystemComp);
    }

    const URLAttributeSet* AttributeSet = AbilitySystemComp->GetSet<URLAttributeSet>();
    return AttributeSet ? AttributeSet->GetLastDamageTaken() : 0.0f;
}

// ★ 체력 회복 (흡혈 등에서 사용). ApplyIncomingDamage와 대칭되는 형태로 작성했습니다.
void URLHealthComponent::Heal(float Amount)
{
    // 회복량이 없거나 이미 죽었다면 무시합니다 (죽은 뒤에 흡혈로 살아나는 걸 방지).
    if (Amount <= 0.0f || bIsDead || !AbilitySystemComp)
    {
        return;
    }

    const FGameplayEffectSpecHandle Spec = AbilitySystemComp->MakeOutgoingSpec(URLGE_Heal::StaticClass(), 1.0f, AbilitySystemComp->MakeEffectContext());
    if (!Spec.IsValid())
    {
        return;
    }
    Spec.Data->SetSetByCallerMagnitude(RLGameplayTags::Data_Heal, Amount);

    // 실제로 회복된 양만큼 음수 HealthDelta로 방송됩니다 (풀피면 방송 없음).
    AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

// ★ 최대 체력 증감 (패시브 아이템 장착/해제, 영구 보상 등). 헤더 주석 참고.
void URLHealthComponent::IncreaseMaxHealth(float Delta, bool bApply)
{
    if (FMath::IsNearlyZero(Delta) || !AbilitySystemComp)
    {
        return;
    }

    const FGameplayAttribute HealthAttr = URLAttributeSet::GetHealthAttribute();
    const FGameplayAttribute MaxHealthAttr = URLAttributeSet::GetMaxHealthAttribute();

    // 최대 체력과 현재 체력을 한 번에 바꾸고, 방송은 마지막에 한 번만 합니다.
    bSuppressBroadcast = true;

    const float OldMaxHealth = AbilitySystemComp->GetNumericAttribute(MaxHealthAttr);
    RLGameplayEffectUtils::ApplyStatBonusEffect(AbilitySystemComp, URLGE_StatBonus_Instant::StaticClass(), { { MaxHealthAttr, Delta } });
    const float ActualDelta = AbilitySystemComp->GetNumericAttribute(MaxHealthAttr) - OldMaxHealth;

    if (bApply && ActualDelta > 0.0f)
    {
        // 획득으로 최대 체력이 늘어난 경우, 늘어난 만큼 현재 체력도 함께 채워줍니다.
        AbilitySystemComp->SetNumericAttributeBase(HealthAttr, AbilitySystemComp->GetNumericAttribute(HealthAttr) + ActualDelta);
    }
    // 최대치가 줄어 현재 체력이 넘치는 경우의 clamp는 URLAttributeSet::PostAttributeChange가 처리합니다.

    bSuppressBroadcast = false;

    SyncFromAttributes();
    // 데미지/회복이 아닌 최대치 변경 이벤트이므로 HealthDelta는 0으로 방송합니다 (체력바 갱신용).
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.0f);
}

void URLHealthComponent::FillHealthForMaxHealthIncrease(float Amount)
{
    if (Amount <= 0.0f || bIsDead || !AbilitySystemComp)
    {
        return;
    }

    {
        TGuardValue<bool> SuppressGuard(bSuppressBroadcast, true);
        const FGameplayAttribute HealthAttr = URLAttributeSet::GetHealthAttribute();
        AbilitySystemComp->SetNumericAttributeBase(HealthAttr, AbilitySystemComp->GetNumericAttribute(HealthAttr) + Amount);
    }

    SyncFromAttributes();
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.0f);
}

float URLHealthComponent::GetMaxHealthBase() const
{
    return AbilitySystemComp ? AbilitySystemComp->GetNumericAttributeBase(URLAttributeSet::GetMaxHealthAttribute()) : MaxHealth;
}

void URLHealthComponent::RestoreHealthDirect(float InCurrentHealth, float InMaxHealth)
{
    if (!AbilitySystemComp)
    {
        return;
    }

    bSuppressBroadcast = true;
    const float NewMaxHealth = FMath::Max(1.0f, InMaxHealth);
    AbilitySystemComp->SetNumericAttributeBase(URLAttributeSet::GetMaxHealthAttribute(), NewMaxHealth);
    // 현재 체력은 아이템 보너스까지 포함한 최대치 기준으로 clamp (AttributeSet::PreAttributeBaseChange)
    AbilitySystemComp->SetNumericAttributeBase(URLAttributeSet::GetHealthAttribute(), FMath::Max(0.0f, InCurrentHealth));
    bSuppressBroadcast = false;

    SyncFromAttributes();

    // 복원은 사망 연출을 다시 틀지 않도록 OnDeath 없이 상태만 맞춥니다 (기존 동작과 동일).
    bIsDead = (CurrentHealth <= 0.0f);
    AbilitySystemComp->SetLooseGameplayTagCount(RLGameplayTags::State_Dead, bIsDead ? 1 : 0);

    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.0f);
}
