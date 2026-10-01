#include "RLRewardSelectWidget.h"
#include "RLRewardSubsystem.h"
#include "RLRewardChest.h"
#include "RLCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void URLRewardSelectWidget::InitializeWidget(ARLCharacter* InPlayer, const TArray<FRLRewardData>& InRewards, ARLRewardChest* InSourceChest)
{
	TargetPlayer = InPlayer;
	RewardOptions = InRewards;
	SourceChest = InSourceChest;

	// 게임 일시정지 (안전하게 보상 선택할 수 있도록)
	UGameplayStatics::SetGamePaused(this, true);

	// 마우스 커서 활성화 및 UI 입력 모드 설정
	if (TargetPlayer.IsValid())
	{
		if (APlayerController* PC = Cast<APlayerController>(TargetPlayer->GetController()))
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(TakeWidget());
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
	}

	// 블루프린트 연출/UI 갱신 이벤트 호출
	OnRewardsReady(RewardOptions);
}

void URLRewardSelectWidget::SelectReward(int32 OptionIndex)
{
	if (RewardOptions.IsValidIndex(OptionIndex) && TargetPlayer.IsValid())
	{
		if (URLRewardSubsystem* Subsystem = URLRewardSubsystem::GetRewardSubsystem(this))
		{
			Subsystem->ApplyReward(TargetPlayer.Get(), RewardOptions[OptionIndex]);
		}

		OnRewardChosen(OptionIndex);
	}

	// 보상 제공 상자 소멸 처리 (이펙트 끄고 딜레이 후 파괴)
	if (SourceChest.IsValid())
	{
		SourceChest->FinishAndDestroyChest();
	}

	CloseWidget();
}

void URLRewardSelectWidget::CloseWidget()
{
	// 게임 일시정지 해제
	UGameplayStatics::SetGamePaused(this, false);

	// 플레이어 기본 입력 모드로 원복
	if (TargetPlayer.IsValid())
	{
		if (APlayerController* PC = Cast<APlayerController>(TargetPlayer->GetController()))
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
	}

	RemoveFromParent();
}
