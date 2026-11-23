#include "RLPlayerController.h"
#include "RLCharacter.h"
#include "RLEnemyCharacter.h"
#include "RLHealthComponent.h"
#include "RLArenaManager.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "RLSkillComponent.h"
#include "RLInventoryComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "RLRewardChest.h"
#include "RLPortal.h"
#include "RLSaveSubsystem.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

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
		if (QuickSlot1_Action)
		{
			EnhancedInputComponent->BindAction(QuickSlot1_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnQuickSlot1_Started);
		}
		if (QuickSlot2_Action)
		{
			EnhancedInputComponent->BindAction(QuickSlot2_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnQuickSlot2_Started);
		}
		if (QuickSlot3_Action)
		{
			EnhancedInputComponent->BindAction(QuickSlot3_Action, ETriggerEvent::Started, this, &ARLPlayerController::OnQuickSlot3_Started);
		}
	}

#if !UE_BUILD_SHIPPING
	// ★ 테스트용 치트 키 (패키징 Shipping 빌드에서는 제외)
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ARLPlayerController::KillAllEnemies);
	InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ARLPlayerController::KillSelf);
#endif
}

void ARLPlayerController::KillSelf()
{
	APawn* ControlledPawn = GetPawn();
	URLHealthComponent* HealthComp = ControlledPawn ? ControlledPawn->FindComponentByClass<URLHealthComponent>() : nullptr;
	if (!HealthComp || HealthComp->bIsDead)
	{
		return;
	}

	// 실제 피격과 같은 경로(GAS 피해 → HealthComp → OnDeath → HandleDeath)로 사망시킵니다.
	// 방어력에 깎이지 않도록 트루 데미지로 남은 체력만큼 넣습니다.
	HealthComp->ApplyIncomingDamage(HealthComp->CurrentHealth, nullptr, true);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Cheat] 플레이어 즉사"));
	}
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

namespace
{
	bool IsStage1BossMap(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}
		const FString CleanMap = UWorld::RemovePIEPrefix(World->GetMapName());
		return CleanMap.Equals(TEXT("1-B_Soul"), ESearchCase::IgnoreCase) ||
		       CleanMap.Equals(TEXT("L_Soul1_B"), ESearchCase::IgnoreCase) ||
		       CleanMap.Equals(TEXT("L_Soul"), ESearchCase::IgnoreCase);
	}

	void SaveBossVictoryRunProgress(ARLPlayerController* PC)
	{
		if (!PC) return;

		if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(PC))
		{
			if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(PC->GetPawn()))
			{
				const FString RawMapName = PC->GetWorld() ? PC->GetWorld()->GetMapName() : TEXT("1-B_Soul");
				const FString CleanMapName = UWorld::RemovePIEPrefix(RawMapName);
				const int32 StageNum = FMath::Max(1, SaveSys->GetActiveRunData().StageNumber);

				// ★ 보스 승리 시 이번 런에서 모은 임시 영혼(RunSouls)을 영구 영혼(TotalSouls)으로 즉시 정산하고
				//   RunSouls를 0으로 비운 뒤 저장합니다 (승리 후 영혼 누락 및 중복 입금 방지)
				const int32 EarnedSouls = SaveSys->GetRunSouls();
				if (EarnedSouls > 0)
				{
					SaveSys->AddTotalSouls(EarnedSouls);
					SaveSys->AddRunSouls(-EarnedSouls);
				}

				SaveSys->SaveActiveRun(PlayerChar, FName(*CleanMapName), StageNum, PlayerChar->GetActorLocation());
				SaveSys->RecordStageClear(StageNum);
			}
		}
	}
}

bool ARLPlayerController::ScheduleBossStageTransitionPortal(float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!IsStage1BossMap(World))
	{
		return false;
	}

	if (SpawnedBossStagePortal.IsValid())
	{
		return true;
	}

	if (DelaySeconds <= 0.0f)
	{
		SpawnBossStageTransitionPortal();
	}
	else if (!World->GetTimerManager().IsTimerActive(BossPortalSpawnTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			BossPortalSpawnTimerHandle,
			this,
			&ARLPlayerController::SpawnBossStageTransitionPortal,
			DelaySeconds,
			false);
	}

	return true;
}

void ARLPlayerController::SpawnBossStageTransitionPortal()
{
	UWorld* World = GetWorld();
	if (!World || SpawnedBossStagePortal.IsValid())
	{
		return;
	}

	TSubclassOf<ARLPortal> PortalClass = LoadClass<ARLPortal>(nullptr, TEXT("/Game/Blueprint/BP_Portal.BP_Portal_C"));
	if (!PortalClass)
	{
		PortalClass = ARLPortal::StaticClass();
	}

	// 1-B_Soul 맵 중앙 (0, 0) 위치에 포탈 생성
	const FVector PortalLoc(0.0f, 0.0f, 150.0f);
	const FRotator PortalRot = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARLPortal* NewPortal = World->SpawnActor<ARLPortal>(PortalClass, PortalLoc, PortalRot, SpawnParams);
	if (!NewPortal)
	{
		return;
	}

	SpawnedBossStagePortal = NewPortal;
	NewPortal->SetActorScale3D(FVector(1.0f, 1.8f, 3.2f));
	NewPortal->SetTravelMapByPath(TEXT("/Game/Maps/2-1_Graveyard"));

	if (WatchedBossChest.IsValid())
	{
		NewPortal->SetLinkedChestToClean(WatchedBossChest.Get());
	}

	// 포탈 외형 메시(SM_SoulPortal) 부착 (1-1, 1-2와 동일하게 Yaw 90도 회전하여 정면을 바라보도록 설정)
	if (UStaticMesh* PortalMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SoulPortal_Standalone/Meshes/SM_SoulPortal.SM_SoulPortal")))
	{
		if (UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(NewPortal, TEXT("BossStagePortalMesh")))
		{
			MeshComp->SetStaticMesh(PortalMesh);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComp->SetupAttachment(NewPortal->GetRootComponent());
			MeshComp->RegisterComponent();
			MeshComp->SetWorldLocationAndRotation(FVector(0.0f, 0.0f, 150.0f), FRotator(0.0f, 90.0f, 0.0f));
			MeshComp->SetWorldScale3D(FVector(0.2f, 0.2f, 0.2f));
		}
	}

	// 포탈 주변 영혼 빛(PointLight) 및 파티클(NS_Ambient_SoulWisps) 연출
	if (UPointLightComponent* LightComp = NewObject<UPointLightComponent>(NewPortal, TEXT("BossStagePortalLight")))
	{
		LightComp->SetupAttachment(NewPortal->GetRootComponent());
		LightComp->SetIntensity(4000.0f);
		LightComp->SetAttenuationRadius(600.0f);
		LightComp->SetLightColor(FLinearColor(0.55f, 0.25f, 1.0f));
		LightComp->RegisterComponent();
		LightComp->SetWorldLocation(FVector(0.0f, 0.0f, 220.0f));
	}

	if (UNiagaraSystem* PortalFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Effect/Ambient/NS_Ambient_SoulWisps.NS_Ambient_SoulWisps")))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, PortalFX, FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator, FVector(0.6f, 0.6f, 1.0f));
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("🌀 [포탈 생성] 중앙에 다음 스테이지(2-1)로 이동하는 포탈이 열렸습니다!"));
	}
}

void ARLPlayerController::HandleBossChestClaimed(ARLRewardChest* Chest)
{
	if (!Chest || Chest != WatchedBossChest.Get())
	{
		return;
	}

	Chest->OnRewardClaimed.RemoveDynamic(this, &ARLPlayerController::HandleBossChestClaimed);
	WatchedBossChest = nullptr;

	// ★ 보스 보상 획득 시점에 진행 상황 즉시 저장
	SaveBossVictoryRunProgress(this);

	// 1-B 보스 맵에서는 게임이 끝나지 않고 중앙 포탈을 통해 2-1로 진행하므로 승리(게임 종료) 모달을 띄우지 않음
	if (IsStage1BossMap(GetWorld()))
	{
		ScheduleBossStageTransitionPortal(0.0f);
		return;
	}

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
	// ★ 보스 처치 및 보상 획득 상태를 활성 런에 저장하여, 메인 메뉴에서 이어하기 시
	//   보스가 이미 쓰러진 상태로 시작하도록 합니다.
	SaveBossVictoryRunProgress(this);

	if (VictoryWidget)
	{
		VictoryWidget->RemoveFromParent();
		VictoryWidget = nullptr;
	}

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
	if (!ControlledPawn)
	{
		return;
	}

	// ★ 레벨에 배치된 포탈의 오버랩 설정이 누락되어도 반경 360cm 안에 들어가면 자동으로 상호작용 프롬프트 표시
	if (ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn()))
	{
		ARLPortal* ClosestPortal = nullptr;
		float ClosestDistSq = FMath::Square(360.0f);
		for (TActorIterator<ARLPortal> It(GetWorld()); It; ++It)
		{
			ARLPortal* Portal = *It;
			if (IsValid(Portal) && Portal->CanActivatePortal())
			{
				const float DistSq = FVector::DistSquared2D(RLChar->GetActorLocation(), Portal->GetActorLocation());
				if (DistSq <= ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					ClosestPortal = Portal;
				}
			}
		}

		if (ClosestPortal)
		{
			RLChar->SetNearbyPortal(ClosestPortal);
		}
		else
		{
			RLChar->SetNearbyPortal(nullptr);
		}
	}

	if (!OcclusionMPC || !PlayerCameraManager)
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
	if (bUseVirtualCursor)
	{
		// 자동 테스트 봇: 가상 커서 바로 위에서 아래로 쏜다. 이후 바닥 판정 규칙은 마우스와 동일.
		RayOrigin = VirtualCursorLocation + FVector(0.0f, 0.0f, 3000.0f);
		RayDirection = FVector::DownVector;
	}
	else if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection))
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
		const bool bIsRewardChest = Hit.GetActor() && Hit.GetActor()->IsA<ARLRewardChest>();
		const bool bFacesUp = Hit.ImpactNormal.Z >= GroundMinNormalZ;
		const bool bNearFootHeight = Hit.ImpactPoint.Z <= PawnFootZ + GroundMaxHeightAbovePawn;

		if (!bIsPawn && !bIsRewardChest && bFacesUp && bNearFootHeight)
		{
			OutHit = Hit;
			return true;
		}

		if (bIsPawn || bIsRewardChest)
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

// ==========================================
// 자동 테스트 / 봇 전용 (DevTools)
// ==========================================
void ARLPlayerController::SetVirtualCursor(FVector WorldLocation)
{
	bUseVirtualCursor = true;
	VirtualCursorLocation = WorldLocation;
}

void ARLPlayerController::ClearVirtualCursor()
{
	bUseVirtualCursor = false;
}

void ARLPlayerController::SimulateInput(ERLSimInput Input, ERLSimInputPhase Phase)
{
	const bool bStarted = Phase == ERLSimInputPhase::Started;
	const bool bTriggered = Phase == ERLSimInputPhase::Triggered;

	switch (Input)
	{
	case ERLSimInput::Move:
		if (bStarted) { OnMoveStarted(); }
		else if (bTriggered) { OnMoveTriggered(); }
		break;
	case ERLSimInput::BasicAttack:
		if (bStarted) { OnBasicAttack_Started(); }
		else if (bTriggered) { OnBasicAttack_Triggered(); }
		else { OnBasicAttack_Completed(); }
		break;
	case ERLSimInput::SkillQ:
		if (bStarted) { OnSkillQ_Started(); }
		else if (bTriggered) { OnSkillQ_Triggered(); }
		else { OnSkillQ_Completed(); }
		break;
	case ERLSimInput::SkillW:
		if (bStarted) { OnSkillW_Started(); }
		else if (bTriggered) { OnSkillW_Triggered(); }
		else { OnSkillW_Completed(); }
		break;
	case ERLSimInput::SkillE:
		if (bStarted) { OnSkillE_Started(); }
		else if (bTriggered) { OnSkillE_Triggered(); }
		else { OnSkillE_Completed(); }
		break;
	case ERLSimInput::SkillR:
		if (bStarted) { OnSkillR_Started(); }
		else if (bTriggered) { OnSkillR_Triggered(); }
		else { OnSkillR_Completed(); }
		break;
	case ERLSimInput::Dodge:
		if (bStarted) { OnDodge_Started(); }
		break;
	case ERLSimInput::Interact:
		if (bStarted) { OnInteract_Started(); }
		break;
	case ERLSimInput::QuickSlot1:
		if (bStarted) { UseQuickSlot(0); }
		break;
	case ERLSimInput::QuickSlot2:
		if (bStarted) { UseQuickSlot(1); }
		break;
	case ERLSimInput::QuickSlot3:
		if (bStarted) { UseQuickSlot(2); }
		break;
	}
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
// 퀵슬롯 (1/2/3 키 → 소비 아이템 사용)
// ==========================================
void ARLPlayerController::UseQuickSlot(int32 QuickIndex)
{
	ARLCharacter* RLChar = Cast<ARLCharacter>(GetPawn());
	if (!RLChar)
	{
		return;
	}

	// 사망 후에는 물약 사용 불가
	const URLHealthComponent* HealthComp = RLChar->FindComponentByClass<URLHealthComponent>();
	if (HealthComp && HealthComp->bIsDead)
	{
		return;
	}

	if (URLInventoryComponent* InvComp = RLChar->GetInventoryComponent())
	{
		InvComp->UseQuickSlot(QuickIndex);
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
		// 경직 중엔 클릭 이동 불가 (SimpleMoveToLocation은 AddMovementInput을 거치지 않아서 여기서 막음)
		if (ControlledChar->IsStunned()) return;

		if (URLSkillComponent* SkillComp = ControlledChar->GetSkillComponent())
		{
			if (SkillComp->IsAiming()) return;
		}
	}

	FHitResult Hit;
	if (GetGroundHitUnderCursor(Hit))
	{
		FVector MoveTarget = Hit.Location;
		// 상자 등 동적 장애물 밑바닥처럼 NavMesh가 파인 지점을 클릭해도 가장 가까운 이동 가능 지점으로 보정
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation ProjectedLoc;
			if (NavSys->ProjectPointToNavigation(MoveTarget, ProjectedLoc, FVector(300.0f, 300.0f, 300.0f)))
			{
				MoveTarget = ProjectedLoc.Location;
			}
		}
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, MoveTarget);
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