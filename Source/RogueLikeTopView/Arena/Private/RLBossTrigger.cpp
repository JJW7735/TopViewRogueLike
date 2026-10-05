#include "RLBossTrigger.h"
#include "RLArenaMoodController.h"
#include "RLCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

ARLBossTrigger::ARLBossTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->InitBoxExtent(FVector(300.f, 300.f, 150.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	BossSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("BossSpawnPoint"));
	BossSpawnPoint->SetupAttachment(RootComponent);
	BossSpawnPoint->ArrowSize = 3.f;
	BossSpawnPoint->SetRelativeLocation(FVector(1000.f, 0.f, 0.f));
}

void ARLBossTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (!MoodController)
	{
		for (TActorIterator<ARLArenaMoodController> It(GetWorld()); It; ++It)
		{
			if (It->GetLevel() == GetLevel())
			{
				MoodController = *It;
				break;
			}
		}
	}

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ARLBossTrigger::OnTriggerBeginOverlap);
}

void ARLBossTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<ARLCharacter>(OtherActor))
	{
		TriggerBossEncounter();
	}
}

void ARLBossTrigger::TriggerBossEncounter()
{
	if (bTriggerOnce && bTriggered)
	{
		return;
	}
	bTriggered = true;

	if (bTriggerOnce)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (BossClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		// AutoPossessAI(PlacedInWorldOrSpawned)로 AI 컨트롤러가 자동 빙의됨
		SpawnedBoss = GetWorld()->SpawnActor<APawn>(BossClass, BossSpawnPoint->GetComponentLocation(), BossSpawnPoint->GetComponentRotation(), SpawnParams);
	}

	if (MoodController)
	{
		MoodController->SetBossPhase(true);
	}

	if (SpawnedBoss)
	{
		OnBossSpawned(SpawnedBoss);
	}
}
