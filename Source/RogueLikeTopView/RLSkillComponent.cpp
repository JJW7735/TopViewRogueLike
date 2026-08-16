#include "RLSkillComponent.h"
#include "RLCharacter.h"
#include "RLDecalIndicator.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RLProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"

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

    // 1. 조준(Aiming) 상태: 플레이어가 이동해도 마우스 커서 위치 실시간 추적
    if (CurrentSkillState == EPlayerSkillState::Aiming && DecalIndicator && OwnerCharacter)
    {
        if (URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot))
        {
            DecalIndicator->UpdateIndicatorTransform(OwnerCharacter->GetActorLocation(), GetMouseGroundLocation(), Skill->MaxRange);
        }
    }

    // 2. 캐스팅(Casting) 상태: 100% 도달할 때까지 게이지 누적
    if (CurrentSkillState == EPlayerSkillState::Casting)
    {
        URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
        if (!Skill)
        {
            InterruptCasting(TEXT("스킬 데이터 누락"));
            return;
        }

        CurrentAccumulatedTime += DeltaTime;
        float Ratio = FMath::Clamp(CurrentAccumulatedTime / FMath::Max(0.01f, Skill->CastDuration), 0.0f, 1.0f);

        // UI에 게이지 브로드캐스트
        OnCastProgressChanged.Broadcast(CurrentActiveSlot, CurrentAccumulatedTime, Skill->CastDuration, Ratio);

        // 일반 전방 캐스팅일 때만 마우스 방향 회전 허용
        if (Skill->CastType != ESkillCastType::GroundTarget)
        {
            RotateOwnerToMouseCursor();
        }

        // ★ 100% 끝까지 시전 완료했을 때만 발동!
        if (CurrentAccumulatedTime >= Skill->CastDuration)
        {
            FinishCasting();
        }
    }

    // 3. 쿨타임 UI 브로드캐스트
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

    // ① 지점 조작 스킬 (GroundTarget): 이동 멈춤 없이 조준 가이드만 활성화!
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

    StopPlayerMovement();
    RotateOwnerToMouseCursor();

    switch (Skill->CastType)
    {
    case ESkillCastType::Instant:
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        else OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
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
        if (Skill->SkillMontage) OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);

        FVector DashDirection = OwnerCharacter->GetActorForwardVector();
        OwnerCharacter->LaunchCharacter(DashDirection * Skill->DashPower, true, true);

        OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);

        CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
        CurrentSkillState = EPlayerSkillState::Idle;
        break;
    }

    case ESkillCastType::Charge:
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
            CurrentAccumulatedTime = Skill->CastDuration; // 오차 방지를 위해 최대치로 고정
            OnSkillInputCompleted(Slot); // 손을 떼지 않아도 강제로 완료(발사) 처리
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
// 4. 좌클릭 확정 (ConfirmGroundCast) ➔ 이동 멈추고 캐스팅 돌입
// ========================================================
void URLSkillComponent::ConfirmGroundCast()
{
    if (CurrentSkillState != EPlayerSkillState::Aiming || !DecalIndicator || !OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (!Skill) return;

    // 1. 마우스로 찍은 목표 위치 저장
    PendingExplosionLocation = DecalIndicator->GetTargetLocation();

    // 2. 캐릭터 회전 및 이동 정지
    FVector Dir = (PendingExplosionLocation - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
    if (!Dir.IsNearlyZero())
    {
        OwnerCharacter->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
    }
    StopPlayerMovement();

    // 3. 지점 캐스팅 모드 시작 (CastDuration > 0)
    if (Skill->CastDuration > 0.0f)
    {
        CurrentSkillState = EPlayerSkillState::Casting;
        CurrentAccumulatedTime = 0.0f;

        // 바닥에 장판 고정
        DecalIndicator->LockTargetLocation(PendingExplosionLocation);

        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }

        // 캐스팅 UI 켜기
        OnCastStateChanged.Broadcast(CurrentActiveSlot, true);
    }
    else
    {
        // 시전 시간이 0이면 즉발 폭발
        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }

        OwnerCharacter->CreateLocationHitbox(PendingExplosionLocation, Skill->IndicatorRadius, Skill->DamageData, true);
        CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);

        DecalIndicator->SetIndicatorVisibility(false);
        CurrentSkillState = EPlayerSkillState::Idle;
    }
}

// ========================================================
// 5. ★ 캐스팅 100% 완료 ➔ 이때만 데미지 발동 & 쿨타임 등록
// ========================================================
void URLSkillComponent::FinishCasting()
{
    if (CurrentSkillState != EPlayerSkillState::Casting || !OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (Skill)
    {
        // ① 지점 캐스팅 스킬 ➔ 목표 지점에 강력한 폭발 발생!
        if (Skill->CastType == ESkillCastType::GroundTarget)
        {
            OwnerCharacter->CreateLocationHitbox(PendingExplosionLocation, Skill->IndicatorRadius, Skill->DamageData, true);
        }
        // ② 전방 캐스팅 스킬 ➔ 전방 박스 판정 발생
        else
        {
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
        }

        // 쿨타임 등록 (성공 시에만 적용)
        CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);
    }

    // 장판 데칼 끄기 & UI 닫기
    if (DecalIndicator)
    {
        DecalIndicator->SetIndicatorVisibility(false);
    }

    OnCastStateChanged.Broadcast(CurrentActiveSlot, false);
    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
}

// ========================================================
// 6. ★ 캐스팅 도중 끊김/취소 ➔ 스킬 완전 불발 (장판 삭제 & 데미지 0)
// ========================================================
void URLSkillComponent::InterruptCasting(const FString& Reason)
{
    if (CurrentSkillState != EPlayerSkillState::Casting) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (OwnerCharacter && Skill && Skill->SkillMontage)
    {
        OwnerCharacter->StopAnimMontage(Skill->SkillMontage);
    }

    // ★ 장판 데칼 즉시 삭제 (바닥에 남아있지 않음)
    if (DecalIndicator)
    {
        DecalIndicator->SetIndicatorVisibility(false);
    }

    // 캐스팅 UI 끄기
    OnCastStateChanged.Broadcast(CurrentActiveSlot, false);

    // ★ 데미지 판정 없이 상태만 초기화 (스킬 완전 실패)
    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
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