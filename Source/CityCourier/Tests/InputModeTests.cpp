#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Vehicles/ScooterPawn.h"
#include "Vehicles/ScooterMovementComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierInputModeTest,"CityCourier.InputModes.DrivePhoneMapPauseResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierInputModeTest::RunTest(const FString& Parameters)
{
    FTestCourierWorld F; F.Box(FVector(0,0,-50),FVector(100000,100000,50));
    auto* GI=NewObject<UGameInstance>(); F.World->SetGameInstance(GI);
    FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase")); F.World->SetGameMode(URL);
    auto* Rider=F.World->SpawnActor<ACourierCharacter>(FVector(0,-150,90),FRotator::ZeroRotator);
    auto* Bike=F.World->SpawnActor<AScooterPawn>(FVector(0,0,52),FRotator::ZeroRotator);
    auto* PC=F.World->SpawnActor<ACourierController>(); PC->PlayerState=F.World->SpawnActor<APlayerState>(); PC->Possess(Rider); Bike->TryMount(Rider);
    auto Step=[&]() { for(int I=0;I<120;++I) Bike->GetScooterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr); };
    Bike->SetDriveInput(1,0,false); Step(); TestTrue(TEXT("Driving starts"),Bike->GetSpeedKmh()>10);
    PC->SetInterfaceMode(ECourierInterfaceMode::Phone); Step(); TestTrue(TEXT("Phone brakes"),Bike->GetSpeedKmh()<.01);
    Bike->SetDriveInput(1,0,false); Step(); TestTrue(TEXT("Background drive input blocked"),Bike->GetSpeedKmh()<.01);
    const double PhoneTime=F.World->GetTimeSeconds(); F.World->Tick(LEVELTICK_All,.1f);
    TestTrue(TEXT("Phone world time advances"),F.World->GetTimeSeconds()>PhoneTime);
    PC->SetInterfaceMode(ECourierInterfaceMode::Map); const double MapTime=F.World->GetTimeSeconds(); F.World->Tick(LEVELTICK_All,.1f);
    TestTrue(TEXT("Map world time advances"),F.World->GetTimeSeconds()>MapTime);
    PC->SetInterfaceMode(ECourierInterfaceMode::Pause); const double PauseTime=F.World->GetTimeSeconds(); F.World->Tick(LEVELTICK_All,.1f);
    TestTrue(TEXT("Pause freezes world time"),F.World->IsPaused() && F.World->GetTimeSeconds()==PauseTime);
    PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay); Step();
    TestFalse(TEXT("Resume unpauses"),F.World->IsPaused()); TestTrue(TEXT("Resume has no residual throttle"),Bike->GetSpeedKmh()<.01);
    Bike->SetDriveInput(1,0,false); Step(); TestTrue(TEXT("New input accelerates"),Bike->GetSpeedKmh()>10);
    return true;
}
#endif
