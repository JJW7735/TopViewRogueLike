#include "RLMetaUpgradeWidget.h"
#include "RLMetaUpgradeRowWidget.h"
#include "RLSaveSubsystem.h"
#include "RLCharacter.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void URLMetaUpgradeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Reset)
	{
		Btn_Reset->OnClicked.AddUniqueDynamic(this, &URLMetaUpgradeWidget::HandleResetClicked);
	}
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddUniqueDynamic(this, &URLMetaUpgradeWidget::HandleExitClicked);
	}
	if (Txt_FailReason)
	{
		Txt_FailReason->SetVisibility(ESlateVisibility::Hidden);
	}
	if (Panel_RunLockNotice)
	{
		Panel_RunLockNotice->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
	{
		SaveSys->OnSoulsChanged.AddUniqueDynamic(this, &URLMetaUpgradeWidget::HandleSoulsChanged);
		UpdateSoulsText(SaveSys->GetTotalSouls());
	}

	// 런 진행 중이면 초기화 버튼도 흐리게 (클릭 시 안내)
	if (Btn_Reset && IsUpgradeLocked())
	{
		Btn_Reset->SetRenderOpacity(0.4f);
	}

	RebuildRows();
}

void URLMetaUpgradeWidget::NativeDestruct()
{
	if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
	{
		SaveSys->OnSoulsChanged.RemoveDynamic(this, &URLMetaUpgradeWidget::HandleSoulsChanged);
	}

	Super::NativeDestruct();
}

void URLMetaUpgradeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 게임이 일시정지되면 월드 타이머가 멈추므로 위젯 Tick으로 실패 문구를 숨김
	if (FailReasonRemaining > 0.0f)
	{
		FailReasonRemaining -= InDeltaTime;
		if (FailReasonRemaining <= 0.0f)
		{
			if (Txt_FailReason)
			{
				Txt_FailReason->SetVisibility(ESlateVisibility::Hidden);
			}
			if (Panel_RunLockNotice)
			{
				Panel_RunLockNotice->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void URLMetaUpgradeWidget::OpenMetaUpgrade(ARLCharacter* InPlayer)
{
	TargetPlayer = InPlayer;
	const bool bInGame = TargetPlayer.IsValid();

	// 메인 메뉴(캐릭터 없음)에서는 배경 연출이 멈추지 않도록 일시정지하지 않음
	bPausedByWidget = bInGame && bPauseGameWhenOpen;
	if (bPausedByWidget)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC && bInGame)
	{
		PC = Cast<APlayerController>(TargetPlayer->GetController());
	}
	if (!PC)
	{
		return;
	}

	// UserWidget은 기본적으로 포커스 불가라 SetWidgetToFocus 시 경고가 나므로 먼저 허용
	SetIsFocusable(true);

	if (bInGame)
	{
		// 보상 선택 위젯(URLRewardSelectWidget::InitializeWidget)과 같은 입력 모드
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
	else
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(InputMode);
	}
	PC->bShowMouseCursor = true;
}

void URLMetaUpgradeWidget::CloseMetaUpgrade()
{
	if (bPausedByWidget)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedByWidget = false;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC && TargetPlayer.IsValid())
	{
		PC = Cast<APlayerController>(TargetPlayer->GetController());
	}

	// 인게임은 보상 선택 위젯(URLRewardSelectWidget::CloseWidget)과 같은 방식으로 원복.
	// 메인 메뉴는 UI Only + 커서를 유지하고 포커스만 돌아갈 위젯으로 넘김
	if (PC && TargetPlayer.IsValid())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	if (ReturnWidgetClass && PC)
	{
		if (UUserWidget* ReturnWidget = CreateWidget<UUserWidget>(PC, ReturnWidgetClass))
		{
			ReturnWidget->AddToViewport();
			if (!TargetPlayer.IsValid() && ReturnWidget->IsFocusable())
			{
				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(ReturnWidget->TakeWidget());
				PC->SetInputMode(InputMode);
			}
		}
	}

	RemoveFromParent();
}

void URLMetaUpgradeWidget::RebuildRows()
{
	for (URLMetaUpgradeRowWidget* Row : RowWidgets)
	{
		if (Row)
		{
			Row->OnUpgradeClicked.RemoveDynamic(this, &URLMetaUpgradeWidget::HandleRowUpgradeClicked);
		}
	}
	RowWidgets.Reset();

	if (!Box_Upgrades)
	{
		return;
	}
	Box_Upgrades->ClearChildren();

	URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this);
	if (!SaveSys)
	{
		return;
	}

	TSubclassOf<URLMetaUpgradeRowWidget> TargetRowClass = RowWidgetClass;
	if (!TargetRowClass)
	{
		TargetRowClass = LoadClass<URLMetaUpgradeRowWidget>(
			nullptr, TEXT("/Game/Blueprint/UI/WBP_MetaUpgradeRow.WBP_MetaUpgradeRow_C"));
	}
	if (!TargetRowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MetaUpgradeWidget] 행 위젯 클래스를 찾을 수 없습니다."));
		return;
	}

	const bool bLocked = IsUpgradeLocked();
	for (const FRLMetaUpgradeInfo& Info : SaveSys->GetAllMetaUpgradeInfos())
	{
		URLMetaUpgradeRowWidget* Row = CreateWidget<URLMetaUpgradeRowWidget>(this, TargetRowClass);
		if (!Row)
		{
			continue;
		}

		Box_Upgrades->AddChild(Row);
		Row->SetRunLocked(bLocked);
		Row->SetUpgradeInfo(Info);
		Row->OnUpgradeClicked.AddUniqueDynamic(this, &URLMetaUpgradeWidget::HandleRowUpgradeClicked);
		RowWidgets.Add(Row);
	}
}

void URLMetaUpgradeWidget::RefreshExistingRows()
{
	URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this);
	if (!SaveSys)
	{
		return;
	}

	for (URLMetaUpgradeRowWidget* Row : RowWidgets)
	{
		FRLMetaUpgradeInfo Info;
		if (Row && SaveSys->GetMetaUpgradeInfo(Row->GetUpgradeKey(), Info))
		{
			Row->SetUpgradeInfo(Info);
		}
	}
}

bool URLMetaUpgradeWidget::RequestUpgrade(FName UpgradeKey)
{
	URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this);
	if (!SaveSys)
	{
		return false;
	}

	if (IsUpgradeLocked())
	{
		ShowRunLockNotice();
		return false;
	}

	FString FailReason;
	if (!SaveSys->UpgradeMetaTrait(UpgradeKey, FailReason))
	{
		ShowFailReason(FailReason);
		BP_OnUpgradeFailed(UpgradeKey, FailReason);
		return false;
	}

	// 소울 차감은 OnSoulsChanged로 카운터/구매 가능 여부가 이미 갱신됨. 레벨이 오른 해당 행만 다시 세팅
	for (URLMetaUpgradeRowWidget* Row : RowWidgets)
	{
		FRLMetaUpgradeInfo Info;
		if (Row && Row->GetUpgradeKey() == UpgradeKey && SaveSys->GetMetaUpgradeInfo(UpgradeKey, Info))
		{
			Row->SetUpgradeInfo(Info);
			BP_OnUpgradeSucceeded(UpgradeKey, Info.CurrentLevel);
			break;
		}
	}
	return true;
}

void URLMetaUpgradeWidget::RequestResetAll()
{
	// 런 진행 중 초기화(전액 환불)도 막음 — 강화와 같은 이유
	if (IsUpgradeLocked())
	{
		ShowRunLockNotice();
		return;
	}

	if (URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this))
	{
		SaveSys->ResetAllMetaUpgrades(true);
		UpdateSoulsText(SaveSys->GetTotalSouls());
		RefreshExistingRows();
	}
}

void URLMetaUpgradeWidget::HandleSoulsChanged(int32 TotalSouls, int32 Delta)
{
	UpdateSoulsText(TotalSouls);

	// 보유 소울이 바뀌면 다른 행의 구매 가능(bCanAfford) 표시도 달라지므로 전체 상태를 재조회
	RefreshExistingRows();
}

void URLMetaUpgradeWidget::HandleRowUpgradeClicked(FName UpgradeKey)
{
	RequestUpgrade(UpgradeKey);
}

void URLMetaUpgradeWidget::HandleResetClicked()
{
	RequestResetAll();
}

void URLMetaUpgradeWidget::HandleExitClicked()
{
	CloseMetaUpgrade();
}

void URLMetaUpgradeWidget::UpdateSoulsText(int32 TotalSouls)
{
	if (Txt_Souls)
	{
		Txt_Souls->SetText(FText::Format(NSLOCTEXT("RLMeta", "SoulsFmt", "보유 소울: {0}"), FText::AsNumber(TotalSouls)));
	}
}

void URLMetaUpgradeWidget::ShowFailReason(const FString& Reason)
{
	if (!Txt_FailReason)
	{
		return;
	}

	Txt_FailReason->SetText(FText::FromString(Reason));
	Txt_FailReason->SetVisibility(ESlateVisibility::HitTestInvisible);
	FailReasonRemaining = FailReasonDisplayTime;
}

bool URLMetaUpgradeWidget::IsUpgradeLocked() const
{
	const URLSaveSubsystem* SaveSys = URLSaveSubsystem::Get(this);
	return SaveSys && SaveSys->IsMetaUpgradeLocked();
}

void URLMetaUpgradeWidget::ShowRunLockNotice()
{
	ShowFailReason(TEXT("게임 진행 중에는 영혼 강화를 할 수 없습니다."));

	if (Panel_RunLockNotice)
	{
		Panel_RunLockNotice->SetVisibility(ESlateVisibility::HitTestInvisible);
		FailReasonRemaining = FailReasonDisplayTime;
	}

	BP_OnUpgradeBlockedByActiveRun();
}
