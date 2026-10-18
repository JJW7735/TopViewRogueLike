#pragma once

#include "CoreMinimal.h"
#include "RLGameplayAbility_Skill.h"
#include "GameplayEffectTypes.h"
#include "RLSkillAbilities.generated.h"

// =====================================================================
// ★ CastType별 스킬 GA. 예전 URLSkillComponent의 상태 머신(switch + Tick 분기)을 타입별 클래스로 분리했습니다.
// 기본 매핑은 URLGameplayAbility_Skill::GetDefaultAbilityClass, 개별 스킬은 URLSkillData::AbilityClassOverride로 교체 가능.
// =====================================================================

// 즉발 근접 (Instant): 몽타주 + 박스 히트 1회 (bHitOnAnimNotify면 애님 노티파이에서 히트). 기본 공격(LMB) 콤보 지원
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_Instant : public URLGameplayAbility_Skill
{
	GENERATED_BODY()

protected:
	virtual void ActivateSkill() override;

private:
	void ResetCombo() { ComboCount = 0; }

	int32 ComboCount = 0;
	FTimerHandle ComboResetTimerHandle;
};

// 투사체 (Projectile): 전방으로 ARLProjectile 발사
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_Projectile : public URLGameplayAbility_Skill
{
	GENERATED_BODY()

protected:
	virtual void ActivateSkill() override;
};

// 돌진 (Dash): 짧게 전진 + 박스 히트
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_Dash : public URLGameplayAbility_Skill
{
	GENERATED_BODY()

protected:
	virtual void ActivateSkill() override;
};

// 버프 (Buff): 이동속도 증가 Duration GE (시간이 지나면 GAS가 알아서 해제)
// 오라 이펙트는 URLGE_SkillBuff의 GameplayCue.Skill.Buff가 GE 수명에 맞춰 켜고 끔
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_Buff : public URLGameplayAbility_Skill
{
	GENERATED_BODY()

protected:
	virtual void ActivateSkill() override;

private:
	FActiveGameplayEffectHandle BuffEffectHandle;
};

// 캐스팅 (Cast): 게이지가 다 차면 발동, 도중 피격/회피/이동 시 취소. State.Casting 부여
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_Cast : public URLGameplayAbility_Skill
{
	GENERATED_BODY()

public:
	virtual bool IsInterruptibleCasting() const override { return bInCastPhase; }

protected:
	virtual void ActivateSkill() override;
	virtual void TickSkill(float DeltaTime) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 시전 시작 (몽타주 + UI 방송 + 틱)
	void BeginCastPhase();
	// 게이지 완료 시 실제 효과. 기본은 전방 박스 히트
	virtual void ReleaseCast();
	void CompleteCast();

	bool bInCastPhase = false;
	bool bHasTriggeredPreCastMotion = false;
	float CastElapsed = 0.0f;
};

// 장판 (GroundTarget): 조준(데칼) → 좌클릭 확정 → (CastDuration > 0이면 캐스팅) → 지정 위치 폭발/장판 스폰
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_GroundTarget : public URLGA_Skill_Cast
{
	GENERATED_BODY()

public:
	// URLSkillComponent::ConfirmGroundCast (조준 중 좌클릭)
	void ConfirmTarget();
	bool IsAiming() const { return bAiming; }

protected:
	virtual void ActivateSkill() override;
	virtual void TickSkill(float DeltaTime) override;
	virtual void ReleaseCast() override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void Detonate();

	bool bAiming = false;
	FVector TargetLocation = FVector::ZeroVector;
};

// 차지 (Charge / ChargeDash): 누르고 있는 동안 힘을 모으고, 떼거나 최대치에 도달하면 배율 적용 히트
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_Charge : public URLGameplayAbility_Skill
{
	GENERATED_BODY()

public:
	virtual void OnSkillInputReleased() override { Release(); }

protected:
	virtual void ActivateSkill() override;
	virtual void TickSkill(float DeltaTime) override;

private:
	void Release();

	float ChargeElapsed = 0.0f;
};

// 홀드 (Hold): 누르고 있는 동안 TickDamageInterval마다 히트, 떼거나 CastDuration이 지나면 종료
UCLASS()
class ROGUELIKETOPVIEW_API URLGA_Skill_Hold : public URLGameplayAbility_Skill
{
	GENERATED_BODY()

public:
	virtual void OnSkillInputReleased() override { Release(); }

protected:
	virtual void ActivateSkill() override;
	virtual void TickSkill(float DeltaTime) override;

private:
	void Release();

	float HoldElapsed = 0.0f;
	float LastHitTime = 0.0f;
};
