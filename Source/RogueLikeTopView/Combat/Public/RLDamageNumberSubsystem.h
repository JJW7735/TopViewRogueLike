#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RLDamageNumberSubsystem.generated.h"

class UUserWidget;
class URLDamageNumberSubsystem;

/** 화면에 떠 있는 데미지 숫자 하나 */
USTRUCT()
struct FRLActiveDamageNumber
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UUserWidget> Widget = nullptr;

	FVector WorldLocation = FVector::ZeroVector;
	float JitterX = 0.0f;
	float Elapsed = 0.0f;
};

/**
 * 적 한 마리의 HealthComp->OnHealthChanged를 듣는 중계 오브젝트.
 * 다이나믹 델리게이트는 "누가 맞았는지"를 넘겨주지 않으므로, 적마다 하나씩 만들어 대상 액터를 기억시킵니다.
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLDamageNumberListener : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<AActor> Victim;
	TWeakObjectPtr<URLDamageNumberSubsystem> OwnerSubsystem;

	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth, float HealthDelta);
};

/**
 * 데미지 숫자 팝업 (맞은 적 머리 위에 숫자가 떠올랐다가 사라짐).
 *
 * 기존 전투 코드를 건드리지 않도록 월드에 스폰되는 ARLEnemyCharacter의 HealthComp->OnHealthChanged를
 * 이 서브시스템이 알아서 구독합니다. 위젯은 WBP_DamageNumber(TextBlock "Txt_Damage" 필요)를 사용합니다.
 *
 * TODO(B안): 크리티컬 구분은 ARLCharacter에 bIsCritical을 담은 피해 델리게이트가 생기면 그쪽 구독으로 교체.
 */
UCLASS()
class ROGUELIKETOPVIEW_API URLDamageNumberSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** 지정 위치(월드)에 데미지 숫자를 띄웁니다. BP에서 테스트용으로 직접 호출해도 됩니다. */
	UFUNCTION(BlueprintCallable, Category = "DamageNumber")
	void SpawnDamageNumber(float Damage, FVector WorldLocation);

	/** 숫자가 떠 있는 시간(초) */
	float Lifetime = 0.8f;

	/** 수명 동안 위로 떠오르는 거리(화면 픽셀) */
	float RiseDistance = 70.0f;

	/** 이 시점(수명 비율)부터 투명해지기 시작 */
	float FadeStartRatio = 0.4f;

	/** 처음 등장할 때 살짝 커졌다 줄어드는 크기 */
	float PopScale = 1.4f;

	/** 같은 자리에 연타가 겹치지 않도록 좌우로 흩뿌리는 폭(화면 픽셀) */
	float HorizontalJitter = 25.0f;

	/** 적의 캡슐 꼭대기에서 위로 더 띄울 높이(월드 단위) */
	float HeadOffset = 20.0f;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void TryRegisterActor(AActor* Actor);
	void HandleActorSpawned(AActor* SpawnedActor);
	bool UpdateDamageNumber(FRLActiveDamageNumber& Entry, APlayerController* PC) const;

	UPROPERTY()
	TArray<TObjectPtr<URLDamageNumberListener>> Listeners;

	UPROPERTY()
	TSubclassOf<UUserWidget> DamageNumberWidgetClass;

	UPROPERTY()
	TArray<FRLActiveDamageNumber> ActiveNumbers;
	FDelegateHandle ActorSpawnedHandle;
};
