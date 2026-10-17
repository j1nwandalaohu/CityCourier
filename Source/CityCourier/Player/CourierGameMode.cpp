#include "Player/CourierGameMode.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Vehicles/ScooterPawn.h"
#include "Engine/Canvas.h"
#include "Delivery/DeliverySubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "World/CityDefinition.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

ACourierGameMode::ACourierGameMode()
{
    DefaultPawnClass = ACourierCharacter::StaticClass(); PlayerControllerClass = ACourierController::StaticClass();
    HUDClass = ACourierPrototypeHUD::StaticClass();
}
void ACourierGameMode::StartPlay()
{
    Super::StartPlay();
    auto* City = LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    for (TActorIterator<AScooterPawn> It(GetWorld()); It; ++It)
    {
        GetWorld()->GetSubsystem<UDeliverySubsystem>()->Configure(City,GetGameInstance()->GetSubsystem<UProfileSubsystem>(),*It);
        break;
    }
}
void ACourierPrototypeHUD::DrawHUD()
{
    Super::DrawHUD();
    DrawText(TEXT("PROTOTYPE | WASD Move   Shift Run   Mouse Look   E Interact"), FLinearColor::White, 25, 25, nullptr, 1.2f);
#if !UE_BUILD_SHIPPING
    if (const auto* D = GetWorld()->GetSubsystem<UDeliverySubsystem>())
    {
        const auto& S = D->GetState();
        const auto& P = GetGameInstance()->GetSubsystem<UProfileSubsystem>()->GetProfile();
        DrawRect(FLinearColor(0,0,0,.8f),20,90,670,170);
        DrawText(FString::Printf(TEXT("DEV ORDERS | Balance %.2f | Completed %d Failed %d | F4 Cancel"),P.BalanceCents/100.0,P.CompletedCount,P.FailedCount),FLinearColor::White,30,100);
        if (S.IsActive())
        {
            DrawText(FString::Printf(TEXT("%s -> %s | %s | %.1f / %.0f s | Food %d%%"),*S.Offer.MerchantId.ToString(),*S.Offer.DestinationId.ToString(),S.Status==EDeliveryStatus::AwaitingPickup?TEXT("PICKUP"):TEXT("DELIVER"),S.ElapsedSeconds,S.Offer.DeadlineSeconds,S.IntegrityPercent),FLinearColor::Yellow,30,130);
        }
        else
        {
            int32 I=0; for (const auto& O : D->GetOffers())
            { DrawText(FString::Printf(TEXT("F%d  %s -> %s   %.2f   %.0f seconds"),I+1,*O.MerchantId.ToString(),*O.DestinationId.ToString(),O.BaseFeeCents/100.0,O.DeadlineSeconds),FLinearColor::White,30,130+25*I); ++I; }
            DrawText(FString::Printf(TEXT("Last settlement: %s  %.2f"),D->GetLastSettlement().bCompleted?TEXT("COMPLETED"):TEXT("FAILED / NONE"),D->GetLastSettlement().PayoutCents/100.0),FLinearColor::Yellow,30,230);
        }
    }
#endif
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
