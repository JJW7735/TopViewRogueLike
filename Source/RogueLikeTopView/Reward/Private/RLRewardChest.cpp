#include "RLRewardChest.h"
#include "RLRewardSubsystem.h"
#include "RLRewardSelectWidget.h"
#include "RLCharacter.h"
#include "RLTypes.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "NiagaraComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ARLRewardChest::ARLRewardChest()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	ChestBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestBaseMesh"));
	ChestBaseMesh->SetupAttachment(RootComponent);
	ChestBaseMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ChestBaseMesh->SetCollisionResponseToChannel(ECC_Ground, ECR_Ignore);

	ChestLidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestLidMesh"));
	ChestLidMesh->SetupAttachment(ChestBaseMesh);
	ChestLidMesh->SetCollisionProfileName(TEXT("NoCollision"));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractionSphere->SetCollisionResponseToChannel(ECC_Ground, ECR_Ignore);
	InteractionSphere->SetGenerateOverlapEvents(true);

	ChestFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ChestFX"));
	ChestFX->SetupAttachment(RootComponent);
	ChestFX->bAutoActivate = true;

	// WBP_RewardSelect 블루프린트 위젯을 기본값으로 자동 연동
	static ConstructorHelpers::FClassFinder<URLRewardSelectWidget> DefaultWidgetClassFinder(
		TEXT("/Game/Blueprint/UI/WBP_RewardSelect.WBP_RewardSelect_C"));
	if (DefaultWidgetClassFinder.Succeeded())
	{
		RewardWidgetClass = DefaultWidgetClassFinder.Class;
	}
}

void ARLRewardChest::BeginPlay()
{
	Super::BeginPlay();

	// 상자 메시(BP에서 추가/오버라이드한 컴포넌트 포함)가 우클릭 바닥 트레이스(ECC_Ground)를 가로막지 않도록 보장
	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* PrimComp : PrimComps)
	{
		if (PrimComp)
		{
			PrimComp->SetCollisionResponseToChannel(ECC_Ground, ECR_Ignore);
		}
	}

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadius);
		InteractionSphere->SetGenerateOverlapEvents(true);
		InteractionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLRewardChest::OnOverlapBegin);
		InteractionSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ARLRewardChest::OnOverlapEnd);

		// 스폰 시점에 이미 상호작용 범위 안에 서 있는 플레이어는 BeginOverlap이 바인딩 전에 지나가므로 직접 등록
		InteractionSphere->UpdateOverlaps();
		TryRegisterOverlappedPlayer();

		// 레벨 시작 시 배치된 상자 등 HUD/오버랩 초기화 순서 차이를 대비해 다음 틱에 한 번 더 확인
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			TryRegisterOverlappedPlayer();
		}));
	}
}

void ARLRewardChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARLCharacter* PlayerChar = CurrentOverlappedPlayer.Get())
	{
		if (PlayerChar->GetNearbyChest() == this)
		{
			PlayerChar->SetNearbyChest(nullptr);
		}
	}
	CurrentOverlappedPlayer = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ARLRewardChest::TryRegisterOverlappedPlayer()
{
	if (bIsOpen || !InteractionSphere || CurrentOverlappedPlayer.IsValid())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	InteractionSphere->GetOverlappingActors(OverlappingActors, ARLCharacter::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(Actor))
		{
			CurrentOverlappedPlayer = PlayerChar;
			PlayerChar->SetNearbyChest(this);
			break;
		}
	}
}

void ARLRewardChest::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsOpen)
	{
		return;
	}

	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		CurrentOverlappedPlayer = PlayerChar;
		PlayerChar->SetNearbyChest(this);
	}
}

void ARLRewardChest::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		if (CurrentOverlappedPlayer == PlayerChar)
		{
			PlayerChar->SetNearbyChest(nullptr);
			CurrentOverlappedPlayer = nullptr;
		}
	}
}

void ARLRewardChest::OpenChest(ARLCharacter* Interactor)
{
	if (bIsOpen || !Interactor)
	{
		return;
	}

	bIsOpen = true;

	// 상호작용 등록 해제
	Interactor->SetNearbyChest(nullptr);
	CurrentOverlappedPlayer = nullptr;

	// 뚜껑 회전 연출
	if (ChestLidMesh)
	{
		ChestLidMesh->SetRelativeRotation(FRotator(-65.0f, 0.0f, 0.0f));
	}

	// 블루프린트 이벤트 호출 (효과음, 이펙트 전환 등)
	OnChestOpened(Interactor);

	// 보상 서브시스템에서 3지선다 카드 추첨
	URLRewardSubsystem* Subsystem = URLRewardSubsystem::GetRewardSubsystem(this);
	if (!Subsystem)
	{
		return;
	}

	TArray<FRLRewardData> Rewards = Subsystem->RollRewards(CardCount);

	// 1. UI 위젯이 있으면 화면에 3지선다 팝업
	APlayerController* PC = Cast<APlayerController>(Interactor->GetController());
	TSubclassOf<URLRewardSelectWidget> TargetWidgetClass = RewardWidgetClass;
	if (!TargetWidgetClass)
	{
		TargetWidgetClass = LoadClass<URLRewardSelectWidget>(
			nullptr, TEXT("/Game/Blueprint/UI/WBP_RewardSelect.WBP_RewardSelect_C"));
	}

	if (PC && TargetWidgetClass)
	{
		URLRewardSelectWidget* Widget = CreateWidget<URLRewardSelectWidget>(PC, TargetWidgetClass);
		if (Widget)
		{
			Widget->AddToViewport(50);
			Widget->InitializeWidget(Interactor, Rewards, this, ERLRewardTriggerSource::Chest);
			return;
		}
	}

	// 2. UI 위젯 에셋이 아직 없을 때의 즉시 동작 Fallback:
	// 3장 추첨 결과 목록을 화면에 띄우고, 첫 번째 보상을 캐릭터에 즉시 지급하여 스탯을 바로 올려줍니다!
	if (GEngine && Rewards.Num() > 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
			FString::Printf(TEXT("📦 [상자 개봉!] 추첨된 카드 3장: ①%s  ②%s  ③%s"),
				*Rewards[0].RewardName.ToString(),
				Rewards.Num() > 1 ? *Rewards[1].RewardName.ToString() : TEXT(""),
				Rewards.Num() > 2 ? *Rewards[2].RewardName.ToString() : TEXT("")));

		// 1번 카드를 플레이어에게 즉시 지급
		Subsystem->ApplyReward(Interactor, Rewards[0]);
		FinishAndDestroyChest();
	}
}

void ARLRewardChest::FinishAndDestroyChest(float DelaySeconds)
{
	// 상호작용 및 물리 충돌 즉시 비활성화 (플레이어 이동 방해 제거)
	SetActorEnableCollision(false);
	if (ChestBaseMesh)
	{
		ChestBaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 빛나는 나이아가라 이펙트 정지
	if (ChestFX)
	{
		ChestFX->Deactivate();
	}

	// 블루프린트 연출 훅 호출 (소멸 VFX/SFX 등)
	OnChestClaimed();
	OnRewardClaimed.Broadcast(this);

	const float ActualDelay = (DelaySeconds >= 0.0f) ? DelaySeconds : DestroyDelay;
	if (ActualDelay <= 0.0f)
	{
		Destroy();
	}
	else
	{
		SetLifeSpan(ActualDelay);
	}
}

void ARLRewardChest::ResetChest()
{
	bIsOpen = false;
	if (ChestLidMesh)
	{
		ChestLidMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
}
