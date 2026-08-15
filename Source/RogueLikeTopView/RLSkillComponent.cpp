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

    // 인디케이터 액터 스폰 후 숨김 대기 (오브젝트 풀링)
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

    // 1. 장판 조준 상태일 때 마우스 커서 위치로 데칼 실시간 갱신
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

    // ★ 지점 지정(GroundTarget) 스킬 ➔ 조준 모드 진입
    if (Skill->CastType == ESkillCastType::GroundTarget)
    {
        StartGroundTargetAiming(Slot);
        return;
    }

    // 이미 조준 중인 상태에서 다른 스킬을 누르면 조준 취소
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

    CurrentAccumulatedTime += GetWorld()->GetDeltaSeconds();
    URLSkillData* Skill = EquippedSkills[Slot];
    if (!Skill) return;

    RotateOwnerToMouseCursor();

    // 홀딩 스킬 처리 로직 슬롯
    if (CurrentSkillState == EPlayerSkillState::Holding)
    {
        // 틱 데미지 주기 처리 가능
    }

    // 최대 시전 시간(CastDuration) 초과 시 자동 완료
    if (CurrentAccumulatedTime >= Skill->CastDuration)
    {
        OnSkillInputCompleted(Slot);
    }
}

void URLSkillComponent::OnSkillInputCompleted(ESkillSlot Slot)
{
    if (CurrentSkillState == EPlayerSkillState::Idle || CurrentActiveSlot != Slot) return;

    URLSkillData* Skill = EquippedSkills.FindRef(Slot);
    if (!Skill) return;

    // 1. 차징 스킬: 모은 시간에 비례한 데미지 증폭 (Lerp 공식)
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

        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }
        else
        {
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, FinalDamage, true);
        }
    }
    // 2. 캐스팅 스킬 완료 처리
    else if (CurrentSkillState == EPlayerSkillState::Casting)
    {
        if (Skill->SkillMontage)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }
        else
        {
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
        }
    }

    // 공통 쿨타임 등록 및 초기화
    CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);

    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("스킬 종료! (누른 시간: %.1f초)"), CurrentAccumulatedTime));
    CurrentSkillState = EPlayerSkillState::Idle;
    CurrentAccumulatedTime = 0.0f;
}

bool URLSkillComponent::TryCastSkill(ESkillSlot Slot)
{
    OnSkillInputStarted(Slot);
    return true;
}

// ─────────────────────────────────────────────
// 바닥 조준 (Ground Target) 세부 구현
// ─────────────────────────────────────────────
void URLSkillComponent::StartGroundTargetAiming(ESkillSlot Slot)
{
    URLSkillData* Skill = EquippedSkills.FindRef(Slot);
    if (!Skill || !DecalIndicator) return;

    CurrentSkillState = EPlayerSkillState::Aiming;
    CurrentActiveSlot = Slot;

    DecalIndicator->SetupIndicator(EIndicatorType::Circle, Skill->IndicatorRadius);
    DecalIndicator->SetIndicatorVisibility(true);

    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("🎯 조준 모드: [좌클릭: 시전 / 우클릭: 취소]"));
}

void URLSkillComponent::ConfirmGroundCast()
{
    if (CurrentSkillState != EPlayerSkillState::Aiming || !DecalIndicator || !OwnerCharacter) return;

    URLSkillData* Skill = EquippedSkills.FindRef(CurrentActiveSlot);
    if (Skill)
    {
        FVector TargetPos = DecalIndicator->GetActorLocation();
        FVector Dir = (TargetPos - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
        if (!Dir.IsNearlyZero())
        {
            OwnerCharacter->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
        }

        // 조준 지점 폭발 히트박스 발동
        OwnerCharacter->CreateBoxHitbox(FVector(Skill->IndicatorRadius, Skill->IndicatorRadius, 100.f), 0.f, Skill->DamageData, true);
        CooldownEndTimes.Add(CurrentActiveSlot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);

        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("💥 [%s] 지점 폭발 완료!"), *Skill->SkillName.ToString()));
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
    GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("🚫 스킬 조준 취소"));
}