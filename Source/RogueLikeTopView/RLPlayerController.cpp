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
	// ★ 드래그 시 커서 고정
	DefaultMouseCursor = EMouseCursor::Default;
}

void ARLPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// ★ 마우스 입력 모드 캡처 시 커서 숨김 방지 세팅
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

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
		if (RightClickMoveAction)
		{
			EnhancedInputComponent->BindAction(RightClickMoveAction, ETriggerEvent::Started, this, &ARLPlayerController::OnMoveStarted);
			EnhancedInputComponent->BindAction(RightClickMoveAction, ETriggerEvent::Triggered, this, &ARLPlayerController::OnMoveTriggered);
		}
		if (BasicAttackAction)
		{
			EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &ARLPlayerController::OnBasicAttack_Started);
			EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Triggered, this, &ARLPlayerController::OnBasicAttack_Triggered);
			EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Completed, this, &ARLPlayerController::OnBasicAttack_Completed);
		}
		if (SkillQ_Action)
		{
			EnhancedInputComponent->BindAction(SkillQ_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillQ_Started);
			EnhancedInputComponent->BindAction(SkillQ_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillQ_Triggered);
			EnhancedInputComponent->BindAction(SkillQ_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillQ_Completed);
		}
		if (SkillW_Action)
		{
			EnhancedInputComponent->BindAction(SkillW_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillW_Started);
			EnhancedInputComponent->BindAction(SkillW_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillW_Triggered);
			EnhancedInputComponent->BindAction(SkillW_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillW_Completed);
		}
		if (SkillE_Action)
		{
			EnhancedInputComponent->BindAction(SkillE_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillE_Started);
			EnhancedInputComponent->BindAction(SkillE_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillE_Triggered);
			EnhancedInputComponent->BindAction(SkillE_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillE_Completed);
		}
		if (SkillR_Action)
		{
			EnhancedInputComponent->BindAction(SkillR_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnSkillR_Started);
			EnhancedInputComponent->BindAction(SkillR_Action, ETriggerEvent::Triggered, this, &ARLPlayerController::OnSkillR_Triggered);
			EnhancedInputComponent->BindAction(SkillR_Action, ETriggerEvent::Completed, this, &ARLPlayerController::OnSkillR_Completed);
		}
		if (DodgeAction)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &ARLPlayerController::OnDodge_Started);
		}
	}
}

void ARLPlayerController::OnDodge_Started()
{
	if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
	{
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
		{
			RLChar->Dodge(Hit.ImpactPoint);
		}
	}
}

// ==========================================
// 좌클릭(기본 공격) 발동 로직
// ==========================================
void ARLPlayerController::OnBasicAttack_Started()
{
	if (ARLCharacter* ControlledChar = Cast<ARLCharacter>(GetPawn()))
	{
		if (URLSkillComponent* SkillComp = ControlledChar->GetSkillComponent())
		{
			if (SkillComp->IsAiming())
			{
				SkillComp->ConfirmGroundCast();
				return;
			}
			SkillComp->OnSkillInputStarted(ESkillSlot::Slot_LMB);
		}
	}
}

void ARLPlayerController::OnBasicAttack_Triggered()
{
	if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
		if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
			SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_LMB);
}

void ARLPlayerController::OnBasicAttack_Completed()
{
	if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
		if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent())
			SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_LMB);
}

// ==========================================
// 이동 발동 로직
// ==========================================
void ARLPlayerController::OnMoveStarted()
{
	if (ARLCharacter* ControlledChar = Cast<ARLCharacter>(GetPawn()))
	{
		if (URLSkillComponent* SkillComp = ControlledChar->GetSkillComponent())
		{
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
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), FXCursor, Hit.Location, Hit.ImpactNormal.Rotation());
		}
	}
}

void ARLPlayerController::OnMoveTriggered()
{
	if (ARLCharacter* ControlledChar = Cast<ARLCharacter>(GetPawn()))
	{
		if (URLSkillComponent* SkillComp = ControlledChar->GetSkillComponent())
		{
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
// 스킬 Q, W, E, R (중복 제거 및 정리)
// ==========================================
void ARLPlayerController::OnSkillQ_Started() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputStarted(ESkillSlot::Slot_Q); }
void ARLPlayerController::OnSkillQ_Triggered() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_Q); }
void ARLPlayerController::OnSkillQ_Completed() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_Q); }

void ARLPlayerController::OnSkillW_Started() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputStarted(ESkillSlot::Slot_W); }
void ARLPlayerController::OnSkillW_Triggered() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_W); }
void ARLPlayerController::OnSkillW_Completed() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_W); }

void ARLPlayerController::OnSkillE_Started() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputStarted(ESkillSlot::Slot_E); }
void ARLPlayerController::OnSkillE_Triggered() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_E); }
void ARLPlayerController::OnSkillE_Completed() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_E); }

void ARLPlayerController::OnSkillR_Started() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputStarted(ESkillSlot::Slot_R); }
void ARLPlayerController::OnSkillR_Triggered() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputTriggered(ESkillSlot::Slot_R); }
void ARLPlayerController::OnSkillR_Completed() { if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn())) if (URLSkillComponent* SkillComp = RLChar->GetSkillComponent()) SkillComp->OnSkillInputCompleted(ESkillSlot::Slot_R); }