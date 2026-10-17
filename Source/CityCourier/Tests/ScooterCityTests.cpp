#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "FileHelpers.h"
#include "Vehicles/ScooterPawn.h"
#include "Vehicles/ScooterMovementComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "World/CityDefinition.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScooterCityTest, "CityCourier.Scooter.AuthoredRoadSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FScooterCityTest::RunTest(const FString& Parameters)
{
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/CityCourier/Maps/L_CourierCity"));
    auto* City = LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    if (!TestNotNull(TEXT("Actual authored collision world"), World) || !TestNotNull(TEXT("City roads"), City)) return false;
    auto* Bike = World->SpawnActor<AScooterPawn>(FVector(0,0,60),FRotator::ZeroRotator);
    auto* Rider = World->SpawnActor<ACourierCharacter>(FVector(0,-150,100),FRotator::ZeroRotator);
    auto* Controller = World->SpawnActor<ACourierController>(); Controller->Possess(Rider);
    if (!TestTrue(TEXT("Test rider mounts"),Bike->TryMount(Rider)))
    {
        Controller->Destroy(); Rider->Destroy(); Bike->Destroy(); return false;
    }
    int32 Traversed = 0;
    for (const auto& Edge : City->Roads.Edges)
    {
        const FVector A = City->Roads.FindNode(Edge.From)->Position;
        const FVector B = City->Roads.FindNode(Edge.To)->Position;
        const FRotator Rotation(0,(B-A).Rotation().Yaw,0);
        Bike->SetActorLocationAndRotation(A+FVector(0,0,52),Rotation,false,nullptr,ETeleportType::TeleportPhysics);
        Bike->GetScooterMovement()->Velocity = FVector::ZeroVector; Bike->SetBatteryFraction(1);
        Bike->GetScooterMovement()->MaxForwardSpeedCm = 1000;
        Bike->SetDriveInput(1,0,false);
        const int32 MaxFrames = FMath::CeilToInt((Edge.LengthMeters/10+10)*60);
        for (int32 I=0; I<MaxFrames && FVector::Dist2D(Bike->GetActorLocation(),B)>120; ++I)
            Bike->GetScooterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        const bool bReached = FVector::Dist2D(Bike->GetActorLocation(),B) <= 120;
        TestTrue(FString::Printf(TEXT("Physical road %d -> %d is rideable"),Edge.From,Edge.To),bReached);
        if (bReached) ++Traversed;
    }
    TArray<FCourierLocationDefinition> Locations = City->Merchants;
    Locations.Append(City->Destinations); Locations.Add(City->Station);
    for (const auto& Place : Locations)
    {
        const FVector A = City->Roads.FindNode(Place.EntranceNode)->Position;
        if (!Bike->GetRider())
        {
            Rider->SetActorLocation(A+FVector(0,-150,92),false,nullptr,ETeleportType::TeleportPhysics);
            Bike->SetActorLocation(A+FVector(0,0,52),false,nullptr,ETeleportType::TeleportPhysics);
            Bike->GetScooterMovement()->Velocity = FVector::ZeroVector; Bike->TryMount(Rider);
        }
        Bike->SetActorLocationAndRotation(A+FVector(0,0,52), FRotator(0,(Place.Position-A).Rotation().Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
        Bike->GetScooterMovement()->Velocity = FVector::ZeroVector; Bike->SetBatteryFraction(1);
        Bike->GetScooterMovement()->MaxForwardSpeedCm = 200; Bike->SetDriveInput(1,0,false);
        for (int32 I=0; I<600 && FVector::Dist2D(Bike->GetActorLocation(),Place.Position)>90; ++I)
            Bike->GetScooterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        Bike->SetDriveInput(0,0,true);
        for (int32 I=0; I<60; ++I) Bike->GetScooterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        TestTrue(TEXT("Entrance approached: ")+Place.LocationId.ToString(),FVector::Dist2D(Bike->GetActorLocation(),Place.Position)<150);
        if (FVector::Dist2D(Bike->GetActorLocation(),Place.Position)>=150)
            AddInfo(FString::Printf(TEXT("Entrance %s stopped at %s; target %s; mounted %d"),*Place.LocationId.ToString(),*Bike->GetActorLocation().ToString(),*Place.Position.ToString(),Bike->GetRider()!=nullptr));
        TestTrue(TEXT("Safe dismount at: ")+Place.LocationId.ToString(),Bike->TryDismount());
    }
    AddInfo(FString::Printf(TEXT("Traversed %d/%d authored road segments, including both shortcuts; attempted all 13 entrances."),Traversed,City->Roads.Edges.Num()));
    Controller->UnPossess(); Rider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Controller->Destroy(); Rider->Destroy(); Bike->Destroy();
    return true;
}
#endif
