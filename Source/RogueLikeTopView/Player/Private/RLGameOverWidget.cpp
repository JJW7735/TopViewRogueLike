#include "RLGameOverWidget.h"
#include "RLCharacter.h"
#include "RLInventoryComponent.h"
#include "RLSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void URLGameOverWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 게임오버 위젯이 화면에 생성되면 자동으로 마우스 커서 활성화 및 입력 모드 전환
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	// 수치가 아직 초기화되지 않았다면 자동 초기화 수행
	if (SoulsEarnedThisRun == 0 && TotalSouls == 0)
	{
		ARLCharacter* Player = Cast<ARLCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		SetupGameOver(Player, -1);
	}
}

void URLGameOverWidget::SetupGameOver(ARLCharacter* DeadPlayer, int32 InSoulsEarned)
{
	URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this);
	if (SaveSys)
	{
		TotalSouls = SaveSys->GetTotalSouls();
		SoulsEarnedThisRun = (InSoulsEarned >= 0) ? InSoulsEarned : SaveSys->GetLastSettledRunSouls();
	}
	else
	{
		SoulsEarnedThisRun = FMath::Max(0, InSoulsEarned);
	}

	if (DeadPlayer)
	{
		CharacterName = DeadPlayer->CharacterDisplayName;
		if (URLInventoryComponent* Inv = DeadPlayer->GetInventoryComponent())
		{
			GoldLost = Inv->GetGold();
		}
	}

	OnGameOverDataReady(SoulsEarnedThisRun, TotalSouls, GoldLost, StageReached);
}

void URLGameOverWidget::RestorePlayerInput()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}

	UGameplayStatics::SetGamePaused(this, false);
}

void URLGameOverWidget::RetryRun(FName RestartLevelName)
{
	// 런 세이브 완전 초기화
	if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
	{
		SaveSys->ClearActiveRun(true);
	}

	RestorePlayerInput();
	RemoveFromParent();

	FName TargetLevel = RestartLevelName;
	if (TargetLevel == NAME_None && GetWorld())
	{
		TargetLevel = FName(*UWorld::RemovePIEPrefix(GetWorld()->GetMapName()));
	}

	if (TargetLevel != NAME_None)
	{
		UGameplayStatics::OpenLevel(this, TargetLevel);
	}
}

void URLGameOverWidget::QuitToMainMenu(FName MainMenuLevelName)
{
	if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
	{
		SaveSys->ClearActiveRun(true);
	}

	RestorePlayerInput();
	RemoveFromParent();

	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}
