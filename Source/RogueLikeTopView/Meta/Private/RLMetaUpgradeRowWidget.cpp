#include "RLMetaUpgradeRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void URLMetaUpgradeRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Upgrade)
	{
		Btn_Upgrade->OnClicked.AddUniqueDynamic(this, &URLMetaUpgradeRowWidget::HandleUpgradeButtonClicked);
	}
}

void URLMetaUpgradeRowWidget::SetUpgradeInfo(const FRLMetaUpgradeInfo& InInfo)
{
	CachedInfo = InInfo;

	if (Txt_Name)
	{
		Txt_Name->SetText(InInfo.DisplayName);
	}

	if (Txt_Desc)
	{
		Txt_Desc->SetText(InInfo.Description);
	}

	if (Txt_Level)
	{
		Txt_Level->SetText(FText::Format(NSLOCTEXT("RLMeta", "RowLevelFmt", "{0}/{1}"),
			FText::AsNumber(InInfo.CurrentLevel), FText::AsNumber(InInfo.MaxLevel)));
	}

	const FText CurrentEffect = FormatBonusValue(InInfo.UpgradeKey, InInfo.CurrentBonusValue);

	if (Txt_Effect)
	{
		if (InInfo.bIsMaxLevel)
		{
			Txt_Effect->SetText(CurrentEffect);
		}
		else
		{
			Txt_Effect->SetText(FText::Format(NSLOCTEXT("RLMeta", "RowEffectFmt", "{0} → {1}"),
				CurrentEffect, FormatBonusValue(InInfo.UpgradeKey, InInfo.NextBonusValue)));
		}
	}

	if (Txt_Cost)
	{
		Txt_Cost->SetVisibility(InInfo.bIsMaxLevel ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		Txt_Cost->SetText(FText::Format(NSLOCTEXT("RLMeta", "RowCostFmt", "{0} 소울"), FText::AsNumber(InInfo.NextLevelCost)));
	}

	if (Txt_UpgradeLabel)
	{
		Txt_UpgradeLabel->SetText(InInfo.bIsMaxLevel
			? NSLOCTEXT("RLMeta", "RowMax", "MAX")
			: NSLOCTEXT("RLMeta", "RowUpgrade", "강화"));
	}

	if (Btn_Upgrade)
	{
		// MAX면 비활성화. 소울 부족이면 클릭은 허용해 실패 사유를 띄우되 흐리게 표시
		Btn_Upgrade->SetIsEnabled(!InInfo.bIsMaxLevel);
		Btn_Upgrade->SetRenderOpacity((InInfo.bIsMaxLevel || InInfo.bCanAfford) ? 1.0f : CannotAffordOpacity);
	}

	BP_OnUpgradeInfoUpdated(InInfo);
}

FText URLMetaUpgradeRowWidget::FormatBonusValue_Implementation(FName UpgradeKey, float Value) const
{
	static const FName CritKey(TEXT("Crit_Up"));
	static const FName SoulGainKey(TEXT("SoulGain_Up"));
	static const FName StartGoldKey(TEXT("StartGold_Up"));

	FNumberFormattingOptions Opts;
	Opts.MaximumFractionalDigits = 1;

	if (UpgradeKey == CritKey)
	{
		// 치명타 확률은 퍼센트 포인트 단위로 저장됨 (2.0 = 2%)
		return FText::Format(NSLOCTEXT("RLMeta", "BonusPercent", "+{0}%"), FText::AsNumber(Value, &Opts));
	}
	if (UpgradeKey == SoulGainKey)
	{
		// 영혼 획득량은 배율로 저장됨 (0.15 = 15%)
		return FText::Format(NSLOCTEXT("RLMeta", "BonusPercent", "+{0}%"), FText::AsNumber(Value * 100.0f, &Opts));
	}
	if (UpgradeKey == StartGoldKey)
	{
		return FText::Format(NSLOCTEXT("RLMeta", "BonusGold", "+{0}G"), FText::AsNumber(Value, &Opts));
	}
	return FText::Format(NSLOCTEXT("RLMeta", "BonusFlat", "+{0}"), FText::AsNumber(Value, &Opts));
}

void URLMetaUpgradeRowWidget::HandleUpgradeButtonClicked()
{
	OnUpgradeClicked.Broadcast(CachedInfo.UpgradeKey);
}
