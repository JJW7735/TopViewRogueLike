#include "RLDamageNumberSubsystem.h"
#include "RLEnemyCharacter.h"
#include "RLHealthComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

void URLDamageNumberListener::HandleHealthChanged(float CurrentHealth, float MaxHealth, float HealthDelta)
{
	// HealthDelta: 피해 = 양수, 회복 = 음수, 최대 체력 변경 = 0 (URLHealthComponent 규칙)
	if (HealthDelta <= 0.0f || !Victim.IsValid() || !OwnerSubsystem.IsValid())
	{
		return;
	}

	// 캡슐 꼭대기(머리 위)에서 띄웁니다
	FVector Location = Victim->GetActorLocation();
	if (const ACharacter* VictimCharacter = Cast<ACharacter>(Victim.Get()))
	{
		Location.Z += VictimCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	Location.Z += OwnerSubsystem->HeadOffset;

	OwnerSubsystem->SpawnDamageNumber(HealthDelta, Location);
}

bool URLDamageNumberSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URLDamageNumberSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	DamageNumberWidgetClass = LoadClass<UUserWidget>(
		nullptr, TEXT("/Game/Blueprint/UI/WBP_DamageNumber.WBP_DamageNumber_C"));
	if (!DamageNumberWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RLDamageNumberSubsystem] WBP_DamageNumber를 찾을 수 없어 데미지 숫자를 표시하지 않습니다."));
		return;
	}

	// 레벨에 미리 배치된 적 + 이후 스폰되는 적 모두 구독
	for (TActorIterator<ARLEnemyCharacter> It(&InWorld); It; ++It)
	{
		TryRegisterActor(*It);
	}
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &URLDamageNumberSubsystem::HandleActorSpawned));
}

void URLDamageNumberSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}

	for (FRLActiveDamageNumber& Entry : ActiveNumbers)
	{
		if (IsValid(Entry.Widget))
		{
			Entry.Widget->RemoveFromParent();
		}
	}
	ActiveNumbers.Empty();
	Listeners.Empty();

	Super::Deinitialize();
}

void URLDamageNumberSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	TryRegisterActor(SpawnedActor);
}

void URLDamageNumberSubsystem::TryRegisterActor(AActor* Actor)
{
	// 적만 표시 (플레이어 피격은 제외)
	if (!Cast<ARLEnemyCharacter>(Actor))
	{
		return;
	}

	URLHealthComponent* HealthComp = Actor->FindComponentByClass<URLHealthComponent>();
	if (!HealthComp)
	{
		return;
	}

	for (const URLDamageNumberListener* Existing : Listeners)
	{
		if (Existing && Existing->Victim.Get() == Actor)
		{
			return;
		}
	}

	URLDamageNumberListener* Listener = NewObject<URLDamageNumberListener>(this);
	Listener->Victim = Actor;
	Listener->OwnerSubsystem = this;
	HealthComp->OnHealthChanged.AddDynamic(Listener, &URLDamageNumberListener::HandleHealthChanged);
	Listeners.Add(Listener);
}

void URLDamageNumberSubsystem::SpawnDamageNumber(float Damage, FVector WorldLocation)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !DamageNumberWidgetClass)
	{
		return;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, DamageNumberWidgetClass);
	if (!Widget)
	{
		return;
	}

	if (UTextBlock* DamageText = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Txt_Damage"))))
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));
	}

	// 보상 선택창(ZOrder 50)보다 아래
	Widget->AddToViewport(5);
	Widget->SetDesiredSizeInViewport(FVector2D(200.0f, 60.0f));
	Widget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);

	FRLActiveDamageNumber Entry;
	Entry.Widget = Widget;
	Entry.WorldLocation = WorldLocation;
	Entry.JitterX = FMath::FRandRange(-HorizontalJitter, HorizontalJitter);

	// 생성된 프레임에 바로 위치를 잡아 (0,0)에서 한 프레임 깜빡이는 것 방지
	UpdateDamageNumber(Entry, PC);
	ActiveNumbers.Add(Entry);
}

void URLDamageNumberSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Listeners.RemoveAll([](const URLDamageNumberListener* Listener)
	{
		return !Listener || !Listener->Victim.IsValid();
	});

	if (ActiveNumbers.Num() == 0)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	for (int32 i = ActiveNumbers.Num() - 1; i >= 0; --i)
	{
		FRLActiveDamageNumber& Entry = ActiveNumbers[i];
		Entry.Elapsed += DeltaTime;

		if (!PC || !IsValid(Entry.Widget) || !UpdateDamageNumber(Entry, PC))
		{
			if (IsValid(Entry.Widget))
			{
				Entry.Widget->RemoveFromParent();
			}
			ActiveNumbers.RemoveAtSwap(i);
		}
	}
}

bool URLDamageNumberSubsystem::UpdateDamageNumber(FRLActiveDamageNumber& Entry, APlayerController* PC) const
{
	const float T = FMath::Clamp(Entry.Elapsed / FMath::Max(Lifetime, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	if (Entry.Elapsed >= Lifetime)
	{
		return false;
	}

	FVector2D ScreenPos;
	if (!PC->ProjectWorldLocationToScreen(Entry.WorldLocation, ScreenPos))
	{
		// 화면 밖이면 이번 프레임만 숨김
		Entry.Widget->SetRenderOpacity(0.0f);
		return true;
	}

	// 위로 떠오르기 (처음엔 빠르게, 끝으로 갈수록 느리게)
	const float Rise = RiseDistance * FMath::InterpEaseOut(0.0f, 1.0f, T, 2.0f);
	ScreenPos += FVector2D(Entry.JitterX, -Rise);
	Entry.Widget->SetPositionInViewport(ScreenPos, true);

	// 등장 직후 살짝 커졌다가 원래 크기로
	const float PopT = FMath::Clamp(T / 0.15f, 0.0f, 1.0f);
	const float Scale = FMath::Lerp(PopScale, 1.0f, PopT);
	Entry.Widget->SetRenderScale(FVector2D(Scale, Scale));

	// 후반부 페이드아웃
	const float Opacity = (T < FadeStartRatio) ? 1.0f : 1.0f - (T - FadeStartRatio) / (1.0f - FadeStartRatio);
	Entry.Widget->SetRenderOpacity(Opacity);

	return true;
}

TStatId URLDamageNumberSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URLDamageNumberSubsystem, STATGROUP_Tickables);
}
