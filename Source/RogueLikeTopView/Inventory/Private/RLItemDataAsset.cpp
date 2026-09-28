#include "RLItemDataAsset.h"
#include "RLCharacter.h"
#include "RLHealthComponent.h"

URLItemDataAsset::URLItemDataAsset()
{
}

bool URLItemDataAsset::UseItem_Implementation(ARLCharacter* UserCharacter)
{
	if (!UserCharacter)
	{
		return false;
	}

	bool bUsed = false;

	// 체력 회복 처리
	URLHealthComponent* HealthComp = UserCharacter->FindComponentByClass<URLHealthComponent>();
	if (HealthComp)
	{
		// 이미 풀피라면 낭비 방지를 위해 사용 실패 처리 가능
		if (InstantHealAmount > 0.0f || PercentHealAmount > 0.0f)
		{
			if (HealthComp->CurrentHealth >= HealthComp->MaxHealth)
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("⚠️ 이미 체력이 가득 차 있어 물약을 사용할 수 없습니다!"));
				}
				return false;
			}

			float TotalHeal = InstantHealAmount;
			if (PercentHealAmount > 0.0f)
			{
				TotalHeal += HealthComp->MaxHealth * (PercentHealAmount / 100.0f);
			}

			HealthComp->Heal(TotalHeal);
			bUsed = true;
		}
	}

	return bUsed;
}
