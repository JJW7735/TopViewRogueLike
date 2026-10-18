#include "RLSkillComponent.h"
#include "RLCharacter.h"
#include "RLPlayerController.h"
#include "RLDecalIndicator.h"
#include "RLGameplayAbility_Skill.h"
#include "RLSkillAbilities.h"
#include "RLGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

URLSkillComponent::URLSkillComponent()
{
	// 쿨타임 UI 방송용 (스킬 진행 자체는 GA의 틱 태스크가 처리)
	PrimaryComponentTick.bCanEverTick = true;
}

void URLSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ARLCharacter>(GetOwner());

	if (GetWorld())
	{
		FActorSpawnParameters Params;
		Params.Owner = OwnerCharacter;
		DecalIndicator = GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

		if (DecalIndicator)
		{
			DecalIndicator->SetIndicatorVisibility(false);
		}
	}

	// ★ 캐릭터 BP 클래스 디폴트에서 EquippedSkills 맵에 바로 넣어둔 스킬도 GA를 부여합니다.
	// (BP_Player_Blade/Sorceress/Destroyer는 EquipSkill이 아니라 이 맵으로 스킬을 지정함)
	// ARLCharacter::BeginPlay가 Super 호출 전에 InitAbilityActorInfo를 끝내두므로 여기서 GiveAbility 가능.
	const TMap<ESkillSlot, URLSkillData*> PresetSkills = EquippedSkills;
	for (const TPair<ESkillSlot, URLSkillData*>& Pair : PresetSkills)
	{
		EquipSkill(Pair.Key, Pair.Value);
	}
}

void URLSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 쿨타임 UI 브로드캐스트 (BP_RLCharacter가 바인딩)
	for (const auto& Pair : EquippedSkills)
	{
		if (const URLSkillData* Skill = Pair.Value)
		{
			OnSkillCooldownChanged.Broadcast(Pair.Key, GetRemainingCooldown(Pair.Key), Skill->Cooldown);
		}
	}
}

// ========================================================
// 장착: 스킬 데이터 → GA 부여
// ========================================================
void URLSkillComponent::EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill)
{
	EquippedSkills.Add(Slot, NewSkill);

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	// 같은 슬롯의 기존 스킬 GA 회수 (스킬 교체 / 세이브 복원)
	if (const FGameplayAbilitySpecHandle* OldHandle = SkillSpecHandles.Find(Slot))
	{
		ASC->ClearAbility(*OldHandle);
		SkillSpecHandles.Remove(Slot);
	}

	if (!NewSkill)
	{
		return;
	}

	if (const TSubclassOf<URLGameplayAbility_Skill> AbilityClass = NewSkill->GetAbilityClass())
	{
		// InputID = 슬롯, SourceObject = 스킬 데이터 (GA가 활성화 시 꺼내 씀)
		FGameplayAbilitySpec Spec(AbilityClass, 1, static_cast<int32>(Slot), NewSkill);
		SkillSpecHandles.Add(Slot, ASC->GiveAbility(Spec));
	}
}

// ========================================================
// 입력 처리
// ========================================================
void URLSkillComponent::OnSkillInputStarted(ESkillSlot Slot)
{
	if (OwnerCharacter && OwnerCharacter->IsDashing()) return;
	if (!EquippedSkills.FindRef(Slot)) return;

	UAbilitySystemComponent* ASC = GetASC();
	const FGameplayAbilitySpecHandle* Handle = SkillSpecHandles.Find(Slot);
	if (!ASC || !Handle)
	{
		return;
	}

	// 장판 조준 중에 다른(또는 같은) 스킬을 누르면 조준을 취소하고 새로 발동
	if (IsAiming())
	{
		CancelGroundCast();
	}

	// 쿨타임/시전 중 차단은 GA의 CheckCooldown·ActivationBlockedTags가 판단
	if (ASC->TryActivateAbility(*Handle))
	{
		CurrentActiveSlot = Slot;
	}
}

void URLSkillComponent::OnSkillInputTriggered(ESkillSlot Slot)
{
	// 누르고 있는 동안의 진행(차지 누적, 홀드 틱 데미지)은 GA의 틱 태스크가 처리합니다.
	// Enhanced Input 바인딩 호환을 위해 함수만 남겨둡니다.
}

void URLSkillComponent::OnSkillInputCompleted(ESkillSlot Slot)
{
	URLGameplayAbility_Skill* Ability = GetSkillAbility(Slot);
	if (Ability && Ability->IsActive())
	{
		Ability->OnSkillInputReleased();
	}
}

// ========================================================
// 장판 / 시전 제어
// ========================================================
void URLSkillComponent::ConfirmGroundCast()
{
	if (URLGA_Skill_GroundTarget* GroundAbility = Cast<URLGA_Skill_GroundTarget>(GetSkillAbility(CurrentActiveSlot)))
	{
		if (GroundAbility->IsActive() && GroundAbility->IsAiming())
		{
			GroundAbility->ConfirmTarget();
		}
	}
}

void URLSkillComponent::CancelGroundCast()
{
	if (!IsAiming())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		if (const FGameplayAbilitySpecHandle* Handle = SkillSpecHandles.Find(CurrentActiveSlot))
		{
			ASC->CancelAbilityHandle(*Handle);
		}
	}
}

void URLSkillComponent::InterruptCasting(const FString& Reason)
{
	URLGameplayAbility_Skill* Ability = GetSkillAbility(CurrentActiveSlot);
	if (!Ability || !Ability->IsActive() || !Ability->IsInterruptibleCasting())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Skill] 시전 취소: %s"), *Reason);
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->CancelAbilityHandle(SkillSpecHandles.FindRef(CurrentActiveSlot));
	}
}

// ========================================================
// 애니메이션 노티파이 (ABP_RLCharacter / wizard_ABP_RLCharacter)
// ========================================================
void URLSkillComponent::TriggerActiveSkillHit()
{
	if (URLGameplayAbility_Skill* Ability = GetSkillAbility(CurrentActiveSlot))
	{
		Ability->HandleAnimHitNotify();
	}
}

void URLSkillComponent::TriggerActiveSkillDash()
{
	if (URLGameplayAbility_Skill* Ability = GetSkillAbility(CurrentActiveSlot))
	{
		Ability->HandleAnimDashNotify();
	}
}

// ========================================================
// 상태 조회
// ========================================================
bool URLSkillComponent::IsOnCooldown(ESkillSlot Slot) const
{
	return GetRemainingCooldown(Slot) > 0.0f;
}

float URLSkillComponent::GetRemainingCooldown(ESkillSlot Slot) const
{
	const URLGameplayAbility_Skill* Ability = GetSkillAbility(Slot);
	return Ability ? Ability->GetCooldownRemaining() : 0.0f;
}

bool URLSkillComponent::IsAiming() const   { return HasStateTag(RLGameplayTags::State_Skill_Aiming); }
bool URLSkillComponent::IsCasting() const  { return HasStateTag(RLGameplayTags::State_Casting); }
bool URLSkillComponent::IsCharging() const { return HasStateTag(RLGameplayTags::State_Skill_Charging); }
bool URLSkillComponent::IsHolding() const  { return HasStateTag(RLGameplayTags::State_Skill_Holding); }

EPlayerSkillState URLSkillComponent::GetSkillState() const
{
	if (IsAiming())   return EPlayerSkillState::Aiming;
	if (IsCasting())  return EPlayerSkillState::Casting;
	if (IsCharging()) return EPlayerSkillState::Charging;
	if (IsHolding())  return EPlayerSkillState::Holding;
	return EPlayerSkillState::Idle;
}

// ========================================================
// 내부 헬퍼
// ========================================================
UAbilitySystemComponent* URLSkillComponent::GetASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

URLGameplayAbility_Skill* URLSkillComponent::GetSkillAbility(ESkillSlot Slot) const
{
	UAbilitySystemComponent* ASC = GetASC();
	const FGameplayAbilitySpecHandle* Handle = SkillSpecHandles.Find(Slot);
	if (!ASC || !Handle)
	{
		return nullptr;
	}

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(*Handle);
	return Spec ? Cast<URLGameplayAbility_Skill>(Spec->GetPrimaryInstance()) : nullptr;
}

bool URLSkillComponent::HasStateTag(const FGameplayTag& Tag) const
{
	const UAbilitySystemComponent* ASC = GetASC();
	return ASC && ASC->HasMatchingGameplayTag(Tag);
}

void URLSkillComponent::StopPlayerMovement()
{
	if (!OwnerCharacter) return;
	if (OwnerCharacter->GetCharacterMovement()) OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
	if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController())) PC->StopMovement();
}

void URLSkillComponent::RotateOwnerToMouseCursor()
{
	if (!OwnerCharacter) return;
	if (ARLPlayerController* PC = Cast<ARLPlayerController>(OwnerCharacter->GetController()))
	{
		FHitResult Hit;
		if (PC->GetGroundHitUnderCursor(Hit))
		{
			const FVector LookDir = (Hit.ImpactPoint - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
			if (!LookDir.IsNearlyZero())
			{
				OwnerCharacter->SetActorRotation(FRotator(0.f, LookDir.Rotation().Yaw, 0.f));
			}
		}
	}
}

FVector URLSkillComponent::GetMouseGroundLocation() const
{
	if (!OwnerCharacter) return FVector::ZeroVector;
	if (ARLPlayerController* PC = Cast<ARLPlayerController>(OwnerCharacter->GetController()))
	{
		FHitResult Hit;
		if (PC->GetGroundHitUnderCursor(Hit)) return Hit.ImpactPoint;
	}
	return OwnerCharacter->GetActorLocation();
}
