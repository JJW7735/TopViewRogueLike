#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RLAbilityTask_Tick.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FRLAbilityTickDelegate, float /*DeltaTime*/);

// ★ 어빌리티가 살아 있는 동안 매 프레임 OnTick을 방송하는 태스크.
// 캐스팅 게이지, 차지 누적, 홀드 틱 데미지, 장판 조준 데칼 갱신처럼 "프레임마다 진행되는" 스킬에서 씁니다.
// 어빌리티가 끝나면 태스크도 자동으로 정리됩니다 (예전 URLSkillComponent::TickComponent의 상태별 분기를 대체).
UCLASS()
class ROGUELIKETOPVIEW_API URLAbilityTask_Tick : public UAbilityTask
{
	GENERATED_BODY()

public:
	URLAbilityTask_Tick(const FObjectInitializer& ObjectInitializer);

	static URLAbilityTask_Tick* CreateTickTask(UGameplayAbility* OwningAbility);

	virtual void TickTask(float DeltaTime) override;

	FRLAbilityTickDelegate OnTick;
};
