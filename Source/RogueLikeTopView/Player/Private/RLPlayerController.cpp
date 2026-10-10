#include "RLPlayerController.h"
#include "RLCharacter.h"
#include "RLEnemyCharacter.h"
#include "RLArenaManager.h"
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
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "RLRewardChest.h"
#include "RLSaveSubsystem.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

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

void ARLPlayerController::ShowStageClearScreen()
{
	TSubclassOf<UUserWidget> TargetWidgetClass = StageClearWidgetClass;
	if (!TargetWidgetClass)
	{
		TargetWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Blueprint/UI/WBP_StageClear.WBP_StageClear_C"));
		if (!TargetWidgetClass)
		{
			return;
		}
	}

	// 이전 클리어 화면이 남아있으면 먼저 정리
	HideStageClearScreen();

	StageClearWidget = CreateWidget<UUserWidget>(this, TargetWidgetClass);
	if (!StageClearWidget)
	{
		return;
	}

	// 마우스 클릭(이동/공격)을 가로채지 않도록 히트 테스트 제외
	StageClearWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	StageClearWidget->AddToViewport();

	GetWorldTimerManager().SetTimer(StageClearHideTimerHandle, this, &ARLPlayerController::HideStageClearScreen,
		FMath::Max(StageClearDisplayDuration, 0.1f), false);
}

void ARLPlayerController::HideStageClearScreen()
{
	GetWorldTimerManager().ClearTimer(StageClearHideTimerHandle);

	if (StageClearWidget)
	{
		StageClearWidget->RemoveFromParent();
		StageClearWidget = nullptr;
	}
}

void ARLPlayerController::WatchBossRewardChest(ARLRewardChest* Chest)
{
	if (!Chest)
	{
		return;
	}

	if (ARLRewardChest* Prev = WatchedBossChest.Get())
	{
		Prev->OnRewardClaimed.RemoveDynamic(this, &ARLPlayerController::HandleBossChestClaimed);
	}

	WatchedBossChest = Chest;
	Chest->OnRewardClaimed.AddUniqueDynamic(this, &ARLPlayerController::HandleBossChestClaimed);
}

void ARLPlayerController::HandleBossChestClaimed(ARLRewardChest* Chest)
{
	if (!Chest || Chest != WatchedBossChest.Get())
	{
		return;
	}

	Chest->OnRewardClaimed.RemoveDynamic(this, &ARLPlayerController::HandleBossChestClaimed);
	WatchedBossChest = nullptr;

	ShowVictoryScreen();
}

void ARLPlayerController::ShowVictoryScreen()
{
	if (VictoryWidget)
	{
		return;
	}

	TSubclassOf<UUserWidget> TargetWidgetClass = VictoryWidgetClass;
	if (!TargetWidgetClass)
	{
		TargetWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Blueprint/UI/WBP_Victory.WBP_Victory_C"));
		if (!TargetWidgetClass)
		{
			return;
		}
	}

	// 1. 위젯 생성
	VictoryWidget = CreateWidget<UUserWidget>(this, TargetWidgetClass);
	if (!VictoryWidget)
	{
		return;
	}

	// 2. 표시할 값 채우기 (WBP_Victory의 BP 변수: CharacterNameText / StageReached)
	FText CharacterName;
	if (const ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
	{
		CharacterName = RLChar->CharacterDisplayName;
	}

	int32 StageReached = 1;
	if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
	{
		StageReached = SaveSys->GetActiveRunData().StageNumber;
	}

	if (FTextProperty* NameProp = FindFProperty<FTextProperty>(VictoryWidget->GetClass(), TEXT("CharacterNameText")))
	{
		NameProp->SetPropertyValue_InContainer(VictoryWidget, CharacterName);
	}
	if (FIntProperty* StageProp = FindFProperty<FIntProperty>(VictoryWidget->GetClass(), TEXT("StageReached")))
	{
		StageProp->SetPropertyValue_InContainer(VictoryWidget, StageReached);
	}

	// 3. 값을 텍스트에 반영 (Construct에서도 호출되지만, 값 채운 직후 명시적으로 한 번 더 호출)
	if (UFunction* ApplyFn = VictoryWidget->FindFunction(TEXT("ApplyVictoryData")))
	{
		VictoryWidget->ProcessEvent(ApplyFn, nullptr);
	}

	// 메인 메뉴 버튼 연결
	if (UButton* MainMenuButton = Cast<UButton>(VictoryWidget->GetWidgetFromName(TEXT("Btn_MainMenu"))))
	{
		MainMenuButton->OnClicked.AddUniqueDynamic(this, &ARLPlayerController::HandleVictoryMainMenuClicked);
	}

	// 4. 화면에 추가 + 모달 처리 (일시정지, 커서 표시, UI 전용 입력)
	VictoryWidget->AddToViewport(100);

	UGameplayStatics::SetGamePaused(this, true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(VictoryWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ARLPlayerController::HandleVictoryMainMenuClicked()
{
	// QuitToMainMenuAfterDeath와 같은 방식으로 입력/일시정지를 원복하고 메인 메뉴로 이동.
	// (런 정리/소울 정산은 이 단계에서 하지 않음)
	UGameplayStatics::SetGamePaused(this, false);

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;

	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
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
	FVector TraceStart = RayOrigin;

	// 바닥이 아닌 면(구조물 윗면, 천장 안쪽, 기둥 옆면)에 맞으면 그 지점 바로 뒤에서 이어서 다시 쏩니다.
	// 컴포넌트 통째로 무시하면 안 되는 이유: 아치 천장과 터널 바닥이 한 메시로 합쳐진 에셋(예: SM_HC_RuinsAndCorridors)에서
	// 천장 때문에 바닥까지 같이 무시되어 터널 안 바닥을 클릭할 수 없게 됨. (Params 두 번째 인자 true = 폴리곤 단위 트레이스)
	constexpr int32 MaxRetrace = 16;
	for (int32 Attempt = 0; Attempt < MaxRetrace; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Ground, Params))
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

		if (bIsPawn)
		{
			Params.AddIgnoredActor(Hit.GetActor());
			continue;
		}

		// ARLArenaManager 또는 폰을 막지 않는 컴포넌트(TriggerVolume 등)는 바닥이 아니므로 즉시 무시하고 관통
		if (Hit.GetActor() && Hit.GetActor()->IsA<ARLArenaManager>())
		{
			Params.AddIgnoredActor(Hit.GetActor());
			continue;
		}

		if (Hit.GetComponent() && Hit.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			Params.AddIgnoredComponent(Hit.GetComponent());
			continue;
		}

		// 시작점이 이미 메시 안이면(bStartPenetrating) 조금 더 크게 전진
		TraceStart = Hit.bStartPenetrating ? TraceStart + RayDirection * 10.0f : Hit.ImpactPoint + RayDirection * 1.0f;
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