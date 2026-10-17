#include "Player/CourierController.h"
#include "Player/CourierCharacter.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerInput.h"

void ACourierController::BeginPlay()
{
    Super::BeginPlay();
    Mapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CityCourier/Input/IMC_Courier.IMC_Courier"));
    if (auto* Local = GetLocalPlayer())
        if (auto* Inputs = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            if (Mapping) Inputs->AddMappingContext(Mapping, 0);
    SetControlRotation(FRotator(-12, 0, 0)); SetInterfaceMode(ECourierInterfaceMode::Gameplay);
}
void ACourierController::OnPossess(APawn* NewPawn)
{
    Super::OnPossess(NewPawn);
    if (auto* Rider = Cast<ACourierCharacter>(NewPawn)) Courier = Rider;
}
void ACourierController::SetInterfaceMode(ECourierInterfaceMode Mode)
{
    if (PlayerInput) PlayerInput->FlushPressedKeys();
    InterfaceMode = Mode;
    if (Courier.IsValid()) Courier->SetInputSuppressed(Mode != ECourierInterfaceMode::Gameplay);
    SetPause(Mode == ECourierInterfaceMode::Pause);
    bShowMouseCursor = Mode != ECourierInterfaceMode::Gameplay;
    ResetIgnoreLookInput(); ResetIgnoreMoveInput();
    if (Mode == ECourierInterfaceMode::Gameplay) SetInputMode(FInputModeGameOnly());
    else { SetIgnoreLookInput(true); SetIgnoreMoveInput(true); SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false)); }
}
