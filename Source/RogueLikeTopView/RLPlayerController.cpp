#include "RLPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"

ARLPlayerController::ARLPlayerController()
{
    bShowMouseCursor = true;
}

void ARLPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void ARLPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EnhancedInputComponent->BindAction(RightClickMoveAction, ETriggerEvent::Started, this, &ARLPlayerController::OnMoveStarted);
        EnhancedInputComponent->BindAction(RightClickMoveAction, ETriggerEvent::Triggered, this, &ARLPlayerController::OnMoveTriggered);
    }
}

void ARLPlayerController::OnMoveStarted()
{
    FHitResult Hit;
    if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
    {
        if (FXCursor)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(),
                FXCursor,
                Hit.Location,
                Hit.ImpactNormal.Rotation()
            );
        }
    }
}

void ARLPlayerController::OnMoveTriggered()
{
    FHitResult Hit;
    if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Hit.Location);
    }
}