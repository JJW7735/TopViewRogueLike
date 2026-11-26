#include "RLRewardSelectWidget.h"
#include "RLRewardSubsystem.h"
#include "RLRewardChest.h"
#include "RLCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"

void URLRewardSelectWidget::InitializeWidget(ARLCharacter* InPlayer, const TArray<FRLRewardData>& InRewards, ARLRewardChest* InSourceChest, ERLRewardTriggerSource InTriggerSource)
{
	TargetPlayer = InPlayer;
	RewardOptions = InRewards;
	SourceChest = InSourceChest;
	TriggerSource = InTriggerSource;

	// 상자: 신중하게 고를 수 있도록 게임 일시정지. 레벨업: 전투 흐름이 끊기지 않도록 멈추지 않음.
	bPausedGameOnOpen = (TriggerSource == ERLRewardTriggerSource::Chest);
	if (bPausedGameOnOpen)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}

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

	// 아이콘 로드: WBP는 Icon을 "Resolve Soft Reference"(로드 안 함)로 꺼내 쓰므로,
	// 메모리에 없는 텍스처는 null이 되어 아이콘이 비어 보인다. 상점(RLShopWidget)과 동일하게 여기서 동기 로드해 둔다.
	LoadedIcons.Reset();
	for (const FRLRewardData& Reward : RewardOptions)
	{
		if (UTexture2D* IconTex = Reward.Icon.LoadSynchronous())
		{
			LoadedIcons.Add(IconTex);
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

	// ★ 위젯을 먼저 닫아 일시정지/입력 모드를 원복한 뒤 상자를 정리한다.
	// 상자의 OnRewardClaimed를 받아 새 모달(예: 보스 상자 → 승리 화면)이 게임을 다시 일시정지할 수 있도록,
	// CloseWidget의 일시정지 해제가 그 뒤에 덮어쓰지 않게 하기 위함.
	ARLRewardChest* ChestToFinish = SourceChest.Get();
	const bool bWasLevelUp = (TriggerSource == ERLRewardTriggerSource::LevelUp);
	ARLCharacter* PlayerToNotify = TargetPlayer.Get();

	CloseWidget();

	// 보상 제공 상자 소멸 처리 (이펙트 끄고 딜레이 후 파괴)
	if (ChestToFinish)
	{
		ChestToFinish->FinishAndDestroyChest();
	}

	// 레벨업 보상 완료 알림 (미선택 카운트 차감 및 대기 중인 추가 레벨업 카드 표시)
	if (bWasLevelUp && PlayerToNotify)
	{
		PlayerToNotify->OnLevelUpRewardClaimed();
	}
}

void URLRewardSelectWidget::CloseWidget()
{
	// 이 위젯이 직접 일시정지시켰을 때만 해제 (레벨업 경로는 애초에 멈추지 않았음)
	if (bPausedGameOnOpen)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedGameOnOpen = false;
	}

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
