#include "Player/CourierController.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerInput.h"
#include "Delivery/DeliverySubsystem.h"

void ACourierController::SetupInputComponent()
{
    Super::SetupInputComponent();
#if !UE_BUILD_SHIPPING
    InputComponent->BindKey(EKeys::F1,IE_Pressed,this,&ACourierController::DevFirst);
    InputComponent->BindKey(EKeys::F2,IE_Pressed,this,&ACourierController::DevSecond);
    InputComponent->BindKey(EKeys::F3,IE_Pressed,this,&ACourierController::DevThird);
    InputComponent->BindKey(EKeys::F4,IE_Pressed,this,&ACourierController::DevCancel);
#endif
}
bool ACourierController::CourierOrder(int32 Number)
{
    auto* D = GetWorld()->GetSubsystem<UDeliverySubsystem>();
    return D && D->GetOffers().IsValidIndex(Number-1) && D->TryAccept(D->GetOffers()[Number-1].InstanceId);
}
UDeliverySubsystem* ACourierController::GetDeliverySystem() const
{ return GetWorld()->GetSubsystem<UDeliverySubsystem>(); }
void ACourierController::DevCancel()
{
    if (auto* D = GetWorld()->GetSubsystem<UDeliverySubsystem>()) D->TryCancel(EDeliveryFailure::Cancelled);
}

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
    if (auto* Scooter = Cast<AScooterPawn>(GetPawn())) Scooter->SetDriveInput(0,0,true);
    if (Courier.IsValid()) Courier->SetInputSuppressed(Mode != ECourierInterfaceMode::Gameplay);
    SetPause(Mode == ECourierInterfaceMode::Pause);
    bShowMouseCursor = Mode != ECourierInterfaceMode::Gameplay;
    ResetIgnoreLookInput(); ResetIgnoreMoveInput();
    if (Mode == ECourierInterfaceMode::Gameplay) SetInputMode(FInputModeGameOnly());
    else { SetIgnoreLookInput(true); SetIgnoreMoveInput(true); SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false)); }
}
