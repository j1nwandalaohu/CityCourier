#include "Player/CourierGameMode.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Vehicles/ScooterPawn.h"
#include "Engine/Canvas.h"

ACourierGameMode::ACourierGameMode()
{
    DefaultPawnClass = ACourierCharacter::StaticClass(); PlayerControllerClass = ACourierController::StaticClass();
    HUDClass = ACourierPrototypeHUD::StaticClass();
}
void ACourierPrototypeHUD::DrawHUD()
{
    Super::DrawHUD();
    DrawText(TEXT("PROTOTYPE | WASD Move   Shift Run   Mouse Look   E Interact"), FLinearColor::White, 25, 25, nullptr, 1.2f);
    if (PlayerOwner)
        if (const auto* Scooter = Cast<AScooterPawn>(PlayerOwner->GetPawn()))
            DrawText(FString::Printf(TEXT("%.1f km/h   Battery %.1f%%   Space Brake   E Dismount"),
                Scooter->GetSpeedKmh(), Scooter->GetBatteryFraction()*100), FLinearColor::White,25,55,nullptr,1.2f);
    if (const auto* Controller = Cast<ACourierController>(PlayerOwner))
        if (const auto* Character = Controller->GetCourier())
            if (!Character->GetInteractionPrompt().IsEmpty())
            {
                DrawRect(FLinearColor(0,0,0,.75), Canvas->SizeX * .5f - 100, Canvas->SizeY * .75f, 200, 40);
                DrawText(Character->GetInteractionPrompt().ToString(), FLinearColor::White, Canvas->SizeX * .5f - 80, Canvas->SizeY * .75f + 10);
            }
}
