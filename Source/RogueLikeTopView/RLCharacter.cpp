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

// Sets default values
ARLCharacter::ARLCharacter()
{
    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
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

    // 데미지 수신 로그 출력 (★ 나중에 팀원의 체력 감소 코드가 들어갈 위치)
    FString CauserName = DamageData.DamageCauser ? DamageData.DamageCauser->GetName() : TEXT("Environment");
    FString Msg = FString::Printf(TEXT("💥 [%s] 피격! 데미지: %.1f (공격자: %s)"), *GetName(), DamageData.BaseDamage, *CauserName);
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Msg);

    // 넉백 처리 (타격 방향 반대로 캐릭터를 튕겨냄)
    if (DamageData.KnockbackForce > 0.0f)
    {
        FVector KnockbackDir = (GetActorLocation() - HitInfo.ImpactPoint).GetSafeNormal2D();
        // Z축으로 살짝 띄워주면서 지면 마찰을 무시하고 밀어냄
        LaunchCharacter(KnockbackDir * DamageData.KnockbackForce + FVector(0.f, 0.f, 150.f), true, true);
    }

    AController* InstigatorController = DamageData.DamageCauser ? DamageData.DamageCauser->GetInstigatorController() : nullptr;
    UGameplayStatics::ApplyDamage(this, DamageData.BaseDamage, InstigatorController, DamageData.DamageCauser, UDamageType::StaticClass());
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
    // 공격자 위치에서 정면(Forward) 방향으로 Offset만큼 떨어진 곳을 중심점으로 설정
    FVector Center = GetActorLocation() + (GetActorForwardVector() * ForwardOffset);
    FRotator Rotation = GetActorRotation();

    TArray<FHitResult> HitResults;
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this); // 공격자 본인은 타격 판정에서 제외

    EDrawDebugTrace::Type DebugType = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

    // ECC_GameTraceChannel1 : 2단계에서 새로 만든 첫 번째 트레이스 채널(RL_Hitbox)
    bool bHit = UKismetSystemLibrary::BoxTraceMulti(
        GetWorld(),
        Center,
        Center, // 동일 지점 스윕 (Box 영역 즉시 Overlap 검사)
        BoxExtent,
        Rotation,
        UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1),
        false,
        ActorsToIgnore,
        DebugType,
        HitResults,
        true,
        FLinearColor::Red,   // 적중 안 됨 표시 색상
        FLinearColor::Green, // 적중 됨 표시 색상
        0.5f                 // 디버그 박스 화면 표시 시간 (0.5초)
    );

    if (bHit)
    {
        TArray<AActor*> HitActors; // 1번의 공격 판정 내 중복 피격 방지

        for (const FHitResult& Hit : HitResults)
        {
            AActor* TargetActor = Hit.GetActor();
            if (TargetActor && !HitActors.Contains(TargetActor))
            {
                HitActors.Add(TargetActor);

                // 맞은 대상이 ARLCharacter 타입인 경우 피격 함수 호출
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
    AttackData.KnockbackForce = 0.0f; // 시원하게 날아가는 넉백 수치

    // 가로(X)=100, 세로(Y)=80, 높이(Z)=50 크기의 박스를 캐릭터 전방 130cm 앞에 생성
    CreateBoxHitbox(FVector(100.f, 80.f, 50.f), 130.f, AttackData, true);
}

void ARLCharacter::Dodge(FVector CursorLocation)
{
    // 1. 현재 내 캐릭터의 위치를 가져옵니다.
    FVector MyLocation = GetActorLocation();

    // 2. 방향 벡터 공식: 목적지(마우스) - 출발지(내 위치)
    FVector DashDirection = CursorLocation - MyLocation;

    // 3. 탑뷰 게임이므로 하늘로 솟아오르거나 땅으로 파고들지 않게 Z축을 0으로 지워버립니다.
    DashDirection.Z = 0.0f;

    // 마우스가 캐릭터와 완전히 똑같은 위치일 때 발생하는 에러를 방지합니다.
    if (DashDirection.IsNearlyZero())
    {
        return;
    }

    // 4. 방향만 남기기 위해 벡터의 길이를 1로 정규화(Normalize)합니다.
    DashDirection.Normalize();

    // 5. 묵직한 탱커에게 어울리는 돌진 거리를 설정합니다. (원하시는 대로 숫자를 조절하세요)
    float DashPower = 2500.0f;

    // 6. 언리얼 엔진의 캐릭터 발사 함수! 기존의 관성을 무시하고(true) 즉각 쏘아 보냅니다.
    LaunchCharacter(DashDirection * DashPower, true, true);

    // 7. 쏘아 보냄과 동시에 캐릭터가 회피하는 방향을 즉시 바라보도록 회전시킵니다.
    SetActorRotation(DashDirection.Rotation());
}

// Called every frame
void ARLCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ARLCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

}