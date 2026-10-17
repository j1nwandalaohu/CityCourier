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
#include "Tests/ProfileTestStorage.h"
#include "Profile/ProfileSubsystem.h"
#include "Delivery/DeliverySubsystem.h"
#include "World/CityDefinition.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierEscapePauseTest,"CityCourier.InputModes.EscapeFromOpenInterfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierEscapePauseTest::RunTest(const FString& Parameters)
{
    FTestCourierWorld F; F.Box(FVector(0,0,-50),FVector(100000,100000,50));
    auto* GI=NewObject<UGameInstance>(); F.World->SetGameInstance(GI);
    FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase")); F.World->SetGameMode(URL);
    auto* Rider=F.World->SpawnActor<ACourierCharacter>(FVector(0,-150,90),FRotator::ZeroRotator);
    auto* Bike=F.World->SpawnActor<AScooterPawn>(FVector(0,0,52),FRotator::ZeroRotator);
    auto* PC=F.World->SpawnActor<ACourierController>(); PC->PlayerState=F.World->SpawnActor<APlayerState>();
    PC->Possess(Rider); TestTrue(TEXT("Mount test bike"),Bike->TryMount(Rider));
    auto* Profile=NewObject<UProfileSubsystem>(GI);
    Profile->SetStorageForTests(MakeShared<FTestProfileStorage>()); Profile->LoadProfile();
    auto* City=LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    auto* Delivery=NewObject<UDeliverySubsystem>(F.World);
    if(!TestTrue(TEXT("Configure real order clock"),Delivery->Configure(City,Profile,Bike))) return false;
    TestTrue(TEXT("Accept order"),Delivery->TryAccept(Delivery->GetOffers()[0].InstanceId));
    for(const auto Mode : {ECourierInterfaceMode::Phone,ECourierInterfaceMode::Map,ECourierInterfaceMode::Station,ECourierInterfaceMode::Settlement})
    {
        const FString Label=FString::Printf(TEXT("Interface %d: "),static_cast<int32>(Mode));
        Bike->SetDriveInput(1,1,false); PC->SetInterfaceMode(Mode);
        const double BeforeOpenTime=F.World->GetTimeSeconds(); F.World->Tick(LEVELTICK_All,.1f); Delivery->Tick(.1f);
        TestTrue(Label+TEXT("open interface keeps time running"),F.World->GetTimeSeconds()>BeforeOpenTime);
        // The same handler is called by both Enhanced Input and Slate's Escape key.
        PC->TogglePauseMenu();
        TestTrue(Label+TEXT("Escape opens Pause"),PC->GetInterfaceMode()==ECourierInterfaceMode::Pause);
        const double WorldTime=F.World->GetTimeSeconds(),OrderTime=Delivery->GetState().ElapsedSeconds;
        F.World->Tick(LEVELTICK_All,.1f); Delivery->Tick(.1f);
        TestTrue(Label+TEXT("Escape freezes world"),F.World->IsPaused() && F.World->GetTimeSeconds()==WorldTime);
        TestEqual(Label+TEXT("Escape freezes active order"),Delivery->GetState().ElapsedSeconds,OrderTime);
        PC->TogglePauseMenu();
        TestTrue(Label+TEXT("second Escape resumes"),PC->GetInterfaceMode()==ECourierInterfaceMode::Gameplay && !F.World->IsPaused());
        for(int32 I=0;I<120;++I) Bike->GetScooterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        TestTrue(Label+TEXT("resume has no held throttle"),Bike->GetSpeedKmh()<.01f);
        F.World->Tick(LEVELTICK_All,.1f); Delivery->Tick(.1f);
        TestTrue(Label+TEXT("order resumes"),Delivery->GetState().ElapsedSeconds>OrderTime);
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
    }
    return true;
}
#endif
