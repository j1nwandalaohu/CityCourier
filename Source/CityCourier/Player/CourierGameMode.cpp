#include "Player/CourierGameMode.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Vehicles/ScooterPawn.h"
#include "Engine/Canvas.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "World/CityDefinition.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

ACourierGameMode::ACourierGameMode()
{
    DefaultPawnClass = ACourierCharacter::StaticClass(); PlayerControllerClass = ACourierController::StaticClass();
    HUDClass = AHUD::StaticClass();
}
void ACourierGameMode::StartPlay()
{
    Super::StartPlay();
    auto* City = LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    for (TActorIterator<AScooterPawn> It(GetWorld()); It; ++It)
    {
        GetWorld()->GetSubsystem<UDeliverySubsystem>()->Configure(City,GetGameInstance()->GetSubsystem<UProfileSubsystem>(),*It);
        GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()->Configure(GetWorld()->GetSubsystem<UDeliverySubsystem>());
        break;
    }
}
