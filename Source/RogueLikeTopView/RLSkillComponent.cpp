#include "RLSkillComponent.h"
#include "RLCharacter.h"
#include "RLDecalIndicator.h"
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

    // 인디케이터 액터 풀링 생성
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
}

void URLSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 1. 장판 조준 상태: 실시간 데칼 위치 갱신
    if (CurrentSkillState == EPlayerSkillState::Aiming && DecalIndicator && OwnerCharacter)
    {
        if (URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot))
        {
            FVector MousePos = GetMouseGroundLocation();
            FVector PlayerPos = OwnerCharacter->GetActorLocation();
            DecalIndicator->UpdateIndicatorTransform(PlayerPos, MousePos, Skill->MaxRange);
        }
    }

    // 2. 쿨타임 UI 브로드캐스트
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

        FVector LookDir = (ImpactPoint - CharLoc).GetSafeNormal2D();
        if (!LookDir.IsNearlyZero())
        {
            FRotator TargetRot = LookDir.Rotation();
            OwnerCharacter->SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
        }
    }
}

FVector URLSkillComponent::GetMouseGroundLocation() const
{
    if (!OwnerCharacter) return FVector::ZeroVector;
    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC) return FVector::ZeroVector;

    FHitResult Hit;
    if (PC->GetHitResultUnderCursor(ECC_Visibility, true, Hit))
    {
        return Hit.ImpactPoint;
    }
    return OwnerCharacter->GetActorLocation();
}

void URLSkillComponent::OnSkillInputStarted(ESkillSlot Slot)
{
    if (!EquippedSkills.Contains(Slot) || !EquippedSkills[Slot]) return;
    if (IsOnCooldown(Slot)) return;

    URLSkillData* Skill = EquippedSkills[Slot];

    // 1. 장판 지정 스킬 (GroundTarget)
    if (Skill->CastType == ESkillCastType::GroundTarget)
    {
        if (OwnerCharacter)
        {
            if (OwnerCharacter->GetCharacterMovement())
            {
                OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
            }
            if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
            {
                PC->StopMovement();
            }
        }

        StartGroundTargetAiming(Slot);
        return;
    }

    // 다른 스킬 입력 시 장판 조준 취소
    if (CurrentSkillState == EPlayerSkillState::Aiming)
    {
        CancelGroundCast();
    }

    if (CurrentSkillState != EPlayerSkillState::Idle) return;

    CurrentActiveSlot = Slot;
    CurrentAccumulatedTime = 0.0f;

    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
    }

    RotateOwnerToMouseCursor();

    switch (Skill->CastType)
    {
    case ESkillCastType::Instant:
        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }
        else
        {
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
        }
        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;

        // ★ 투사체 발사 복구
    case ESkillCastType::Projectile:
        if (OwnerCharacter && Skill->ProjectileClass)
        {
            if (Skill->SkillMontage)
            {
                OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
            }

            FVector Forward = OwnerCharacter->GetActorForwardVector();
            FVector SpawnLocation = OwnerCharacter->GetActorLocation() + (Forward * Skill->ProjectileSpawnForwardOffset);
            FRotator SpawnRotation = OwnerCharacter->GetActorRotation();

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = OwnerCharacter;
            SpawnParams.Instigator = OwnerCharacter;

            GetWorld()->SpawnActor<AActor>(Skill->ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
        }
        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;

    case ESkillCastType::Cast:
        CurrentSkillState = EPlayerSkillState::Casting;
        break;

    case ESkillCastType::Charge:
        CurrentSkillState = EPlayerSkillState::Charging;
        break;

    case ESkillCastType::Hold:
        CurrentSkillState = EPlayerSkillState::Holding;
        break;

    default:
        break;
    }
}

void URLSkillComponent::OnSkillInputTriggered(ESkillSlot Slot)
{
    if (CurrentSkillState == EPlayerSkillState::Aiming || CurrentSkillState == EPlayerSkillState::Idle || CurrentActiveSlot != Slot)
    {
        return;
    }

    CurrentAccumulatedTime += GetWorld()->GetDeltaSeconds();
    URLSkillData* Skill = EquippedSkills[Slot];
    if (!Skill) return;

    RotateOwnerToMouseCursor();

    if (CurrentAccumulatedTime >= Skill->CastDuration)
    {
        OnSkillInputCompleted(Slot);
    }
}

void URLSkillComponent::OnSkillInputCompleted(ESkillSlot Slot)
{
    if (CurrentSkillState == EPlayerSkillState::Aiming || CurrentSkillState == EPlayerSkillState::Idle || CurrentActiveSlot != Slot)
    {
        return;
    }

    URLSkillData* Skill = EquippedSkills.FindRef(Slot);
    if (!Skill) return;

    if (CurrentSkillState == EPlayerSkillState::Charging)
    {
        float Multiplier = 1.0f;
        if (CurrentAccumulatedTime >= Skill->MinChargeTime)
        {
            float Alpha = FMath::Clamp(CurrentAccumulatedTime / Skill->CastDuration, 0.0f, 1.0f);
            Multiplier = FMath::Lerp(1.0f, Skill->MaxDamageMultiplier, Alpha);
        }

        FRLDamageData FinalDamage = Skill->DamageData;
        FinalDamage.BaseDamage *= Multiplier;

        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        else OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, FinalDamage, true);
    }
    else if (CurrentSkillState == EPlayerSkillState::Casting)
    {
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        else OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
    }

    CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
}

bool URLSkillComponent::TryCastSkill(ESkillSlot Slot)
{
    OnSkillInputStarted(Slot);
    return true;
}

void URLSkillComponent::StartGroundTargetAiming(ESkillSlot Slot)
{
    URLSkillData* Skill = EquippedSkills.FindRef(Slot);
    if (!Skill || !DecalIndicator) return;

    CurrentSkillState = EPlayerSkillState::Aiming;
    CurrentActiveSlot = Slot;

    DecalIndicator->SetupIndicator(EIndicatorType::Circle, Skill->IndicatorRadius, Skill->MaxRange);
    DecalIndicator->SetIndicatorVisibility(true);
}

void URLSkillComponent::ConfirmGroundCast()
{
    if (CurrentSkillState != EPlayerSkillState::Aiming || !DecalIndicator || !OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (Skill)
    {
        FVector TargetPos = DecalIndicator->GetTargetLocation();

        FVector Dir = (TargetPos - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
        if (!Dir.IsNearlyZero())
        {
            OwnerCharacter->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
        }

        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }

        OwnerCharacter->CreateLocationHitbox(TargetPos, Skill->IndicatorRadius, Skill->DamageData, true);
        CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
    }

    CurrentSkillState = EPlayerSkillState::Idle;
    DecalIndicator->SetIndicatorVisibility(false);
}

void URLSkillComponent::CancelGroundCast()
{
    if (CurrentSkillState != EPlayerSkillState::Aiming) return;

    CurrentSkillState = EPlayerSkillState::Idle;
    if (DecalIndicator)
    {
        DecalIndicator->SetIndicatorVisibility(false);
    }
}