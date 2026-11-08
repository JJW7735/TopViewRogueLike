#include "RLBuffEntryWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

void URLBuffEntryWidget::SetupEntry(const FRLAcquiredReward& Entry)
{
	const FRLRewardData& Reward = Entry.Reward;

	if (IconImage)
	{
		// 아이콘은 소프트 레퍼런스라 여기서 로드 (WBP_InventorySlot.UpdateSlot과 같은 방식)
		if (UTexture2D* IconTexture = Reward.Icon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(IconTexture);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (NameText)
	{
		NameText->SetText(Entry.Count > 1
			? FText::Format(NSLOCTEXT("RLBuff", "NameWithCount", "{0} ×{1}"), Reward.RewardName, FText::AsNumber(Entry.Count))
			: Reward.RewardName);
		NameText->SetColorAndOpacity(FSlateColor(Reward.GetRarityColor()));
	}

	if (EntryBorder)
	{
		FSlateBrush Brush = EntryBorder->Background;
		Brush.OutlineSettings.Color = FSlateColor(Reward.GetRarityColor());
		EntryBorder->SetBrush(Brush);
	}

	SetToolTipText(Reward.Description);

	BP_OnEntrySetup(Entry);
}
