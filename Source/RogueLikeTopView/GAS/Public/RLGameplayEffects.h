#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RLGameplayEffects.generated.h"

// ★ C++에서 직접 쓰는 공용 GE. 수치는 전부 SetByCaller로 런타임에 넘깁니다.
// 기획자가 튜닝할 GE(버프, 아이템 등)는 2단계부터 블루프린트 GE 에셋(GE_*)으로 만듭니다.

// 피해: IncomingDamage 메타 어트리뷰트에 Data.Damage 만큼 더함 → URLAttributeSet::PostGameplayEffectExecute에서 Health 감소
UCLASS()
class ROGUELIKETOPVIEW_API URLGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URLGE_Damage();
};

// 회복: Health에 Data.Heal 만큼 더함 (최대치 클램프는 AttributeSet 담당)
UCLASS()
class ROGUELIKETOPVIEW_API URLGE_Heal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URLGE_Heal();
};
