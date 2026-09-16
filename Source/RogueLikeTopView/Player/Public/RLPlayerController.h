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