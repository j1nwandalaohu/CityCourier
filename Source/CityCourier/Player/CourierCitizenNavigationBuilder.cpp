#include "Player/CourierArtBuilder.h"
#if WITH_EDITOR
#include "World/CityDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Builders/CubeBuilder.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Components/BrushComponent.h"
#include "ActorFactories/ActorFactory.h"
#include "Buildings/CourierBuildingActor.h"
#include "NavModifierVolume.h"
#include "NavAreas/NavArea_Null.h"
#endif

bool UCourierArtBuilder::BuildCitizenNavigation(UWorld* World)
{
#if WITH_EDITOR
    if(!World || World->IsGameWorld())return false;
    const auto* City=LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if(!City || !Nav)return false;
    const FName OwnedTag(TEXT("CC_CitizenNavigation"));
    // Validate the complete source before replacing our own bounds.
    TArray<TPair<FVector,FVector>> Segments;
    for(const auto& Edge:City->Roads.Edges)
    {
        const auto* A=City->Roads.FindNode(Edge.From);const auto* B=City->Roads.FindNode(Edge.To);
        if(!A || !B || A->Position.ContainsNaN() || B->Position.ContainsNaN())return false;
        if(FVector::Dist2D(A->Position,B->Position)>100)Segments.Add({A->Position,B->Position});
    }
    if(Segments.IsEmpty())return false;
    struct FInteriorMask { FVector Center,Size;float Yaw; };
    TArray<FInteriorMask> Interiors;
    for(TActorIterator<ACourierBuildingActor> It(World);It;++It)
    {
        const auto* B=It->GetDefinition();if(!B)return false;
        FBox Local(ForceInit);
        for(const auto& Box:B->Boxes)
            Local+=FBox(-Box.Size*.5,Box.Size*.5).TransformBy(FTransform(FRotator(0,Box.Yaw,0),Box.Center));
        if(!Local.IsValid)return false;
        Interiors.Add({B->ToWorld(Local.GetCenter()),Local.GetSize()+FVector(40,40,40),B->Yaw});
    }
    TArray<AVolume*> Old;
    for(TActorIterator<AVolume> It(World);It;++It)if(It->ActorHasTag(OwnedTag))Old.Add(*It);
    TArray<AVolume*> Created;
    for(const auto& Edge:Segments)
    {
        auto* Bounds=World->SpawnActor<ANavMeshBoundsVolume>();
        auto* Builder=NewObject<UCubeBuilder>();
        if(!Bounds)break;
        Builder->X=FVector::Dist2D(Edge.Key,Edge.Value)+1600;Builder->Y=1600;Builder->Z=350;
        UActorFactory::CreateBrushForVolumeActor(Bounds,Builder);
        if(!Bounds->GetBrushComponent()->Brush || Bounds->GetComponentsBoundingBox(true).GetSize().GetMin()<1)
        {World->DestroyActor(Bounds);break;}
        Bounds->Tags.Add(OwnedTag);
        Bounds->SetActorLabel(FString::Printf(TEXT("CC_CitizenNavigation_%d"),Created.Num()));
        Bounds->SetActorLocationAndRotation((Edge.Key+Edge.Value)*.5+FVector(0,0,70),(Edge.Value-Edge.Key).Rotation());
        Bounds->GetBrushComponent()->SetCanEverAffectNavigation(false);
        Bounds->MarkPackageDirty();Created.Add(Bounds);
    }
    if(Created.Num()!=Segments.Num())
    {for(auto* Bounds:Created)World->DestroyActor(Bounds);return false;}
    // Building structure deliberately opts out of the navigation octree.
    // Explicit null-area footprints therefore exclude interiors rather than
    // relying on those hidden/non-nav wall instances to cut holes in the mesh.
    for(const auto& Mask:Interiors)
    {
        auto* Volume=World->SpawnActor<ANavModifierVolume>();auto* Builder=NewObject<UCubeBuilder>();
        if(!Volume){for(auto* V:Created)World->DestroyActor(V);return false;}
        Builder->X=Mask.Size.X;Builder->Y=Mask.Size.Y;Builder->Z=Mask.Size.Z;
        UActorFactory::CreateBrushForVolumeActor(Volume,Builder);
        if(!Volume->GetBrushComponent()->Brush)
        {World->DestroyActor(Volume);for(auto* V:Created)World->DestroyActor(V);return false;}
        Volume->Tags.Add(OwnedTag);Volume->SetActorLabel(FString::Printf(TEXT("CC_CitizenInteriorMask_%d"),Created.Num()));
        Volume->SetActorLocationAndRotation(Mask.Center,FRotator(0,Mask.Yaw,0));
        Volume->SetAreaClass(UNavArea_Null::StaticClass());Volume->MarkPackageDirty();Created.Add(Volume);
    }
    for(auto* Volume:Old)World->DestroyActor(Volume);
    for(auto* Volume:Created)if(auto* Bounds=Cast<ANavMeshBoundsVolume>(Volume))Nav->OnNavigationBoundsUpdated(Bounds);
    Nav->Build();
    bool bMatchingData=false;
    for(TActorIterator<ANavigationData> It(World);It;++It)
    {
        It->EnsureBuildCompletion();
        bMatchingData|=FMath::IsNearlyEqual(It->GetConfig().AgentRadius,40.f) && FMath::IsNearlyEqual(It->GetConfig().AgentHeight,180.f);
    }
    World->MarkPackageDirty();return bMatchingData;
#else
    return false;
#endif
}
