#include "RLShopStation.h"
#include "RLShopWidget.h"
#include "RLCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

ARLShopStation::ARLShopStation()
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

	InteractPromptText = NSLOCTEXT("RLShop", "StationPrompt", "F키 눌러 상점 열기");
}

void ARLShopStation::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadius);
		InteractionSphere->SetGenerateOverlapEvents(true);
		InteractionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARLShopStation::OnOverlapBegin);
		InteractionSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ARLShopStation::OnOverlapEnd);

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
					PlayerChar->SetNearbyShop(this);
					break;
				}
			}
		}));
	}
}

void ARLShopStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 언로드 등으로 사라질 때 캐릭터에 남은 댕글링 참조 정리
	if (ARLCharacter* PlayerChar = CurrentOverlappedPlayer.Get())
	{
		if (PlayerChar->GetNearbyShop() == this)
		{
			PlayerChar->SetNearbyShop(nullptr);
		}
	}
	CurrentOverlappedPlayer = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ARLShopStation::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		CurrentOverlappedPlayer = PlayerChar;
		PlayerChar->SetNearbyShop(this);
	}
}

void ARLShopStation::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ARLCharacter* PlayerChar = Cast<ARLCharacter>(OtherActor))
	{
		if (CurrentOverlappedPlayer == PlayerChar)
		{
			PlayerChar->SetNearbyShop(nullptr);
			CurrentOverlappedPlayer = nullptr;
		}
	}
}

bool ARLShopStation::IsWidgetOpen() const
{
	return ActiveWidget.IsValid() && ActiveWidget->IsInViewport();
}

void ARLShopStation::Interact_Implementation(ARLCharacter* Interactor)
{
	if (!Interactor)
	{
		return;
	}

	// 이미 열려 있는 상태에서 F키를 다시 누르면 상점을 닫는다 (토글)
	if (IsWidgetOpen())
	{
		if (URLShopWidget* OpenWidget = ActiveWidget.Get())
		{
			OpenWidget->CloseShop();
		}
		ActiveWidget = nullptr;
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Interactor->GetController());
	TSubclassOf<URLShopWidget> TargetWidgetClass = ShopWidgetClass;
	if (!TargetWidgetClass)
	{
		TargetWidgetClass = LoadClass<URLShopWidget>(
			nullptr, TEXT("/Game/Blueprint/UI/WBP_ShopTest.WBP_ShopTest_C"));
	}

	if (!PC || !TargetWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShopStation] 위젯 클래스 또는 PlayerController가 없어 상점 UI를 열 수 없습니다."));
		return;
	}

	URLShopWidget* Widget = CreateWidget<URLShopWidget>(PC, TargetWidgetClass);
	if (!Widget)
	{
		return;
	}

	Widget->AddToViewport(50);
	Widget->OpenShop(Interactor);
	ActiveWidget = Widget;

	OnStationUsed(Interactor);
}
