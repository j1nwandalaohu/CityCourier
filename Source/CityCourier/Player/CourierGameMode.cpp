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
#include "World/RiderStation.h"
#include "Vehicles/RiderRecoveryComponent.h"
#include "Ambient/AmbientTrafficManager.h"
#include "Ambient/CourierAudioComponent.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Buildings/CourierBuildingActor.h"
#if !UE_BUILD_SHIPPING
#include "Validation/PackagedValidationDriver.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

ACourierGameMode::ACourierGameMode()
{
    DefaultPawnClass = ACourierCharacter::StaticClass(); PlayerControllerClass = ACourierController::StaticClass();
    HUDClass = AHUD::StaticClass();
}
void ACourierGameMode::StartPlay()
{
    Super::StartPlay();
    auto* City = LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    for(TActorIterator<ACourierBuildingActor> It(GetWorld());It;++It)
    { GetWorld()->GetSubsystem<UCourierBuildingSubsystem>()->Configure(It->Catalog); break; }
    auto* Traffic=GetWorld()->SpawnActor<AAmbientTrafficManager>();Traffic->Configure(City);
    auto* Audio=NewObject<UCourierAudioComponent>(this);AddInstanceComponent(Audio);Audio->RegisterComponent();
    for (TActorIterator<AScooterPawn> It(GetWorld()); It; ++It)
    {
        GetWorld()->GetSubsystem<UDeliverySubsystem>()->Configure(City,GetGameInstance()->GetSubsystem<UProfileSubsystem>(),*It);
        GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()->Configure(GetWorld()->GetSubsystem<UDeliverySubsystem>());
        for(TActorIterator<ARiderStation> Station(GetWorld());Station;++Station)
        {
            Station->Configure(GetGameInstance()->GetSubsystem<UProfileSubsystem>(),*It);
            if(auto* PC=Cast<ACourierController>(GetWorld()->GetFirstPlayerController()))
                PC->GetRecovery()->Configure(City,*It,PC->GetCourier(),GetWorld()->GetSubsystem<UDeliverySubsystem>(),*Station);
            break;
        }
        break;
    }
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("CourierValidation")))GetWorld()->SpawnActor<APackagedValidationDriver>();
#endif
}
