// Fill out your copyright notice in the Description page of Project Settings.

#include "RLCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "RLHealthComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "RLSkillComponent.h"
#include "RLStatComponent.h"
#include "RLSkillData.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "RLProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RLPortal.h"

ARLCharacter::ARLCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->TargetArmLength = 1300.f;
    CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
    CameraBoom->bDoCollisionTest = false;

    TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    TopDownCamera->bUsePawnControlRotation = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
    GetCharacterMovement()->bConstrainToPlane = true;
    GetCharacterMovement()->bRequestedMoveUseAcceleration = true;
    GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;
    GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;

    bUseControllerRotationYaw = false;

    HealthComp = CreateDefaultSubobject<URLHealthComponent>(TEXT("HealthComp"));
    SkillComp = CreateDefaultSubobject<URLSkillComponent>(TEXT("SkillComp"));
    StatComp = CreateDefaultSubobject<URLStatComponent>(TEXT("StatComp"));
}

void ARLCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (HealthComp)
    {
        HealthComp->OnDeath.AddDynamic(this, &ARLCharacter::HandleDeath);
    }

    if (SkillComp)
    {
        if (DefaultSkill_Q) SkillComp->EquipSkill(ESkillSlot::Slot_Q, DefaultSkill_Q);
        if (DefaultSkill_W) SkillComp->EquipSkill(ESkillSlot::Slot_W, DefaultSkill_W);
        if (DefaultSkill_E) SkillComp->EquipSkill(ESkillSlot::Slot_E, DefaultSkill_E);
        if (DefaultSkill_R) SkillComp->EquipSkill(ESkillSlot::Slot_R, DefaultSkill_R);
        if (DefaultSkill_LMB) SkillComp->EquipSkill(ESkillSlot::Slot_LMB, DefaultSkill_LMB);
    }

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;

        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false); // 드래그 중에도 커서 숨기지 않음!
        PC->SetInputMode(InputMode);
    }

    if (StatComp && GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = StatComp->MoveSpeed;
    }
}

void ARLCharacter::ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo)
{
    // ★ 같은 팀이면 피해를 무시합니다 (아군 피해 방지).
    // 근접 박스 판정 / 장판 판정 / 투사체 판정이 전부 이 함수를 거쳐서 데미지를 넣기 때문에,
    // 여기 한 곳만 막아주면 세 가지 공격 방식 모두 한 번에 아군 피해가 방지됩니다.
    if (ARLCharacter* CauserCharacter = Cast<ARLCharacter>(DamageData.DamageCauser))
    {
        if (CauserCharacter->TeamID == TeamID)
        {
            return;
        }
    }

    if (bIsInvulnerable && !DamageData.bIgnoreInvincibility)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Dodge / Invulnerable!"));
        return;
    }

    FString CauserName = DamageData.DamageCauser ? DamageData.DamageCauser->GetName() : TEXT("Environment");
    if (DamageData.bIsCritical)
    {
        FString Msg = FString::Printf(TEXT("⚡⚡ [CRITICAL HIT!!] [%s] 피격! 데미지: %.1f (공격자: %s) ⚡⚡"), *GetName(), DamageData.BaseDamage, *CauserName);
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange, Msg);
    }
    else
    {
        FString Msg = FString::Printf(TEXT("💥 [%s] 피격! 데미지: %.1f (공격자: %s)"), *GetName(), DamageData.BaseDamage, *CauserName);
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Msg);
    }

    if (DamageData.KnockbackForce > 0.0f)
    {
        float KnockbackMultiplier = DamageData.bIsCritical ? 1.25f : 1.0f;
        FVector KnockbackDir = (GetActorLocation() - HitInfo.ImpactPoint).GetSafeNormal2D();
        LaunchCharacter(KnockbackDir * (DamageData.KnockbackForce * KnockbackMultiplier) + FVector(0.f, 0.f, 150.f), true, true);
    }

    AController* InstigatorController = DamageData.DamageCauser ? DamageData.DamageCauser->GetInstigatorController() : nullptr;
    UGameplayStatics::ApplyDamage(this, DamageData.BaseDamage, InstigatorController, DamageData.DamageCauser, UDamageType::StaticClass());

    if (SkillComp && SkillComp->IsCasting())
    {
        SkillComp->InterruptCasting(TEXT("피격으로 인한 시전 취소"));
    }
}

// ★ BP에서 FHitResult를 직접 조립하기 어려우니, ImpactPoint만 받아서 내부에서 FHitResult를 만들어
// ReceiveCustomDamage로 넘겨주는 래퍼입니다. (인페르노/메테오 같은 장판형 BP에서 사용)
void ARLCharacter::ReceiveCustomDamageAtLocation(const FRLDamageData& DamageData, FVector ImpactPoint)
{
    FHitResult Hit;
    Hit.ImpactPoint = ImpactPoint;
    ReceiveCustomDamage(DamageData, Hit);
}

void ARLCharacter::HandleDeath()
{
    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Black, FString::Printf(TEXT("💀 [%s] 체력이 0이 되어 사망했습니다!"), *GetName()));

    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    OnCharacterDeath();
}


void ARLCharacter::SetNearbyPortal(ARLPortal* Portal)
{
    NearbyPortal = Portal;
    OnNearbyPortalChanged(Portal != nullptr);
}

void ARLCharacter::TryInteract()
{
    if (NearbyPortal)
    {
        NearbyPortal->ActivatePortal(this);
    }
}

void ARLCharacter::CreateBoxHitbox(FVector BoxExtent, float ForwardOffset, const FRLDamageData& DamageData, bool bDrawDebug)
{
    FVector Center = GetActorLocation() + (GetActorForwardVector() * ForwardOffset);
    FRotator Rotation = GetActorRotation();

    TArray<FHitResult> HitResults;
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    EDrawDebugTrace::Type DebugType = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

    bool bHit = UKismetSystemLibrary::BoxTraceMulti(
        GetWorld(),
        Center,
        Center,
        BoxExtent,
        Rotation,
        UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1),
        false,
        ActorsToIgnore,
        DebugType,
        HitResults,
        true,
        FLinearColor::Red,
        FLinearColor::Green,
        0.5f
    );

    if (bHit)
    {
        TArray<AActor*> HitActors;

        for (const FHitResult& Hit : HitResults)
        {
            AActor* TargetActor = Hit.GetActor();
            if (TargetActor && !HitActors.Contains(TargetActor))
            {
                HitActors.Add(TargetActor);

                if (ARLCharacter* TargetCharacter = Cast<ARLCharacter>(TargetActor))
                {
                    FRLDamageData FinalData = DamageData;
                    FinalData.DamageCauser = this;
                    TargetCharacter->ReceiveCustomDamage(FinalData, Hit);
                }
            }
        }
    }
}

void ARLCharacter::TestAttack()
{
    FRLDamageData AttackData;
    AttackData.BaseDamage = 25.0f;
    AttackData.KnockbackForce = 0.0f;

    CreateBoxHitbox(FVector(100.f, 80.f, 50.f), 130.f, AttackData, true);
}

void ARLCharacter::Dodge(FVector CursorLocation)
{
    if (!bCanDash) return; // 쿨타임 중이면 무시

    FVector MyLocation = GetActorLocation();
    FVector DashDirection = CursorLocation - MyLocation;
    DashDirection.Z = 0.0f;

    if (DashDirection.IsNearlyZero()) return;
    DashDirection.Normalize();

    bCanDash = false;
    bIsInvulnerable = true;

    // 기존 이동 정지
    if (AController* PC = GetController())
    {
        PC->StopMovement();
    }

    // =====================================================================
    // ★ [거리와 속도 분리 계산의 핵심]
    // 1. 어디까지 갈 것인가? (현재 위치 + 방향 * 설정한 거리)
    FVector TargetLocation = MyLocation + (DashDirection * DashDistance);

    // 2. 그 거리를 이 속도로 가려면 몇 초가 걸리는가? (시간 = 거리 / 속력)
    float CalculatedDuration = DashDistance / DashSpeed;

    // 3. 물리 엔진 대신, 우리가 만든 정확한 좌표 이동 함수에 꽂아 넣기!
    StartDashMovement(TargetLocation, CalculatedDuration);
    // =====================================================================

    SetActorRotation(DashDirection.Rotation());

    if (DashMontage)
    {
        PlayAnimMontage(DashMontage);
    }

    if (SkillComp && SkillComp->IsCasting())
    {
        SkillComp->InterruptCasting(TEXT("회피기로 인한 시전 취소"));
    }

    GetWorldTimerManager().SetTimer(InvincibilityTimerHandle, this, &ARLCharacter::EndInvincibility, InvincibilityDuration, false);
    GetWorldTimerManager().SetTimer(DashCooldownTimerHandle, this, &ARLCharacter::ResetDashCooldown, DashCooldown, false);
}

void ARLCharacter::EndInvincibility()
{
    bIsInvulnerable = false;
}

void ARLCharacter::ResetDashCooldown()
{
    bCanDash = true;
}

// ★ 절대 방어막 (적 AI가 ApplyDamage를 써도 무적이면 다 씹어버림)
float ARLCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsInvulnerable)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("대쉬 무적 판정! 대미지 무효화!"));
        return 0.0f;
    }
    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

// ★ 부드러운 대시 이동 시작
void ARLCharacter::StartDashMovement(FVector TargetLocation, float Duration)
{
    DashStartLocation = GetActorLocation();
    DashTargetLocation = TargetLocation;
    DashDuration = FMath::Max(0.01f, Duration);
    DashElapsedTime = 0.0f;
    bIsDashing = true;
    bIsSkillMoving = true; // Tick의 이동 억제 로직을 우회
}

void ARLCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ★ 대시 진행 중이면 매 프레임 부드럽게 이동
    if (bIsDashing)
    {
        DashElapsedTime += DeltaTime;
        float Alpha = FMath::Clamp(DashElapsedTime / DashDuration, 0.0f, 1.0f);
        FVector NewLocation = FMath::Lerp(DashStartLocation, DashTargetLocation, Alpha);
        SetActorLocation(NewLocation, true);

        if (Alpha >= 1.0f)
        {
            bIsDashing = false;
            bIsSkillMoving = false; // 대시 끝나면 다시 이동 억제 로직 활성화
        }
        return; // 대시 중엔 아래 몽타주 이동 억제 로직 건너뜀
    }

    if (bIsSkillMoving)
    {
        return;
    }


    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        if (AnimInstance->IsAnyMontagePlaying())
        {
            if (GetCharacterMovement())
            {
                GetCharacterMovement()->StopMovementImmediately();
            }

            if (APlayerController* PC = Cast<APlayerController>(GetController()))
            {
                PC->StopMovement();
            }
        }
    }
}

void ARLCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ARLCharacter::CreateLocationHitbox(FVector TargetLocation, float Radius, const FRLDamageData& DamageData, bool bDrawDebug)
{
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<FHitResult> HitResults;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    EDrawDebugTrace::Type DebugTrace = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

    bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
        GetWorld(),
        TargetLocation,
        TargetLocation + FVector(0.f, 0.f, 1.f),
        Radius,
        ObjectTypes,
        false,
        ActorsToIgnore,
        DebugTrace,
        HitResults,
        true,
        FLinearColor::Red,
        FLinearColor::Green,
        1.5f
    );

    if (bHit)
    {
        TSet<AActor*> DamagedActors;
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (HitActor && !DamagedActors.Contains(HitActor))
            {
                DamagedActors.Add(HitActor);
                if (ARLCharacter* TargetChar = Cast<ARLCharacter>(HitActor))
                {
                    FRLDamageData FinalDamage = DamageData;
                    FinalDamage.DamageCauser = this;
                    TargetChar->ReceiveCustomDamage(FinalDamage, Hit);
                }
            }
        }
    }
}

// ★ 원거리 공격: RLSkillComponent의 ESkillCastType::Projectile 분기와 동일한 로직입니다.
// (플레이어 스킬 투사체와 몬스터 원거리 패턴 투사체가 같은 방식으로 스폰되도록 통일)
void ARLCharacter::SpawnProjectileAttack(TSubclassOf<AActor> ProjectileClass, float ForwardOffset, float ProjectileSpeed, const FRLDamageData& DamageData)
{
    if (!ProjectileClass)
    {
        return;
    }

    FVector SpawnLoc = GetActorLocation() + (GetActorForwardVector() * ForwardOffset);
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (ARLProjectile* SpawnedProjectile = GetWorld()->SpawnActor<ARLProjectile>(ProjectileClass, SpawnLoc, GetActorRotation(), SpawnParams))
    {
        FRLDamageData FinalDamage = DamageData;
        FinalDamage.DamageCauser = this;
        SpawnedProjectile->DamageData = FinalDamage;

        if (SpawnedProjectile->MovementComp)
        {
            SpawnedProjectile->MovementComp->Velocity = GetActorRotation().Vector() * ProjectileSpeed;
        }
    }
}

void ARLCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        if (AnimInstance->IsAnyMontagePlaying())
        {
            return;
        }
    }

    Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

