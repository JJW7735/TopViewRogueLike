#include "RLRewardSubsystem.h"
#include "RLCharacter.h"
#include "RLStatComponent.h"
#include "RLHealthComponent.h"
#include "RLRewardDataAsset.h"
#include "RLItemPoolDataAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void URLRewardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// ★ DataAsset 풀(DA_ItemPool)의 보상 카드가 있으면 그걸 쓰고, 없으면 하드코딩 풀을 폴백으로 사용
	if (const URLItemPoolDataAsset* PoolAsset = URLItemPoolDataAsset::LoadDefault())
	{
		URLItemPoolDataAsset::CollectRewards(PoolAsset->RewardCards, RewardPool);
	}
	if (RewardPool.Num() == 0)
	{
		BuildDefaultRewardPool();
	}
}

URLRewardSubsystem* URLRewardSubsystem::GetRewardSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URLRewardSubsystem>() : nullptr;
}

void URLRewardSubsystem::RegisterRewardDataAsset(URLRewardDataAsset* DataAsset)
{
	if (!DataAsset)
	{
		return;
	}

	for (const FRLRewardData& NewReward : DataAsset->Rewards)
	{
		RewardPool.Add(NewReward);
	}
}

TArray<FRLRewardData> URLRewardSubsystem::RollRewards(int32 Count)
{
	TArray<FRLRewardData> SelectedRewards;

	if (RewardPool.Num() == 0)
	{
		BuildDefaultRewardPool();
	}

	if (RewardPool.Num() == 0)
	{
		return SelectedRewards;
	}

	const int32 DesiredCount = FMath::Min(Count, RewardPool.Num());
	TArray<int32> ChosenIndices;

	for (int32 Slot = 0; Slot < DesiredCount; ++Slot)
	{
		// 1. 슬롯별 희귀도 주사위 (일반 55%, 희귀 28%, 영웅 13%, 전설 4%)
		const float RarityRoll = FMath::FRandRange(0.0f, 100.0f);
		ERLRewardRarity TargetRarity = ERLRewardRarity::Common;

		if (RarityRoll < 4.0f)
		{
			TargetRarity = ERLRewardRarity::Legendary;
		}
		else if (RarityRoll < 17.0f)
		{
			TargetRarity = ERLRewardRarity::Epic;
		}
		else if (RarityRoll < 45.0f)
		{
			TargetRarity = ERLRewardRarity::Rare;
		}
		else
		{
			TargetRarity = ERLRewardRarity::Common;
		}

		// 2. 해당 희귀도에 속하고 아직 뽑히지 않은 후보 인덱스 수집
		TArray<int32> Candidates;
		for (int32 i = 0; i < RewardPool.Num(); ++i)
		{
			if (!ChosenIndices.Contains(i) && RewardPool[i].Rarity == TargetRarity)
			{
				Candidates.Add(i);
			}
		}

		// 해당 희귀도의 카드가 부족하면 아직 안 뽑힌 아무 카드나 후보로 선택
		if (Candidates.Num() == 0)
		{
			for (int32 i = 0; i < RewardPool.Num(); ++i)
			{
				if (!ChosenIndices.Contains(i))
				{
					Candidates.Add(i);
				}
			}
		}

		if (Candidates.Num() == 0)
		{
			break;
		}

		// 3. SelectionWeight 기반 가중치 랜덤 선택
		float TotalWeight = 0.0f;
		for (int32 CandidateIndex : Candidates)
		{
			TotalWeight += FMath::Max(0.1f, RewardPool[CandidateIndex].SelectionWeight);
		}

		float RandomWeight = FMath::FRandRange(0.0f, TotalWeight);
		int32 FinalChosenCandidate = Candidates[0];

		for (int32 CandidateIndex : Candidates)
		{
			const float W = FMath::Max(0.1f, RewardPool[CandidateIndex].SelectionWeight);
			if (RandomWeight <= W)
			{
				FinalChosenCandidate = CandidateIndex;
				break;
			}
			RandomWeight -= W;
		}

		ChosenIndices.Add(FinalChosenCandidate);
		SelectedRewards.Add(RewardPool[FinalChosenCandidate]);
	}

	return SelectedRewards;
}

bool URLRewardSubsystem::ApplyReward(ARLCharacter* TargetCharacter, const FRLRewardData& Reward)
{
	if (!TargetCharacter)
	{
		return false;
	}

	URLStatComponent* StatComp = TargetCharacter->GetStatComponent();
	URLHealthComponent* HealthComp = TargetCharacter->FindComponentByClass<URLHealthComponent>();

	for (const FRLStatBonus& Bonus : Reward.StatBonuses)
	{
		// 전투 스탯 8종은 StatComp가 처리 (클램프/이동속도 반영 포함)
		if (StatComp && StatComp->AddStatBonus(Bonus.TargetStat, Bonus.Value))
		{
			continue;
		}

		switch (Bonus.TargetStat)
		{
		case ERLTargetStat::MaxHealth:
			if (HealthComp)
			{
				HealthComp->IncreaseMaxHealth(Bonus.Value, true);
			}
			break;

		case ERLTargetStat::HealPercent:
			if (HealthComp)
			{
				const float HealAmount = HealthComp->MaxHealth * (Bonus.Value / 100.0f);
				HealthComp->Heal(HealAmount);
			}
			break;

		default:
			break;
		}
	}

	if (StatComp)
	{
		StatComp->NotifyStatsChanged();
	}

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("✨ [보상 획득!] [%s] %s"),
			*Reward.GetRarityDisplayName().ToString(), *Reward.RewardName.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, Reward.GetRarityColor().ToFColor(true), Msg);
	}

	OnRewardApplied.Broadcast(TargetCharacter, Reward);
	return true;
}

void URLRewardSubsystem::BuildDefaultRewardPool()
{
	RewardPool.Reset();

	// ----------------------------------------------------
	// 1. 일반 등급 (Common)
	// ----------------------------------------------------
	{
		FRLRewardData R;
		R.RewardID = FName("SharpBlade");
		R.RewardName = NSLOCTEXT("RLReward", "SharpBlade", "날카로운 칼날");
		R.Description = NSLOCTEXT("RLReward", "SharpBlade_Desc", "공격력이 5 증가합니다.");
		R.Rarity = ERLRewardRarity::Common;
		R.StatBonuses.Add({ ERLTargetStat::AttackPower, 5.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_DeepCut.T_DeepCut")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("MagicSpark");
		R.RewardName = NSLOCTEXT("RLReward", "MagicSpark", "마력의 불꽃");
		R.Description = NSLOCTEXT("RLReward", "MagicSpark_Desc", "주문력이 5 증가합니다.");
		R.Rarity = ERLRewardRarity::Common;
		R.StatBonuses.Add({ ERLTargetStat::AbilityPower, 5.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_StormAttack.T_StormAttack")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("PreciseAim");
		R.RewardName = NSLOCTEXT("RLReward", "PreciseAim", "정밀한 조준");
		R.Description = NSLOCTEXT("RLReward", "PreciseAim_Desc", "치명타 확률이 5% 증가합니다.");
		R.Rarity = ERLRewardRarity::Common;
		R.StatBonuses.Add({ ERLTargetStat::CritChance, 5.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_PiercingAttack.T_PiercingAttack")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("ToughHide");
		R.RewardName = NSLOCTEXT("RLReward", "ToughHide", "단단한 가죽");
		R.Description = NSLOCTEXT("RLReward", "ToughHide_Desc", "방어력이 5 증가합니다.");
		R.Rarity = ERLRewardRarity::Common;
		R.StatBonuses.Add({ ERLTargetStat::Defense, 5.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_EnergyRecharge.T_EnergyRecharge")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("FirstAid");
		R.RewardName = NSLOCTEXT("RLReward", "FirstAid", "응급 치료");
		R.Description = NSLOCTEXT("RLReward", "FirstAid_Desc", "최대 체력의 30%를 즉시 회복합니다.");
		R.Rarity = ERLRewardRarity::Common;
		R.StatBonuses.Add({ ERLTargetStat::HealPercent, 30.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Regeneration.T_Regeneration")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("LightSteps");
		R.RewardName = NSLOCTEXT("RLReward", "LightSteps", "가벼운 발걸음");
		R.Description = NSLOCTEXT("RLReward", "LightSteps_Desc", "이동 속도가 40 증가합니다.");
		R.Rarity = ERLRewardRarity::Common;
		R.StatBonuses.Add({ ERLTargetStat::MoveSpeed, 40.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Evade.T_Evade")));
		RewardPool.Add(R);
	}

	// ----------------------------------------------------
	// 2. 희귀 등급 (Rare)
	// ----------------------------------------------------
	{
		FRLRewardData R;
		R.RewardID = FName("GiantsMight");
		R.RewardName = NSLOCTEXT("RLReward", "GiantsMight", "거인의 완력");
		R.Description = NSLOCTEXT("RLReward", "GiantsMight_Desc", "공격력이 15 증가합니다.");
		R.Rarity = ERLRewardRarity::Rare;
		R.StatBonuses.Add({ ERLTargetStat::AttackPower, 15.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_FuryAttack.T_FuryAttack")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("ArcaneWisdom");
		R.RewardName = NSLOCTEXT("RLReward", "ArcaneWisdom", "비전 지혜");
		R.Description = NSLOCTEXT("RLReward", "ArcaneWisdom_Desc", "주문력이 15 증가합니다.");
		R.Rarity = ERLRewardRarity::Rare;
		R.StatBonuses.Add({ ERLTargetStat::AbilityPower, 15.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_StormAttack.T_StormAttack")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("DeadlyStrike");
		R.RewardName = NSLOCTEXT("RLReward", "DeadlyStrike", "치명적인 일격");
		R.Description = NSLOCTEXT("RLReward", "DeadlyStrike_Desc", "치명타 피해가 25% 증가합니다.");
		R.Rarity = ERLRewardRarity::Rare;
		R.StatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.25f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_CursedCut.T_CursedCut")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("IronPlate");
		R.RewardName = NSLOCTEXT("RLReward", "IronPlate", "강철 갑주");
		R.Description = NSLOCTEXT("RLReward", "IronPlate_Desc", "방어력이 15 증가합니다.");
		R.Rarity = ERLRewardRarity::Rare;
		R.StatBonuses.Add({ ERLTargetStat::Defense, 15.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_EnergyRecharge.T_EnergyRecharge")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("VitalityElixir");
		R.RewardName = NSLOCTEXT("RLReward", "VitalityElixir", "활력의 비약");
		R.Description = NSLOCTEXT("RLReward", "VitalityElixir_Desc", "최대 체력이 40 증가하고 즉시 회복됩니다.");
		R.Rarity = ERLRewardRarity::Rare;
		R.StatBonuses.Add({ ERLTargetStat::MaxHealth, 40.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Regeneration.T_Regeneration")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("BerserkerRage");
		R.RewardName = NSLOCTEXT("RLReward", "BerserkerRage", "광전사의 분노");
		R.Description = NSLOCTEXT("RLReward", "BerserkerRage_Desc", "공격 속도가 20% 증가합니다.");
		R.Rarity = ERLRewardRarity::Rare;
		R.StatBonuses.Add({ ERLTargetStat::AttackSpeed, 0.20f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Berserk.T_Berserk")));
		RewardPool.Add(R);
	}

	// ----------------------------------------------------
	// 3. 영웅 등급 (Epic)
	// ----------------------------------------------------
	{
		FRLRewardData R;
		R.RewardID = FName("EssenceOfValor");
		R.RewardName = NSLOCTEXT("RLReward", "EssenceOfValor", "용기의 정수");
		R.Description = NSLOCTEXT("RLReward", "EssenceOfValor_Desc", "공격력 +25, 치명타 확률 +10%가 증가합니다.");
		R.Rarity = ERLRewardRarity::Epic;
		R.StatBonuses.Add({ ERLTargetStat::AttackPower, 25.0f });
		R.StatBonuses.Add({ ERLTargetStat::CritChance, 10.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_UltimatePower.T_UltimatePower")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("SwordmasterSecret");
		R.RewardName = NSLOCTEXT("RLReward", "SwordmasterSecret", "검성의 비기");
		R.Description = NSLOCTEXT("RLReward", "SwordmasterSecret_Desc", "공격력 +30, 공격 속도 +15%가 증가합니다.");
		R.Rarity = ERLRewardRarity::Epic;
		R.StatBonuses.Add({ ERLTargetStat::AttackPower, 30.0f });
		R.StatBonuses.Add({ ERLTargetStat::AttackSpeed, 0.15f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_FuryAttack.T_FuryAttack")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("PinnacleOfSorcery");
		R.RewardName = NSLOCTEXT("RLReward", "PinnacleOfSorcery", "마도학의 극의");
		R.Description = NSLOCTEXT("RLReward", "PinnacleOfSorcery_Desc", "주문력 +30, 공격 속도 +15%가 증가합니다.");
		R.Rarity = ERLRewardRarity::Epic;
		R.StatBonuses.Add({ ERLTargetStat::AbilityPower, 30.0f });
		R.StatBonuses.Add({ ERLTargetStat::AttackSpeed, 0.15f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_StormAttack.T_StormAttack")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("ArcaneGrimoire");
		R.RewardName = NSLOCTEXT("RLReward", "ArcaneGrimoire", "비전 학자의 비전서");
		R.Description = NSLOCTEXT("RLReward", "ArcaneGrimoire_Desc", "주문력 +28, 치명타 확률 +10%가 증가합니다.");
		R.Rarity = ERLRewardRarity::Epic;
		R.StatBonuses.Add({ ERLTargetStat::AbilityPower, 28.0f });
		R.StatBonuses.Add({ ERLTargetStat::CritChance, 10.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_CursedFury.T_CursedFury")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("IndomitableWill");
		R.RewardName = NSLOCTEXT("RLReward", "IndomitableWill", "불굴의 의지");
		R.Description = NSLOCTEXT("RLReward", "IndomitableWill_Desc", "방어력 +20, 피해 감소율 +6%가 증가합니다.");
		R.Rarity = ERLRewardRarity::Epic;
		R.StatBonuses.Add({ ERLTargetStat::Defense, 20.0f });
		R.StatBonuses.Add({ ERLTargetStat::DamageReduction, 6.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_EnergyRecharge.T_EnergyRecharge")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("ColossusHeart");
		R.RewardName = NSLOCTEXT("RLReward", "ColossusHeart", "거신의 심장");
		R.Description = NSLOCTEXT("RLReward", "ColossusHeart_Desc", "최대 체력이 80 증가하고 완전 회복됩니다.");
		R.Rarity = ERLRewardRarity::Epic;
		R.StatBonuses.Add({ ERLTargetStat::MaxHealth, 80.0f });
		R.StatBonuses.Add({ ERLTargetStat::HealPercent, 100.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Regeneration.T_Regeneration")));
		RewardPool.Add(R);
	}

	// ----------------------------------------------------
	// 4. 전설 등급 (Legendary)
	// ----------------------------------------------------
	{
		FRLRewardData R;
		R.RewardID = FName("GodslayerBlade");
		R.RewardName = NSLOCTEXT("RLReward", "GodslayerBlade", "신살자의 검");
		R.Description = NSLOCTEXT("RLReward", "GodslayerBlade_Desc", "공격력 +50, 치명타 확률 +15%, 치명타 피해 +50%가 대폭 증가합니다!");
		R.Rarity = ERLRewardRarity::Legendary;
		R.StatBonuses.Add({ ERLTargetStat::AttackPower, 50.0f });
		R.StatBonuses.Add({ ERLTargetStat::CritChance, 15.0f });
		R.StatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.50f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_UltimatePower.T_UltimatePower")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("AkashicRecords");
		R.RewardName = NSLOCTEXT("RLReward", "AkashicRecords", "아카식 레코드");
		R.Description = NSLOCTEXT("RLReward", "AkashicRecords_Desc", "주문력 +50, 치명타 확률 +15%, 치명타 피해 +50%가 대폭 증가합니다!");
		R.Rarity = ERLRewardRarity::Legendary;
		R.StatBonuses.Add({ ERLTargetStat::AbilityPower, 50.0f });
		R.StatBonuses.Add({ ERLTargetStat::CritChance, 15.0f });
		R.StatBonuses.Add({ ERLTargetStat::CritDamageMultiplier, 0.50f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_UltimatePower.T_UltimatePower")));
		RewardPool.Add(R);
	}
	{
		FRLRewardData R;
		R.RewardID = FName("PhoenixFeather");
		R.RewardName = NSLOCTEXT("RLReward", "PhoenixFeather", "불사조의 깃털");
		R.Description = NSLOCTEXT("RLReward", "PhoenixFeather_Desc", "최대 체력 +120, 완전 회복, 피해 감소율 +12%의 궁극의 가호를 받습니다!");
		R.Rarity = ERLRewardRarity::Legendary;
		R.StatBonuses.Add({ ERLTargetStat::MaxHealth, 120.0f });
		R.StatBonuses.Add({ ERLTargetStat::HealPercent, 100.0f });
		R.StatBonuses.Add({ ERLTargetStat::DamageReduction, 12.0f });
		R.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/KiroshoSkIconsPk0/Textures/T_Regeneration.T_Regeneration")));
		RewardPool.Add(R);
	}
}
