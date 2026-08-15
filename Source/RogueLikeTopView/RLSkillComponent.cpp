#include "RLSkillComponent.h"
#include "RLCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

URLSkillComponent::URLSkillComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void URLSkillComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ARLCharacter>(GetOwner());
}

void URLSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 쿨타임 UI 갱신을 위해 브로드캐스트
    for (const auto& Pair : EquippedSkills)
    {
        ESkillSlot Slot = Pair.Key;
        URLSkillData* Skill = Pair.Value;
        if (Skill && IsOnCooldown(Slot))
        {
            float Remaining = GetRemainingCooldown(Slot);
            OnSkillCooldownChanged.Broadcast(Slot, Remaining, Skill->Cooldown);
        }
    }
}

void URLSkillComponent::EquipSkill(ESkillSlot Slot, URLSkillData* NewSkill)
{
    EquippedSkills.Add(Slot, NewSkill);
}

bool URLSkillComponent::IsOnCooldown(ESkillSlot Slot) const
{
    if (!CooldownEndTimes.Contains(Slot)) return false;
    return GetWorld()->GetTimeSeconds() < CooldownEndTimes[Slot];
}

float URLSkillComponent::GetRemainingCooldown(ESkillSlot Slot) const
{
    if (!IsOnCooldown(Slot)) return 0.0f;
    return FMath::Max(0.0f, CooldownEndTimes[Slot] - GetWorld()->GetTimeSeconds());
}

void URLSkillComponent::RotateOwnerToMouseCursor()
{
    if (!OwnerCharacter) return;

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC) return;

    FHitResult Hit;
    if (PC->GetHitResultUnderCursor(ECC_Visibility, true, Hit))
    {
        FVector ImpactPoint = Hit.ImpactPoint;
        FVector CharLoc = OwnerCharacter->GetActorLocation();

        // 2D 평면상 마우스 방향 계산
        FVector LookDir = (ImpactPoint - CharLoc).GetSafeNormal2D();
        if (!LookDir.IsNearlyZero())
        {
            FRotator TargetRot = LookDir.Rotation();
            OwnerCharacter->SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
        }
    }
}




void URLSkillComponent::OnSkillInputStarted(ESkillSlot Slot)
{
    if (CurrentSkillState != EPlayerSkillState::Idle) return; // 이미 다른 스킬 사용 중이면 무시
    if (!EquippedSkills.Contains(Slot) || !EquippedSkills[Slot]) return;
    if (IsOnCooldown(Slot)) return;

    URLSkillData* Skill = EquippedSkills[Slot];
    CurrentActiveSlot = Slot;
    CurrentAccumulatedTime = 0.0f; // 게이지 초기화

    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
        OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();

    RotateOwnerToMouseCursor();

    switch (Skill->CastType)
    {
    case ESkillCastType::Instant:
        // 즉발 스킬 처리
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        else OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);

        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        break;

    case ESkillCastType::Cast:
        CurrentSkillState = EPlayerSkillState::Casting;
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("캐스팅 시작..."));
        break;

    case ESkillCastType::Charge:
        CurrentSkillState = EPlayerSkillState::Charging;
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("기 모으는 중 (차징)..."));
        break;

    case ESkillCastType::Hold:
        CurrentSkillState = EPlayerSkillState::Holding;
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, TEXT("난도질 시작 (홀딩)..."));
        break;

    default:
        break;
    }
}

void URLSkillComponent::OnSkillInputTriggered(ESkillSlot Slot)
{
    if (CurrentSkillState == EPlayerSkillState::Idle || CurrentActiveSlot != Slot) return;

    // 누르고 있는 동안 게이지 누적
    CurrentAccumulatedTime += GetWorld()->GetDeltaSeconds();
    URLSkillData* Skill = EquippedSkills[Slot];

    // 홀딩 스킬의 경우 누르고 있는 동안 틱 데미지 발생
    if (CurrentSkillState == EPlayerSkillState::Holding)
    {
        // (여기에 TickDamageInterval 마다 데미지를 주는 로직이 들어갑니다)
    }

    // 만약 최대 시간(CastDuration)을 초과했다면 강제 완료 처리
    if (CurrentAccumulatedTime >= Skill->CastDuration)
    {
        OnSkillInputCompleted(Slot);
    }
}

void URLSkillComponent::OnSkillInputCompleted(ESkillSlot Slot)
{
    if (CurrentSkillState == EPlayerSkillState::Idle || CurrentActiveSlot != Slot) return;

    URLSkillData* Skill = EquippedSkills[Slot];

    // 1. 차징 스킬: 손을 뗐을 때 모은 시간에 비례하여 데미지 발사
    if (CurrentSkillState == EPlayerSkillState::Charging)
    {
        float Multiplier = 1.0f;

        // 최소 시간도 못 채우고 툭 뗐을 경우에도 씹히지 않고 기본 1배 데미지로 즉시 발사!
        if (CurrentAccumulatedTime < Skill->MinChargeTime)
        {
            Multiplier = 1.0f;
        }
        else
        {
            // 최소 ~ 최대(CastDuration) 사이일 때 누적 시간에 비례하여 데미지 선형 증가 (Lerp)
            float Alpha = FMath::Clamp(CurrentAccumulatedTime / Skill->CastDuration, 0.0f, 1.0f);
            Multiplier = FMath::Lerp(1.0f, Skill->MaxDamageMultiplier, Alpha);
        }

        FRLDamageData FinalDamage = Skill->DamageData;
        FinalDamage.BaseDamage *= Multiplier; // 모은 시간에 비례한 데미지 뻥튀기!
        OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, FinalDamage, true);
    }

    // 2. 공통 종료 처리: 쿨타임 시작 및 상태 초기화
    CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);

    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("스킬 종료! (누른 시간: %.1f초)"), CurrentAccumulatedTime));
    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
}