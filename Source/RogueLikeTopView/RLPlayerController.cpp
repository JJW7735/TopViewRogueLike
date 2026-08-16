#include "RLPlayerController.h"
#include "RLCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "RLSkillComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"

ARLPlayerController::ARLPlayerController()
{
    bShowMouseCursor = true;
}

void ARLPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void ARLPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // 1. 이동 및 기본 공격 바인딩
        if (RightClickMoveAction)
        {
            EnhancedInputComponent->BindAction(RightClickMoveAction, ETriggerEvent::Started, this, &ARLPlayerController::OnMoveStarted);
            EnhancedInputComponent->BindAction(RightClickMoveAction, ETriggerEvent::Triggered, this, &ARLPlayerController::OnMoveTriggered);
        }
        if (TestAttackAction)
        {
            EnhancedInputComponent->BindAction(TestAttackAction, ETriggerEvent::Started, this, &ARLPlayerController::OnTestAttack);
        }

        // 2. Q 스킬 바인딩
        if (SkillQ_Action)
        {
            EnhancedInputComponent->BindAction(SkillQ_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillQ_Started);
            EnhancedInputComponent->BindAction(SkillQ_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillQ_Triggered);
            EnhancedInputComponent->BindAction(SkillQ_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillQ_Completed);
        }

        // 3. W 스킬 바인딩
        if (SkillW_Action)
        {
            EnhancedInputComponent->BindAction(SkillW_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillW_Started);
            EnhancedInputComponent->BindAction(SkillW_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillW_Triggered);
            EnhancedInputComponent->BindAction(SkillW_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillW_Completed);
        }

        // 4. E 스킬 바인딩
        if (SkillE_Action)
        {
            EnhancedInputComponent->BindAction(SkillE_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillE_Started);
            EnhancedInputComponent->BindAction(SkillE_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillE_Triggered);
            EnhancedInputComponent->BindAction(SkillE_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillE_Completed);
        }

        // 5. R 스킬 바인딩
        if (SkillR_Action)
        {
            EnhancedInputComponent->BindAction(SkillR_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillR_Started);
            EnhancedInputComponent->BindAction(SkillR_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillR_Triggered);
            EnhancedInputComponent->BindAction(SkillR_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillR_Completed);
        }
    }
}

void ARLPlayerController::OnTestAttack()
{
    if (ARLCharacter* ControlledChar = Cast<ARLCharacter>(GetPawn()))
    {
        if (URLSkillComponent* SkillComp = ControlledChar->GetSkillComponent())
        {
            // ★ 장판 조준 모드일 때 좌클릭 -> 스킬 시전 확정!
            if (SkillComp->IsAiming())
            {
                SkillComp->ConfirmGroundCast();
                return;
            }
        }

        // 평상시 기본 공격
        ControlledChar->TestAttack();
    }
}

void ARLPlayerController::OnMoveStarted()
{
    if (ARLCharacter* ControlledChar = Cast<ARLCharacter>(GetPawn()))
    {
        if (URLSkillComponent* SkillComp = ControlledChar->GetSkillComponent())
        {
            // ★ 장판 조준 중 우클릭 시 이동하지 않고 조준 취소!
            if (SkillComp->IsAiming())
            {
                SkillComp->CancelGroundCast();
                return;
            }

            if (SkillComp->IsCasting())
            {
                SkillComp->InterruptCasting(TEXT("이동 명령으로 인한 시전 취소"));
            }
        }
    }

    FHitResult Hit;
    if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
    {
        if (FXCursor)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(),
                FXCursor,
                Hit.Location,
                Hit.ImpactNormal.Rotation()
            );
        }
    }
}

void ARLPlayerController::OnMoveTriggered()
{
    if (ARLCharacter* ControlledChar = Cast<ARLCharacter>(GetPawn()))
    {
        if (URLSkillComponent* SkillComp = ControlledChar->GetSkillComponent())
        {
            // ★ 조준 중에는 우클릭 이동 차단
            if (SkillComp->IsAiming()) return;
        }
    }

    FHitResult Hit;
    if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Hit.Location);
    }
}

// ==========================================
// Q 스킬 발동 로직
// ==========================================
void ARLPlayerController::OnSkillQ_Started()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputStarted(ESkillSlot::Slot_Q);
}
void ARLPlayerController::OnSkillQ_Triggered()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_Q);
}
void ARLPlayerController::OnSkillQ_Completed()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_Q);
}

// ==========================================
// W 스킬 발동 로직
// ==========================================
void ARLPlayerController::OnSkillW_Started()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputStarted(ESkillSlot::Slot_W);
}
void ARLPlayerController::OnSkillW_Triggered()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_W);
}
void ARLPlayerController::OnSkillW_Completed()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_W);
}

// ==========================================
// E 스킬 발동 로직
// ==========================================
void ARLPlayerController::OnSkillE_Started()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputStarted(ESkillSlot::Slot_E);
}
void ARLPlayerController::OnSkillE_Triggered()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_E);
}
void ARLPlayerController::OnSkillE_Completed()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_E);
}

// ==========================================
// R 스킬 발동 로직
// ==========================================
void ARLPlayerController::OnSkillR_Started()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputStarted(ESkillSlot::Slot_R);
}
void ARLPlayerController::OnSkillR_Triggered()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_R);
}
void ARLPlayerController::OnSkillR_Completed()
{
    if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
        if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
            SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_R);
}