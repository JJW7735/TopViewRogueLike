#include "RLAbilitySystemComponent.h"

URLAbilitySystemComponent::URLAbilitySystemComponent()
{
	// 싱글플레이 전용이라 GE 복제가 필요 없습니다.
	SetIsReplicatedByDefault(false);
}
