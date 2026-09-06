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
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

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
        if (Skill)
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
    {
        UAnimMontage* MontageToPlay = Skill->SkillMontage;

        // ===================================================================
        // LMB(평타) 전용 콤보 로직.
        // 아래 세 조건을 전부 만족해야만 실행됨:
        //   1) 이 입력이 정확히 Slot_LMB일 때
        //   2) 스킬 데이터에서 bUseCombo를 명시적으로 켰을 때
        //   3) ComboMontages 배열에 실제로 몽타주가 들어있을 때
        // 친구들 스킬(다른 슬롯이든, 같은 Slot_LMB든)은 bUseCombo가 기본 false라서
        // 절대 이 블록에 들어오지 않고, 원래 로직(MontageToPlay = SkillMontage)만 탐.
        // ===================================================================
        if (Slot == ESkillSlot::Slot_LMB && Skill->bUseCombo && Skill->ComboMontages.Num() > 0)
        {
            if (ComboCount > 0 && ComboCount - 1 < Skill->ComboMontages.Num())
            {
                MontageToPlay = Skill->ComboMontages[ComboCount - 1];
            }
            ComboCount = (ComboCount + 1) % (Skill->ComboMontages.Num() + 1);

            GetWorld()->GetTimerManager().ClearTimer(ComboResetTimerHandle);
            GetWorld()->GetTimerManager().SetTimer(
                ComboResetTimerHandle, this, &URLSkillComponent::ResetCombo, Skill->ComboResetTime, false);
        }
        // ===================================================================

        if (MontageToPlay)
        {
            OwnerCharacter->PlayAnimMontage(MontageToPlay);
        }

        // ===== 평타(Instant) 칠 때도 이펙트 스폰 =====
        if (Skill->ReleaseVFX)
        {
            SpawnSkillVFX(
                Skill->ReleaseVFX,
                OwnerCharacter->GetActorLocation(),
                Skill->bAttachToWeapon,
                Skill->WeaponSocketName
            );
        }
        // ==============================================================

        if (!Skill->bHitOnAnimNotify)
        {
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
        }
        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;
    }

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

        FVector DashDirection = OwnerCharacter->GetActorForwardVector();
        FVector TargetLocation = OwnerCharacter->GetActorLocation() + DashDirection * (Skill->DashPower * 0.3f);

        OwnerCharacter->StartDashMovement(TargetLocation, 0.15f);

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

        // ===== 버프 지속 이펙트 스폰 (캐릭터에 부착, 버프 끝나면 제거) =====
        if (Skill->ReleaseVFX)
        {
            ActiveBuffVFXComp = SpawnSkillVFX(
                Skill->ReleaseVFX,
                OwnerCharacter->GetActorLocation(),
                true,       // 캐릭터에 부착
                NAME_None   // 특정 소켓 없이 메시 루트에 부착
            );
        }
        // ==============================================================

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

        if (Skill->ChargeVFX)
        {
            ActiveCastParticleComp = SpawnSkillVFX(
                Skill->ChargeVFX,
                PendingExplosionLocation,
                Skill->bAttachToWeapon,
                Skill->WeaponSocketName
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

        if (Skill->ReleaseVFX)
        {
            SpawnSkillVFX(
                Skill->ReleaseVFX,
                PendingExplosionLocation,
                Skill->bAttachToWeapon,
                Skill->WeaponSocketName
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
                ActiveCastParticleComp->DestroyComponent(); // 이 부분은 그대로 두셔도 잘 작동합니다.
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

            if (Skill->ReleaseVFX)
            {
                SpawnSkillVFX(
                    Skill->ReleaseVFX,
                    PendingExplosionLocation,
                    Skill->bAttachToWeapon,
                    Skill->WeaponSocketName
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

    // 수정된 부분: Destroy 대신 Deactivate 사용 (자연스러운 이펙트 소멸)
    if (ActiveCastParticleComp)
    {
        ActiveCastParticleComp->Deactivate();
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

void URLSkillComponent::TriggerActiveSkillDash()
{
    if (!OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    // 스킬 데이터가 있고, 스킬 타입이 Dash 계열일 때만 발동
    if (Skill && (Skill->CastType == ESkillCastType::Dash || Skill->CastType == ESkillCastType::ChargeDash))
    {
        FVector DashDirection = OwnerCharacter->GetActorForwardVector();
        FVector LaunchVelocity = DashDirection * Skill->DashPower;

        // 캐릭터를 앞으로 강하게 발사 (XY축 속도 덮어쓰기 True)
        OwnerCharacter->LaunchCharacter(LaunchVelocity, true, false);
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

    // 버프 이펙트도 같이 제거
    if (ActiveBuffVFXComp)
    {
        ActiveBuffVFXComp->Deactivate();
        ActiveBuffVFXComp = nullptr;
    }
}

// ↓↓↓ 여기 추가 ↓↓↓
void URLSkillComponent::ResetCombo()
{
    ComboCount = 0;
}
// ↑↑↑ 여기까지 ↑↑↑

// 나이아가라와 캐스케이드를 구분해서 소켓 부착 또는 바닥에 생성해 주는 완벽한 헬퍼 함수입니다.
// 나이아가라와 캐스케이드를 구분해서 소켓 부착 또는 바닥에 생성해 주는 완벽한 헬퍼 함수입니다.
UFXSystemComponent* URLSkillComponent::SpawnSkillVFX(UFXSystemAsset* VFXAsset, FVector Location, bool bAttach, FName SocketName)
{
    if (!VFXAsset || !OwnerCharacter) return nullptr;

    // =========================================================================
    // [핵심 추가] 소켓을 가진 진짜 메시(사람 몸뚱이인지 망치인지)를 스스로 찾아냅니다.
    // =========================================================================
    USceneComponent* TargetAttachComp = OwnerCharacter->GetMesh(); // 기본값은 사람 몸뚱이

    if (bAttach && SocketName != NAME_None)
    {
        // 캐릭터에 붙어있는 모든 메시 컴포넌트(무기, 장갑 등)를 배열로 가져옵니다.
        TArray<UMeshComponent*> AllMeshes;
        OwnerCharacter->GetComponents<UMeshComponent>(AllMeshes);

        // 가져온 메시들을 하나씩 검사해서 우리가 찾는 소켓(예: HammerHeadSocket)이 있는지 확인합니다.
        for (UMeshComponent* MeshComp : AllMeshes)
        {
            if (MeshComp->DoesSocketExist(SocketName))
            {
                TargetAttachComp = MeshComp; // 소켓이 망치에 있다면, 타겟을 망치로 바꿉니다!
                break; // 찾았으니 검색 종료
            }
        }
    }
    // =========================================================================

    // 1. 나이아가라 검사
    if (UNiagaraSystem* NiagaraSys = Cast<UNiagaraSystem>(VFXAsset))
    {
        if (bAttach && TargetAttachComp)
        {
            // OwnerCharacter->GetMesh() 대신 스스로 찾아낸 TargetAttachComp를 사용합니다.
            return Cast<UFXSystemComponent>(UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSys, TargetAttachComp, SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true));
        }
        return Cast<UFXSystemComponent>(UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraSys, Location, FRotator::ZeroRotator));
    }
    // 2. 캐스케이드 검사
    else if (UParticleSystem* CascadeSys = Cast<UParticleSystem>(VFXAsset))
    {
        if (bAttach && TargetAttachComp)
        {
            // OwnerCharacter->GetMesh() 대신 스스로 찾아낸 TargetAttachComp를 사용합니다.
            return Cast<UFXSystemComponent>(UGameplayStatics::SpawnEmitterAttached(CascadeSys, TargetAttachComp, SocketName, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true));
        }
        return Cast<UFXSystemComponent>(UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), CascadeSys, Location, FRotator::ZeroRotator));
    }

    return nullptr;
}