#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RLSkillData.h"
#include "RLPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UNiagaraSystem;
class UMaterialParameterCollection;

UCLASS()
class ROGUELIKETOPVIEW_API ARLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARLPlayerController();

	// 테스트용: 현재 레벨의 모든 적(플레이어 제외) 즉사. 키보드 K 또는 콘솔 "KillAllEnemies"
	UFUNCTION(BlueprintCallable, Exec, Category = "Cheat|Enemy")
	void KillAllEnemies();

	// ★ 커서 아래의 "바닥" 지점. Ground 채널로 트레이스하되, 위를 향한 면(ImpactNormal.Z >= GroundMinNormalZ)이면서
	// 조종 중인 폰의 발 높이 + GroundMaxHeightAbovePawn 이하인 지점만 채택합니다. 아치 윗면/기둥 옆면/캐릭터에 맞으면
	// 그 컴포넌트를 무시하고 다시 쏴서 그 아래 바닥을 찾습니다. 못 찾으면 false (이동/조준 명령 무시).
	// 이동·회피·땅 조준 스킬·마우스 방향 회전이 모두 이 함수를 씁니다.
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool GetGroundHitUnderCursor(FHitResult& OutHit) const;

	// 바닥으로 인정할 최소 법선 Z (0.7 ≈ 45도 이하 경사)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Ground")
	float GroundMinNormalZ = 0.7f;

	// 폰 발 높이보다 이만큼(cm) 넘게 높은 면은 바닥으로 인정하지 않음 (구조물 윗면 제외)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Ground")
	float GroundMaxHeightAbovePawn = 150.0f;

	// ★ 시야 가림 디더 페이드용 MPC. 매 프레임 PlayerLocation(조종 중인 폰 위치)/CameraLocation을 갱신하면,
	// 구조물 머티리얼의 MF_OcclusionFade가 카메라→플레이어 선 근처의 높은 픽셀을 투명하게 디더링합니다.
	// 비워두면 /Game/Effect/Material/Occlusion/MPC_Occlusion을 실행 중에 불러와 씁니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Occlusion")
	TObjectPtr<UMaterialParameterCollection> OcclusionMPC = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Movement")
	UInputAction* RightClickMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Combat")
	UInputAction* BasicAttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	UNiagaraSystem* FXCursor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Skills")
	UInputAction* SkillQ_Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Skills")
	UInputAction* SkillW_Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Skills")
	UInputAction* SkillE_Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Skills")
	UInputAction* SkillR_Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Action")
	UInputAction* DodgeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Action")
	UInputAction* InteractAction;

private:
	void OnMoveStarted();
	void OnMoveTriggered();

	void OnBasicAttack_Started();
	void OnBasicAttack_Triggered();
	void OnBasicAttack_Completed();

	void OnSkillQ_Started();
	void OnSkillQ_Triggered();
	void OnSkillQ_Completed();

	void OnSkillW_Started();
	void OnSkillW_Triggered();
	void OnSkillW_Completed();

	void OnSkillE_Started();
	void OnSkillE_Triggered();
	void OnSkillE_Completed();

	void OnSkillR_Started();
	void OnSkillR_Triggered();
	void OnSkillR_Completed();

	void OnDodge_Started();
	void OnInteract_Started();
};