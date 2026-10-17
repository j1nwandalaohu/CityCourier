#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/BoxComponent.h"

struct FTestCourierWorld
{
    UWorld* World;
    FTestCourierWorld()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    }
    ~FTestCourierWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
    AActor* Box(FVector Center, FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        auto* Shape = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Shape); Shape->SetBoxExtent(Extent);
        Shape->SetCollisionProfileName(TEXT("BlockAll")); Shape->RegisterComponent();
        Actor->SetActorLocation(Center); return Actor;
    }
};
#endif
