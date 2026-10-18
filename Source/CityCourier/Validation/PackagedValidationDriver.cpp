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
#include "UI/CourierSettings.h"
#include "Ambient/AmbientVehicle.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "UnrealClient.h"
#include "EngineGlobals.h"
#include "RenderTimer.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/CourierMenuWidget.h"
#include "UI/CourierPhoneWidget.h"
#include "Components/Slider.h"
#include "Vehicles/RiderRecoveryComponent.h"
#include "UObject/UnrealType.h"
#include "AudioMixerBlueprintLibrary.h"
#endif

APackagedValidationDriver::APackagedValidationDriver()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void APackagedValidationDriver::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    StartTime=PreviousTime=StageTime=FPlatformTime::Seconds();
    Output=FPaths::ProjectSavedDir()/TEXT("Validation/Packaged");
    FParse::Value(FCommandLine::Get(),TEXT("CourierValidationOutput="),Output);
    FParse::Value(FCommandLine::Get(),TEXT("CourierValidationSeconds="),MinimumSeconds);
    FParse::Value(FCommandLine::Get(),TEXT("CourierValidationMode="),Mode);
    if(Mode==TEXT("Checks") || Mode==TEXT("FeatureUI") || Mode==TEXT("FutureEnvelopeCheck") || Mode==TEXT("CharacterActions"))PrimaryActorTick.bTickEvenWhenPaused=true;
    IFileManager::Get().MakeDirectory(*Output,true);
    PC=Cast<ACourierController>(GetWorld()->GetFirstPlayerController());
    Rider=PC?PC->GetCourier():nullptr;
    Delivery=GetWorld()->GetSubsystem<UDeliverySubsystem>();Profile=PC?PC->GetProfileSystem():nullptr;
    City=Delivery?Delivery->GetCity():nullptr;
    for(TActorIterator<AScooterPawn> It(GetWorld());It;++It){Bike=*It;break;}
    for(TActorIterator<ARiderStation> It(GetWorld());It;++It){Station=*It;break;}
    if(!PC || !Rider || !Bike || !Station || !City || !Profile){Finish(TEXT("Validation startup dependencies missing"));return;}
    InitialCompleted=Profile->GetProfile().CompletedCount;InitialFailed=Profile->GetProfile().FailedCount;
    LastPosition=Bike->GetActorLocation();
    FPlayerProfile Settings=Profile->GetProfile();Settings.QualityLevel=1;UCourierSettings::Apply(Settings);
    GEngine->GetGameUserSettings()->SetScreenResolution(FIntPoint(1920,1080));
    GEngine->GetGameUserSettings()->SetFullscreenMode(EWindowMode::Windowed);
    GEngine->GetGameUserSettings()->SetVSyncEnabled(false);GEngine->GetGameUserSettings()->SetFrameRateLimit(0);
    GEngine->GetGameUserSettings()->ApplyResolutionSettings(false);
    PC->ConsoleCommand(TEXT("t.MaxFPS 0"));PC->ConsoleCommand(TEXT("r.VSync 0"));
    Frames=TEXT("CaptureSource,ElapsedSeconds,FrameMs,GameThreadMs,RenderThreadMs,GpuMs,WorkingSetMB,TextureMemoryMB,Phase\n");
    Events=TEXT("ElapsedSeconds,Event,Merchant,Destination,PayoutCents,BalanceCents\n");
    if(Mode==TEXT("Reload"))Stage=EStage::Reload;
    WriteStatus(TEXT("progress.json"));
#else
    SetActorTickEnabled(false);
#endif
}
#if !UE_BUILD_SHIPPING
void APackagedValidationDriver::Capture(const FString& Name)
{FScreenshotRequest::RequestScreenshot(Output/(Name+TEXT(".png")),true,false);}
void APackagedValidationDriver::WriteStatus(const FString& Filename,const FString& Error)
{
    const auto P=Profile?Profile->GetProfile():FPlayerProfile();
    FVector Position=Bike?Bike->GetActorLocation():FVector::ZeroVector;
    FString SafeError=Error.Replace(TEXT("\\"),TEXT("/")).Replace(TEXT("\""),TEXT("'"));
    const FString Text=FString::Printf(TEXT("{\"elapsed_seconds\":%.3f,\"stage\":%d,\"order_index\":%d,\"point_index\":%d,\"target\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"speed_kmh\":%.2f,\"battery\":%.5f,\"distance_m\":%.2f,\"completed\":%d,\"failed\":%d,\"balance_cents\":%lld,\"power\":%d,\"handling\":%d,\"battery_level\":%d,\"active_order\":%s,\"error\":\"%s\"}"),
        FPlatformTime::Seconds()-StartTime,(int32)Stage,OrderIndex,PointIndex,*Target.ToString(),Position.X,Position.Y,Bike?Bike->GetSpeedKmh():0,Bike?Bike->GetBatteryFraction():0,DistanceMeters,P.CompletedCount,P.FailedCount,P.BalanceCents,P.PowerLevel,P.HandlingLevel,P.BatteryLevel,Delivery&&Delivery->GetState().IsActive()?TEXT("true"):TEXT("false"),*SafeError);
    FFileHelper::SaveStringToFile(Text,*(Output/Filename));
}
void APackagedValidationDriver::Finish(const FString& Error)
{
    if(bFinished)return;bFinished=true;
    if(Mode==TEXT("Checks") || Mode==TEXT("FeatureUI"))FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*(FPaths::ProjectSavedDir()/TEXT("SaveGames/CourierProfile.sav")),false);
    if(Bike)Bike->SetDriveInput(0,0,true);
    FFileHelper::SaveStringToFile(Frames,*(Output/(Mode==TEXT("Reload")?TEXT("reload-frames.csv"):TEXT("frames.csv"))));
    FFileHelper::SaveStringToFile(Events,*(Output/(Mode==TEXT("Reload")?TEXT("reload-events.csv"):TEXT("events.csv"))));
    WriteStatus(Mode==TEXT("Reload")?TEXT("reload.json"):TEXT("result.json"),Error);
    UE_LOG(LogTemp,Display,TEXT("Courier packaged validation finished: %s"),Error.IsEmpty()?TEXT("PASS"):*Error);
    FPlatformMisc::RequestExit(false);
}
bool APackagedValidationDriver::WalkTo(FVector Destination)
{
    FVector Delta=Destination-Rider->GetActorLocation();Delta.Z=0;
    if(Delta.Size2D()<110){Rider->GetCharacterMovement()->StopMovementImmediately();return true;}
    Rider->AddMovementInput(Delta.GetSafeNormal(),1,true);
    PC->SetControlRotation(FRotator(-12,Delta.Rotation().Yaw,0));return false;
}
void APackagedValidationDriver::BeginTrip(FName Location)
{
    Target=Location;auto Route=City->FindRouteFrom(Bike->GetActorLocation(),Location);
    if(!Route.bReachable){Finish(TEXT("No route to ")+Location.ToString());return;}
    Points=Route.Points;Points.Pop(); // Park on the road, then physically walk to the entrance.
    PointIndex=0;Stage=EStage::Drive;StageTime=FPlatformTime::Seconds();
}
bool APackagedValidationDriver::FollowRoad(float DeltaSeconds)
{
    if(Points.IsEmpty())return true;
    const FVector Position=Bike->GetActorLocation();
    while(PointIndex<Points.Num()-1 && FVector::Dist2D(Position,Points[PointIndex])<280)++PointIndex;
    const bool bLast=PointIndex==Points.Num()-1;
    FVector Aim=Points[PointIndex];float Distance=FVector::Dist2D(Position,Aim);
    const float Speed=FMath::Abs(Bike->GetSpeedKmh())/.036f;
    if(bLast && Distance<240)
    {Bike->SetDriveInput(0,0,true);return Speed<10;}
    FVector Direction=(Aim-Position).GetSafeNormal2D();
    float Angle=FMath::FindDeltaAngleDegrees(Bike->GetActorRotation().Yaw,Direction.Rotation().Yaw);
    float DesiredSpeed=1000;
    const bool bCorner=bLast || FVector::DotProduct(Direction,(Points[PointIndex+1]-Points[PointIndex]).GetSafeNormal2D())<.95;
    if(bCorner && Distance<1500)DesiredSpeed=FMath::Lerp(230.f,1000.f,FMath::Clamp((Distance-250)/1250,0.f,1.f));
    if(FMath::Abs(Angle)>25)DesiredSpeed=FMath::Min(DesiredSpeed,250.f);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ValidationTraffic),false,Bike);Query.AddIgnoredActor(Rider);
    FHitResult Hit;
    if(GetWorld()->SweepSingleByChannel(Hit,Position,Position+Bike->GetActorForwardVector()*1000,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(48),Query))
    {
        if(Hit.GetActor() && Hit.GetActor()->IsA<AAmbientVehicle>())
        {
            const FVector Right(-Direction.Y,Direction.X,0);
            const float Side=FVector::DotProduct(Hit.GetActor()->GetActorLocation()-Position,Right)>0?-1.f:1.f;
            Aim=Position+Direction*1200+Right*450*Side;
            Angle=FMath::FindDeltaAngleDegrees(Bike->GetActorRotation().Yaw,(Aim-Position).Rotation().Yaw);
            DesiredSpeed=FMath::Min(DesiredSpeed,350.f);
        }
        else DesiredSpeed=FMath::Min(DesiredSpeed,180.f);
    }
    Bike->SetDriveInput(Speed<DesiredSpeed-12?1.f:0.f,FMath::Clamp(Angle/35.f,-1.f,1.f),Speed>DesiredSpeed+30);
    return false;
}
void APackagedValidationDriver::NextOrder()
{
    if(Mode==TEXT("FeatureRoute") && OrderIndex>=1)
    {
        const auto& P=Profile->GetProfile();
        if(P.OrderHistory.Num()!=1 || !P.OrderHistory[0].bCompleted || P.BalanceCents!=P.OrderHistory[0].PayoutCents || !Profile->SaveProfile())
        { Finish(TEXT("Continuous delivery history/income mismatch")); return; }
        PC->OpenOrderHistory(); Capture(TEXT("route-history")); Stage=EStage::FinishWait; StageTime=FPlatformTime::Seconds(); return;
    }
    if(OrderIndex>=8)
    {
        if(FPlatformTime::Seconds()-StartTime>=MinimumSeconds){bFinalizeAtStation=true;BeginTrip(City->Station.LocationId);return;}
        Target=City->Destinations[(int32)((FPlatformTime::Seconds()-StartTime)/60)%8].LocationId;
        if(Bike->GetBatteryFraction()<.4f)Target=City->Station.LocationId;
        BeginTrip(Target);return;
    }
    const auto& Merchant=City->Merchants[OrderIndex%4];const auto& Destination=City->Destinations[OrderIndex];
    const double Distance=City->FindRouteFrom(Bike->GetActorLocation(),Merchant.LocationId).LengthMeters+
        City->FindRouteBetween(Merchant,Destination).LengthMeters+City->FindRouteBetween(Destination,City->Station).LengthMeters+150;
    const double Remaining=Bike->GetBatteryFraction()*(3000+Profile->GetProfile().BatteryLevel*1000);
    if(Remaining<Distance){BeginTrip(City->Station.LocationId);return;}
    for(int32 Attempt=0;Attempt<256;++Attempt)
    {
        Delivery->RefreshOffers(Bike->GetActorLocation());
        for(const auto Offer:Delivery->GetOffers())if(Offer.MerchantId==Merchant.LocationId && Offer.DestinationId==Destination.LocationId)
        {
            if(Mode==TEXT("FeatureRoute"))
            {
                if(!PC->PreviewOffer(Offer.InstanceId)) { Finish(TEXT("Continuous route preview rejected")); return; }
                Capture(TEXT("route-preview")); Stage=EStage::Preview; StageTime=FPlatformTime::Seconds(); return;
            }
            if(!Delivery->TryAccept(Offer.InstanceId)){Finish(TEXT("Could not accept fixture offer"));return;}
            Events+=FString::Printf(TEXT("%.3f,accepted,%s,%s,0,%lld\n"),FPlatformTime::Seconds()-StartTime,*Offer.MerchantId.ToString(),*Offer.DestinationId.ToString(),Profile->GetProfile().BalanceCents);
            BeginTrip(Merchant.LocationId);return;
        }
    }
    Finish(TEXT("Could not select required merchant/destination pair"));
}
void APackagedValidationDriver::RunInterfaceChecks(double Now)
{
    if(Now-StageTime<(InterfaceCheck==0?8:3))return;StageTime=Now;
    TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UCourierMenuWidget::StaticClass(),false);
    auto* Menu=Widgets.IsEmpty()?nullptr:Cast<UCourierMenuWidget>(Widgets[0]);
    if(!Menu){Finish(TEXT("Packaged menu widget missing"));return;}
    auto Call=[&](UObject* Object,const TCHAR* Name){Object->ProcessEvent(Object->FindFunctionChecked(FName(Name)),nullptr);};
    const FString SavePath=FPaths::ProjectSavedDir()/TEXT("SaveGames/CourierProfile.sav");
    switch(InterfaceCheck)
    {
        case 0:UAudioMixerBlueprintLibrary::StartRecordingOutput(this,75);PC->TogglePhone();Capture(TEXT("ui-phone"));break;
        case 1:PC->ToggleMap();Capture(TEXT("ui-map"));break;
        case 2:PC->SetInterfaceMode(ECourierInterfaceMode::Pause);Capture(TEXT("ui-pause"));break;
        case 3:Call(Menu,TEXT("QualityLow"));Capture(TEXT("ui-low"));break;
        case 4:Call(Menu,TEXT("QualityMedium"));Capture(TEXT("ui-medium"));break;
        case 5:Call(Menu,TEXT("QualityHigh"));Capture(TEXT("ui-high"));break;
        case 6:
        {
            auto* Property=FindFProperty<FObjectProperty>(Menu->GetClass(),TEXT("Volume"));
            auto* Slider=Property?Cast<USlider>(Property->GetObjectPropertyValue_InContainer(Menu)):nullptr;
            if(!Slider){Finish(TEXT("Volume slider missing"));return;}
            Slider->SetValue(.45f);Call(Menu,TEXT("CommitVolume"));
            if(!FMath::IsNearlyEqual(Profile->GetProfile().MasterVolume,.45f)){Finish(TEXT("Volume not applied"));return;}
            Call(Menu,TEXT("QualityMedium"));break;
        }
        case 7:
            if(!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SavePath,true)){Finish(TEXT("Could not arrange save-error fixture"));return;}
            Profile->UpdateSettings(.5f,1);Menu->RefreshFromState();
            if(!Profile->HasUnsavedChanges()){Finish(TEXT("Read-only save did not report failure"));return;}
            Capture(TEXT("ui-save-error"));break;
        case 8:
            FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SavePath,false);Call(Menu,TEXT("RetrySave"));
            if(Profile->HasUnsavedChanges()){Finish(TEXT("Save retry did not recover"));return;}
            Capture(TEXT("ui-save-recovered"));break;
        case 9:Call(Menu,TEXT("Quit"));Capture(TEXT("ui-quit-confirm"));break;
        case 10:Call(Menu,TEXT("Resume"));Call(Menu,TEXT("Resume"));break;
        case 11:
            // Isolated fault fixture only. Continuous route mode never teleports or refills.
            Bike->SetActorLocation(FVector(4000,-12000,52),false,nullptr,ETeleportType::TeleportPhysics);
            Rider->SetActorLocation(FVector(4000,-12130,90),false,nullptr,ETeleportType::TeleportPhysics);
            if(!Bike->TryMount(Rider)){Finish(TEXT("Rescue fixture mount failed"));return;}
            Delivery->TryAccept(Delivery->GetOffers()[0].InstanceId);Bike->SetBatteryFraction(0);PC->TogglePhone();Capture(TEXT("ui-empty-battery"));break;
        case 12:case 13:
        {
            Widgets.Reset();UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UCourierPhoneWidget::StaticClass(),false);
            if(Widgets.IsEmpty()){Finish(TEXT("Phone missing"));return;}
            Call(Widgets[0],TEXT("Rescue"));
            if(InterfaceCheck==12)Capture(TEXT("ui-rescue-confirm"));
            else
            {
                if(Bike->GetBatteryFraction()<.99f || Delivery->GetState().IsActive() || Profile->GetProfile().FailedCount!=InitialFailed+1)
                {Finish(TEXT("Confirmed rescue result incorrect"));return;}
                PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);Capture(TEXT("ui-rescued"));
            }
            break;
        }
        case 14:
            if(!Bike->TryDismount()){Finish(TEXT("Rescue dismount failed"));return;}
            Rider->SetActorLocation(FVector(800,-11550,-2000),false,nullptr,ETeleportType::TeleportPhysics);
            PC->GetRecovery()->SetRecoverHeld(true);break;
        case 15:
            PC->GetRecovery()->SetRecoverHeld(false);
            if(Rider->GetActorLocation().Z<0){Finish(TEXT("Two-second recovery failed"));return;}
            Capture(TEXT("ui-recovered"));break;
        case 16:
            UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("packaged-ui-audio"),Output);
            PC->SetInterfaceMode(ECourierInterfaceMode::Pause);Call(Menu,TEXT("Quit"));Capture(TEXT("ui-exit-confirm"));break;
        default:
            WriteStatus(TEXT("ui-result.json"));Call(Menu,TEXT("Quit"));bFinished=true;return;
    }
    ++InterfaceCheck;
}
#endif
void APackagedValidationDriver::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
    if(bFinished)return;
    const double Now=FPlatformTime::Seconds(),Elapsed=Now-StartTime,FrameMs=(Now-PreviousTime)*1000;PreviousTime=Now;
    const FVector Position=Bike->GetActorLocation();DistanceMeters+=FVector::Dist2D(Position,LastPosition)/100;LastPosition=Position;
    static double LastMemorySample=0,MemoryMB=0,TextureMB=0;
    if(Now-LastMemorySample>1)
    {
        LastMemorySample=Now;MemoryMB=FPlatformMemory::GetStats().UsedPhysical/(1024.*1024.);
        FTextureMemoryStats Stats;RHIGetTextureMemoryStats(Stats);TextureMB=(Stats.StreamingMemorySize+Stats.NonStreamingMemorySize)/(1024.*1024.);
    }
    const FString Phase=Stage==EStage::Performance?TEXT("performance"):Stage==EStage::Warmup?TEXT("warmup"):TEXT("delivery");
    Frames+=FString::Printf(TEXT("%s,%.6f,%.6f,%.4f,%.4f,%.4f,%.2f,%.2f,%s\n"),FPlatformProperties::RequiresCookedData()?TEXT("PackagedGame"):TEXT("EditorGame"),Elapsed,FrameMs,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),MemoryMB,TextureMB,*Phase);
    if(Now-LastStatus>5){LastStatus=Now;WriteStatus(TEXT("progress.json"));}
    if(Mode==TEXT("Checks")){RunInterfaceChecks(Now);return;}
    if(Mode==TEXT("CharacterActions")){RunCharacterActionChecks(Now);return;}
    if(Mode==TEXT("FeatureUI") || Mode==TEXT("FeatureReload") || Mode==TEXT("FeatureRouteReload") || Mode==TEXT("LegacyCheck") || Mode==TEXT("FutureEnvelopeCheck")){RunFeatureChecks(Now);return;}
    if(Stage==EStage::Preview)
    {
        if(Now-StageTime<3)return;
        const auto* Preview=PC->GetOfferPreview(); if(!Preview){Finish(TEXT("Lost continuous route preview"));return;}
        const auto Offer=*Preview; PC->CloseOfferPreview();
        if(!PC->AcceptOffer(Offer.InstanceId)){Finish(TEXT("Continuous route accept failed"));return;}
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
        Events+=FString::Printf(TEXT("%.3f,accepted,%s,%s,0,%lld\n"),Elapsed,*Offer.MerchantId.ToString(),*Offer.DestinationId.ToString(),Profile->GetProfile().BalanceCents);
        GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()->SetNavigationMode(ECourierNavigationMode::Station);
        BeginTrip(City->Station.LocationId);return;
    }
    if(Elapsed>MinimumSeconds+1500){Finish(TEXT("Validation exceeded time limit"));return;}
    if(Delivery->GetState().Status==EDeliveryStatus::Failed && Profile->GetProfile().FailedCount>InitialFailed){Finish(TEXT("Delivery failed during continuous route"));return;}
    if(Stage==EStage::Reload)
    {
        if(Elapsed<8)return;
        if(Delivery->GetState().IsActive() || Profile->GetProfile().CompletedCount<8 || Profile->GetProfile().PowerLevel<1){Finish(TEXT("Reload persistence or active-order discard failed"));return;}
        Capture(TEXT("reload-persisted"));Stage=EStage::FinishWait;StageTime=Now;return;
    }
    if(Stage==EStage::FinishWait){if(Now-StageTime>3)Finish();return;}
    if(Stage==EStage::Warmup)
    {
        if(Mode==TEXT("FeatureRoute"))
        {
            if(Elapsed<8)return;
            if(!Bike->TryMount(Rider)){Finish(TEXT("Initial route mount failed"));return;}
            NextOrder();return;
        }
        if(Elapsed<30)return;
        FVector2D ViewSize;GEngine->GameViewport->GetViewportSize(ViewSize);
        auto Value=[](const TCHAR* Name){auto* C=IConsoleManager::Get().FindConsoleVariable(Name);return C?C->GetFloat():-1.f;};
        const FString Metadata=FString::Printf(TEXT("{\"capture_source\":\"%s\",\"cpu\":\"%s\",\"gpu\":\"%s\",\"ram_mb\":%.0f,\"rhi\":\"%s\",\"width\":%.0f,\"height\":%.0f,\"quality\":1,\"lumen_gi\":%.0f,\"lumen_reflections\":%.0f,\"hardware_raytracing\":%.0f,\"screen_percentage\":%.2f,\"warmup_seconds\":30,\"performance_seconds\":180}"),
            FPlatformProperties::RequiresCookedData()?TEXT("PackagedGame"):TEXT("EditorGame"),*FPlatformMisc::GetCPUBrand(),*GRHIAdapterName,FPlatformMemory::GetConstants().TotalPhysical/(1024.*1024.),GDynamicRHI->GetName(),ViewSize.X,ViewSize.Y,Value(TEXT("r.Lumen.DiffuseIndirect.Allow")),Value(TEXT("r.Lumen.Reflections.Allow")),Value(TEXT("r.RayTracing")),Value(TEXT("r.ScreenPercentage")));
        FFileHelper::SaveStringToFile(Metadata,*(Output/TEXT("metadata.json")));
        if(!Bike->TryMount(Rider)){Finish(TEXT("Initial mount failed"));return;}
        Points={FVector(12000,-12000,0),FVector(12000,0,0),FVector(12000,12000,0),FVector(0,12000,0),FVector(-12000,12000,0),FVector(-12000,0,0),FVector(-12000,-12000,0),FVector(0,-12000,0)};
        PointIndex=0;Stage=EStage::Performance;PerformanceStart=Now;return;
    }
    if(Stage==EStage::Performance)
    {
        if(!bCapturedPerformance && Now-PerformanceStart>2){bCapturedPerformance=true;Capture(TEXT("performance-start"));}
        if(Now-PerformanceStart>=180)
        {
            bPerfDone=true;Capture(TEXT("performance-end"));FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv")));
            BeginTrip(City->Station.LocationId);return;
        }
        if(FollowRoad(DeltaSeconds))PointIndex=0;
        return;
    }
    if(Stage==EStage::Drive)
    {
        if(Now-StageTime>240){Finish(TEXT("Drive timeout to ")+Target.ToString());return;}
        if(!FollowRoad(DeltaSeconds))return;
        if(!Bike->TryDismount()){Finish(TEXT("Safe dismount unavailable at ")+Target.ToString());return;}
        Stage=EStage::Walk;StageTime=Now;return;
    }
    if(Stage==EStage::Walk)
    {
        if(Now-StageTime>30){Finish(TEXT("Walk timeout to ")+Target.ToString());return;}
        const auto* Place=City->FindLocation(Target);if(!Place){Finish(TEXT("Missing target definition"));return;}
        if(!WalkTo(Place->Position+FVector(0,-70,0)))return;
        if(Target==City->Station.LocationId)
        {
            if(bFinalizeAtStation)
            {
                Station->TryPurchase(EUpgradeKind::Power,Rider);Station->TryPurchase(EUpgradeKind::Handling,Rider);Station->TryPurchase(EUpgradeKind::Battery,Rider);
                if(Profile->GetProfile().PowerLevel<1){Finish(TEXT("Insufficient earnings for upgrade"));return;}
                Delivery->RefreshOffers(Rider->GetActorLocation());Delivery->TryAccept(Delivery->GetOffers()[0].InstanceId); // Must be discarded on reload.
                Profile->SaveProfile();PC->OpenStation(Station);Capture(TEXT("final-upgrades"));Stage=EStage::FinishWait;StageTime=Now;return;
            }
            if(!Station->BeginCharge(Rider)){Finish(TEXT("Station charge rejected"));return;}
            Stage=EStage::Charging;StageTime=Now;Capture(TEXT("station-charge"));return;
        }
        if(OrderIndex<8)
        {
            if(Delivery->GetState().Status==EDeliveryStatus::AwaitingPickup)
            {
                if(!Delivery->TryPickup(Target,Rider)){Finish(TEXT("Pickup rejected"));return;}
                Capture(FString::Printf(TEXT("order-%02d-picked-up"),OrderIndex+1));
            }
            else if(Delivery->GetState().Status==EDeliveryStatus::Delivering)
            {
                const auto Offer=Delivery->GetState().Offer;
                if(!Delivery->TryDeliver(Target,Rider)){Finish(TEXT("Delivery rejected"));return;}
                ++OrderIndex;
                Events+=FString::Printf(TEXT("%.3f,delivered,%s,%s,%lld,%lld\n"),Elapsed,*Offer.MerchantId.ToString(),*Offer.DestinationId.ToString(),Delivery->GetLastSettlement().PayoutCents,Profile->GetProfile().BalanceCents);
                Capture(FString::Printf(TEXT("order-%02d-settled"),OrderIndex));
                Stage=EStage::ReturnBike;StageTime=Now+2;return;
            }
        }
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);Stage=EStage::ReturnBike;StageTime=Now;return;
    }
    if(Stage==EStage::Charging)
    {
        if(Station->IsCharging())return;
        if(Bike->GetBatteryFraction()<.99f){Finish(TEXT("Charge ended without full battery"));return;}
        Stage=EStage::ReturnBike;StageTime=Now;return;
    }
    if(Stage==EStage::ReturnBike)
    {
        if(Now<StageTime)return;
        if(PC->GetInterfaceMode()!=ECourierInterfaceMode::Gameplay)PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
        if(Now-StageTime>30){Finish(TEXT("Return to bike timed out"));return;}
        if(Mode==TEXT("FeatureRoute")) GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()->SetNavigationMode(ECourierNavigationMode::ParkedBike);
        if(!Bike->TryMount(Rider)){WalkTo(Bike->GetActorLocation());return;}
        if(Mode==TEXT("FeatureRoute"))
        {
            auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>(); Nav->UpdateNavigation(Bike->GetActorLocation(),.5);
            if(Nav->GetNavigationMode()!=ECourierNavigationMode::Order){Finish(TEXT("Mount did not restore order navigation"));return;}
        }
        if(Delivery->GetState().Status==EDeliveryStatus::Delivering || (Mode==TEXT("FeatureRoute") && Delivery->GetState().IsActive()))BeginTrip(Delivery->GetTargetId());else NextOrder();
    }
#endif
}
#if UE_BUILD_SHIPPING
void APackagedValidationDriver::BeginTrip(FName){}
bool APackagedValidationDriver::FollowRoad(float){return false;}
bool APackagedValidationDriver::WalkTo(FVector){return false;}
void APackagedValidationDriver::NextOrder(){}
void APackagedValidationDriver::Finish(const FString&){}
void APackagedValidationDriver::WriteStatus(const FString&,const FString&){}
void APackagedValidationDriver::Capture(const FString&){}
void APackagedValidationDriver::RunInterfaceChecks(double){}
#endif
