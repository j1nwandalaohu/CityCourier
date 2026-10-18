#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/WorldTestSupport.h"
#include "Tests/ProfileTestStorage.h"
#include "Tests/HistoryTestSupport.h"
#include "World/RiderStation.h"
#include "Vehicles/RiderRecoveryComponent.h"
#include "Vehicles/ScooterPawn.h"
#include "Vehicles/ScooterMovementComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "Engine/GameInstance.h"
struct FStationFixture : FTestCourierWorld
{
    UCityDefinition* City;
    UProfileSubsystem* Profile;
    UDeliverySubsystem* Delivery;
    ARiderStation* Station;
    ACourierCharacter* Rider;
    ACourierController* Controller;
    AScooterPawn* Bike;
    URiderRecoveryComponent* Recovery;
    TSharedRef<FTestProfileStorage> Storage=MakeShared<FTestProfileStorage>();
    double Now=100;
    FStationFixture()
    {
        Box(FVector(0,0,-50),FVector(40000,40000,50));
        City=LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
        Profile=NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); Profile->SetStorageForTests(Storage); Profile->LoadProfile();
        Station=World->SpawnActor<ARiderStation>(City->Station.Position,FRotator::ZeroRotator); Station->CityDefinition=City;
        Rider=World->SpawnActor<ACourierCharacter>(City->Station.Position+FVector(0,0,90),FRotator::ZeroRotator);
        Controller=World->SpawnActor<ACourierController>(); Controller->Possess(Rider);
        Bike=World->SpawnActor<AScooterPawn>(City->Station.Position+FVector(150,-200,52),FRotator::ZeroRotator);
        Station->Configure(Profile,Bike);
        Delivery=NewObject<UDeliverySubsystem>(World); Delivery->SetClockForTests([this](){return Now;}); Delivery->Configure(City,Profile,Bike);
        Recovery=NewObject<URiderRecoveryComponent>(Controller); Recovery->RegisterComponent(); Recovery->Configure(City,Bike,Rider,Delivery,Station);
    }
    void Hold(float Seconds) { Recovery->SetRecoverHeld(true); Recovery->TickComponent(Seconds,LEVELTICK_All,nullptr); }
    void Credit(int32 Count=20) { for(int32 I=0;I<Count;++I) Profile->ApplySettlement(MakeTestSettlement(FGuid::NewGuid(),true,3200)); }
};
#endif
