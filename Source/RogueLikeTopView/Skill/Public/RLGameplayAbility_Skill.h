#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RLSkillData.h"
#include "RLGameplayAbility_Skill.generated.h"

class ARLCharacter;
class URLSkillComponent;
class URLAbilityTask_Tick;

// ★ 플레이어 스킬 GA 공통 베이스.
// - 스킬 수치는 URLSkillData(DA_Skill_*)에 그대로 두고, GA는 "어떻게 발동하는가"(CastType별 흐름)만 담당합니다.
//   URLSkillComponent::EquipSkill이 GiveAbility할 때 스킬 데이터를 Spec의 SourceObject로 넘깁니다.
// - 쿨타임: URLGE_SkillCooldown(Data.Cooldown 초)을 자기 자신에게 걸고 핸들로 남은 시간을 조회합니다.
// - 입력: URLSkillComponent가 슬롯 입력을 받아 TryActivate / OnSkillInputReleased로 넘겨줍니다.
// - 진행 상태(조준/시전/차지/홀드)는 State.* 태그로 표시되어, 시전 중에는 다른 스킬이 자동으로 막힙니다.
UCLASS(Abstract)
class ROGUELIKETOPVIEW_API URLGameplayAbility_Skill : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URLGameplayAbility_Skill();

	// CastType → 기본 GA 클래스 매핑 (URLSkillData::AbilityClassOverride가 비어 있을 때 사용)
	static TSubclassOf<URLGameplayAbility_Skill> GetDefaultAbilityClass(ESkillCastType CastType);

	// ── URLSkillComponent가 부르는 입력/노티파이 훅 ──
	virtual void OnSkillInputReleased() {}
	// 애님 노티파이(ABP의 TriggerActiveSkillHit): 기본은 스킬 히트박스 1회
	virtual void HandleAnimHitNotify();
	// 애님 노티파이(ABP의 TriggerActiveSkillDash): 돌진 계열이면 전방으로 발사
	virtual void HandleAnimDashNotify();
	// 시전(캐스팅) 중단 (피격/회피/이동 명령)
	virtual bool IsInterruptibleCasting() const { return false; }

	float GetCooldownRemaining() const;

	// 공격력/주문력 계수 + 치명타가 반영된 데미지 데이터 (예전 URLSkillComponent::CalculateFinalDamage)
	static FRLDamageData BuildDamageData(ARLCharacter* InCaster, const URLSkillData* InSkill);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 하위 클래스가 구현하는 실제 발동 흐름 (Skill/Caster는 유효함이 보장됨)
	virtual void ActivateSkill() PURE_VIRTUAL(URLGameplayAbility_Skill::ActivateSkill, );

	// ── 하위 클래스용 헬퍼 ──
	// 시전 준비: (선택) 이동 정지 + 마우스 방향으로 회전
	void PrepareCaster(bool bStopMovement) const;
	void RotateToCursor() const;
	void FinishSkill(bool bCancelled = false);
	void CommitSkillCooldown();
	void SetSkillStateTag(const FGameplayTag& NewStateTag); // 무효 태그를 넘기면 상태 해제
	URLAbilityTask_Tick* StartTickTask();                   // 매 프레임 TickSkill 호출
	virtual void TickSkill(float DeltaTime) {}

	void PlaySkillMontage(UAnimMontage* Montage) const;
	void StopSkillMontage() const;
	void JumpSkillMontageToSection(FName SectionName) const;
	void CreateSkillBoxHitbox(float DamageMultiplier = 1.0f, bool bDrawDebug = true) const;

	// 연출은 GameplayCue로 보냄 (SourceObject = 스킬 데이터, Location = 이펙트 위치). 실제 스폰은 RLGameplayCueNotifies
	FGameplayCueParameters MakeSkillCueParams(const FVector& Location) const;
	void ExecuteSkillCue(const FGameplayTag& CueTag, const FVector& Location);
	// 어빌리티가 끝나면(취소 포함) 자동 제거되는 지속 큐
	void AddSkillCue(const FGameplayTag& CueTag, const FVector& Location);

	URLSkillComponent* GetSkillComponent() const;

	// 활성화 시점에 캐싱 (어빌리티가 끝난 뒤의 애님 노티파이에서도 쓰기 위해)
	UPROPERTY()
	TObjectPtr<URLSkillData> Skill = nullptr;

	UPROPERTY()
	TObjectPtr<ARLCharacter> Caster = nullptr;

	// 장착 슬롯 (GiveAbility 때 Spec의 InputID로 넘겨받음). UI 델리게이트 방송에 사용
	ESkillSlot Slot = ESkillSlot::Slot_Q;

private:
	// ApplyCooldown이 const라서 mutable
	mutable FActiveGameplayEffectHandle CooldownEffectHandle;

	FGameplayTag CurrentStateTag;
};
