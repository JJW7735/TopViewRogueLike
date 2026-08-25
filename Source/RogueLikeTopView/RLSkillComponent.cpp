#include "RLSkillComponent.h"
#include "RLCharacter.h"
#include "RLDecalIndicator.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RLProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Animation/AnimInstance.h"

URLSkillComponent::URLSkillComponent()
{
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
}

void URLSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (CurrentSkillState == EPlayerSkillState::Aiming && DecalIndicator && OwnerCharacter)
    {
        if (URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot))
        {
            DecalIndicator->UpdateIndicatorTransform(OwnerCharacter->GetActorLocation(), GetMouseGroundLocation(), Skill->MaxRange);
        }
    }

    if (CurrentSkillState == EPlayerSkillState::Casting)
    {
        URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
        if (!Skill)
        {
            InterruptCasting(TEXT("스킬 데이터 누락"));
            return;
        }

        CurrentAccumulatedTime += DeltaTime;

        // 캐스팅 종료 0.5초 전에 End(공격) 모션 미리 점프
        const float PreMotionLeadTime = 0.5f;
        const float MotionTriggerTime = FMath::Max(0.0f, Skill->CastDuration - PreMotionLeadTime);

        if (!bHasTriggeredPreCastMotion && CurrentAccumulatedTime >= MotionTriggerTime)
        {
            bHasTriggeredPreCastMotion = true;
            if (Skill->SkillMontage && OwnerCharacter)
            {
                if (UAnimInstance* AnimInst = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr)
                {
                    AnimInst->Montage_JumpToSection(FName("End"), Skill->SkillMontage);
                }
            }
        }

        float Ratio = FMath::Clamp(CurrentAccumulatedTime / FMath::Max(0.01f, Skill->CastDuration), 0.0f, 1.0f);
        OnCastProgressChanged.Broadcast(CurrentActiveSlot, CurrentAccumulatedTime, Skill->CastDuration, Ratio);

        if (Skill->CastType != ESkillCastType::GroundTarget)
        {
            RotateOwnerToMouseCursor();
        }

        if (CurrentAccumulatedTime >= Skill->CastDuration)
        {
            FinishCasting();
        }
    }

    for (const auto& Pair : EquippedSkills)
    {
        ESkillSlot Slot = Pair.Key;
        URLSkillData* Skill = Pair.Value;
        if (Skill && IsOnCooldown(Slot))
        {
            OnSkillCooldownChanged.Broadcast(Slot, GetRemainingCooldown(Slot), Skill->Cooldown);
        }
    }
}

// ========================================================
// 1. 스킬 입력 시작 (Started)
// ========================================================
void URLSkillComponent::OnSkillInputStarted(ESkillSlot Slot)
{
    if (!EquippedSkills.Contains(Slot) || !EquippedSkills[Slot]) return;
    if (IsOnCooldown(Slot)) return;

    URLSkillData* Skill = EquippedSkills[Slot];

    if (Skill->CastType == ESkillCastType::GroundTarget)
    {
        StartGroundTargetAiming(Slot);
        return;
    }

    if (CurrentSkillState == EPlayerSkillState::Aiming)
    {
        CancelGroundCast();
    }

    if (CurrentSkillState != EPlayerSkillState::Idle) return;

    CurrentActiveSlot = Slot;
    CurrentAccumulatedTime = 0.0f;
    LastHoldTickTime = 0.0f;

    if (Skill->CastType != ESkillCastType::Dash)
    {
        StopPlayerMovement();
    }
    RotateOwnerToMouseCursor();

    switch (Skill->CastType)
    {
    case ESkillCastType::Instant:
        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }
        OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;

    case ESkillCastType::Projectile:
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        if (Skill->ProjectileClass)
        {
            FVector SpawnLoc = OwnerCharacter->GetActorLocation() + (OwnerCharacter->GetActorForwardVector() * Skill->ProjectileSpawnForwardOffset);
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = OwnerCharacter;
            SpawnParams.Instigator = OwnerCharacter;

            ARLProjectile* SpawnedProjectile = GetWorld()->SpawnActor<ARLProjectile>(
                Skill->ProjectileClass, SpawnLoc, OwnerCharacter->GetActorRotation(), SpawnParams);

            if (SpawnedProjectile)
            {
                SpawnedProjectile->DamageData = Skill->DamageData;
                SpawnedProjectile->MovementComp->Velocity =
                    OwnerCharacter->GetActorRotation().Vector() * Skill->ProjectileSpeed;
            }
        }
        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;

    case ESkillCastType::Cast:
        StartCasting(Slot);
        break;

    case ESkillCastType::Dash:
    {
        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }

        const FVector DashDirection = OwnerCharacter->GetActorForwardVector();
        const FVector NewLocation = OwnerCharacter->GetActorLocation() + DashDirection * (Skill->DashPower * 0.05f);

        OwnerCharacter->SetActorLocation(NewLocation, false);
        OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);

        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;
    }

    case ESkillCastType::Charge:
    case ESkillCastType::ChargeDash:
        CurrentSkillState = EPlayerSkillState::Charging;
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        break;

    case ESkillCastType::Buff:
    {
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);

        if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
        {
            MoveComp->MaxWalkSpeed = MoveComp->MaxWalkSpeed * Skill->SpeedMultiplier;

            GetWorld()->GetTimerManager().SetTimer(
                SpeedBuffTimerHandle,
                this,
                &URLSkillComponent::EndSpeedBuff,
                Skill->BuffDuration,
                false
            );
        }

        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;
    }

    case ESkillCastType::Hold:
        CurrentSkillState = EPlayerSkillState::Holding;
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        break;

    default:
        break;
    }
}

// ========================================================
// 2. 키 누르고 있는 중 (Triggered - 차징 / 홀딩)
// ========================================================
void URLSkillComponent::OnSkillInputTriggered(ESkillSlot Slot)
{
    if (CurrentActiveSlot != Slot) return;

    URLSkillData* Skill = EquippedSkills.FindRef(Slot);
    if (!Skill) return;

    if (CurrentSkillState == EPlayerSkillState::Holding)
    {
        RotateOwnerToMouseCursor();
        CurrentAccumulatedTime += GetWorld()->GetDeltaSeconds();

        if (CurrentAccumulatedTime - LastHoldTickTime >= Skill->TickDamageInterval)
        {
            LastHoldTickTime = CurrentAccumulatedTime;
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, false);
        }

        if (CurrentAccumulatedTime >= Skill->CastDuration)
        {
            OnSkillInputCompleted(Slot);
        }
    }
    else if (CurrentSkillState == EPlayerSkillState::Charging)
    {
        RotateOwnerToMouseCursor();
        CurrentAccumulatedTime = FMath::Min(CurrentAccumulatedTime + GetWorld()->GetDeltaSeconds(), Skill->CastDuration);

        if (CurrentAccumulatedTime >= Skill->CastDuration)
        {
            CurrentAccumulatedTime = Skill->CastDuration;
            OnSkillInputCompleted(Slot);
        }
    }
}

// ========================================================
// 3. 키 뗌 (Completed)
// ========================================================
void URLSkillComponent::OnSkillInputCompleted(ESkillSlot Slot)
{
    if (CurrentSkillState == EPlayerSkillState::Casting || CurrentSkillState == EPlayerSkillState::Aiming || CurrentActiveSlot != Slot)
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
            float Denominator = FMath::Max(0.01f, Skill->CastDuration - Skill->MinChargeTime);
            float Alpha = FMath::Clamp((CurrentAccumulatedTime - Skill->MinChargeTime) / Denominator, 0.0f, 1.0f);
            Multiplier = FMath::Lerp(1.0f, Skill->MaxDamageMultiplier, Alpha);
        }

        if (Skill->CastType == ESkillCastType::ChargeDash && CurrentAccumulatedTime >= Skill->MinChargeTime)
        {
            FVector DashDirection = OwnerCharacter->GetActorForwardVector();
            OwnerCharacter->LaunchCharacter(DashDirection * Skill->DashPower, true, true);
        }

        FRLDamageData FinalDamage = Skill->DamageData;
        FinalDamage.BaseDamage *= Multiplier;

        OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, FinalDamage, true);
        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
    }
    else if (CurrentSkillState == EPlayerSkillState::Holding)
    {
        if (Skill->SkillMontage) OwnerCharacter->StopAnimMontage(Skill->SkillMontage);
        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
    }

    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
}

// ========================================================
// 4. 좌클릭 확정 (ConfirmGroundCast) ➔ 지점 설치
// ========================================================
void URLSkillComponent::ConfirmGroundCast()
{
    if (CurrentSkillState != EPlayerSkillState::Aiming || !DecalIndicator || !OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (!Skill) return;

    PendingExplosionLocation = DecalIndicator->GetTargetLocation();

    FVector Dir = (PendingExplosionLocation - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
    if (!Dir.IsNearlyZero())
    {
        OwnerCharacter->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
    }
    StopPlayerMovement();

    if (Skill->CastDuration > 0.0f)
    {
        CurrentSkillState = EPlayerSkillState::Casting;
        CurrentAccumulatedTime = 0.0f;
        bHasTriggeredPreCastMotion = false;

        DecalIndicator->LockTargetLocation(PendingExplosionLocation);

        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }

        if (Skill->ChargeParticle)
        {
            ActiveCastParticleComp = UGameplayStatics::SpawnEmitterAtLocation(
                GetWorld(),
                Skill->ChargeParticle,
                PendingExplosionLocation,
                FRotator::ZeroRotator,
                true
            );
        }

        OnCastStateChanged.Broadcast(CurrentActiveSlot, true);
    }
    else
    {
        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }

        if (Skill->ReleaseParticle)
        {
            UGameplayStatics::SpawnEmitterAtLocation(
                GetWorld(),
                Skill->ReleaseParticle,
                PendingExplosionLocation,
                FRotator::ZeroRotator,
                true
            );
        }

        if (Skill->GroundActorClass)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = OwnerCharacter;
            SpawnParams.Instigator = OwnerCharacter;

            GetWorld()->SpawnActor<AActor>(
                Skill->GroundActorClass,
                PendingExplosionLocation,
                FRotator::ZeroRotator,
                SpawnParams
            );
        }
        else
        {
            OwnerCharacter->CreateLocationHitbox(PendingExplosionLocation, Skill->IndicatorRadius, Skill->DamageData, true);
        }

        CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);

        DecalIndicator->SetIndicatorVisibility(false);
        CurrentSkillState = EPlayerSkillState::Idle;
    }
}

// ========================================================
// 5. 캐스팅 100% 완료
// ========================================================
void URLSkillComponent::FinishCasting()
{
    if (CurrentSkillState != EPlayerSkillState::Casting || !OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (Skill)
    {
        if (Skill->CastType == ESkillCastType::GroundTarget)
        {
            if (ActiveCastParticleComp)
            {
                ActiveCastParticleComp->DeactivateSystem();
                ActiveCastParticleComp = nullptr;
            }

            // 만약 CastDuration이 0.5초 이하라서 선행 점프를 못 탄 경우를 대비한 안전 장치
            if (!bHasTriggeredPreCastMotion && Skill->SkillMontage)
            {
                if (UAnimInstance* AnimInst = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr)
                {
                    AnimInst->Montage_JumpToSection(FName("End"), Skill->SkillMontage);
                }
            }

            if (Skill->ReleaseParticle)
            {
                UGameplayStatics::SpawnEmitterAtLocation(
                    GetWorld(),
                    Skill->ReleaseParticle,
                    PendingExplosionLocation,
                    FRotator::ZeroRotator,
                    true
                );
            }

            if (Skill->GroundActorClass)
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = OwnerCharacter;
                SpawnParams.Instigator = OwnerCharacter;

                GetWorld()->SpawnActor<AActor>(
                    Skill->GroundActorClass,
                    PendingExplosionLocation,
                    FRotator::ZeroRotator,
                    SpawnParams
                );
            }
            else
            {
                OwnerCharacter->CreateLocationHitbox(PendingExplosionLocation, Skill->IndicatorRadius, Skill->DamageData, true);
            }
        }
        else
        {
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
        }

        CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
    }

    if (DecalIndicator)
    {
        DecalIndicator->SetIndicatorVisibility(false);
    }

    OnCastStateChanged.Broadcast(CurrentActiveSlot, false);
    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
    bHasTriggeredPreCastMotion = false;
}

// ========================================================
// 6. 캐스팅 끊김 / 캔슬 처리
// ========================================================
void URLSkillComponent::InterruptCasting(const FString& Reason)
{
    if (CurrentSkillState != EPlayerSkillState::Casting) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (OwnerCharacter && Skill && Skill->SkillMontage)
    {
        OwnerCharacter->StopAnimMontage(Skill->SkillMontage);
    }

    if (ActiveCastParticleComp)
    {
        ActiveCastParticleComp->DestroyComponent();
        ActiveCastParticleComp = nullptr;
    }

    if (DecalIndicator)
    {
        DecalIndicator->SetIndicatorVisibility(false);
    }

    OnCastStateChanged.Broadcast(CurrentActiveSlot, false);

    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
    bHasTriggeredPreCastMotion = false;
}

void URLSkillComponent::TriggerActiveSkillHit()
{
    if (!OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (Skill)
    {
        OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
    }
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

void URLSkillComponent::StartCasting(ESkillSlot Slot)
{
    URLSkillData* Skill = EquippedSkills.FindRef(Slot);
    if (!Skill || !OwnerCharacter) return;

    CurrentSkillState = EPlayerSkillState::Casting;
    CurrentActiveSlot = Slot;
    CurrentAccumulatedTime = 0.0f;
    bHasTriggeredPreCastMotion = false;

    StopPlayerMovement();

    if (Skill->SkillMontage)
    {
        OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
    }

    OnCastStateChanged.Broadcast(Slot, true);
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

void URLSkillComponent::StopPlayerMovement()
{
    if (!OwnerCharacter) return;
    if (OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
    }
    if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
    {
        PC->StopMovement();
    }
}

void URLSkillComponent::RotateOwnerToMouseCursor()
{
    if (!OwnerCharacter) return;
    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC) return;

    FHitResult Hit;
    if (PC->GetHitResultUnderCursor(ECC_Visibility, true, Hit))
    {
        FVector LookDir = (Hit.ImpactPoint - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
        if (!LookDir.IsNearlyZero())
        {
            OwnerCharacter->SetActorRotation(FRotator(0.f, LookDir.Rotation().Yaw, 0.f));
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

void URLSkillComponent::EndSpeedBuff()
{
    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed = 600.0f;
    }
}