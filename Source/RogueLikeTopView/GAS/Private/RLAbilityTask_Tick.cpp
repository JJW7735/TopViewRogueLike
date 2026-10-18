#include "RLAbilityTask_Tick.h"

URLAbilityTask_Tick::URLAbilityTask_Tick(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URLAbilityTask_Tick* URLAbilityTask_Tick::CreateTickTask(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<URLAbilityTask_Tick>(OwningAbility);
}

void URLAbilityTask_Tick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	OnTick.Broadcast(DeltaTime);
}
