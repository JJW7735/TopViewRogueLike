#include "RLDamageExecution.h"
#include "AbilitySystemComponent.h"
#include "RLAttributeSet.h"
#include "RLGameplayTags.h"

namespace
{
	struct FRLDamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(Defense);
		DECLARE_ATTRIBUTE_CAPTUREDEF(DamageReduction);

		FRLDamageStatics()
		{
			// 대상(Target)의 값을, 적용 시점에 캡처 (bSnapshot = false)
			DEFINE_ATTRIBUTE_CAPTUREDEF(URLAttributeSet, Defense, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(URLAttributeSet, DamageReduction, Target, false);
		}
	};

	const FRLDamageStatics& DamageStatics()
	{
		static FRLDamageStatics Statics;
		return Statics;
	}
}

URLDamageExecution::URLDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
	RelevantAttributesToCapture.Add(DamageStatics().DamageReductionDef);
}

float URLDamageExecution::CalculateMitigatedDamage(float RawDamage, float Defense, float DamageReduction, float& OutReducedDamage)
{
	OutReducedDamage = 0.0f;
	if (RawDamage <= 0.0f)
	{
		return 0.0f;
	}

	// 1. 방어력 배율 (점근적 공식). 음수 방어력(방어력 감소 디버프)은 추가 피해
	const float DefenseMultiplier = Defense >= 0.0f
		? 100.0f / (100.0f + Defense)
		: 2.0f - (100.0f / (100.0f - Defense));

	// 2. 퍼센트 피해 감소율 (최대 80%)
	const float ReductionMultiplier = 1.0f - (FMath::Clamp(DamageReduction, 0.0f, 80.0f) / 100.0f);

	// 3. 최종 피해 (최소 1 보장)
	const float FinalDamage = FMath::Max(1.0f, RawDamage * DefenseMultiplier * ReductionMultiplier);
	OutReducedDamage = FMath::Max(0.0f, RawDamage - FinalDamage);
	return FinalDamage;
}

void URLDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const float RawDamage = FMath::Max(0.0f, Spec.GetSetByCallerMagnitude(RLGameplayTags::Data_Damage, false, 0.0f));
	if (RawDamage <= 0.0f)
	{
		return;
	}

	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	const bool bIgnoreDefense = Spec.GetSetByCallerMagnitude(RLGameplayTags::Data_TrueDamage, false, 0.0f) > 0.0f
		|| (TargetASC && TargetASC->HasMatchingGameplayTag(RLGameplayTags::State_IgnoreDefense));

	float FinalDamage = RawDamage;
	if (!bIgnoreDefense)
	{
		FAggregatorEvaluateParameters EvaluateParams;
		EvaluateParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
		EvaluateParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

		float Defense = 0.0f;
		float DamageReduction = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DefenseDef, EvaluateParams, Defense);
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DamageReductionDef, EvaluateParams, DamageReduction);

		float ReducedDamage = 0.0f;
		FinalDamage = CalculateMitigatedDamage(RawDamage, Defense, DamageReduction, ReducedDamage);
	}

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(URLAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
}
