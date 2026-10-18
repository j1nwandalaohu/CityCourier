#include "Validation/PackagedValidationDriver.h"
#if !UE_BUILD_SHIPPING
#include "Buildings/CourierBuildingSubsystem.h"
#include "World/CityDefinition.h"
#include "Player/CourierCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "DynamicRHI.h"

void APackagedValidationDriver::RunInteriorPerformance(double Now)
{
    if(Mode==TEXT("InteriorsPerformance") && !FPlatformProperties::RequiresCookedData()) { Finish(TEXT("Performance acceptance requires the cooked executable")); return; }
    auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>();
    if(!S || !S->IsEnabled()) { Finish(TEXT("Performance interiors unavailable")); return; }
    const auto* C=S->GetCatalog();
    if(Now-FeatureWaitStart>5)
    {
        FeatureWaitStart=Now;
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"step\":%d,\"waypoint\":%d,\"waypoints\":%d,\"cycles\":%d,\"ready_buildings\":%d,\"active_doors\":%d,\"rider\":\"%s\"}"),InteriorStep,InteriorIndex,InteriorRoute.Points.Num(),InteriorRepeats,S->GetReadyCount(),S->GetActiveDoorCount(),*Rider->GetActorLocation().ToString()),*(Output/TEXT("interior-progress.json")));
    }
    if(InteriorStep==0)
    {
        if(Now-StartTime<30) return;
        // Fixed representative six-storey route near the real PlayerStart.
        // Normal input walks the entire route; there is no setup teleport.
        int32 Index=INDEX_NONE; double Nearest=MAX_dbl;
        for(int32 I=0;I<C->Buildings.Num();++I)
        {
            const auto& B=C->Buildings[I];
            if(B.Kind!=TEXT("residential") || B.Floors.Num()!=7) continue;
            const double Distance=FVector::Dist2D(Rider->GetActorLocation(),B.ToWorld(B.Nodes[B.EntranceNode].Position));
            if(Distance<Nearest) { Nearest=Distance; Index=I; }
        }
        if(Index==INDEX_NONE) { Finish(TEXT("Six-storey benchmark building missing")); return; }
        const auto& B=C->Buildings[Index];
        const auto* Top=B.Rooms.FindByPredicate([](const auto& R){return R.Floor==6 && R.Kind!=TEXT("hall") && R.Kind!=TEXT("stairs");});
        const auto* Low=B.Rooms.FindByPredicate([](const auto& R){return R.Floor==1 && R.Kind!=TEXT("hall") && R.Kind!=TEXT("stairs");});
        const FVector Entry=B.ToWorld(B.Nodes[B.EntranceNode].Position);
        const FRoadNode* Street=nullptr; double Best=MAX_dbl;
        for(const auto& N:City->Roads.Nodes)
        {
            const double Distance=FVector::Dist2D(N.Position,Entry);
            if(Distance>=5200 && Distance<Best) { Best=Distance; Street=&N; }
        }
        if(!Top || !Low || !Street) { Finish(TEXT("Complete performance route missing")); return; }
        InteriorTargets={B.ToWorld(Top->StandingPoint),Street->Position,B.ToWorld(Low->StandingPoint)};
        InteriorTargetIndex=Index;
        FVector2D Size; GEngine->GameViewport->GetViewportSize(Size);
        auto Value=[](const TCHAR* Name){auto* V=IConsoleManager::Get().FindConsoleVariable(Name);return V?V->GetFloat():-1.f;};
        const FString Metadata=FString::Printf(TEXT("{\"capture_source\":\"%s\",\"cpu\":\"%s\",\"gpu\":\"%s\",\"ram_mb\":%.0f,\"rhi\":\"%s\",\"width\":%.0f,\"height\":%.0f,\"quality\":1,\"lumen_gi\":%.0f,\"lumen_reflections\":%.0f,\"hardware_raytracing\":%.0f,\"screen_percentage\":%.2f,\"warmup_seconds\":30,\"performance_seconds\":180,\"building\":\"%s\",\"route\":\"real PlayerStart to sixth-floor room; stairs down; beyond 52m detail release; reenter first-floor room with preserved open doors; repeat street/reentry\"}"),
            FPlatformProperties::RequiresCookedData()?TEXT("PackagedGame"):TEXT("EditorGame"),*FPlatformMisc::GetCPUBrand(),*GRHIAdapterName,FPlatformMemory::GetConstants().TotalPhysical/(1024.*1024.),GDynamicRHI->GetName(),Size.X,Size.Y,Value(TEXT("r.Lumen.DiffuseIndirect.Allow")),Value(TEXT("r.Lumen.Reflections.Allow")),Value(TEXT("r.RayTracing")),Value(TEXT("r.ScreenPercentage")),*B.Id.ToString());
        FFileHelper::SaveStringToFile(Metadata,*(Output/TEXT("metadata.json")));
        Stage=EStage::Performance; PerformanceStart=Now;
        BeginInteriorJourney(InteriorTargets[0]); InteriorStep=1; return;
    }
    if(InteriorStep==1 && FollowInteriorJourney())
    { BeginInteriorJourney(InteriorTargets[1]); InteriorStep=2; return; }
    if(InteriorStep==2 && FollowInteriorJourney())
    { InteriorStep=3; InteriorWait=Now; return; }
    if(InteriorStep==3 && Now-InteriorWait>.3)
    {
        if(S->GetDetailState(InteriorTargetIndex)!=ECourierBuildingDetailState::Unloaded)
        { Finish(TEXT("Benchmark occupied detail cache did not release beyond 52m")); return; }
        BeginInteriorJourney(InteriorTargets[2]); InteriorStep=4; return;
    }
    if(InteriorStep==4 && FollowInteriorJourney())
    {
        const auto Address=S->Locate(Rider->GetActorLocation());
        if(Address.Building!=InteriorTargetIndex || Address.Floor!=1 || S->GetDetailState(InteriorTargetIndex)!=ECourierBuildingDetailState::Ready)
        { Finish(TEXT("Benchmark reentry/load failed")); return; }
        const auto& B=C->Buildings[InteriorTargetIndex];
        const auto* Entry=B.Doors.FindByPredicate([&](const auto& D){return D.NodeA==B.EntranceNode || D.NodeB==B.EntranceNode;});
        if(!Entry) { Finish(TEXT("Benchmark real entry door missing")); return; }
        const auto* Door=S->ActivateDoor(Entry->Id);
        if(!Door || !Door->GetDoorState().bWantOpen || !FMath::IsNearlyEqual(Door->GetDoorState().Angle,Entry->OpenAngle,.1f))
        { Finish(TEXT("Benchmark session door pose did not survive unload/reentry")); return; }
        ++InteriorRepeats; BeginInteriorJourney(InteriorTargets[1]); InteriorStep=2; return;
    }
    if(Stage==EStage::Performance && Now-PerformanceStart>=180)
    {
        if(InteriorRepeats<1) { Finish(TEXT("Three-minute route did not complete required unload/reentry")); return; }
        Stage=EStage::Cruise; Rider->GetCharacterMovement()->StopMovementImmediately();
        const FString Result=FString::Printf(TEXT("{\"sixth_floor_reached\":true,\"release_reentry_cycles\":%d,\"session_door_pose_preserved\":true,\"max_detail_preparation_ms\":%.3f}"),InteriorRepeats,S->GetMaxPreparationMs());
        FFileHelper::SaveStringToFile(Result,*(Output/TEXT("interior-performance.json"))); Capture(TEXT("interior-benchmark-end"));
        InteriorStep=100; InteriorWait=Now;
    }
    if(InteriorStep==100 && Now-InteriorWait>2) Finish();
}
#else
void APackagedValidationDriver::RunInteriorPerformance(double){}
#endif
