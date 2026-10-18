#include "Validation/PackagedValidationDriver.h"
#if !UE_BUILD_SHIPPING
#include "Buildings/CourierBuildingSubsystem.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Player/CourierActionComponent.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "World/RiderStation.h"
#include "World/CityDefinition.h"
#include "Vehicles/ScooterPawn.h"
#include "Profile/ProfileSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "DynamicRHI.h"
#include "Ambient/AmbientVehicle.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"

namespace
{
// Validation input must walk around real traffic, never disable its collision.
TArray<FVector> TrafficDetour(UWorld* World,ACourierCharacter* Rider,FVector Goal)
{
    const FVector Start=Rider->GetActorLocation(), Direction=(Goal-Start).GetSafeNormal2D();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LiveTrafficWalking),false,Rider); FHitResult Hit;
    auto Clear=[&](FVector A,FVector Z){return !World->SweepSingleByChannel(Hit,A,Z,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(40,80),Query);};
    if(Clear(Start,Start+Direction*FMath::Min(500.,FVector::Dist2D(Start,Goal))) || !Cast<AAmbientVehicle>(Hit.GetActor())) return {};
    FVector End=Start+Direction*FMath::Min(1000.,FVector::Dist2D(Start,Goal));
    TArray<FVector> Nodes{Start,End};
    for(TActorIterator<AAmbientVehicle> It(World);It;++It)
    {
        if(!It->GetActorEnableCollision() || FVector::Dist2D(Start,It->GetActorLocation())>1800) continue;
        const auto* Root=Cast<UPrimitiveComponent>(It->GetRootComponent()); if(!Root) continue;
        const FBox Box=Root->Bounds.GetBox().ExpandBy(95);
        for(int32 X:{0,1}) for(int32 Y:{0,1}) Nodes.Emplace(X?Box.Max.X:Box.Min.X,Y?Box.Max.Y:Box.Min.Y,Start.Z);
    }
    TArray<double> Costs; Costs.Init(MAX_dbl,Nodes.Num()); Costs[0]=0;
    TArray<int32> Previous; Previous.Init(INDEX_NONE,Nodes.Num()); TArray<bool> Done; Done.Init(false,Nodes.Num());
    for(int32 Step=0;Step<Nodes.Num();++Step)
    {
        int32 Current=INDEX_NONE; double Best=MAX_dbl;
        for(int32 I=0;I<Nodes.Num();++I) if(!Done[I] && Costs[I]<Best) { Current=I; Best=Costs[I]; }
        if(Current==INDEX_NONE) break;
        if(Current==1)
        {
            TArray<FVector> Path;
            for(int32 I=1;I>0;I=Previous[I]) Path.Insert(Nodes[I]-FVector(0,0,90),0);
            return Path;
        }
        Done[Current]=true;
        for(int32 I=0;I<Nodes.Num();++I) if(!Done[I])
        {
            const double Next=Costs[Current]+FVector::Distance(Nodes[Current],Nodes[I]);
            if(Next<Costs[I] && Clear(Nodes[Current],Nodes[I])) { Costs[I]=Next; Previous[I]=Current; }
        }
    }
    return {};
}
}

bool APackagedValidationDriver::BeginInteriorJourney(FVector Destination)
{
    const auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>();
    InteriorRoute=City->FindJourneyToPosition(Rider->GetActorLocation(),Destination,*S->GetCatalog()); InteriorIndex=0;
    InteriorWait=FPlatformTime::Seconds();
    InteriorLastMoveTime=InteriorWait; InteriorLastPosition=Rider->GetActorLocation();
    if(!InteriorRoute.bReachable || !GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()->IsRouteClear(InteriorRoute))
    { Finish(TEXT("Interior composed journey unavailable")); return false; }
    return true;
}
bool APackagedValidationDriver::FollowInteriorJourney()
{
    auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>();
    const double Now=FPlatformTime::Seconds();
    if(FVector::Dist(Rider->GetActorLocation(),InteriorLastPosition)>20) { InteriorLastPosition=Rider->GetActorLocation(); InteriorLastMoveTime=Now; }
    if(Now-InteriorLastMoveTime>15)
    {
        const FVector P=Rider->GetActorLocation(), Goal=InteriorRoute.Points[InteriorIndex];
        FCollisionQueryParams Query(SCENE_QUERY_STAT(InteriorStuckDiagnosis),false,Rider); FHitResult Obstacle,Ground;
        GetWorld()->SweepSingleByChannel(Obstacle,P,P+(Goal-P).GetSafeNormal2D()*80,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Query);
        GetWorld()->LineTraceSingleByChannel(Ground,Goal+FVector(0,0,300),Goal-FVector(0,0,100),ECC_Visibility,Query);
        FString ObstacleTags; if(Obstacle.GetActor()) for(FName Tag:Obstacle.GetActor()->Tags) ObstacleTags+=Tag.ToString()+TEXT(",");
        FString Doors;
        for(auto* D:S->GetNearbyDoors(P,200))
        {
            FText Reason; const bool Allowed=D->CanInteract(Rider,Reason); FHitResult Sight;
            GetWorld()->LineTraceSingleByChannel(Sight,Rider->GetPawnViewLocation(),D->GetInteractionLocation()+FVector(0,0,50),ECC_Visibility,Query);
            Doors+=FString::Printf(TEXT(" %s allowed=%d reason=%s eye=%s sight=%s component=%s impact=%s"),*D->GetDefinition().Id.ToString(),Allowed,*Reason.ToString(),*Rider->GetPawnViewLocation().ToString(),*GetNameSafe(Sight.GetActor()),*GetNameSafe(Sight.GetComponent()),*Sight.ImpactPoint.ToString());
        }
        Finish(FString::Printf(TEXT("No physical walking progress at %s waypoint %d goal %s; obstacle %s tags %s impact %s normal %s; ground %s at %s; step %.1f mode %d doors:%s"),*P.ToString(),InteriorIndex,*Goal.ToString(),*GetNameSafe(Obstacle.GetActor()),*ObstacleTags,*Obstacle.ImpactPoint.ToString(),*Obstacle.ImpactNormal.ToString(),*GetNameSafe(Ground.GetActor()),*Ground.ImpactPoint.ToString(),Rider->GetCharacterMovement()->MaxStepHeight,int32(Rider->GetCharacterMovement()->MovementMode),*Doors)); return false;
    }
    if(FPlatformTime::Seconds()-InteriorWait>420) { Finish(TEXT("Interior walking timeout at ")+Rider->GetActorLocation().ToString()); return false; }
    while(InteriorIndex<InteriorRoute.Points.Num())
    {
        FVector Destination=InteriorRoute.Points[InteriorIndex]+FVector(0,0,90);
        if(!S->Locate(Destination).IsValid())
        {
            FHitResult Ground; FCollisionQueryParams Query(SCENE_QUERY_STAT(LiveOutdoorGround),false,Rider);
            if(GetWorld()->LineTraceSingleByChannel(Ground,Destination+FVector(0,0,60),Destination-FVector(0,0,190),ECC_Visibility,Query) && Ground.Normal.Z>=.7)
                Destination.Z=Ground.ImpactPoint.Z+90;
        }
        FVector Delta=Destination-Rider->GetActorLocation();
        if(Delta.Size2D()<10 && FMath::Abs(Delta.Z)<30 && !Rider->GetCharacterMovement()->IsFalling()) { ++InteriorIndex; continue; }
        const FVector A=InteriorIndex>0?InteriorRoute.Points[InteriorIndex-1]:Rider->GetActorLocation()-FVector(0,0,90);
        const FVector Z=InteriorRoute.Points[InteriorIndex]; bool Wait=false;
        for(auto* D:S->GetNearbyDoors(Rider->GetActorLocation(),195))
        {
            if(!InteriorRoute.RequiredDoors.Contains(D->GetDefinition().Id)) continue;
            const FVector P=D->GetInteractionLocation()-FVector(0,0,85);
            if(FMath::PointDistToSegment(P,A,Z)>75) continue;
            if(!D->GetDoorState().bWantOpen)
            {
                const auto Result=D->TryToggle(Rider);
                if(!Result.bSucceeded) Wait=true;
            }
            if(D->GetDoorState().bBlocked) { Finish(TEXT("Live door sweep blocked: ")+D->GetDefinition().Id.ToString()); return false; }
            Wait|=D->IsMoving();
        }
        if(!Wait)
        {
            if(!S->Locate(Rider->GetActorLocation()).IsValid())
            {
                const auto Detour=TrafficDetour(GetWorld(),Rider,Destination);
                if(!Detour.IsEmpty())
                {
                    InteriorRoute.Points.Insert(Detour,InteriorIndex);
                    UE_LOG(LogTemp,Display,TEXT("Interior validator walking around traffic via %d physical waypoints"),Detour.Num());
                    return false;
                }
            }
            // Same held-Shift path as the player, without changing time or physics.
            // The separate fixed performance route keeps ordinary walking speed.
            Rider->SetSprinting(Mode==TEXT("Interiors"));
            Rider->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(Delta.Size2D()/65.,.15,1.),true);
            PC->SetControlRotation(FRotator(-12,Delta.Rotation().Yaw,0));
        }
        else Rider->GetCharacterMovement()->StopMovementImmediately();
        return false;
    }
    Rider->GetCharacterMovement()->StopMovementImmediately(); return true;
}
void APackagedValidationDriver::RunInteriorChecks(double Now,float)
{
    auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>();
    if(!S || !S->IsEnabled()) { Finish(TEXT("Packaged complete building subsystem unavailable")); return; }
    const auto* C=S->GetCatalog(); const double Elapsed=Now-StartTime;
    if(Mode==TEXT("InteriorsBikeCheck") && InteriorStep==0)
    {
        if(Elapsed<8) return;
        BeginInteriorJourney(Bike->GetActorLocation()+Bike->GetActorRightVector()*150);
        InteriorStep=7;
    }
    if(Now-FeatureWaitStart>5)
    {
        FeatureWaitStart=Now;
        const FString Status=FString::Printf(TEXT("{\"step\":%d,\"waypoint\":%d,\"waypoints\":%d,\"layout_index\":%d,\"layout_count\":%d,\"ready_buildings\":%d,\"active_doors\":%d,\"max_preparation_ms\":%.3f,\"rider\":\"%s\"}"),InteriorStep,InteriorIndex,InteriorRoute.Points.Num(),InteriorTargetIndex,InteriorTargets.Num(),S->GetReadyCount(),S->GetActiveDoorCount(),S->GetMaxPreparationMs(),*Rider->GetActorLocation().ToString());
        FFileHelper::SaveStringToFile(Status,*(Output/TEXT("interior-progress.json")));
    }
    if(Mode==TEXT("InteriorsReload"))
    {
        if(Elapsed<8) return;
        const auto& P=Profile->GetProfile();
        if(P.OrderHistory.IsEmpty() || !P.OrderHistory[0].bCompleted || !P.OrderHistory[0].DestinationName.Contains(TEXT("楼")) || Delivery->GetState().IsActive() || S->Locate(Rider->GetActorLocation()).IsValid())
        { Finish(TEXT("Indoor history/restart validation failed")); return; }
        PC->OpenOrderHistory(); Capture(TEXT("interior-history-after-restart"));
        if(Elapsed>12) Finish(); return;
    }
    // Ten layout classes span the entire city. Reserve the actual walking time
    // for two complete return cycles in addition to the minimum soak duration.
    if(Elapsed>MinimumSeconds+1200) { Finish(TEXT("Interior soak exceeded time limit")); return; }
    if(Delivery->GetState().Status==EDeliveryStatus::Failed) { Finish(TEXT("Indoor validation order failed")); return; }
    if(InteriorStep==0)
    {
        if(!bInteriorOfferPrepared)
        {
            int32 Highest=0;
            for(const auto& Binding:C->Bindings) if(Binding.Role==TEXT("destination"))
            {
                const auto& B=C->Buildings[C->FindBuilding(Binding.BuildingId)]; const int32 Floor=B.Rooms[B.FindRoom(Binding.RoomId)].Floor;
                if(Floor>Highest) { Highest=Floor; InteriorDeliveryTarget=Binding.LocationId; }
            }
            // Fix the exact pickup/destination pair before frame sampling.
            FMath::RandInit(560619);
            for(int32 Attempt=0;Attempt<128 && !bInteriorOfferPrepared;++Attempt)
            {
                Delivery->RefreshOffers(Rider->GetActorLocation());
                bInteriorOfferPrepared=Delivery->GetOffers().ContainsByPredicate([&](const auto& O){return O.MerchantId==City->Merchants[0].LocationId && O.DestinationId==InteriorDeliveryTarget;});
            }
            if(!bInteriorOfferPrepared) { Finish(TEXT("Fixed highest-floor offer unavailable")); return; }
        }
        if(Elapsed<30) return;
        FVector2D Size; GEngine->GameViewport->GetViewportSize(Size);
        auto Value=[](const TCHAR* Name){auto* V=IConsoleManager::Get().FindConsoleVariable(Name);return V?V->GetFloat():-1.f;};
        const FString Metadata=FString::Printf(TEXT("{\"capture_source\":\"%s\",\"cpu\":\"%s\",\"gpu\":\"%s\",\"ram_mb\":%.0f,\"rhi\":\"%s\",\"width\":%.0f,\"height\":%.0f,\"quality\":1,\"lumen_gi\":%.0f,\"lumen_reflections\":%.0f,\"hardware_raytracing\":%.0f,\"screen_percentage\":%.2f,\"warmup_seconds\":30,\"performance_seconds\":180,\"route\":\"station to courier pickup to highest-floor delivery, doors and stairs\"}"),
            FPlatformProperties::RequiresCookedData()?TEXT("PackagedGame"):TEXT("EditorGame"),*FPlatformMisc::GetCPUBrand(),*GRHIAdapterName,FPlatformMemory::GetConstants().TotalPhysical/(1024.*1024.),GDynamicRHI->GetName(),Size.X,Size.Y,Value(TEXT("r.Lumen.DiffuseIndirect.Allow")),Value(TEXT("r.Lumen.Reflections.Allow")),Value(TEXT("r.RayTracing")),Value(TEXT("r.ScreenPercentage")));
        FFileHelper::SaveStringToFile(Metadata,*(Output/TEXT("metadata.json")));
        TSet<FString> Layouts;
        for(const auto& B:C->Buildings)
        {
            const FString Key=FString::Printf(TEXT("%d_%.2f_%.2f"),B.Variant,B.Size.X,B.Size.Y); if(Layouts.Contains(Key)) continue; Layouts.Add(Key);
            const int32 Top=B.Floors.Num()-(B.Floors.Last().bRoof?1:0);
            const auto* R=B.Rooms.FindByPredicate([&](const auto& Room){return Room.Floor==Top && Room.Kind!=TEXT("hall") && Room.Kind!=TEXT("stairs") && Room.Kind!=TEXT("roof");});
            if(R) InteriorTargets.Add(B.ToWorld(R->StandingPoint));
        }
        Stage=EStage::Performance; PerformanceStart=Now;
        GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()->SetNavigationMode(ECourierNavigationMode::Station);
        BeginInteriorJourney(Station->GetActorLocation()); InteriorStep=1; return;
    }
    if(Stage==EStage::Performance && Now-PerformanceStart>=180)
    { Stage=EStage::Cruise; bPerfDone=true; FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("frames.csv"))); Capture(TEXT("interior-performance-end")); }
    if(InteriorStep==1 && FollowInteriorJourney())
    {
        Capture(TEXT("interior-station"));
        if(!Station->BeginCharge(Rider)) { Finish(TEXT("Interior station charge rejected")); return; }
        InteriorStep=2; InteriorWait=Now; return;
    }
    if(InteriorStep==2 && Now-InteriorWait>5.5)
    {
        for(const auto& O:Delivery->GetOffers()) if(O.MerchantId==City->Merchants[0].LocationId && O.DestinationId==InteriorDeliveryTarget)
        {
            if(!PC->PreviewOffer(O.InstanceId)) { Finish(TEXT("Interior route preview failed")); return; }
            Target=O.MerchantId; InteriorStep=20; InteriorWait=Now; return;
        }
        Delivery->RefreshOffers(Rider->GetActorLocation()); InteriorWait=Now-3; return;
    }
    if(InteriorStep==20 && Now-InteriorWait>.3)
    { Capture(TEXT("interior-full-route-preview")); InteriorStep=21; InteriorWait=Now; return; }
    if(InteriorStep==21 && Now-InteriorWait>.2)
    {
        const auto* Preview=PC->GetOfferPreview(); if(!Preview) { Finish(TEXT("Indoor preview disappeared")); return; }
        const FGuid Id=Preview->InstanceId; PC->CloseOfferPreview();
        if(!PC->AcceptOffer(Id)) { Finish(TEXT("Interior offer acceptance failed")); return; }
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay); BeginInteriorJourney(S->GetManagedLocation(Target)->GetActorLocation()); InteriorStep=3; return;
    }
    if(InteriorStep==3 && FollowInteriorJourney())
    {
        if(!Delivery->TryPickup(Target,Rider)) { Finish(TEXT("Physical indoor pickup failed")); return; }
        Capture(TEXT("interior-picked-up")); Target=Delivery->GetTargetId(); BeginInteriorJourney(S->GetManagedLocation(Target)->GetActorLocation()); InteriorStep=4; return;
    }
    if(InteriorStep==4 && FollowInteriorJourney())
    {
        if(!Delivery->TryDeliver(Target,Rider)) { Finish(TEXT("Physical highest-floor delivery failed")); return; }
        ++OrderIndex; Capture(TEXT("interior-highest-floor-delivered")); PC->ToggleMap(); InteriorStep=5; InteriorWait=Now; return;
    }
    if(InteriorStep==5 && Now-InteriorWait>2)
    {
        PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
        if(InteriorTargetIndex<InteriorTargets.Num()) { BeginInteriorJourney(InteriorTargets[InteriorTargetIndex++]); InteriorStep=6; }
        else { BeginInteriorJourney(Bike->GetActorLocation()+Bike->GetActorRightVector()*150); InteriorStep=7; }
        return;
    }
    if(InteriorStep==6 && FollowInteriorJourney())
    {
        const auto A=S->Locate(Rider->GetActorLocation()); if(!A.IsValid()) { Finish(TEXT("Layout-class target room not reached")); return; }
        const auto& B=C->Buildings[A.Building]; const FString Key=FString::Printf(TEXT("%d_%.2f_%.2f"),B.Variant,B.Size.X,B.Size.Y); InteriorVisitedLayouts.Add(Key);
        Capture(FString::Printf(TEXT("interior-layout-%02d"),InteriorTargetIndex));
        InteriorStep=5; InteriorWait=Now; return;
    }
    if(InteriorStep==7)
    {
        // The bike is a real solid obstacle. Use its normal interaction range
        // before trying to reach a waypoint on the far side of its collision.
        const bool Mounted=FVector::DistSquared(Rider->GetActorLocation(),Bike->GetInteractionLocation())<195*195 &&
            !S->Locate(Rider->GetActorLocation()).IsValid() && Bike->TryMount(Rider);
        if(!Mounted)
        {
            if(!FollowInteriorJourney()) return;
            if(S->Locate(Rider->GetActorLocation()).IsValid() || !Bike->TryMount(Rider)) { Finish(TEXT("Return from upstairs to parked bike failed")); return; }
        }
        if(!Bike->TryDismount()) { Finish(TEXT("Bike dismount failed")); return; }
        ++InteriorRepeats;
        if(Mode==TEXT("InteriorsBikeCheck"))
        {
            const auto* Body=Cast<UPrimitiveComponent>(Bike->GetRootComponent());
            if(!Body || Body->GetCollisionObjectType()!=ECC_GameTraceChannel1 || S->Locate(Rider->GetActorLocation()).IsValid())
            { Finish(TEXT("Actual scooter vehicle-channel/outside dismount regression")); return; }
            FFileHelper::SaveStringToFile(TEXT("{\"normal_mount_dismount\":true,\"vehicle_channel\":true,\"dismount_outside\":true}"),*(Output/TEXT("bike-check.json")));
            Finish(); return;
        }
        if(Elapsed>=MinimumSeconds && InteriorRepeats>=2)
        {
            if(!Profile->SaveProfile()) { Finish(TEXT("Interior profile persistence failed")); return; }
            const FString Result=FString::Printf(TEXT("{\"layout_classes\":%d,\"completed_indoor_orders\":%d,\"cache_return_cycles\":%d,\"building_count\":%d,\"door_count\":%d,\"elapsed_seconds\":%.3f}"),InteriorVisitedLayouts.Num(),OrderIndex,InteriorRepeats,C->Buildings.Num(),[&](){int32 N=0;for(const auto& B:C->Buildings) N+=B.Doors.Num();return N;}(),Elapsed);
            FFileHelper::SaveStringToFile(Result,*(Output/TEXT("interior-play.json"))); PC->OpenOrderHistory(); Capture(TEXT("interior-history")); InteriorStep=100; InteriorWait=Now; return;
        }
        InteriorTargetIndex=FMath::Max(0,InteriorTargets.Num()-2); InteriorStep=5; InteriorWait=Now; return;
    }
    if(InteriorStep==100 && Now-InteriorWait>3) Finish();
}
#else
bool APackagedValidationDriver::BeginInteriorJourney(FVector){return false;}
bool APackagedValidationDriver::FollowInteriorJourney(){return false;}
void APackagedValidationDriver::RunInteriorChecks(double,float){}
#endif
