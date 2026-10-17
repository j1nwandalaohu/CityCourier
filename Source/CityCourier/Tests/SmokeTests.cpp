#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierStartupMapTest,
    "CityCourier.Smoke.StartupMap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCourierStartupMapTest::RunTest(const FString& Parameters)
{
    // Catches an omitted/corrupt startup map, or a bootstrap that saves an
    // empty level instead of a usable initial scene. Does not assert visuals.
    UWorld* World = LoadObject<UWorld>(nullptr,
        TEXT("/Game/CityCourier/Maps/L_CourierCity.L_CourierCity"));
    if (!TestNotNull(TEXT("Startup map can be loaded"), World))
    {
        return false;
    }
    if (!TestNotNull(TEXT("Startup map contains a persistent level"), World->PersistentLevel))
    {
        return false;
    }

    int32 PlayerStarts = 0;
    int32 GroundActors = 0;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!IsValid(Actor)) { continue; }
        if (Actor->IsA<APlayerStart>()) { ++PlayerStarts; }
        if (Actor->IsA<AStaticMeshActor>() && Actor->ActorHasTag(TEXT("CC_BootstrapGround")))
        {
            ++GroundActors;
        }
    }
    TestTrue(TEXT("Startup map has a player spawn"), PlayerStarts > 0);
    TestEqual(TEXT("Bootstrap ground exists exactly once"), GroundActors, 1);
    return true;
}

#endif
