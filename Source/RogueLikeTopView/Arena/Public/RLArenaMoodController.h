#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RLArenaMoodController.generated.h"

class ULightComponentBase;
class UExponentialHeightFogComponent;
class APostProcessVolume;
class UNiagaraComponent;
class UMaterialInstanceDynamic;

/**
 * 보스 페이즈 전환 시 색/세기를 바꿀 라이트 그룹.
 * ActorTag가 붙은 (같은 레벨의) 액터들의 모든 라이트 컴포넌트(포인트/디렉셔널/스카이라이트)가 대상.
 */
USTRUCT(BlueprintType)
struct FRLMoodLightGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FName ActorTag;

	// 보스 페이즈 라이트 색 (Linear)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FLinearColor BossColor = FLinearColor::White;

	// 보스 페이즈 세기 = 원래 세기 * 이 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	float BossIntensityScale = 1.f;
};

/**
 * 보스 페이즈 전환 시 바꿀 머티리얼 벡터 파라미터.
 * ActorTag가 붙은 액터의 메시 머티리얼 중 ParameterName을 가진 슬롯을 MID로 바꿔 값을 보간.
 */
USTRUCT(BlueprintType)
struct FRLMoodMaterialParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FName ActorTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FName ParameterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood")
	FLinearColor BossValue = FLinearColor::White;
};

/**
 * 아레나(보스룸) 분위기 컨트롤러.
 * 평상시(Calm) 레벨에 배치된 값을 BeginPlay에 캐싱해 두고, SetBossPhase(true) 호출 시
 * TransitionDuration 동안 보스 페이즈 값으로 보간한다. 보스 등장 연출에서 SetBossPhase를 호출하면 됨.
 * 자신이 속한 레벨(스트리밍 서브레벨)의 액터만 건드린다.
 */
UCLASS()
class ROGUELIKETOPVIEW_API ARLArenaMoodController : public AActor
{
	GENERATED_BODY()

public:
	ARLArenaMoodController();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Mood")
	void SetBossPhase(bool bBoss, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "Mood")
	void ToggleBossPhase();

	UFUNCTION(BlueprintPure, Category = "Mood")
	bool IsBossPhase() const { return bBossPhase; }

	// 전환이 시작될 때 호출 (BP에서 사운드/카메라 연출 등 추가용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Mood")
	void OnBossPhaseChanged(bool bBoss);

protected:
	virtual void BeginPlay() override;

	// 평상시 -> 보스 페이즈 전환 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood", meta = (ClampMin = "0.0"))
	float TransitionDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|Lights")
	TArray<FRLMoodLightGroup> LightGroups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|Materials")
	TArray<FRLMoodMaterialParam> MaterialParams;

	// 이 태그가 붙은 PostProcessVolume의 BlendWeight를 0 -> 1로 보간 (보스 전용 색보정 볼륨)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|PostProcess")
	FName BossPostProcessTag = TEXT("Mood_BossPP");

	// 평상시에만 켜지는 FX / 보스 페이즈에만 켜지는 FX (나이아가라)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|FX")
	FName CalmFXTag = TEXT("Mood_CalmFX");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|FX")
	FName BossFXTag = TEXT("Mood_BossFX");

	// 레벨의 ExponentialHeightFog 색 보간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|Fog")
	bool bAffectFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|Fog", meta = (EditCondition = "bAffectFog"))
	FLinearColor BossFogInscatteringColor = FLinearColor(0.004f, 0.02f, 0.008f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mood|Fog", meta = (EditCondition = "bAffectFog"))
	FLinearColor BossVolumetricFogAlbedo = FLinearColor(0.6f, 0.85f, 0.65f);

	// 테스트용: 키를 누르면 페이즈 토글 (Shipping 빌드 제외)
	UPROPERTY(EditAnywhere, Category = "Mood|Debug")
	bool bEnableDebugToggleKey = true;

	UPROPERTY(EditAnywhere, Category = "Mood|Debug", meta = (EditCondition = "bEnableDebugToggleKey"))
	FKey DebugToggleKey = EKeys::P;

private:
	struct FLightEntry
	{
		TWeakObjectPtr<ULightComponentBase> Light;
		FLinearColor CalmColor;
		float CalmIntensity;
		FLinearColor BossColor;
		float BossIntensity;
	};

	struct FMaterialEntry
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		FName ParameterName;
		FLinearColor CalmValue;
		FLinearColor BossValue;
	};

	void GatherTargets();
	void ApplyAlpha(float Alpha);
	void SetFXActive(bool bBoss);
	void BindDebugKey();
	bool IsInMyLevel(const AActor* Actor) const;

	TArray<FLightEntry> LightEntries;
	TArray<FMaterialEntry> MaterialEntries;
	TArray<TWeakObjectPtr<APostProcessVolume>> BossPostProcessVolumes;
	TArray<TWeakObjectPtr<UNiagaraComponent>> CalmFX;
	TArray<TWeakObjectPtr<UNiagaraComponent>> BossFX;

	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	FLinearColor CalmFogInscatteringColor = FLinearColor::Black;
	FLinearColor CalmVolumetricFogAlbedo = FLinearColor::White;

	bool bBossPhase = false;
	float CurrentAlpha = 0.f;
};
