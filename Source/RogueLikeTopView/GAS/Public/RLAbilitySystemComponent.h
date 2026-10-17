#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RLAbilitySystemComponent.generated.h"

// ★ 프로젝트 공용 ASC. 플레이어/몬스터 모두 ARLCharacter에서 생성합니다.
// 싱글플레이라 PlayerState가 아닌 캐릭터 자신이 Owner/Avatar를 겸합니다 (맵 이동 시 상태 이월은 RLSaveSubsystem 담당).
// 지금은 빈 껍데기이고, 4단계(스킬 → GA)에서 InputTag 기반 어빌리티 발동 함수가 여기에 추가될 예정입니다.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URLAbilitySystemComponent();
};
