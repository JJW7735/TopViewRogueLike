#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "RLAttributeSet.generated.h"

// GetHealth() / SetHealth() / InitHealth() / GetHealthAttribute() 를 한 번에 만들어주는 매크로
#define RL_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// ★ 전투 스탯 전체를 담는 AttributeSet. 기존 URLHealthComponent(체력) + URLStatComponent(전투 스탯)를 대체합니다.
// 레벨/경험치는 전투 스탯이 아니라서 URLStatComponent에 그대로 남깁니다.
//
// 값의 범위(클램프)는 반드시 이 클래스 한 곳에서만 처리합니다.
// (기존에는 Inventory/Reward/Shop/Save 4곳에서 CritChance 0~100, DamageReduction 0~80을 각자 클램프하고 있었음)
//
// 기본값은 기존 컴포넌트 기본값과 동일하게 맞춰두었습니다. 캐릭터별 값은 1단계에서 초기화 GE(GE_InitStats_*)로 덮어씁니다.
UCLASS()
class ROGUELIKETOPVIEW_API URLAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URLAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ==========================================
	// ★ 체력
	// ==========================================
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, MaxHealth)

	// ==========================================
	// ★ 공격 스탯
	// ==========================================
	// 공격력 (물리 데미지 기반)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense")
	FGameplayAttributeData AttackPower;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, AttackPower)

	// 주문력 (마법/스킬 데미지 기반)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense")
	FGameplayAttributeData AbilityPower;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, AbilityPower)

	// 치명타 확률 (0 ~ 100, 15 = 15%)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense")
	FGameplayAttributeData CritChance;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, CritChance)

	// 치명타 피해 배율 (1.5 = 150%)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense")
	FGameplayAttributeData CritDamageMultiplier;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, CritDamageMultiplier)

	// 공격 속도 배율 (1.0 = 100%)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Offense")
	FGameplayAttributeData AttackSpeed;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, AttackSpeed)

	// ==========================================
	// ★ 방어 스탯
	// ==========================================
	// 방어력 (받는 피해 배율: 100 / (100 + Defense))
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense")
	FGameplayAttributeData Defense;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, Defense)

	// 받는 피해 감소율 (0 ~ 80%)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense")
	FGameplayAttributeData DamageReduction;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, DamageReduction)

	// ==========================================
	// ★ 이동
	// ==========================================
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Movement")
	FGameplayAttributeData MoveSpeed;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, MoveSpeed)

	// ==========================================
	// ★ 메타 어트리뷰트 (저장되지 않는 임시 값)
	// ==========================================
	// 데미지 GE는 Health를 직접 깎지 않고 여기에 "들어온 피해량"을 넣습니다.
	// PostGameplayEffectExecute에서 이 값을 읽어 Health에 반영한 뒤 0으로 비웁니다.
	// 방어력/치명타 계산은 3단계에서 ExecutionCalculation으로 옮깁니다.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	RL_ATTRIBUTE_ACCESSORS(URLAttributeSet, IncomingDamage)

private:
	// 어트리뷰트별 허용 범위를 한 곳에서 정의
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
