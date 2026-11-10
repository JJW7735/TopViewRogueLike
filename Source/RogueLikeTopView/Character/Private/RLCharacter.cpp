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
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "RLProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RLPortal.h"
#include "RLRewardChest.h"
#include "RLMetaUpgradeStation.h"
#include "RLRewardSubsystem.h"
#include "RLRewardSelectWidget.h"
#include "RLInventoryComponent.h"
#include "RLItemDataAsset.h"
#include "RLSaveSubsystem.h"
#include "RLShopSubsystem.h"
#include "RLShopWidget.h"
#include "RLShopStation.h"
#include "RLDecalIndicator.h"
#include "RLAbilitySystemComponent.h"
#include "RLAttributeSet.h"
#include "RLGameplayTags.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

namespace
{
    // 시작점에서 Dir 방향으로 걸을 수 있는 영역(네비메시) 경계까지의 수평 거리. 경계에 안 닿으면 MaxDist, 네비 밖이면 -1
    float GetNavDistanceToEdge(const ACharacter* Character, const FVector& Dir, float MaxDist)
    {
        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Character->GetWorld());
        FNavLocation StartOnNav;
        if (!NavSys || !NavSys->ProjectPointToNavigation(Character->GetActorLocation(), StartOnNav, FVector(50.0f, 50.0f, 250.0f)))
        {
            return -1.0f;
        }

        const FVector RayEnd = StartOnNav.Location + Dir * MaxDist;
        FVector HitLocation;
        if (UNavigationSystemV1::NavigationRaycast(const_cast<ACharacter*>(Character), StartOnNav.Location, RayEnd, HitLocation, nullptr, Character->GetController()))
        {
            return FVector::Dist2D(StartOnNav.Location, HitLocation);
        }
        return MaxDist;
    }

    // ★ 넉백(LaunchCharacter)으로 날아갈 수평 거리를 추정해서, 낭떠러지/강 너머로 날아가지 않게 수평 속도를 줄임
    // 예상 거리 = 공중 비행(위로 UpSpeed만큼 떴다 떨어지는 시간 x 수평 속도) + 착지 후 제동 거리
    // 마찰은 무시해서 실제보다 길게 잡음 (가장자리 근처에서만 줄어들고, 안전한 곳에서는 원래 세기 그대로)
    float ClampKnockbackSpeedToNav(const ACharacter* Character, const FVector& Dir, float Speed, float UpSpeed)
    {
        const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
        if (!Move || Speed <= 0.0f)
        {
            return Speed;
        }

        const float Gravity = FMath::Max(FMath::Abs(Move->GetGravityZ()), 1.0f);
        const float AirTime = 2.0f * UpSpeed / Gravity;
        const float Braking = FMath::Max(Move->BrakingDecelerationWalking, 1.0f);
        const float PredictedDist = Speed * AirTime + Speed * Speed / (2.0f * Braking);

        const float EdgeDist = GetNavDistanceToEdge(Character, Dir, PredictedDist + 100.0f);
        if (EdgeDist < 0.0f || EdgeDist >= PredictedDist)
        {
            return Speed;
        }

        // Speed*AirTime + Speed^2/(2*Braking) = 안전거리 를 Speed에 대해 풂
        const float SafeDist = FMath::Max(EdgeDist - 30.0f, 0.0f);
        const float A = 1.0f / (2.0f * Braking);
        const float B = AirTime;
        return (-B + FMath::Sqrt(B * B + 4.0f * A * SafeDist)) / (2.0f * A);
    }
}

void ARLCharacter::LaunchClampedToNav(const FVector& LaunchVelocity, bool bXYOverride, bool bZOverride)
{
    const FVector Dir = LaunchVelocity.GetSafeNormal2D();
    const float Speed = LaunchVelocity.Size2D();
    // Z를 덮어쓰지 않으면 위로 거의 안 뜨므로 공중 비행 시간은 위쪽 성분만으로 추정
    const float UpSpeed = FMath::Max(LaunchVelocity.Z, 0.0f);
    const float ClampedSpeed = ClampKnockbackSpeedToNav(this, Dir, Speed, UpSpeed);

    LaunchCharacter(Dir * ClampedSpeed + FVector(0.f, 0.f, LaunchVelocity.Z), bXYOverride, bZOverride);
}

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
    InventoryComp = CreateDefaultSubobject<URLInventoryComponent>(TEXT("InventoryComp"));

    // AttributeSet은 Owner 액터의 서브오브젝트로 만들어두면 ASC가 초기화될 때 자동으로 등록합니다.
    AbilitySystemComp = CreateDefaultSubobject<URLAbilitySystemComponent>(TEXT("AbilitySystemComp"));
    AttributeSet = CreateDefaultSubobject<URLAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ARLCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComp;
}

void ARLCharacter::BeginPlay()
{
    // BP의 BeginPlay에서도 ASC를 쓸 수 있도록 Super 호출 전에 초기화합니다.
    // 싱글플레이라 Owner(논리적 소유자)와 Avatar(월드에 있는 몸)가 모두 캐릭터 자신입니다.
    if (AbilitySystemComp)
    {
        AbilitySystemComp->InitAbilityActorInfo(this, this);
    }

    Super::BeginPlay();

    // ★ 바닥 예고/안내 데칼이 캐릭터 몸(메시, 붙어있는 무기 등)에 문신처럼 투영되지 않도록 데칼 수신을 끕니다.
    // BP 디폴트가 아니라 런타임에 끄는 이유: 레벨에 배치된 인스턴스에 BP 값이 전파되지 않는 문제를 피하기 위함.
    TArray<UPrimitiveComponent*> PrimitiveComps;
    GetComponents<UPrimitiveComponent>(PrimitiveComps);
    for (UPrimitiveComponent* PrimComp : PrimitiveComps)
    {
        PrimComp->SetReceivesDecals(false);
    }

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

    if (StatComp)
    {
        StatComp->OnLevelUp.AddDynamic(this, &ARLCharacter::HandleLevelUp);
    }

    // ★ 세이브 시스템 연동: 플레이어 캐릭터이고, 저장된 활성 런이 남아있다면 자동 복원 (이어하기)
    if (TeamID == ERLTeam::Player && bAutoRestoreRunSaveOnSpawn)
    {
        if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
        {
            // ★ bPendingRunApply가 true일 때만 복원한다. (ResumeActiveRun()이 명시적으로
            //   "이어하기"를 요청했을 때만 true로 세팅됨 — LoadActiveRun(true) 참고)
            //   예전엔 HasActiveRun()만 봤어서, 아직 ClearActiveRun이 안 된 활성 런이 남아있는
            //   채로 Retry 등으로 새로 스폰될 때도 무조건 옛 상태를 복원해버리는 버그가 있었음.
            // ★ 플래그는 체크하자마자 바로 소비한다(one-shot). 아래에서 캐릭터/맵 불일치로 복원을
            //   건너뛰더라도 true로 남아있으면, 다음 스폰(Retry 등)에서 뒤늦게 복원이 걸려버리기 때문.
            const bool bShouldApplyPendingRun = SaveSys->bPendingRunApply;
            SaveSys->bPendingRunApply = false;

            if (bShouldApplyPendingRun && SaveSys->HasActiveRun())
            {
                // ★ 세이프가드: 저장된 런이 "지금 스폰된 캐릭터와 다른 캐릭터" 걸로 저장된 거라면
                //   복원하지 않는다. (정상 흐름에서는 캐릭터 선택 시 기존 런을 폐기하므로 여기 걸릴
                //   일이 없어야 하지만, 혹시 UI 쪽에서 다른 캐릭터로 이어하기가 잘못 걸리는 경우를
                //   막는 안전장치) 이 필드가 비어있는 옛 저장 파일은 구분 없이 복원 허용.
                const FString SavedClassPath = SaveSys->GetActiveRunData().CharacterClass.ToString();
                const bool bCharacterMatches = SavedClassPath.IsEmpty() || SavedClassPath == GetClass()->GetPathName();

                if (bCharacterMatches)
                {
                    const FString CurrentMap = GetWorld() ? UWorld::RemovePIEPrefix(GetWorld()->GetMapName()) : TEXT("");
                    const FString SavedMap = UWorld::RemovePIEPrefix(SaveSys->GetActiveRunData().CurrentMapName.ToString());

                    // 같은 맵이거나 맵 지정이 없으면 즉시 저장 위치 및 스탯/아이템 복원
                    if (SavedMap.IsEmpty() || SavedMap == TEXT("None") || CurrentMap.Equals(SavedMap, ESearchCase::IgnoreCase))
                    {
                        SaveSys->ApplyRunDataToPlayer(this);
                    }
                }
                else if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
                        TEXT("⚠ 저장된 런의 캐릭터가 지금 캐릭터와 달라서 복원을 건너뜁니다."));
                }
            }
            else
            {
                // ★ 신규 런 시작: 저장된 런 이어하기가 아니라면 영구 메타 강화 스탯을 플레이어에게 적용!
                SaveSys->ApplyMetaUpgradesToPlayer(this);

                // 이전 판의 버프 획득 기록(GameInstance 서브시스템에 남아 있음)을 비운다
                if (URLRewardSubsystem* RewardSys = URLRewardSubsystem::GetRewardSubsystem(this))
                {
                    RewardSys->ResetAcquiredRewards();
                }
            }
        }
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

    if (IsInvulnerable() && !DamageData.bIgnoreInvincibility)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Dodge / Invulnerable!"));
        return;
    }

    // ★ 공격자에게 "명중" 알림 (회피 무적/아군은 위에서 걸러짐). 보스 패턴의 "맞으면 연결 안 함" 판정 등에 사용
    if (ARLCharacter* CauserCharacter = Cast<ARLCharacter>(DamageData.DamageCauser))
    {
        ++CauserCharacter->AttackLandedCount;
    }

    // 보스/엘리트처럼 경직 면역(CanBeStaggered == false)인 대상은 넉백으로 밀려나지 않습니다.
    const bool bCanBeStaggered = CanBeStaggered();

    if (bCanBeStaggered && DamageData.bPushAwayFromCenter && DamageData.PushAwayDistance > 0.0f)
    {
        // ★ 일정 거리 밀어내기: 판정 중심 → 나 방향으로 PushAwayDistance만큼 대시 이동.
        // 대시 이동이 네비메시 경계에서 멈춰 주므로 낭떠러지로 떨어지지 않음
        // 판정 함수를 거치지 않은 피해(중심 미설정)면 공격자 위치를 중심으로
        const FVector PushOrigin = (DamageData.PushOrigin.IsZero() && DamageData.DamageCauser) ? DamageData.DamageCauser->GetActorLocation() : DamageData.PushOrigin;
        FVector AwayDir = (GetActorLocation() - PushOrigin).GetSafeNormal2D();
        if (AwayDir.IsNearlyZero())
        {
            AwayDir = DamageData.DamageCauser ? DamageData.DamageCauser->GetActorForwardVector().GetSafeNormal2D() : -GetActorForwardVector().GetSafeNormal2D();
        }

        StartDashMovement(GetActorLocation() + AwayDir * DamageData.PushAwayDistance, DamageData.PushAwayDuration);
    }
    else if (bCanBeStaggered && DamageData.KnockbackForce > 0.0f)
    {
        float KnockbackMultiplier = DamageData.bIsCritical ? 1.25f : 1.0f;
        FVector KnockbackDir = (GetActorLocation() - HitInfo.ImpactPoint).GetSafeNormal2D();
        LaunchClampedToNav(KnockbackDir * (DamageData.KnockbackForce * KnockbackMultiplier) + FVector(0.f, 0.f, 150.f), true, true);
    }

    // ★ 피해 적용 (GAS): 공격자 ASC → URLGE_Damage → URLDamageExecution(방어력/피해감소 경감) → Health 감소.
    // State.IgnoreDefense 태그(보스 대경직 등)나 bTrueDamage면 Execution이 경감을 건너뜁니다.
    const float FinalDamageAmount = HealthComp
        ? HealthComp->ApplyIncomingDamage(DamageData.BaseDamage, DamageData.DamageCauser, DamageData.bTrueDamage)
        : 0.0f;
    const float ReducedAmount = FMath::Max(0.0f, DamageData.BaseDamage - FinalDamageAmount);

    FString CauserName = DamageData.DamageCauser ? DamageData.DamageCauser->GetName() : TEXT("Environment");
    FString DefenseMsg = (ReducedAmount > 0.0f) ? FString::Printf(TEXT(" (방어 감쇄: -%.1f)"), ReducedAmount) : TEXT("");

    if (DamageData.bIsCritical)
    {
        FString Msg = FString::Printf(TEXT("⚡⚡ [CRITICAL HIT!!] [%s] 피격! 데미지: %.1f%s (공격자: %s) ⚡⚡"),
            *GetName(), FinalDamageAmount, *DefenseMsg, *CauserName);
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange, Msg);
    }
    else
    {
        FString Msg = FString::Printf(TEXT("💥 [%s] 피격! 데미지: %.1f%s (공격자: %s)"),
            *GetName(), FinalDamageAmount, *DefenseMsg, *CauserName);
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Msg);
    }

    // 기존 BP 호환: 예전엔 여기서 UGameplayStatics::ApplyDamage를 불러서 BP의 Event AnyDamage /
    // OnTakeAnyDamage가 발동했습니다. 피해는 이미 GAS로 들어갔으니 알림만 똑같이 보냅니다 (이중 피해 없음).
    if (FinalDamageAmount > 0.0f)
    {
        AController* InstigatorController = DamageData.DamageCauser ? DamageData.DamageCauser->GetInstigatorController() : nullptr;
        const UDamageType* DamageTypeCDO = GetDefault<UDamageType>();
        ReceiveAnyDamage(FinalDamageAmount, DamageTypeCDO, InstigatorController, DamageData.DamageCauser);
        OnTakeAnyDamage.Broadcast(this, FinalDamageAmount, DamageTypeCDO, InstigatorController, DamageData.DamageCauser);
    }

    // ★ 흡혈(라이프스틸): 방어력 적용 후 "실제로 들어간" 데미지(FinalDamageAmount) 기준으로 가해자를 회복시킵니다.
    // BaseDamage(방어 전 원본)가 아니라 FinalDamageAmount를 기준으로 삼은 이유는, 방어력이 높은 대상을 때렸을 때
    // 실제로 박히지도 않은 데미지만큼 회복되는 걸 막기 위해서입니다 (예: 보스 SoulDrain 패턴)
    if (DamageData.LifestealRatio > 0.0f)
    {
        if (ARLCharacter* CauserCharacter = Cast<ARLCharacter>(DamageData.DamageCauser))
        {
            if (CauserCharacter->HealthComp)
            {
                CauserCharacter->HealthComp->Heal(FinalDamageAmount * DamageData.LifestealRatio);
            }
        }
    }

    if (bCanBeStaggered && SkillComp && SkillComp->IsCasting())
    {
        SkillComp->InterruptCasting(TEXT("피격으로 인한 시전 취소"));
    }

    // ★ 경직 (넉백 후 잠깐 굳음). 경직 면역 판정은 ApplyStun 안에서
    if (DamageData.StunDuration > 0.0f)
    {
        ApplyStun(DamageData.StunDuration);
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

    // ★ 플레이어 사망 시 로그라이크 규칙: 런 세이브 삭제 및 영혼 메타 정산
    int32 SoulsEarnedThisRun = 0;
    if (TeamID == ERLTeam::Player)
    {
        if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
        {
            // ★ HandlePlayerDeath() 안에서 ClearActiveRun(true)이 RunSouls를 0으로 리셋하므로,
            //   그 전에 값을 먼저 읽어서 게임오버 화면에 넘겨준다.
            SoulsEarnedThisRun = SaveSys->GetRunSouls();
            SaveSys->HandlePlayerDeath();
        }
    }

    OnCharacterDeath(SoulsEarnedThisRun);
}

void ARLCharacter::HandleLevelUp(int32 NewLevel)
{
    // 1. 레벨업 시 체력 완전 회복
    if (HealthComp)
    {
        HealthComp->Heal(HealthComp->MaxHealth);
    }

    // 2. 블루프린트 연출 훅 호출
    OnCharacterLevelUp(NewLevel);

    // 3. 3지선다 보상 카드 선택창 자동 팝업 (처치 연출/드롭이 먼저 보이도록 살짝 지연, 게임은 멈추지 않음)
    GetWorldTimerManager().SetTimer(LevelUpRewardTimerHandle, this, &ARLCharacter::TestOpenRewardUI, 0.4f, false);
}

void ARLCharacter::SetNearbyPortal(ARLPortal* Portal)
{
    NearbyPortal = Portal;
    RefreshInteractPrompt();
}

void ARLCharacter::SetNearbyChest(ARLRewardChest* Chest)
{
    NearbyChest = Chest;
    RefreshInteractPrompt();
}

void ARLCharacter::SetNearbyMetaStation(ARLMetaUpgradeStation* Station)
{
    NearbyMetaStation = Station;
    RefreshInteractPrompt();
}

void ARLCharacter::SetNearbyShop(ARLShopStation* Shop)
{
    NearbyShop = Shop;
    RefreshInteractPrompt();
}

void ARLCharacter::RefreshInteractPrompt()
{
    // TryInteract와 같은 우선순위: 상자 > 포탈 > 강화소 > 상점
    if (NearbyChest)
    {
        OnInteractPromptChanged(true, NearbyChest->InteractPromptText);
    }
    else if (NearbyPortal)
    {
        OnInteractPromptChanged(true, NearbyPortal->InteractPromptText);
    }
    else if (NearbyMetaStation)
    {
        OnInteractPromptChanged(true, NearbyMetaStation->InteractPromptText);
    }
    else if (NearbyShop)
    {
        OnInteractPromptChanged(true, NearbyShop->InteractPromptText);
    }
    else
    {
        OnInteractPromptChanged(false, FText::GetEmpty());
    }
}

void ARLCharacter::TryInteract()
{
    // 보상 상자가 근처에 있으면 상자 먼저 열기
    if (NearbyChest)
    {
        NearbyChest->OpenChest(this);
        return;
    }

    // 포탈이 근처에 있으면 포탈 이동
    if (NearbyPortal)
    {
        NearbyPortal->ActivatePortal(this);
        return;
    }

    // 영구 강화소가 근처에 있으면 강화 상점 UI 열기
    if (NearbyMetaStation)
    {
        NearbyMetaStation->Interact(this);
        return;
    }

    // 상점이 근처에 있으면 상점 UI 열기
    if (NearbyShop)
    {
        NearbyShop->Interact(this);
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("주변에 상호작용할 대상(상자/포탈/강화소/상점)이 없습니다."));
    }
}

void ARLCharacter::TestOpenRewardUI()
{
    URLRewardSubsystem* Subsystem = URLRewardSubsystem::GetRewardSubsystem(this);
    if (!Subsystem)
    {
        return;
    }

    TArray<FRLRewardData> Rewards = Subsystem->RollRewards(3);
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    TSubclassOf<URLRewardSelectWidget> WidgetClass = LoadClass<URLRewardSelectWidget>(
        nullptr, TEXT("/Game/Blueprint/UI/WBP_RewardSelect.WBP_RewardSelect_C"));
    if (WidgetClass)
    {
        URLRewardSelectWidget* Widget = CreateWidget<URLRewardSelectWidget>(PC, WidgetClass);
        if (Widget)
        {
            Widget->AddToViewport(50);
            Widget->InitializeWidget(this, Rewards, nullptr, ERLRewardTriggerSource::LevelUp);
        }
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            TEXT("⚠️ WBP_RewardSelect 에셋을 찾을 수 없습니다. Content/Blueprint/UI/WBP_RewardSelect를 생성해주세요."));
    }
}

void ARLCharacter::TestGiveTestItems()
{
    if (!InventoryComp)
    {
        return;
    }

    URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this);

    // 상점 서브시스템 풀의 에셋을 우선 재사용하여 맵 이동 시 GC로 인한 세이브 유실 방지
    // 1. 체력 회복 물약 (소비 아이템, 5개)
    URLItemDataAsset* PotionItem = ShopSub ? ShopSub->FindItemByID(FName("Item_HealthPotion")) : nullptr;
    if (PotionItem)
    {
        InventoryComp->AddItem(PotionItem, 5);
    }

    // 2. 전사의 완력 반지 (패시브 유물, 공격력 +15, 치명타 +10%)
    URLItemDataAsset* RingItem = ShopSub ? ShopSub->FindItemByID(FName("Item_WarriorRing")) : nullptr;
    if (RingItem)
    {
        InventoryComp->AddItem(RingItem, 1);
    }

    // 3. 비전 마력 반지 (패시브 유물, 주문력 +18, 치명타 +10%)
    URLItemDataAsset* MageRingItem = ShopSub ? ShopSub->FindItemByID(FName("Item_MageRing")) : nullptr;
    if (MageRingItem)
    {
        InventoryComp->AddItem(MageRingItem, 1);
    }

    // 4. 바람의 신속 장화 (패시브 유물, 이동속도 +100)
    URLItemDataAsset* BootsItem = ShopSub ? ShopSub->FindItemByID(FName("Item_WindBoots")) : nullptr;
    if (BootsItem)
    {
        InventoryComp->AddItem(BootsItem, 1);
    }

    // 5. 대마법사의 비전 보주 (패시브 유물, 주문력 +30, 치명타 피해 +25%)
    URLItemDataAsset* ArcaneOrbItem = ShopSub ? ShopSub->FindItemByID(FName("Item_ArcaneOrb")) : nullptr;
    if (ArcaneOrbItem)
    {
        InventoryComp->AddItem(ArcaneOrbItem, 1);
    }

    // 6. 골드 지급 (300 Gold)
    InventoryComp->AddGold(300);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
            TEXT("🎁 [치트] 테스트용 아이템 5종(물약 5개, 전사 반지, 비전 마력 반지, 신속 장화, 비전 보주)과 300 골드가 인벤토리에 지급되었습니다!"));
    }
}

void ARLCharacter::TestToggleMaxHealthItem()
{
    if (!InventoryComp)
    {
        return;
    }

    const FName TestItemID(TEXT("Item_TestVitalityGem"));
    const bool bHadItem = InventoryComp->HasItem(TestItemID);

    if (bHadItem)
    {
        InventoryComp->RemoveItemByID(TestItemID, 1);
    }
    else
    {
        // 맵 이동 후 세이브 복원 때 이 객체를 경로로 다시 찾으므로 GC되면 안 됩니다.
        // Outer는 GC 생존과 무관해서(누가 UPROPERTY로 참조해야 살아남음), 트랜지언트 패키지에 두고 루트에 고정합니다.
        // (상점 아이템은 URLShopSubsystem::ItemPool UPROPERTY가 붙잡고 있어서 같은 문제가 없음)
        URLItemDataAsset* GemItem = FindObject<URLItemDataAsset>(GetTransientPackage(), TEXT("Test_VitalityGem"));
        if (!GemItem)
        {
            GemItem = NewObject<URLItemDataAsset>(GetTransientPackage(), TEXT("Test_VitalityGem"));
            GemItem->AddToRoot();
            GemItem->ItemID = TestItemID;
            GemItem->ItemName = FText::FromString(TEXT("[테스트] 생명의 보석"));
            GemItem->Description = FText::FromString(TEXT("소지 시 최대 체력 +80"));
            GemItem->ItemType = ERLItemType::Passive;
            GemItem->MaxStackSize = 1;
            GemItem->PassiveStatBonuses.Add({ ERLTargetStat::MaxHealth, 80.0f });
        }
        InventoryComp->AddItem(GemItem, 1);
    }

    if (GEngine && HealthComp)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
            FString::Printf(TEXT("💎 [치트] 생명의 보석 %s → 체력 %.0f / %.0f (기본 최대 %.0f)"),
                bHadItem ? TEXT("제거") : TEXT("지급"), HealthComp->CurrentHealth, HealthComp->MaxHealth, HealthComp->GetMaxHealthBase()));
    }
}

void ARLCharacter::TestUseItemSlot(int32 SlotIndex)
{
    if (!InventoryComp)
    {
        return;
    }

    const bool bSuccess = InventoryComp->UseItemAtIndex(SlotIndex);
    if (!bSuccess && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
            FString::Printf(TEXT("❌ %d번 슬롯 아이템 사용 실패 (빈 슬롯이거나 사용 불가)"), SlotIndex));
    }
}

void ARLCharacter::TestAddGold(int32 Amount)
{
    if (InventoryComp)
    {
        InventoryComp->AddGold(Amount);
    }
}

void ARLCharacter::TestAddExp(float Amount)
{
    if (StatComp)
    {
        StatComp->AddExp(Amount);
    }
}

void ARLCharacter::TestOpenShop()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    TSubclassOf<URLShopWidget> WidgetClass = LoadClass<URLShopWidget>(
        nullptr, TEXT("/Game/Blueprint/UI/WBP_ShopTest.WBP_ShopTest_C"));
    if (WidgetClass)
    {
        URLShopWidget* Widget = CreateWidget<URLShopWidget>(PC, WidgetClass);
        if (Widget)
        {
            Widget->AddToViewport(50);
            Widget->OpenShop(this);
            return;
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            TEXT("⚠️ WBP_ShopTest 에셋을 찾을 수 없습니다."));
    }
}

void ARLCharacter::TestBuyShopItem(int32 SlotIndex)
{
    if (URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this))
    {
        FString FailReason;
        const bool bSuccess = ShopSub->BuyShopItem(this, SlotIndex, FailReason);
        if (!bSuccess && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                FString::Printf(TEXT("❌ [상점 구매 실패] %s"), *FailReason));
        }
    }
}

void ARLCharacter::TestRerollShop()
{
    if (URLShopSubsystem* ShopSub = URLShopSubsystem::Get(this))
    {
        FString FailReason;
        const bool bSuccess = ShopSub->RerollShop(this, FailReason);
        if (!bSuccess && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                FString::Printf(TEXT("❌ [새로고침 실패] %s"), *FailReason));
        }
    }
}

void ARLCharacter::CreateBoxHitbox(FVector BoxExtent, float ForwardOffset, const FRLDamageData& DamageData, bool bDrawDebug)
{
    // ★ 이번 히트의 범위 배율 (SetAttackHitScales 미설정이면 1.0). 박스가 정면 쪽으로 길어지고 폭도 넓어짐
    const float HitWidthScale = GetAttackHitWidthScale(AttackHitCounter); // Consume 전에 같은 순번으로 조회
    const float HitScale = ConsumeAttackHitScale();
    BoxExtent *= HitScale;
    BoxExtent.Y *= HitWidthScale;
    ForwardOffset *= HitScale;

    // ★ 높이는 최소한 내 캡슐 전체(발 ~ 머리)를 덮도록. 데이터의 Z가 0이면 캡슐 중심 높이의 얇은 판이 되어서,
    // 덩치 큰 캐릭터(예: 3배 크기 보스2)는 판이 플레이어 머리 위로 지나가 안 맞음
    if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        BoxExtent.Z = FMath::Max(BoxExtent.Z, Capsule->GetScaledCapsuleHalfHeight());
    }

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
                    // 일정 거리 밀어내기(bPushAwayFromCenter)용 중심 = 공격자
                    FinalData.PushOrigin = GetActorLocation();
                    TargetCharacter->ReceiveCustomDamage(FinalData, Hit);
                }
            }
        }
    }
}

ARLDecalIndicator* ARLCharacter::EnsureAttackIndicator()
{
    if (!AttackIndicator && GetWorld())
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AttackIndicator = GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }
    return AttackIndicator;
}

void ARLCharacter::ShowBoxAttackIndicator(FVector BoxExtent, float ForwardOffset, UMaterialInterface* DecalMaterial)
{
    if (!EnsureAttackIndicator())
    {
        return;
    }

    AttackIndicator->SetupBoxIndicator(BoxExtent, DecalMaterial);
    AttackIndicator->UpdateBoxTransform(GetIndicatorOrigin(), ForwardOffset, GetActorRotation().Yaw);
    AttackIndicator->SetIndicatorVisibility(true);
}

void ARLCharacter::ShowSectorAttackIndicator(float Radius, float SafeZoneAngleDegrees, UMaterialInterface* DecalMaterial, float InnerSafeRadius)
{
    if (!EnsureAttackIndicator())
    {
        return;
    }

    AttackIndicator->SetupSectorIndicator(Radius, SafeZoneAngleDegrees, DecalMaterial, InnerSafeRadius);
    AttackIndicator->UpdateSectorTransform(GetIndicatorOrigin(), GetActorRotation().Yaw);
    AttackIndicator->SetIndicatorVisibility(true);
}

FVector ARLCharacter::GetIndicatorOrigin() const
{
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    return GetActorLocation() - FVector(0.f, 0.f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
}

void ARLCharacter::HideAttackIndicator()
{
    if (AttackIndicator)
    {
        AttackIndicator->SetIndicatorVisibility(false);
    }
}

// ★ CreateLocationHitbox와 같은 규격(Radius)의 원형 데칼을 임의의 월드 좌표(Location)에 띄웁니다.
// Box/Sector의 Show*Indicator와 같은 지연 스폰(lazy-spawn) 패턴이지만, EIndicatorType::Circle은
// URLSkillComponent(플레이어 스킬 조준)이 이미 쓰고 있던 타입을 그대로 재사용합니다. MaxRange는
// "사거리 링" 기능인데 보스 판정에는 필요 없어서 항상 0(끔)으로 넘깁니다.
void ARLCharacter::ShowLocationAttackIndicator(FVector Location, float Radius, UMaterialInterface* DecalMaterial)
{
    if (!EnsureAttackIndicator())
    {
        return;
    }

    AttackIndicator->SetupIndicator(EIndicatorType::Circle, Radius, 0.0f, DecalMaterial);
    AttackIndicator->UpdateIndicatorTransform(Location, Location, 0.0f);
    AttackIndicator->SetIndicatorVisibility(true);
}

// ★ StartBoxAttackTelegraph/StartSectorAttackTelegraph 공용 헬퍼로 분리한 노티파이 스캔 로직.
// Montage Notify(AnimNotify_PlayMontageNotify)는 Notify->GetNotifyName()이 Notify Name 값을 돌려주고,
// 스켈레톤 노티파이는 Notify가 null이고 이벤트의 NotifyName에 이름이 들어있어서 둘 다 확인합니다.
TArray<FVector2D> ARLCharacter::ComputeTelegraphWindows(const UAnimMontage* Montage, FName HitNotifyName, float LeadTime) const
{
    TArray<FVector2D> Windows;

    if (!Montage)
    {
        return Windows;
    }

    // ★ 이름이 HitNotifyName"으로 시작하는" 노티파이를 모두 히트로 봅니다 (예: Notify_Hit, Notify_Hit_Soul, Notify_Hit_Soul2).
    // 히트마다 다른 이름을 붙여도(BTT_Attack 분기용) 텔레그래프/균열 구간/히트 배율 순번이 그대로 동작하게 하기 위함.
    const FString HitName = HitNotifyName.ToString();
    TArray<float> HitTimes;
    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    {
        const FString EventName = Event.Notify ? Event.Notify->GetNotifyName() : Event.NotifyName.ToString();
        const bool bMatch = EventName.StartsWith(HitName);
        if (bMatch)
        {
            HitTimes.Add(Event.GetTriggerTime());
        }
    }
    HitTimes.Sort();

    // 콤보 히트 간격이 LeadTime보다 짧으면 직전 히트 시점부터 표시 (구간이 겹쳐서 한 번에 뭉치지 않도록)
    float PrevHitTime = 0.0f;
    for (const float HitTime : HitTimes)
    {
        Windows.Add(FVector2D(FMath::Max(HitTime - LeadTime, PrevHitTime), HitTime));
        PrevHitTime = HitTime;
    }

    return Windows;
}

void ARLCharacter::StartBoxAttackTelegraph(UAnimMontage* Montage, FName HitNotifyName, float LeadTime, FVector BoxExtent, float ForwardOffset, UMaterialInterface* DecalMaterial, AActor* FaceTarget, float TurnSpeed)
{
    StopAttackTelegraph();

    // ★ 돌진 패턴의 앞쪽 연장(DashForwardReach): 사각형 앞 끝만 그만큼 늘림 (길이 절반 + 중심 이동을 Reach/2씩)
    if (DashForwardReach > 0.0f)
    {
        BoxExtent.X += DashForwardReach * 0.5f;
        ForwardOffset += DashForwardReach * 0.5f;
    }

    TelegraphWindows = ComputeTelegraphWindows(Montage, HitNotifyName, LeadTime);
    if (TelegraphWindows.Num() == 0)
    {
        return;
    }

    TelegraphShape = ETelegraphShape::Box;
    TelegraphMontage = Montage;
    TelegraphMaterial = DecalMaterial;
    TelegraphBoxExtent = BoxExtent;
    TelegraphForwardOffset = ForwardOffset;
    TelegraphFaceTarget = FaceTarget;
    TelegraphTurnSpeed = TurnSpeed;
    UpdateAttackTelegraph(0.0f);
}

void ARLCharacter::StartSectorAttackTelegraph(UAnimMontage* Montage, FName HitNotifyName, float LeadTime, float Radius, float SafeZoneAngleDegrees, UMaterialInterface* DecalMaterial, AActor* FaceTarget, float TurnSpeed, float InnerSafeRadius)
{
    StopAttackTelegraph();

    TelegraphWindows = ComputeTelegraphWindows(Montage, HitNotifyName, LeadTime);
    if (TelegraphWindows.Num() == 0)
    {
        return;
    }

    TelegraphShape = ETelegraphShape::Sector;
    TelegraphMontage = Montage;
    TelegraphMaterial = DecalMaterial;
    TelegraphRadius = Radius;
    TelegraphSafeZoneAngle = SafeZoneAngleDegrees;
    TelegraphInnerSafeRadius = InnerSafeRadius;
    TelegraphFaceTarget = FaceTarget;
    TelegraphTurnSpeed = TurnSpeed;
    UpdateAttackTelegraph(0.0f);
}

// ★ StartBoxAttackTelegraph/StartSectorAttackTelegraph와 같은 노티파이 스캔([히트-LeadTime, 히트) 구간
// 계산)을 재사용하되, 데칼이 보스 자신이 아니라 TrackTarget의 위치를 따라다닌다는 점이 다릅니다.
// LeadTime을 크게(몽타주 전체 길이 이상) 주면 사실상 "몽타주 재생 시작부터" 계속 표시/추적하게 됩니다.
void ARLCharacter::StartLocationAttackTelegraph(UAnimMontage* Montage, FName HitNotifyName, float LeadTime, float LockLeadTime, float Radius, UMaterialInterface* DecalMaterial, AActor* TrackTarget, bool bFaceTrackTarget, float TurnSpeed)
{
    StopAttackTelegraph();

    TelegraphWindows = ComputeTelegraphWindows(Montage, HitNotifyName, LeadTime);
    if (TelegraphWindows.Num() == 0)
    {
        return;
    }

    TelegraphShape = ETelegraphShape::Location;
    TelegraphMontage = Montage;
    TelegraphMaterial = DecalMaterial;
    TelegraphRadius = Radius;
    TelegraphLockLeadTime = LockLeadTime;
    TelegraphTrackTarget = TrackTarget;
    TelegraphFaceTarget = bFaceTrackTarget ? TrackTarget : nullptr;
    TelegraphTurnSpeed = TurnSpeed;
    bTelegraphLocationLocked = false;
    UpdateAttackTelegraph(0.0f);
}

void ARLCharacter::SetAttackHitScales(const TArray<float>& Scales)
{
    AttackHitScales = Scales;
    AttackHitCounter = 0;
    AttackHitGrowthScale = 1.0f;
    AttackHitGrowthHitCount = 0;
}

float ARLCharacter::GetAttackHitScale(int32 HitIndex) const
{
    const float Growth = HitIndex < AttackHitGrowthHitCount ? AttackHitGrowthScale : 1.0f;
    if (AttackHitScales.Num() == 0)
    {
        return Growth;
    }
    return FMath::Max(0.01f, AttackHitScales[FMath::Clamp(HitIndex, 0, AttackHitScales.Num() - 1)] * Growth);
}

void ARLCharacter::SetAttackHitWidthScales(const TArray<float>& WidthScales)
{
    AttackHitWidthScales = WidthScales;
}

float ARLCharacter::GetAttackHitWidthScale(int32 HitIndex) const
{
    if (AttackHitWidthScales.Num() == 0)
    {
        return 1.0f;
    }
    return FMath::Max(0.01f, AttackHitWidthScales[FMath::Clamp(HitIndex, 0, AttackHitWidthScales.Num() - 1)]);
}

float ARLCharacter::ConsumeAttackHitScale()
{
    const float Scale = GetAttackHitScale(AttackHitCounter);
    ++AttackHitCounter;
    return Scale;
}

void ARLCharacter::StopAttackTelegraph()
{
    TelegraphMontage = nullptr;
    TelegraphMaterial = nullptr;
    TelegraphFaceTarget = nullptr;
    TelegraphTrackTarget = nullptr;
    bTelegraphLocationLocked = false;
    TelegraphWindows.Reset();

    if (bTelegraphVisible)
    {
        HideAttackIndicator();
        bTelegraphVisible = false;
    }
}

void ARLCharacter::UpdateAttackTelegraph(float DeltaTime)
{
    if (!TelegraphMontage)
    {
        return;
    }

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance || !AnimInstance->Montage_IsPlaying(TelegraphMontage))
    {
        StopAttackTelegraph();
        return;
    }

    const float Position = AnimInstance->Montage_GetPosition(TelegraphMontage);

    bool bShouldShow = false;
    FVector2D ActiveWindow = FVector2D::ZeroVector;
    int32 ActiveWindowIndex = 0;
    for (int32 WindowIndex = 0; WindowIndex < TelegraphWindows.Num(); ++WindowIndex)
    {
        const FVector2D& Window = TelegraphWindows[WindowIndex];
        if (Position >= Window.X && Position < Window.Y)
        {
            bShouldShow = true;
            ActiveWindow = Window;
            ActiveWindowIndex = WindowIndex;
            break;
        }
    }

    if (bShouldShow && !bTelegraphVisible)
    {
        if (TelegraphShape == ETelegraphShape::Location)
        {
            // ★ Location은 보스 자신이 아니라 TrackTarget 위치가 중심이라 데칼 배치에는 보스 방향이
            // 필요 없습니다. (보스가 대상을 바라보는 건 아래에서 매 프레임 따로 처리)
            bTelegraphLocationLocked = false;
            const FVector StartLoc = TelegraphTrackTarget.IsValid() ? TelegraphTrackTarget->GetActorLocation() : GetActorLocation();
            TelegraphLockedLocation = StartLoc;
            TelegraphShownScale = GetAttackHitScale(ActiveWindowIndex);
            ShowLocationAttackIndicator(StartLoc, TelegraphRadius * TelegraphShownScale, TelegraphMaterial);
        }
        else
        {
            // 대상을 정면으로 맞춘 뒤 그 위치/방향으로 데칼 고정 (판정도 그 방향으로 나감)
            FaceTelegraphTarget(-1.0f);

            // 이 구간이 몇 번째 히트인지에 맞춰 HitRangeScales 배율 적용 (실제 판정과 같은 크기)
            TelegraphShownScale = GetAttackHitScale(ActiveWindowIndex);

            if (TelegraphShape == ETelegraphShape::Sector)
            {
                ShowSectorAttackIndicator(TelegraphRadius * TelegraphShownScale, TelegraphSafeZoneAngle, TelegraphMaterial, TelegraphInnerSafeRadius);
            }
            else
            {
                FVector ScaledExtent = TelegraphBoxExtent * TelegraphShownScale;
                ScaledExtent.Y *= GetAttackHitWidthScale(ActiveWindowIndex);
                ShowBoxAttackIndicator(ScaledExtent, TelegraphForwardOffset * TelegraphShownScale, TelegraphMaterial);
            }
        }
    }
    else if (bShouldShow && bTelegraphVisible && TelegraphShape == ETelegraphShape::Box
        && !FMath::IsNearlyEqual(GetAttackHitScale(ActiveWindowIndex), TelegraphShownScale))
    {
        // ★ 연타처럼 히트 구간이 붙어 있으면 데칼이 꺼지지 않고 다음 히트로 넘어가서, 처음 뜰 때 계산한 크기가
        // 계속 유지됨. 이번 히트의 배율이 달라졌으면(HitRangeScales/연타 바퀴별 증가) 방향은 그대로 두고 크기만 다시 그림
        TelegraphShownScale = GetAttackHitScale(ActiveWindowIndex);
        FVector ScaledExtent = TelegraphBoxExtent * TelegraphShownScale;
        ScaledExtent.Y *= GetAttackHitWidthScale(ActiveWindowIndex);
        ShowBoxAttackIndicator(ScaledExtent, TelegraphForwardOffset * TelegraphShownScale, TelegraphMaterial);
    }
    else if (!bShouldShow && bTelegraphVisible)
    {
        HideAttackIndicator();
    }
    else if (!bShouldShow && Position < TelegraphWindows.Last().Y && TelegraphShape != ETelegraphShape::Location)
    {
        // 히트 사이 구간: 다음 히트를 위해 대상 쪽으로 회전 (Location은 보스가 도는 것과 무관하므로 제외)
        FaceTelegraphTarget(DeltaTime);
    }

    // ★ Location 텔레그래프는 Box/Sector처럼 뜰 때 한 번만 배치하고 끝나는 게 아니라, 보이는 동안
    // 계속 대상 위치를 따라가야 합니다 (추적) — 고정 시각(ActiveWindow.Y - LockLeadTime) 전까지는
    // 매 프레임 위치를 갱신하고, 그 시각을 지나면 그 순간 위치에 딱 한 번 고정합니다.
    if (bShouldShow && TelegraphShape == ETelegraphShape::Location && AttackIndicator)
    {
        if (!bTelegraphLocationLocked && Position >= (ActiveWindow.Y - TelegraphLockLeadTime))
        {
            const FVector LockLoc = TelegraphTrackTarget.IsValid() ? TelegraphTrackTarget->GetActorLocation() : TelegraphLockedLocation;
            TelegraphLockedLocation = LockLoc;
            AttackIndicator->LockTargetLocation(LockLoc);
            bTelegraphLocationLocked = true;
        }
        else if (!bTelegraphLocationLocked && TelegraphTrackTarget.IsValid())
        {
            TelegraphLockedLocation = TelegraphTrackTarget->GetActorLocation();
            AttackIndicator->UpdateIndicatorTransform(GetActorLocation(), TelegraphLockedLocation, 0.0f);
        }
    }

    // ★ Location: 마지막 히트 전까지 매 프레임 대상 쪽으로 회전 (고정 후에는 고정 좌표를 바라봄)
    if (TelegraphShape == ETelegraphShape::Location && Position < TelegraphWindows.Last().Y)
    {
        FaceTelegraphTarget(DeltaTime);
    }

    bTelegraphVisible = bShouldShow;
}

void ARLCharacter::FaceTelegraphTarget(float DeltaTime)
{
    const AActor* Target = TelegraphFaceTarget.Get();
    if (!Target)
    {
        return;
    }

    const FVector TargetLocation = (TelegraphShape == ETelegraphShape::Location && bTelegraphLocationLocked)
        ? TelegraphLockedLocation
        : Target->GetActorLocation();
    const FVector ToTarget = TargetLocation - GetActorLocation();
    if (ToTarget.IsNearlyZero())
    {
        return;
    }

    const float TargetYaw = ToTarget.Rotation().Yaw;
    const float CurrentYaw = GetActorRotation().Yaw;
    const float NewYaw = DeltaTime < 0.0f ? TargetYaw : FMath::FixedTurn(CurrentYaw, TargetYaw, TelegraphTurnSpeed * DeltaTime);

    SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
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
    if (IsStunned()) return; // 경직 중엔 회피 불가

    FVector MyLocation = GetActorLocation();
    FVector DashDirection = CursorLocation - MyLocation;
    DashDirection.Z = 0.0f;

    if (DashDirection.IsNearlyZero()) return;
    DashDirection.Normalize();

    bCanDash = false;
    SetInvulnerable(true);

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
    SetInvulnerable(false);
}

void ARLCharacter::SetInvulnerable(bool bNewState)
{
    if (AbilitySystemComp)
    {
        AbilitySystemComp->SetLooseGameplayTagCount(RLGameplayTags::State_Invulnerable, bNewState ? 1 : 0);
    }
}

bool ARLCharacter::IsInvulnerable() const
{
    return AbilitySystemComp && AbilitySystemComp->HasMatchingGameplayTag(RLGameplayTags::State_Invulnerable);
}

void ARLCharacter::ApplyStun(float Duration)
{
    if (Duration <= 0.0f || !CanBeStaggered() || !AbilitySystemComp || (HealthComp && HealthComp->bIsDead))
    {
        return;
    }

    // 이미 경직 중이면 남은 시간보다 길 때만 연장 (짧은 경직이 긴 경직을 덮어쓰지 않도록)
    FTimerManager& TimerManager = GetWorldTimerManager();
    const bool bAlreadyStunned = IsStunned();
    if (bAlreadyStunned && TimerManager.GetTimerRemaining(StunTimerHandle) >= Duration)
    {
        return;
    }
    TimerManager.SetTimer(StunTimerHandle, this, &ARLCharacter::EndStun, Duration, false);

    if (bAlreadyStunned)
    {
        return;
    }

    // 스킬은 GA의 ActivationBlockedTags(State.Stunned)로, 이동/회피는 IsStunned 체크로 막힘
    AbilitySystemComp->SetLooseGameplayTagCount(RLGameplayTags::State_Stunned, 1);

    // 클릭 이동 경로를 끊음 (넉백으로 밀려나는 속도는 유지하려고 StopMovementImmediately는 쓰지 않음)
    if (AController* MyController = GetController())
    {
        MyController->StopMovement();
    }

    if (SkillComp && SkillComp->IsCasting())
    {
        SkillComp->InterruptCasting(TEXT("경직으로 인한 시전 취소"));
    }

    if (StunMontage)
    {
        PlayAnimMontage(StunMontage);
    }

    OnStunStarted(Duration);
}

bool ARLCharacter::IsStunned() const
{
    return AbilitySystemComp && AbilitySystemComp->HasMatchingGameplayTag(RLGameplayTags::State_Stunned);
}

void ARLCharacter::EndStun()
{
    if (AbilitySystemComp)
    {
        AbilitySystemComp->SetLooseGameplayTagCount(RLGameplayTags::State_Stunned, 0);
    }
    OnStunEnded();
}

void ARLCharacter::ResetDashCooldown()
{
    bCanDash = true;
}

// ★ 엔진 기본 ApplyDamage 경로 (BP_EnemyBase의 몬스터 공격 등)
// 절대 방어막: 무적이면 전부 무시. 그 외에는 ReceiveCustomDamage와 똑같이 방어력/피해감소를 적용해 GAS 피해로 넘깁니다.
// (예전엔 이 경로가 경감 없이 들어가서, 플레이어 방어력이 일반 몬스터 공격에 효과가 없었음)
float ARLCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
    if (IsInvulnerable())
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("대쉬 무적 판정! 대미지 무효화!"));
        return 0.0f;
    }

    AActor* Causer = DamageCauser ? DamageCauser : (EventInstigator ? EventInstigator->GetPawn() : nullptr);
    const float FinalDamage = HealthComp ? HealthComp->ApplyIncomingDamage(DamageAmount, Causer, false) : 0.0f;
    if (FinalDamage <= 0.0f)
    {
        return 0.0f;
    }

    // Super는 BP의 Event AnyDamage / OnTakeAnyDamage 알림만 보냅니다 (피해는 위에서 이미 적용). 경감 후 값으로 알림.
    return Super::TakeDamage(FinalDamage, DamageEvent, EventInstigator, DamageCauser);
}

// ★ 부드러운 대시 이동 시작
void ARLCharacter::StartDashMovement(FVector TargetLocation, float Duration)
{
    DashStartLocation = GetActorLocation();

    // ★ 낭떠러지/강 위로 대시해서 떨어지지 않도록, 목표 지점을 걸을 수 있는 영역(네비메시) 경계까지로 자름
    if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        FNavLocation StartOnNav;
        if (NavSys->ProjectPointToNavigation(DashStartLocation, StartOnNav, FVector(50.0f, 50.0f, 250.0f)))
        {
            const FVector RayEnd(TargetLocation.X, TargetLocation.Y, StartOnNav.Location.Z);
            FVector NavHitLocation;
            if (UNavigationSystemV1::NavigationRaycast(this, StartOnNav.Location, RayEnd, NavHitLocation, nullptr, GetController()))
            {
                // 짧아진 거리만큼 시간도 줄여서 대시 속도는 그대로 유지
                const float OriginalDist = FVector::Dist2D(DashStartLocation, TargetLocation);
                if (OriginalDist > KINDA_SMALL_NUMBER)
                {
                    Duration *= FVector::Dist2D(DashStartLocation, NavHitLocation) / OriginalDist;
                }
                TargetLocation.X = NavHitLocation.X;
                TargetLocation.Y = NavHitLocation.Y;
            }
        }
    }

    DashTargetLocation = TargetLocation;
    DashDuration = FMath::Max(0.01f, Duration);
    DashElapsedTime = 0.0f;
    bIsDashing = true;
    bIsSkillMoving = true; // Tick의 이동 억제 로직을 우회
}

// ★ 돌진형 보스 패턴용: StartDashMovement로 이동을 시작하고, 이동하는 동안 몸통 충돌 데미지 판정을 켭니다.
void ARLCharacter::StartDashAttack(FVector TargetLocation, float Duration, float HitRadius, const FRLDamageData& DamageData)
{
    StartDashMovement(TargetLocation, Duration);

    bDashDealsDamage = true;
    DashHitRadius = HitRadius;
    DashDamageData = DamageData;
    DashHitActors.Reset();
}

// ★ StartDashAttack 전용. 보스의 "현재" 위치(=매 틱 이동한 새 위치) 기준으로 구체 스윕해서, 아직 안 맞은
// 대상이 걸리면 CreateLocationHitbox와 같은 방식(ReceiveCustomDamage)으로 데미지를 줍니다.
void ARLCharacter::PerformDashHitCheck()
{
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<FHitResult> HitResults;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    const FVector Location = GetActorLocation();

    // ★ 판정 반지름 = DashHitRadius + 내 몸통(캡슐) 반지름. 몸통으로 부딪히는 공격이라, 덩치 큰 캐릭터(예: 3배 보스2)는
    // 상대가 캡슐에 막혀 중심에서 멀리 멈추므로 몸통 크기를 더해야 닿음. 높이는 원형 판정과 같은 원기둥 방식
    // (후보는 반높이만큼 넉넉한 구체로 모으고 수평 거리로 다시 거름 → 캡슐 중심이 높아도 플레이어 다리까지 닿음)
    const UCapsuleComponent* MyCapsule = GetCapsuleComponent();
    const float BodyRadius = MyCapsule ? MyCapsule->GetScaledCapsuleRadius() : 0.f;
    const float HitRadius = DashHitRadius + BodyRadius;
    const float SearchRadius = HitRadius + (MyCapsule ? MyCapsule->GetScaledCapsuleHalfHeight() : 0.f);

    // 판정 구간 = 몸통 위치 ~ 정면 DashForwardReach 앞 (도끼를 앞으로 내려치는 등 몸보다 앞까지 닿는 돌진)
    const FVector ReachEnd = Location + GetActorForwardVector().GetSafeNormal2D() * DashForwardReach;

    const bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
        GetWorld(),
        Location,
        ReachEnd + FVector(0.f, 0.f, 1.f),
        SearchRadius,
        ObjectTypes,
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResults,
        true
    );

    if (!bHit)
    {
        return;
    }

    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || DashHitActors.Contains(HitActor))
        {
            continue;
        }

        const ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
        const float TargetRadius = HitCharacter && HitCharacter->GetCapsuleComponent() ? HitCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
        const FVector TargetFlat(HitActor->GetActorLocation().X, HitActor->GetActorLocation().Y, Location.Z);
        const FVector ReachEndFlat(ReachEnd.X, ReachEnd.Y, Location.Z);
        if (FMath::PointDistToSegment(TargetFlat, Location, ReachEndFlat) - TargetRadius > HitRadius)
        {
            continue;
        }

        if (ARLCharacter* TargetChar = Cast<ARLCharacter>(HitActor))
        {
            DashHitActors.Add(HitActor);

            FRLDamageData FinalDamage = DashDamageData;
            FinalDamage.DamageCauser = this;
            TargetChar->ReceiveCustomDamage(FinalDamage, Hit);
        }
    }
}

void ARLCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ★ Box 공격 예고 데칼 표시/소멸 (아래 로직의 early return보다 먼저 처리)
    UpdateAttackTelegraph(DeltaTime);

    // ★ 대시 진행 중이면 매 프레임 부드럽게 이동
    if (bIsDashing)
    {
        DashElapsedTime += DeltaTime;
        float Alpha = FMath::Clamp(DashElapsedTime / DashDuration, 0.0f, 1.0f);
        FVector NewLocation = FMath::Lerp(DashStartLocation, DashTargetLocation, Alpha);
        SetActorLocation(NewLocation, true);

        // ★ StartDashAttack으로 시작된 돌진이면(bDashDealsDamage), 이동한 새 위치 기준으로 매 틱 충돌 체크
        if (bDashDealsDamage)
        {
            PerformDashHitCheck();
        }

        if (Alpha >= 1.0f)
        {
            bIsDashing = false;
            bIsSkillMoving = false; // 대시 끝나면 다시 이동 억제 로직 활성화
            bDashDealsDamage = false; // 데미지 판정도 함께 종료
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

void ARLCharacter::CreateLocationHitbox(FVector TargetLocation, float Radius, const FRLDamageData& DamageData, bool bDrawDebug)
{
    Radius *= ConsumeAttackHitScale();

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
                    // 일정 거리 밀어내기(bPushAwayFromCenter)용 중심
                    FinalDamage.PushOrigin = TargetLocation;
                    TargetChar->ReceiveCustomDamage(FinalDamage, Hit);
                }
            }
        }
    }
}

// ★ 보스 "자기 자신"을 중심으로 한 부채꼴(전방위) 판정. CreateLocationHitbox와 거의 동일하게
// SphereTraceMultiForObjects로 Radius 안의 Pawn을 전부 모은 뒤, 그중 "정면 기준 뒤쪽
// SafeZoneAngleDegrees도 폭"의 안전지대 쐐기 안에 있는 대상만 걸러내고 나머지 전부에게 한 번에
// 데미지를 줍니다. 예: SafeZoneAngleDegrees=90 → 정면 기준 좌우 135도씩(총 270도)이 위험지대이고,
// 바로 뒤 90도(좌우 45도씩)만 안전지대입니다.
void ARLCharacter::CreateSectorHitbox(float Radius, float SafeZoneAngleDegrees, const FRLDamageData& DamageData, bool bDrawDebug, float InnerSafeRadius)
{
    Radius *= ConsumeAttackHitScale();

    const FVector Origin = GetActorLocation();
    const FVector ForwardDir = GetActorForwardVector().GetSafeNormal2D();

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<FHitResult> HitResults;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    // 원 전체(Radius)를 우선 스윕으로 다 모으고, 아래에서 각도로 안전지대만 걸러냅니다.
    // (BoxTrace/ConeTrace 같은 부채꼴 전용 트레이스가 엔진에 없어서, 원형 스윕 + 각도 필터링 방식을 씁니다)
    // ★ 구체는 가장자리로 갈수록 위아래가 얇아져서, 캡슐 중심이 높은 큰 캐릭터는 바깥쪽 플레이어 다리 높이까지 안 닿음.
    // 그래서 반지름을 내 캡슐 반높이만큼 넉넉히 잡아 후보를 모으고, 아래에서 수평 거리로 다시 걸러 "원기둥" 판정으로 만듦
    const UCapsuleComponent* MyCapsule = GetCapsuleComponent();
    const float MyHalfHeight = MyCapsule ? MyCapsule->GetScaledCapsuleHalfHeight() : 0.f;
    const float SearchRadius = Radius + MyHalfHeight;

    // 디버그는 후보 수집용 구체(SearchRadius)가 아니라 실제 판정 범위(반지름 Radius, 바닥~머리 원기둥)를 그림 → 예고 데칼과 같은 크기
    if (bDrawDebug)
    {
        DrawDebugCylinder(GetWorld(), Origin - FVector(0.f, 0.f, MyHalfHeight), Origin + FVector(0.f, 0.f, MyHalfHeight), Radius, 48, FColor::Red, false, 1.5f);
    }

    bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
        GetWorld(),
        Origin,
        Origin + FVector(0.f, 0.f, 1.f),
        SearchRadius,
        ObjectTypes,
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResults,
        true,
        FLinearColor::Red,
        FLinearColor::Green,
        1.5f
    );

    if (bHit)
    {
        // 안전지대 쐐기의 절반 폭(정면 기준이 아니라 "정면의 반대 방향(Backward)" 기준 좌우 폭이라
        // Acos로 구한 "정면과의 각도"는 180도에 가까울수록 안전지대에 가깝습니다).
        const float SafeHalfAngle = SafeZoneAngleDegrees * 0.5f;

        TSet<AActor*> DamagedActors;
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor || DamagedActors.Contains(HitActor))
            {
                continue;
            }

            // 원기둥 판정: 수평 거리가 (반지름 + 대상 캡슐 반지름)보다 멀면 제외 (SearchRadius로 넉넉히 모은 후보 정리)
            const ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
            const float TargetRadius = HitCharacter && HitCharacter->GetCapsuleComponent() ? HitCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
            if (FVector::Dist2D(HitActor->GetActorLocation(), Origin) - TargetRadius > Radius)
            {
                continue;
            }

            // ★ 안쪽 안전 반경(거리 기준 안전지대) 안이면 제외. 예: Subjugation 균열 강화판(보스 가까이가 안전)
            if (InnerSafeRadius > 0.0f && FVector::Dist2D(HitActor->GetActorLocation(), Origin) < InnerSafeRadius)
            {
                continue;
            }

            // 보스와 거의 같은 위치(방향 정의 불가)면 안전지대 검사 없이 맞는 걸로 취급
            const FVector ToTarget2D = (HitActor->GetActorLocation() - Origin).GetSafeNormal2D();
            if (!ToTarget2D.IsNearlyZero())
            {
                const float AngleFromForward = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ForwardDir, ToTarget2D)));

                // 정면과의 각도가 (180 - SafeHalfAngle) 이상이면 "뒤쪽 안전지대 쐐기" 안에 있는 것 → 제외
                if (AngleFromForward >= (180.0f - SafeHalfAngle))
                {
                    continue;
                }
            }

            DamagedActors.Add(HitActor);
            if (ARLCharacter* TargetChar = Cast<ARLCharacter>(HitActor))
            {
                FRLDamageData FinalDamage = DamageData;
                FinalDamage.DamageCauser = this;
                // 일정 거리 밀어내기(bPushAwayFromCenter)용 중심
                FinalDamage.PushOrigin = Origin;
                TargetChar->ReceiveCustomDamage(FinalDamage, Hit);
            }
        }
    }

#if ENABLE_DRAW_DEBUG
    if (bDrawDebug)
    {
        // 안전지대 쐐기의 좌/우 경계선을 디버그 라인으로 그려서(Backward 방향 기준 ±SafeHalfAngle),
        // 실제로 어디까지가 안전지대인지 눈으로 확인할 수 있게 합니다.
        const FVector BackwardDir = -ForwardDir;
        const FVector LeftBoundary = BackwardDir.RotateAngleAxis(-(SafeZoneAngleDegrees * 0.5f), FVector::UpVector);
        const FVector RightBoundary = BackwardDir.RotateAngleAxis(SafeZoneAngleDegrees * 0.5f, FVector::UpVector);
        DrawDebugLine(GetWorld(), Origin, Origin + LeftBoundary * Radius, FColor::Blue, false, 1.5f, 0, 3.0f);
        DrawDebugLine(GetWorld(), Origin, Origin + RightBoundary * Radius, FColor::Blue, false, 1.5f, 0, 3.0f);
    }
#endif
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

// ★ 보스 주변에 지원군(잔몹)을 소환합니다. ARLArenaManager::GetSpawnLocation의 네비메시 보정 방식과
// 같은 함수(ProjectPointToNavigation)를 재사용해서, 허공이나 벽 속에 스폰되지 않게 합니다.
void ARLCharacter::SummonMinions(TSubclassOf<AActor> MinionClass, int32 Count, float SpawnRadius)
{
    if (!MinionClass || Count <= 0)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector Origin = GetActorLocation();

    for (int32 i = 0; i < Count; ++i)
    {
        // 보스 중심 원형 범위 내 무작위 지점 (XY 평면 랜덤, Z는 보스와 같은 높이에서 시작해 아래서 보정)
        const FVector2D RandomOffset2D = FMath::RandPointInCircle(SpawnRadius);
        FVector SpawnLocation = Origin + FVector(RandomOffset2D.X, RandomOffset2D.Y, 0.0f);

        // 네비메시 위 안전한 지점으로 보정 (RLArenaManager::GetSpawnLocation과 같은 방식)
        if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
        {
            FNavLocation NavLoc;
            if (NavSys->ProjectPointToNavigation(SpawnLocation, NavLoc, FVector(SpawnRadius, SpawnRadius, 500.0f)))
            {
                SpawnLocation = NavLoc.Location;
            }
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        World->SpawnActor<AActor>(MinionClass, SpawnLocation, GetActorRotation(), SpawnParams);
    }
}

void ARLCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
    if (IsStunned())
    {
        return;
    }

    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        if (AnimInstance->IsAnyMontagePlaying())
        {
            return;
        }
    }

    Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void ARLCharacter::SaveCurrentRun()
{
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        const FString RawMapName = GetWorld() ? GetWorld()->GetMapName() : TEXT("Map_Persistent");
        const FString CleanMapName = UWorld::RemovePIEPrefix(RawMapName);
        SaveSys->SaveActiveRun(this, FName(*CleanMapName), SaveSys->GetActiveRunData().StageNumber);
    }
}

void ARLCharacter::LoadCurrentRun()
{
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        if (SaveSys->LoadActiveRun(false))
        {
            const FString CurrentMap = GetWorld() ? UWorld::RemovePIEPrefix(GetWorld()->GetMapName()) : TEXT("");
            const FString SavedMap = UWorld::RemovePIEPrefix(SaveSys->GetActiveRunData().CurrentMapName.ToString());

            // 같은 맵이면 현재 플레이어에게 즉시 위치 및 모든 스탯/아이템 복원
            if (SavedMap.IsEmpty() || SavedMap == TEXT("None") || CurrentMap.Equals(SavedMap, ESearchCase::IgnoreCase))
            {
                SaveSys->ApplyRunDataToPlayer(this);
            }
            else
            {
                // 다른 맵에서 저장된 경우 해당 레벨을 열고 진입 시 복원
                SaveSys->ResumeActiveRun();
            }
        }
    }
}

void ARLCharacter::ClearCurrentRun()
{
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        SaveSys->ClearActiveRun(true);
    }
}

void ARLCharacter::AddSouls(int32 Amount)
{
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        SaveSys->AddTotalSouls(Amount);
    }
}

void ARLCharacter::UpgradeMetaTrait(FName UpgradeKey)
{
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        FString FailReason;
        const bool bSuccess = SaveSys->UpgradeMetaTrait(UpgradeKey, FailReason);
        if (!bSuccess && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                FString::Printf(TEXT("❌ [특성 강화 실패] %s"), *FailReason));
        }
    }
}

void ARLCharacter::ResetMetaUpgrades()
{
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        SaveSys->ResetAllMetaUpgrades(true);
    }
}

void ARLCharacter::ShowMetaUpgrades()
{
    if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Cyan,
                FString::Printf(TEXT("=== [보유 영혼: %d Souls] ==="), SaveSys->GetTotalSouls()));

            const TArray<FRLMetaUpgradeInfo> Infos = SaveSys->GetAllMetaUpgradeInfos();
            for (const FRLMetaUpgradeInfo& Info : Infos)
            {
                FString StatusStr;
                if (Info.bIsMaxLevel)
                {
                    StatusStr = TEXT("MAX");
                }
                else
                {
                    StatusStr = FString::Printf(TEXT("Lv.%d/%d (다음: %d Souls)"), Info.CurrentLevel, Info.MaxLevel, Info.NextLevelCost);
                }

                GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Yellow,
                    FString::Printf(TEXT("• %s (%s): %s"), *Info.DisplayName.ToString(), *Info.UpgradeKey.ToString(), *StatusStr));
            }
        }
    }
}


