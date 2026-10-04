#include "RLPlayerController.h"
#include "RLCharacter.h"
#include "RLEnemyCharacter.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "RLSkillComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

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
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ARLPlayerController::OnInteract_Started);
		}
	}

#if !UE_BUILD_SHIPPING
	// ★ 테스트용 치트 키 (패키징 Shipping 빌드에서는 제외)
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ARLPlayerController::KillAllEnemies);
#endif
}

void ARLPlayerController::KillAllEnemies()
{
	int32 KilledCount = 0;
	for (TActorIterator<ARLEnemyCharacter> It(GetWorld()); It; ++It)
	{
		ARLEnemyCharacter* Enemy = *It;
		if (IsValid(Enemy) && Enemy != GetPawn())
		{
			if (Enemy->DebugKill())
			{
				++KilledCount;
			}
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("[Cheat] 적 %d마리 처치"), KilledCount));
	}
}

void ARLPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!OcclusionMPC)
	{
		OcclusionMPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Effect/Material/Occlusion/MPC_Occlusion.MPC_Occlusion"));
	}

	const APawn* ControlledPawn = GetPawn();
	if (!OcclusionMPC || !ControlledPawn || !PlayerCameraManager)
	{
		return;
	}

	UKismetMaterialLibrary::SetVectorParameterValue(this, OcclusionMPC, TEXT("PlayerLocation"), FLinearColor(ControlledPawn->GetActorLocation()));
	UKismetMaterialLibrary::SetVectorParameterValue(this, OcclusionMPC, TEXT("CameraLocation"), FLinearColor(PlayerCameraManager->GetCameraLocation()));
}

bool ARLPlayerController::GetGroundHitUnderCursor(FHitResult& OutHit) const
{
	FVector RayOrigin;
	FVector RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	const APawn* ControlledPawn = GetPawn();
	// 폰이 없으면(관전 등) 높이 제한 없이 위를 향한 첫 면을 채택
	float PawnFootZ = UE_BIG_NUMBER * 0.5f;
	if (const ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn))
	{
		PawnFootZ = ControlledCharacter->GetActorLocation().Z - ControlledCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RLGroundUnderCursor), true);
	if (ControlledPawn)
	{
		Params.AddIgnoredActor(ControlledPawn);
	}

	const FVector TraceEnd = RayOrigin + RayDirection * 100000.0f;

	// 바닥이 아닌 것(구조물 윗면, 기둥 옆면, 캐릭터)에 맞으면 그 컴포넌트를 무시하고 다시 쏩니다
	constexpr int32 MaxRetrace = 8;
	for (int32 Attempt = 0; Attempt < MaxRetrace; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, RayOrigin, TraceEnd, ECC_Ground, Params))
		{
			return false;
		}

		const bool bIsPawn = Hit.GetActor() && Hit.GetActor()->IsA<APawn>();
		const bool bFacesUp = Hit.ImpactNormal.Z >= GroundMinNormalZ;
		const bool bNearFootHeight = Hit.ImpactPoint.Z <= PawnFootZ + GroundMaxHeightAbovePawn;

		if (!bIsPawn && bFacesUp && bNearFootHeight)
		{
			OutHit = Hit;
			return true;
		}

		if (!Hit.GetComponent())
		{
			return false;
		}
		Params.AddIgnoredComponent(Hit.GetComponent());
	}

	return false;
}

void ARLPlayerController::OnDodge_Started()
{
	if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
	{
		FHitResult Hit;
		if (GetGroundHitUnderCursor(Hit))
		{
			RLChar->Dodge(Hit.ImpactPoint);
		}
	}
}

void ARLPlayerController::OnInteract_Started()
{
	if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
	{
		RLChar->TryInteract();
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
	if (GetGroundHitUnderCursor(Hit))
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
	if (GetGroundHitUnderCursor(Hit))
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