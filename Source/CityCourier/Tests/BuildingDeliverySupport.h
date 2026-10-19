#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/StationTestSupport.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Buildings/CourierBuildingActor.h"
#include "GameFramework/CharacterMovementComponent.h"
struct FBuildingDeliveryFixture:FStationFixture
{
    UCityBuildingCatalog* Catalog;
    UCourierBuildingSubsystem* Interiors;
    TMap<FName,ACourierBuildingActor*> Buildings;
    FBuildingDeliveryFixture(bool Prepare=true)
    {
        World->AddController(Controller);
        Station->LocationId=City->Station.LocationId;
        auto* Source=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
        Catalog=NewObject<UCityBuildingCatalog>();
        for(FName Id:{City->Station.LocationId,City->Merchants[0].LocationId,City->Destinations[0].LocationId})
        {
            const auto* Binding=Source->FindBinding(Id); Catalog->Bindings.Add(*Binding);
            if(Catalog->FindBuilding(Binding->BuildingId)==INDEX_NONE)
            { Catalog->Buildings.Add(Source->Buildings[Source->FindBuilding(Binding->BuildingId)]); Catalog->ExpectedBuildingIds.Add(Binding->BuildingId); }
        }
        for(int32 I=0;I<Catalog->Buildings.Num();++I)
        {
            const auto& B=Catalog->Buildings[I]; auto* A=World->SpawnActor<ACourierBuildingActor>(B.Origin,FRotator(0,B.Yaw,0));
            check(A->ConfigureBuilding(Catalog,I)); Buildings.Add(B.Id,A);
        }
        Interiors=World->GetSubsystem<UCourierBuildingSubsystem>(); check(Interiors->Configure(Catalog));
        const auto& StationB=Catalog->Buildings[Catalog->FindBuilding(TEXT("B_Station"))];
        Bike->SetActorLocation(StationB.ToWorld(StationB.Nodes[StationB.EntranceNode].Position)+FVector(300,-100,52));
        Rider->SetActorLocation(Bike->GetActorLocation()+FVector(0,150,38)); Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Delivery=World->GetSubsystem<UDeliverySubsystem>(); Delivery->SetClockForTests([this](){return Now;}); Delivery->Configure(City,Profile,Bike);
        if(Prepare) for(int32 I=0;I<Catalog->Buildings.Num();++I)
        { Interiors->RequestBuilding(I); for(int32 J=0;J<Catalog->Buildings.Num();++J) Interiors->PrepareNext(); }
    }
    ADeliveryLocation* Merchant() const { return Interiors->GetManagedLocation(City->Merchants[0].LocationId); }
    ADeliveryLocation* Customer() const { return Interiors->GetManagedLocation(City->Destinations[0].LocationId); }
    void At(ADeliveryLocation* L)
    {
        Rider->SetActorLocation(L->GetActorLocation()+FVector(0,0,90)); Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Interiors->UpdateResidency(Rider->GetActorLocation(),L->BuildingId); for(int32 I=0;I<Catalog->Buildings.Num();++I) Interiors->PrepareNext();
    }
};
#endif
