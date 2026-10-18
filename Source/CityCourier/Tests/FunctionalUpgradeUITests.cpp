#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "UI/CourierPhoneWidget.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpgradeUIFlowTest,"CityCourier.FunctionalUpgrade.UI.NativePhoneAndModes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUpgradeUIFlowTest::RunTest(const FString&)
{
    FStationFixture F;
    auto* GI=NewObject<UGameInstance>(GEngine); F.World->SetGameInstance(GI);
    FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase")); F.World->SetGameMode(URL);
    auto* Local=NewObject<ULocalPlayer>(GEngine); F.Controller->PlayerState=F.World->SpawnActor<APlayerState>(); F.Controller->SetPlayer(Local);
    F.World->AddController(F.Controller);
    auto* D=F.World->GetSubsystem<UDeliverySubsystem>();
    if(!TestNotNull(TEXT("World delivery subsystem"),D)) return false;
    D->SetClockForTests([&F](){return F.Now;}); TestTrue(TEXT("Configure UI data"),D->Configure(F.City,F.Profile,F.Bike));
    auto* N=F.World->GetSubsystem<UDeliveryNavigationSubsystem>(); N->Configure(D);
    auto* Phone=CreateWidget<UCourierPhoneWidget>(F.Controller,UCourierPhoneWidget::StaticClass());
    if(!TestNotNull(TEXT("Native phone constructed"),Phone)) return false;
    if(!TestTrue(TEXT("Phone has real owning controller"),Phone->GetOwningPlayer()==F.Controller)) return false;
    Phone->TakeWidget(); Phone->RefreshFromState(); F.Controller->SetInterfaceMode(ECourierInterfaceMode::Phone);
    auto* Accept=Phone->FindFunction(TEXT("AcceptFirst")); if(!TestNotNull(TEXT("Real card click handler"),Accept)) return false;
    Phone->ProcessEvent(Accept,nullptr); if(!TestTrue(TEXT("Fresh card actually accepts through widget"),D->GetState().IsActive())) return false;
    D->TryCancel(EDeliveryFailure::Cancelled); Phone->RefreshFromState();
    const auto Old=D->GetOffers()[0]; D->RefreshOffers(F.Rider->GetActorLocation());
    Phone->ProcessEvent(Accept,nullptr);
    TestFalse(TEXT("Stale displayed card cannot accept replacement index"),D->GetState().IsActive());
    if(D->GetState().IsActive()) D->TryCancel(EDeliveryFailure::Cancelled);
    const auto Current=D->GetOffers()[0];
    TestTrue(TEXT("Preview valid candidate"),F.Controller->PreviewOffer(Current.InstanceId));
    TestTrue(TEXT("Map preview mode"),F.Controller->GetOfferPreview()!=nullptr && F.Controller->GetInterfaceMode()==ECourierInterfaceMode::Map);
    const auto Before=F.World->GetTimeSeconds(); F.World->Tick(LEVELTICK_All,.1f); TestTrue(TEXT("Preview keeps time running"),F.World->GetTimeSeconds()>Before);
    F.Controller->ToggleMap(); TestTrue(TEXT("Preview returns to phone"),F.Controller->GetInterfaceMode()==ECourierInterfaceMode::Phone && !F.Controller->GetOfferPreview());
    F.Controller->OpenOrderHistory(); TestTrue(TEXT("History subpage opened"),F.Controller->IsOrderHistoryOpen() && F.Controller->GetInterfaceMode()==ECourierInterfaceMode::Phone);
    F.Controller->TogglePauseMenu(); const auto Paused=F.World->GetTimeSeconds(); F.World->Tick(LEVELTICK_All,.1f);
    TestTrue(TEXT("Esc freezes and clears subpage"),F.World->IsPaused() && F.World->GetTimeSeconds()==Paused && !F.Controller->IsOrderHistoryOpen());
    F.Controller->TogglePauseMenu(); TestFalse(TEXT("Resume no stale preview"),F.Controller->GetOfferPreview()!=nullptr);
    F.Controller->OpenOrderHistory(); const auto HistoryTime=F.World->GetTimeSeconds(); F.World->Tick(LEVELTICK_All,.1f); TestTrue(TEXT("History keeps time running"),F.World->GetTimeSeconds()>HistoryTime);
    F.Controller->CloseOrderHistory(); TestTrue(TEXT("Back stays in phone"),!F.Controller->IsOrderHistoryOpen() && F.Controller->GetInterfaceMode()==ECourierInterfaceMode::Phone);
    return true;
}
#endif
