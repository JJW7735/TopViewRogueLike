#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RLTypes.h"
#include "RLGroundAreaEffect.generated.h"

// ★ 인페르노/메테오처럼 GroundActorClass로 스폰되는 장판형 스킬의 공용 베이스입니다.
// RLSkillComponent가 스폰 직후 DamageData에 스탯 스케일링이 적용된 최종 데미지를 넣어줍니다.
// BP에서는 데미지 값을 직접 입력하지 말고 이 DamageData를 참조해서 ReceiveCustomDamage에 넘기세요.
UCLASS()
class ROGUELIKETOPVIEW_API ARLGroundAreaEffect : public AActor
{
	GENERATED_BODY()

public:
	ARLGroundAreaEffect();

	// 스킬 데이터의 AD/AP 스케일링까지 반영된 최종 데미지. RLSkillComponent가 스폰 직후 채워줍니다.
	UPROPERTY(BlueprintReadWrite, Category = "GroundAreaEffect")
	FRLDamageData DamageData;
};
