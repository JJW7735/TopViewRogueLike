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
#include "RLSkillData.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"

// Sets default values
ARLCharacter::ARLCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetUsingAbsoluteRotation(true); // 캐릭터 회전과 카메라 회전 분리
    CameraBoom->TargetArmLength = 1300.f;       // 탑뷰 카메라 거리
    CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f)); // 쿼터뷰 각도
    CameraBoom->bDoCollisionTest = false;      // 벽 충돌 줌인 방지

    TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    TopDownCamera->bUsePawnControlRotation = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f); // 빠른 회전
    GetCharacterMovement()->bConstrainToPlane = true;
    GetCharacterMovement()->bRequestedMoveUseAcceleration = true;
    GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;

    bUseControllerRotationYaw = false;

    HealthComp = CreateDefaultSubobject<URLHealthComponent>(TEXT("HealthComp"));

    // ★ 원본 SkillComp 하나만 생성
    SkillComp = CreateDefaultSubobject<URLSkillComponent>(TEXT("SkillComp"));
}

// Called when the game starts or when spawned
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
    }
}

void ARLCharacter::ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo)
{
    // 무적 상태(회피 중)이면서 관통 공격이 아닌 경우 데미지 무시
    if (bIsInvulnerable && !DamageData.bIgnoreInvincibility)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Dodge / Invulnerable!"));
        return;
    }

    // 데미지 수신 로그 출력
    FString CauserName = DamageData.DamageCauser ? DamageData.DamageCauser->GetName() : TEXT("Environment");
    FString Msg = FString::Printf(TEXT("💥 [%s] 피격! 데미지: %.1f (공격자: %s)"), *GetName(), DamageData.BaseDamage, *CauserName);
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Msg);

    // 넉백 처리
    if (DamageData.KnockbackForce > 0.0f)
    {
        FVector KnockbackDir = (GetActorLocation() - HitInfo.ImpactPoint).GetSafeNormal2D();
        LaunchCharacter(KnockbackDir * DamageData.KnockbackForce + FVector(0.f, 0.f, 150.f), true, true);
    }

    AController* InstigatorController = DamageData.DamageCauser ? DamageData.DamageCauser->GetInstigatorController() : nullptr;
    UGameplayStatics::ApplyDamage(this, DamageData.BaseDamage, InstigatorController, DamageData.DamageCauser, UDamageType::StaticClass());

    // ★ 피격 시 시전 중이던 캐스팅 취소 (SkillComp 참조)
    if (SkillComp && SkillComp->IsCasting())
    {
        SkillComp->InterruptCasting(TEXT("피격으로 인한 시전 취소"));
    }
}

void ARLCharacter::HandleDeath()
{
    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Black, FString::Printf(TEXT("💀 [%s] 체력이 0이 되어 사망했습니다!"), *GetName()));

    // 이동 및 충돌 비활성화
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
    FVector MyLocation = GetActorLocation();
    FVector DashDirection = CursorLocation - MyLocation;
    DashDirection.Z = 0.0f;

    if (DashDirection.IsNearlyZero())
    {
        return;
    }

    DashDirection.Normalize();

    float DashPower = 2500.0f;
    LaunchCharacter(DashDirection * DashPower, true, true);
    SetActorRotation(DashDirection.Rotation());

    // ★ 대쉬 시전 시, 기존에 재생 중이던 공격 몽타주를 강제로 캔슬!
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->Montage_Stop(0.1f);
    }

    // ★ 회피 시 시전 중이던 캐스팅 취소 (SkillComp 참조)
    if (SkillComp && SkillComp->IsCasting())
    {
        SkillComp->InterruptCasting(TEXT("회피기로 인한 시전 취소"));
    }
}

void ARLCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ========================================================
    // ★ 마우스 클릭(Pathfinding) & 모든 미끄러짐 완벽 방지 코드
    // ========================================================
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        // 현재 어떤 몽타주라도 재생 중이라면?
        if (AnimInstance->IsAnyMontagePlaying())
        {
            // 1. 캐릭터 무브먼트의 물리적 이동 속도를 매 프레임 즉시 0으로 만듭니다.
            if (GetCharacterMovement())
            {
                GetCharacterMovement()->StopMovementImmediately();
            }

            // 2. 마우스 클릭으로 인한 길찾기(NavMesh) 이동 명령까지 완전히 취소시킵니다.
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
    // (이곳은 키 바인딩용 함수이므로 방어 코드를 제거하고 깔끔하게 비워둡니다)
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

void ARLCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
    // 1. 캐릭터의 애니메이션 인스턴스를 가져옵니다.
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        // 2. 현재 어떤 몽타주라도 재생 중이라면?
        if (AnimInstance->IsAnyMontagePlaying())
        {
            // 부모 클래스의 이동 로직을 실행하지 않고 즉시 함수를 빠져나갑니다. (이동 명령 완전히 무시!)
            return;
        }
    }

    // 3. 몽타주가 재생 중이 아닐 때만 정상적으로 부모 클래스의 이동을 수행합니다.
    Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}