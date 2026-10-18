#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "RLSkillData.h"
#include "RLSkillComponent.generated.h"

class ARLCharacter;
class ARLDecalIndicator;
class UAbilitySystemComponent;
class URLGameplayAbility_Skill;

// ========================================================
// 델리게이트 선언 (UI 연동용)
// ========================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillCooldownChanged, ESkillSlot, Slot, float, RemainingTime, float, MaxCooldown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnCastProgressChanged, ESkillSlot, Slot, float, CurrentTime, float, MaxDuration, float, ProgressRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCastStateChanged, ESkillSlot, Slot, bool, bIsCasting);

// ========================================================
// 스킬 상태 Enum (State.* 게임플레이 태그에서 계산되는 읽기 전용 값)
// ========================================================
UENUM(BlueprintType)
enum class EPlayerSkillState : uint8
{
	Idle,
	Casting,      // 캐스팅 게이지 채우는 중 (100% 도달 시 발동)
	Charging,     // 차지 힘 모으는 중
	Holding,      // 홀딩 유지 중
	Aiming        // 장판 위치 조준 중
};

// ★ GAS Facade: 스킬의 실제 동작은 CastType별 GA(URLGA_Skill_*, RLSkillAbilities.h)가 담당하고,
// 이 컴포넌트는 기존 API를 그대로 유지한 채 슬롯 ↔ GA를 연결하는 역할만 합니다.
// - EquipSkill: 스킬 데이터를 SourceObject로, 슬롯을 InputID로 GiveAbility
// - 입력(PlayerController) → TryActivateAbility / OnSkillInputReleased
// - ABP 노티파이(TriggerActiveSkillHit/Dash) → 마지막으로 발동한 스킬 GA
// - UI: OnSkillCooldownChanged / OnCastProgressChanged / OnCastStateChanged 그대로 방송
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URLSkillComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================
	// 입력 핸들링 (RLPlayerController가 호출)
	// ========================================================
	void OnSkillInputStarted(ESkillSlot Slot);
	void OnSkillInputTriggered(ESkillSlot Slot);
	void OnSkillInputCompleted(ESkillSlot Slot);

	// ========================================================
	// 스킬 발동 제어 및 애니메이션 노티파이
	// ========================================================
	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void ConfirmGroundCast();

	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void CancelGroundCast();

	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void TriggerActiveSkillHit();

	UFUNCTION(BlueprintCallable, Category = "Skill|Action")
	void TriggerActiveSkillDash();

	void InterruptCasting(const FString& Reason = TEXT("Interrupted"));

	// ========================================================
	// 유틸리티 및 상태 Getter
	// ========================================================
	UFUNCTION(BlueprintCallable, Category = "Skill|Setup")
	void EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill);

	UFUNCTION(BlueprintPure, Category = "Skill|Setup")
	const TMap<ESkillSlot, URLSkillData*>& GetEquippedSkills() const { return EquippedSkills; }

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsOnCooldown(ESkillSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	float GetRemainingCooldown(ESkillSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsAiming() const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsCasting() const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsCharging() const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	bool IsHolding() const;

	UFUNCTION(BlueprintPure, Category = "Skill|State")
	EPlayerSkillState GetSkillState() const;

	// ========================================================
	// 블루프린트 이벤트 (UI 업데이트용)
	// ========================================================
	UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
	FOnSkillCooldownChanged OnSkillCooldownChanged;

	UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
	FOnCastProgressChanged OnCastProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Skill|UI")
	FOnCastStateChanged OnCastStateChanged;

	// ========================================================
	// 스킬 GA가 쓰는 헬퍼
	// ========================================================
	void StopPlayerMovement();
	void RotateOwnerToMouseCursor();
	FVector GetMouseGroundLocation() const;
	ARLDecalIndicator* GetDecalIndicator() const { return DecalIndicator; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	ARLCharacter* OwnerCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Data")
	TMap<ESkillSlot, URLSkillData*> EquippedSkills;

	// 마지막으로 발동한 슬롯 (애님 노티파이 / 장판 확정 / 시전 취소 대상)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|State")
	ESkillSlot CurrentActiveSlot = ESkillSlot::Slot_Q;

	UPROPERTY()
	ARLDecalIndicator* DecalIndicator;

private:
	UAbilitySystemComponent* GetASC() const;
	URLGameplayAbility_Skill* GetSkillAbility(ESkillSlot Slot) const;
	bool HasStateTag(const FGameplayTag& Tag) const;

	// 슬롯별로 GiveAbility한 스킬 GA 핸들
	TMap<ESkillSlot, FGameplayAbilitySpecHandle> SkillSpecHandles;
};
