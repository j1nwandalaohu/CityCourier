#include "Validation/PackagedValidationDriver.h"
#if !UE_BUILD_SHIPPING
#include "Player/CourierController.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
#include "Vehicles/ScooterMovementComponent.h"
#include "World/RiderStation.h"
#include "World/CityDefinition.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "UI/CourierPhoneWidget.h"
#include "UI/CourierMenuWidget.h"
#include "UI/CourierHistoryWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

void APackagedValidationDriver::RunFeatureChecks(double Now)
{
    if(Now-StartTime>180) { GetWorld()->GetWorldSettings()->SetTimeDilation(1); Finish(TEXT("Feature UI timeout")); return; }
    if(Now-StageTime<(InterfaceCheck==0?8:3)) return; StageTime=Now;
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UCourierPhoneWidget::StaticClass(),false);
    auto* Phone=Widgets.IsEmpty()?nullptr:Cast<UCourierPhoneWidget>(Widgets[0]);
    Widgets.Reset(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UCourierMenuWidget::StaticClass(),false);
    auto* Menu=Widgets.IsEmpty()?nullptr:Cast<UCourierMenuWidget>(Widgets[0]);
    Widgets.Reset(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UCourierHistoryWidget::StaticClass(),false);
    auto* History=Widgets.IsEmpty()?nullptr:Cast<UCourierHistoryWidget>(Widgets[0]);
    if(!Phone || !Menu || !History) { Finish(TEXT("Feature UI widgets missing")); return; }
    auto Call=[&](UObject* Object,const TCHAR* Name)
    { if(auto* Function=Object->FindFunction(Name)) Object->ProcessEvent(Function,nullptr); else Finish(FString(TEXT("Missing action: "))+Name); };
    auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>();
    const FString SavePath=FPaths::ProjectSavedDir()/TEXT("SaveGames/CourierProfile.sav");
    if(Mode==TEXT("LegacyCheck"))
    {
        const auto& P=Profile->GetProfile();
        if(P.Version!=2 || P.BalanceCents!=12345 || P.TotalEarnedCents!=30000 || P.CompletedCount!=15 || P.FailedCount!=3 || P.PowerLevel!=1 || P.HandlingLevel!=2 || P.BatteryLevel!=1 || !FMath::IsNearlyEqual(P.MasterVolume,.3f) || P.QualityLevel!=2 || !P.OrderHistory.IsEmpty())
        { Finish(TEXT("Real-game legacy migration mismatch")); return; }
        if(InterfaceCheck++==0) { PC->OpenOrderHistory(); Phone->RefreshFromState(); Capture(TEXT("legacy-migrated")); return; }
        Finish(); return;
    }
    if(Mode==TEXT("FeatureReload"))
    {
        const auto& P=Profile->GetProfile();
        if(P.Version!=2 || P.OrderHistory.Num()!=20 || P.BalanceCents!=38000 || P.CompletedCount!=19 || P.FailedCount!=1 || Delivery->GetState().IsActive())
        { Finish(TEXT("Feature history reload mismatch")); return; }
        for(const auto& H:P.OrderHistory) if(!FOrderHistoryRules::Validate(H)) { Finish(TEXT("Reloaded history invalid")); return; }
        if(InterfaceCheck++==0) { PC->OpenOrderHistory(); Phone->RefreshFromState(); Capture(TEXT("history-reloaded")); return; }
        Finish(); return;
    }
    switch(InterfaceCheck)
    {
        case 0: PC->SetInterfaceMode(ECourierInterfaceMode::Phone); Phone->RefreshFromState(); Capture(TEXT("feature-phone")); break;
        case 1:
            Call(Phone,TEXT("PreviewFirst")); if(!PC->GetOfferPreview()) { Finish(TEXT("Preview button did not open route: ")+PC->GetActionFeedback().ToString()); return; }
            Capture(TEXT("feature-preview")); break;
        case 2:
            PC->ToggleMap(); if(PC->GetInterfaceMode()!=ECourierInterfaceMode::Phone || PC->GetOfferPreview()) { Finish(TEXT("Preview return failed")); return; }
            Call(Phone,TEXT("NavigateStation")); if(Nav->GetNavigationMode()!=ECourierNavigationMode::Station) { Finish(TEXT("Station navigation button failed")); return; }
            Capture(TEXT("feature-station-hud")); break;
        case 3: PC->ToggleMap(); Capture(TEXT("feature-station-map")); break;
        case 4: PC->TogglePauseMenu(); FeatureWorldTime=GetWorld()->GetTimeSeconds(); break;
        case 5:
            if(!GetWorld()->IsPaused() || GetWorld()->GetTimeSeconds()!=FeatureWorldTime) { Finish(TEXT("Map Escape did not freeze time")); return; }
            PC->TogglePauseMenu(); break;
        case 6:
            // Explicit isolated fault fixtures: this mode is UI evidence, not continuous travel.
            Bike->SetActorLocation(FVector(4000,-12000,52)); Bike->GetScooterMovement()->StopMovementImmediately();
            Rider->SetActorLocation(Station->GetInteractionLocation()+FVector(0,-70,90)); PC->OpenStation(Station);
            Call(Menu,TEXT("Charge")); if(Station->IsCharging()) { Finish(TEXT("Remote charging still allowed")); return; }
            Capture(TEXT("feature-charge-denied")); break;
        case 7:
            PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay); Rider->SetActorLocation(FVector(4000,-12150,90));
            if(!Bike->TryMount(Rider)) { Finish(TEXT("Feedback fixture mount failed")); return; }
            Bike->GetScooterMovement()->Velocity=FVector(400,0,0);
            { const auto R=Bike->TryDismountWithFeedback(); if(R.bSucceeded || R.Reason.IsEmpty()) { Finish(TEXT("Speed rejection missing")); return; } PC->ShowActionFeedback(R.Reason); }
            Capture(TEXT("feature-speed-feedback")); break;
        case 8:
            Bike->GetScooterMovement()->StopMovementImmediately(); if(!Bike->TryDismount()) { Finish(TEXT("Find-bike fixture dismount failed")); return; }
            PC->SetInterfaceMode(ECourierInterfaceMode::Phone); Phone->RefreshFromState(); Call(Phone,TEXT("NavigateBike"));
            if(Nav->GetNavigationMode()!=ECourierNavigationMode::ParkedBike) { Finish(TEXT("Find-bike button failed")); return; }
            PC->ToggleMap(); Capture(TEXT("feature-find-bike")); break;
        case 9:
            PC->SetInterfaceMode(ECourierInterfaceMode::Phone); Delivery->RefreshOffers(Rider->GetActorLocation()); Phone->RefreshFromState(); Call(Phone,TEXT("AcceptFirst"));
            if(!Delivery->GetState().IsActive()) { Finish(TEXT("Real accept button failed")); return; }
            PC->OpenOrderHistory(); Phone->RefreshFromState(); GetWorld()->GetWorldSettings()->SetTimeDilation(20); FeatureWaitStart=Now; break;
        case 10:
            if(Delivery->GetState().IsActive()) { if(Now-FeatureWaitStart>45) Finish(TEXT("History did not keep order clock running")); return; }
            GetWorld()->GetWorldSettings()->SetTimeDilation(1);
            if(PC->GetInterfaceMode()!=ECourierInterfaceMode::Settlement || PC->IsOrderHistoryOpen() || Profile->GetProfile().OrderHistory.Num()!=1 || Profile->GetProfile().OrderHistory[0].FailureCode!=static_cast<uint8>(EDeliveryFailure::Expired))
            { Finish(TEXT("Expiry while history open did not settle exactly once")); return; }
            Capture(TEXT("feature-expired-history")); break;
        case 11:
            // Synthetic entries exercise capacity, long names, scrolling and persistence.
            for(int32 I=0;I<19;++I)
            {
                FDeliveryState S; S.Offer={FGuid::NewGuid(),City->Merchants[0].LocationId,City->Destinations[0].LocationId,2000,120}; S.Status=EDeliveryStatus::Completed; S.ElapsedSeconds=30;
                FSettlement Result{S.Offer.InstanceId,true,2000};
                const FString Name=I==18?FString::ChrN(64,TEXT('店')):City->Merchants[0].DisplayName.ToString();
                Result.History=FOrderHistoryRules::MakeEntry(S,2000,Name,City->Destinations[0].DisplayName.ToString(),FDateTime::UtcNow());
                if(!Profile->ApplySettlement(Result)) { Finish(TEXT("History capacity fixture failed")); return; }
            }
            PC->OpenOrderHistory(); Phone->RefreshFromState(); Capture(TEXT("feature-history-1080")); break;
        case 12: History->OpenEntry(Profile->GetProfile().OrderHistory[0].InstanceId); Capture(TEXT("feature-history-detail-1080")); break;
        case 13:
            History->ResetPage(); Phone->WidgetTree->ForEachWidget([](UWidget* W){if(auto* Scroll=Cast<UScrollBox>(W)) Scroll->ScrollToEnd();}); Capture(TEXT("feature-history-bottom")); break;
        case 14:
            GEngine->GetGameUserSettings()->SetScreenResolution(FIntPoint(1280,720)); GEngine->GetGameUserSettings()->ApplyResolutionSettings(false);
            Phone->WidgetTree->ForEachWidget([](UWidget* W){if(auto* Scroll=Cast<UScrollBox>(W)) Scroll->ScrollToStart();}); break;
        case 15: Capture(TEXT("feature-history-720")); break;
        case 16: History->OpenEntry(Profile->GetProfile().OrderHistory[0].InstanceId); Capture(TEXT("feature-history-detail-720")); break;
        case 17:
            PC->SetInterfaceMode(ECourierInterfaceMode::Pause);
            if(!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SavePath,true)) { Finish(TEXT("Cannot set read-only test save")); return; }
            Profile->UpdateSettings(.45f,1); if(!Profile->HasUnsavedChanges()) { Finish(TEXT("Read-only history save did not fail")); return; }
            Menu->RefreshFromState(); Capture(TEXT("feature-save-failed")); break;
        case 18:
            FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SavePath,false); Call(Menu,TEXT("RetrySave"));
            if(Profile->HasUnsavedChanges() || Profile->GetProfile().OrderHistory.Num()!=20 || Profile->GetProfile().BalanceCents!=38000) { Finish(TEXT("Retry duplicated history or failed")); return; }
            Capture(TEXT("feature-save-retried")); break;
        default:
            FFileHelper::SaveStringToFile(TEXT("{\"history_count\":20,\"balance_cents\":38000,\"synthetic_completed\":19,\"expired\":1,\"preview\":true,\"navigation\":true,\"pause\":true,\"save_retry\":true,\"resolutions\":[\"1920x1080\",\"1280x720\"]}"),*(Output/TEXT("feature-checks.json")));
            Finish(); return;
    }
    ++InterfaceCheck;
}
#endif
