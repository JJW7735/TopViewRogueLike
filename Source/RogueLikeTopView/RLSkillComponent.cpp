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

bool URLSkillComponent::TryCastSkill(ESkillSlot Slot)
{
    if (!EquippedSkills.Contains(Slot) || !EquippedSkills[Slot])
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("슬롯에 등록된 스킬이 없습니다."));
        return false;
    }

    if (IsOnCooldown(Slot))
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Orange, FString::Printf(TEXT("쿨타임 중! 남은 시간: %.1f초"), GetRemainingCooldown(Slot)));
        return false;
    }

    URLSkillData* Skill = EquippedSkills[Slot];

    // 1. 기존 우클릭 이동 멈춤
    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
    }

    // 2. 마우스 커서 방향으로 즉시 몸 돌리기
    RotateOwnerToMouseCursor();

    // 3. 스킬 시전 처리
    switch (Skill->CastType)
    {
    case ESkillCastType::Instant:
        // 애니메이션 몽타주가 있다면 재생, 없다면 즉시 히트박스 생성
        if (Skill->SkillMontage && OwnerCharacter)
        {
            OwnerCharacter->PlayAnimMontage(Skill->SkillMontage);
        }
        else if (OwnerCharacter)
        {
            // 몽타주가 아직 없을 때 기본 박스 히트박스 즉시 발동
            OwnerCharacter->CreateBoxHitbox(Skill->HitboxExtent, Skill->HitboxForwardOffset, Skill->DamageData, true);
        }
        break;

    case ESkillCastType::Cast:
    case ESkillCastType::Charge:
        // 추후 캐스팅 게이지 및 몽타주 루프 연결
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("[%s] 캐스팅/차징 시작! (%.1f초)"), *Skill->SkillName.ToString(), Skill->CastDuration));
        break;

    default:
        break;
    }

    // 4. 쿨타임 시작
    CooldownEndTimes.Add(Slot, GetWorld()->GetTimeSeconds() + Skill->Cooldown);

    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("⚡ [%s] 스킬 시전 완료! (쿨타임 %.1f초)"), *Skill->SkillName.ToString(), Skill->Cooldown));
    return true;
}