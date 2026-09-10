#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RLStatComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROGUELIKETOPVIEW_API URLStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URLStatComponent();

	// ==========================================
	// ★ 핵심 스탯 4가지
	// ==========================================

	// 1. 공격력 (물리 데미지 기반)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackPower = 10.0f;

	// 2. 주문력 (마법/스킬 데미지 기반)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AbilityPower = 10.0f;

	// 3. 이동 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MoveSpeed = 600.0f;

	// 4. 공격 속도 배율 (1.0 = 기본 속도 100%, 1.5 = 150% 빠름)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackSpeed = 1.0f;

protected:
	virtual void BeginPlay() override;
};