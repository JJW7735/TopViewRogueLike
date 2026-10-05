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
#include "RLRewardSubsystem.h"
#include "RLRewardSelectWidget.h"
#include "RLInventoryComponent.h"
#include "RLItemDataAsset.h"
#include "RLSaveSubsystem.h"
#include "RLDecalIndicator.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

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
}

void ARLCharacter::BeginPlay()
{
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
        StatComp->OnStatsChanged.AddDynamic(this, &ARLCharacter::HandleStatsChanged);
        StatComp->OnLevelUp.AddDynamic(this, &ARLCharacter::HandleLevelUp);
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->MaxWalkSpeed = StatComp->MoveSpeed;
        }
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

    if (bIsInvulnerable && !DamageData.bIgnoreInvincibility)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Dodge / Invulnerable!"));
        return;
    }

    // 피격자(나)의 방어력 및 피해 감소율 적용
    float FinalDamageAmount = DamageData.BaseDamage;
    float ReducedAmount = 0.0f;

    if (StatComp && !DamageData.bTrueDamage)
    {
        FinalDamageAmount = StatComp->CalculateDefendedDamage(DamageData.BaseDamage, ReducedAmount);
    }

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

    // 보스/엘리트처럼 경직 면역(CanBeStaggered == false)인 대상은 넉백으로 밀려나지 않습니다.
    const bool bCanBeStaggered = CanBeStaggered();

    if (bCanBeStaggered && DamageData.KnockbackForce > 0.0f)
    {
        float KnockbackMultiplier = DamageData.bIsCritical ? 1.25f : 1.0f;
        FVector KnockbackDir = (GetActorLocation() - HitInfo.ImpactPoint).GetSafeNormal2D();
        LaunchCharacter(KnockbackDir * (DamageData.KnockbackForce * KnockbackMultiplier) + FVector(0.f, 0.f, 150.f), true, true);
    }

    AController* InstigatorController = DamageData.DamageCauser ? DamageData.DamageCauser->GetInstigatorController() : nullptr;
    UGameplayStatics::ApplyDamage(this, FinalDamageAmount, InstigatorController, DamageData.DamageCauser, UDamageType::StaticClass());

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


void ARLCharacter::HandleStatsChanged()
{
    if (StatComp && GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = StatComp->MoveSpeed;
    }
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

    // 3. 3지선다 보상 카드 선택창 자동 팝업
    TestOpenRewardUI();
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

void ARLCharacter::RefreshInteractPrompt()
{
    // TryInteract와 같은 우선순위: 상자가 있으면 상자 문구, 없으면 포탈 문구
    if (NearbyChest)
    {
        OnInteractPromptChanged(true, NearbyChest->InteractPromptText);
    }
    else if (NearbyPortal)
    {
        OnInteractPromptChanged(true, NearbyPortal->InteractPromptText);
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

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("주변에 상호작용할 대상(상자/포탈)이 없습니다."));
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
            Widget->InitializeWidget(this, Rewards);
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

    // 1. 체력 회복 물약 (소비 아이템, 5개)
    URLItemDataAsset* PotionItem = NewObject<URLItemDataAsset>(this, TEXT("Test_HealthPotion"));
    PotionItem->ItemID = FName("Item_HealthPotion");
    PotionItem->ItemName = FText::FromString(TEXT("체력 회복 물약"));
    PotionItem->Description = FText::FromString(TEXT("사용 시 체력을 40 즉시 회복합니다. (최대 10개 중첩)"));
    PotionItem->ItemType = ERLItemType::Consumable;
    PotionItem->Rarity = ERLRewardRarity::Common;
    PotionItem->MaxStackSize = 10;
    PotionItem->InstantHealAmount = 40.0f;
    PotionItem->bConsumeOnUse = true;
    PotionItem->GoldValue = 15;
    InventoryComp->AddItem(PotionItem, 5);

    // 2. 전사의 완력 반지 (패시브 유물, 공격력 +15, 치명타 +10%)
    URLItemDataAsset* RingItem = NewObject<URLItemDataAsset>(this, TEXT("Test_WarriorRing"));
    RingItem->ItemID = FName("Item_WarriorRing");
    RingItem->ItemName = FText::FromString(TEXT("전사의 완력 반지"));
    RingItem->Description = FText::FromString(TEXT("소지 시 공격력 +15, 치명타 확률 +10%를 부여합니다."));
    RingItem->ItemType = ERLItemType::Passive;
    RingItem->Rarity = ERLRewardRarity::Rare;
    RingItem->MaxStackSize = 1;
    RingItem->GoldValue = 80;

    FRLStatBonus AtkBonus;
    AtkBonus.TargetStat = ERLTargetStat::AttackPower;
    AtkBonus.Value = 15.0f;
    RingItem->PassiveStatBonuses.Add(AtkBonus);

    FRLStatBonus CritBonus;
    CritBonus.TargetStat = ERLTargetStat::CritChance;
    CritBonus.Value = 10.0f;
    RingItem->PassiveStatBonuses.Add(CritBonus);

    InventoryComp->AddItem(RingItem, 1);

    // 3. 바람의 신속 장화 (패시브 유물, 이동속도 +120)
    URLItemDataAsset* BootsItem = NewObject<URLItemDataAsset>(this, TEXT("Test_WindBoots"));
    BootsItem->ItemID = FName("Item_WindBoots");
    BootsItem->ItemName = FText::FromString(TEXT("바람의 신속 장화"));
    BootsItem->Description = FText::FromString(TEXT("소지 시 이동 속도 +120을 부여합니다."));
    BootsItem->ItemType = ERLItemType::Passive;
    BootsItem->Rarity = ERLRewardRarity::Epic;
    BootsItem->MaxStackSize = 1;
    BootsItem->GoldValue = 120;

    FRLStatBonus SpeedBonus;
    SpeedBonus.TargetStat = ERLTargetStat::MoveSpeed;
    SpeedBonus.Value = 120.0f;
    BootsItem->PassiveStatBonuses.Add(SpeedBonus);

    InventoryComp->AddItem(BootsItem, 1);

    // 4. 빛나는 루비 원석 (재료/가치품, 3개)
    URLItemDataAsset* RubyItem = NewObject<URLItemDataAsset>(this, TEXT("Test_Ruby"));
    RubyItem->ItemID = FName("Item_Ruby");
    RubyItem->ItemName = FText::FromString(TEXT("빛나는 루비 원석"));
    RubyItem->Description = FText::FromString(TEXT("상점에 비싸게 판매할 수 있는 희귀 보석입니다."));
    RubyItem->ItemType = ERLItemType::Material;
    RubyItem->Rarity = ERLRewardRarity::Legendary;
    RubyItem->MaxStackSize = 99;
    RubyItem->GoldValue = 200;
    InventoryComp->AddItem(RubyItem, 3);

    // 5. 골드 지급 (300 Gold)
    InventoryComp->AddGold(300);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green,
            TEXT("🎁 [치트] 테스트용 아이템 4종(물약 5개, 전사 반지 1개, 신속 장화 1개, 루비 3개)과 300 골드가 인벤토리에 지급되었습니다!"));
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

void ARLCharacter::CreateBoxHitbox(FVector BoxExtent, float ForwardOffset, const FRLDamageData& DamageData, bool bDrawDebug)
{
    // ★ 이번 히트의 범위 배율 (SetAttackHitScales 미설정이면 1.0). 박스가 정면 쪽으로 길어지고 폭도 넓어짐
    const float HitWidthScale = GetAttackHitWidthScale(AttackHitCounter); // Consume 전에 같은 순번으로 조회
    const float HitScale = ConsumeAttackHitScale();
    BoxExtent *= HitScale;
    BoxExtent.Y *= HitWidthScale;
    ForwardOffset *= HitScale;

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

void ARLCharacter::ShowBoxAttackIndicator(FVector BoxExtent, float ForwardOffset, UMaterialInterface* DecalMaterial)
{
    if (!AttackIndicator)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AttackIndicator = GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    if (!AttackIndicator)
    {
        return;
    }

    AttackIndicator->SetupBoxIndicator(BoxExtent, DecalMaterial);
    AttackIndicator->UpdateBoxTransform(GetActorLocation(), ForwardOffset, GetActorRotation().Yaw);
    AttackIndicator->SetIndicatorVisibility(true);
}

void ARLCharacter::ShowSectorAttackIndicator(float Radius, float SafeZoneAngleDegrees, UMaterialInterface* DecalMaterial, float InnerSafeRadius)
{
    // ShowBoxAttackIndicator와 완전히 같은 지연 스폰(lazy-spawn) 패턴 — AttackIndicator 액터를
    // Box/Sector 상관없이 하나만 두고 재사용합니다 (한 캐릭터가 한 번에 하나의 예고만 표시하므로 충분).
    if (!AttackIndicator)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AttackIndicator = GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    if (!AttackIndicator)
    {
        return;
    }

    AttackIndicator->SetupSectorIndicator(Radius, SafeZoneAngleDegrees, DecalMaterial, InnerSafeRadius);
    AttackIndicator->UpdateSectorTransform(GetActorLocation(), GetActorRotation().Yaw);
    AttackIndicator->SetIndicatorVisibility(true);
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
    if (!AttackIndicator)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AttackIndicator = GetWorld()->SpawnActor<ARLDecalIndicator>(ARLDecalIndicator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    if (!AttackIndicator)
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
}

float ARLCharacter::GetAttackHitScale(int32 HitIndex) const
{
    if (AttackHitScales.Num() == 0)
    {
        return 1.0f;
    }
    return FMath::Max(0.01f, AttackHitScales[FMath::Clamp(HitIndex, 0, AttackHitScales.Num() - 1)]);
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

    const bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
        GetWorld(),
        Location,
        Location + FVector(0.f, 0.f, 1.f),
        DashHitRadius,
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

void ARLCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
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

    EDrawDebugTrace::Type DebugTrace = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

    // 원 전체(Radius)를 우선 스윕으로 다 모으고, 아래에서 각도로 안전지대만 걸러냅니다.
    // (BoxTrace/ConeTrace 같은 부채꼴 전용 트레이스가 엔진에 없어서, 원형 스윕 + 각도 필터링 방식을 씁니다)
    bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
        GetWorld(),
        Origin,
        Origin + FVector(0.f, 0.f, 1.f),
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


