#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RLSkillData.h"
#include "RLPlayerController.generated.h"

// 전방 선언
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* RightClickMoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* TestAttackAction;

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

private:
    void OnMoveStarted();
    void OnMoveTriggered();
    void OnTestAttack();
    void OnCastSkillQ();
    void OnCastSkillW();
    void OnCastSkillE();
    void OnCastSkillR();
};