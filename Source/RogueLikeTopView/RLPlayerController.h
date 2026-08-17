#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RLSkillData.h"
#include "RLPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UNiagaraSystem;

UCLASS()
class ROGUELIKETOPVIEW_API ARLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARLPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

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

	// ★ 추가됨: 블루프린트에서 스페이스바(회피) 액션을 넣을 칸
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Action")
	UInputAction* DodgeAction;

private:
	// 마우스 이동 & 기본 공격 핸들러
	void OnMoveStarted();
	void OnMoveTriggered();
	
	void OnBasicAttack_Started();
	void OnBasicAttack_Triggered();
	void OnBasicAttack_Completed();

	// Q 스킬 (Started, Triggered, Completed)
	void OnSkillQ_Started();
	void OnSkillQ_Triggered();
	void OnSkillQ_Completed();

	// W 스킬
	void OnSkillW_Started();
	void OnSkillW_Triggered();
	void OnSkillW_Completed();

	// E 스킬
	void OnSkillE_Started();
	void OnSkillE_Triggered();
	void OnSkillE_Completed();

	// R 스킬
	void OnSkillR_Started();
	void OnSkillR_Triggered();
	void OnSkillR_Completed();

	// ★ 추가됨: 스페이스바 눌렀을 때 실행될 함수 선언
	void OnDodge_Started();
};