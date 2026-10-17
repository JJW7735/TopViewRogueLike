#include "RLMetaUpgradeStation.h"
#include "RLMetaUpgradeWidget.h"
#include "RLCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

ARLMetaUpgradeStation::ARLMetaUpgradeStation()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(RootComponent);
	StationMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	InteractPromptText = NSLOCTEXT("RLMeta", "StationPrompt", "F를 눌러 영구 강화");
}

void ARLMetaUpgradeStation::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadius);
		InteractionSphere->SetGenerateOverlapEvents(true);
		InteractionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLMetaUpgradeStation::OnOverlapBegin);
		InteractionSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ARLMetaUpgradeStation::OnOverlapEnd);

		// 레벨 시작 시 이미 범위 안에 스폰된 플레이어는 BeginOverlap이 바인딩 전에 지나가므로 직접 등록.
		// 캐릭터 BeginPlay에서 HUD가 만들어진 뒤 프롬프트가 갱신되도록 다음 틱으로 미룸
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!InteractionSphere || CurrentOverlappedPlayer.IsValid())
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
					PlayerChar->SetNearbyMetaStation(this);
					break;
				}
			}
		}));
	}
}

void ARLMetaUpgradeStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 언로드 등으로 사라질 때 캐릭터에 남은 댕글링 참조 정리
	if (ARLCharacter* PlayerChar = CurrentOverlappedPlayer.Get())
	{
		if (PlayerChar->GetNearbyMetaStation() == this)
		{
			PlayerChar->SetNearbyMetaStation(nullptr);
		}
	}
	CurrentOverlappedPlayer = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ARLMetaUpgradeStation::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		CurrentOverlappedPlayer = PlayerChar;
		PlayerChar->SetNearbyMetaStation(this);
	}
}

void ARLMetaUpgradeStation::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		if (CurrentOverlappedPlayer == PlayerChar)
		{
			PlayerChar->SetNearbyMetaStation(nullptr);
			CurrentOverlappedPlayer = nullptr;
		}
	}
}

bool ARLMetaUpgradeStation::IsWidgetOpen() const
{
	return ActiveWidget.IsValid() && ActiveWidget->IsInViewport();
}

void ARLMetaUpgradeStation::Interact(ARLCharacter* Interactor)
{
	if (!Interactor || IsWidgetOpen())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Interactor->GetController());
	TSubclassOf<URLMetaUpgradeWidget> TargetWidgetClass = MetaWidgetClass;
	if (!TargetWidgetClass)
	{
		TargetWidgetClass = LoadClass<URLMetaUpgradeWidget>(
			nullptr, TEXT("/Game/Blueprint/UI/WBP_MetaUpgrade.WBP_MetaUpgrade_C"));
	}

	if (!PC || !TargetWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MetaUpgradeStation] 위젯 클래스 또는 PlayerController가 없어 강화 UI를 열 수 없습니다."));
		return;
	}

	URLMetaUpgradeWidget* Widget = CreateWidget<URLMetaUpgradeWidget>(PC, TargetWidgetClass);
	if (!Widget)
	{
		return;
	}

	Widget->AddToViewport(50);
	Widget->OpenMetaUpgrade(Interactor);
	ActiveWidget = Widget;

	OnStationUsed(Interactor);
}
