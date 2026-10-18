#include "Buildings/CourierBuildingActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Misc/PackageName.h"

ACourierBuildingActor::ACourierBuildingActor()
{
    PrimaryActorTick.bCanEverTick=false;
    BuildingRoot=CreateDefaultSubobject<USceneComponent>(TEXT("BuildingRoot")); RootComponent=BuildingRoot;
    BuildingRoot->SetMobility(EComponentMobility::Static);
    Exterior=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HollowExterior")); Exterior->SetupAttachment(BuildingRoot);
    Exterior->SetMobility(EComponentMobility::Static); Exterior->SetCollisionProfileName(TEXT("BlockAll")); Exterior->SetCanEverAffectNavigation(false);
    DoorProxies=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorProxies")); DoorProxies->SetupAttachment(BuildingRoot);
    DoorProxies->SetMobility(EComponentMobility::Movable); DoorProxies->SetCollisionProfileName(TEXT("BlockAllDynamic")); DoorProxies->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); DoorProxies->SetStaticMesh(Cube.Object);
}
const FCourierBuildingDefinition* ACourierBuildingActor::GetDefinition() const
{ return Catalog && Catalog->Buildings.IsValidIndex(BuildingIndex)?&Catalog->Buildings[BuildingIndex]:nullptr; }
bool ACourierBuildingActor::ConfigureBuilding(UCityBuildingCatalog* InCatalog,int32 Index)
{
    if(!InCatalog || !InCatalog->Buildings.IsValidIndex(Index)) return false;
    TArray<FString> Errors; if(!InCatalog->Buildings[Index].Validate(Errors)) return false;
    Catalog=InCatalog; BuildingIndex=Index; return RebuildStructure();
}
bool ACourierBuildingActor::RebuildStructure()
{
    const auto* B=GetDefinition(); if(!B) return false;
    if(!FPackageName::DoesPackageExist(TEXT("/Game/CityCourier/Art/Buildings/SM_CourierDoor"))) return false;
    TArray<FString> Errors; if(!B->Validate(Errors)) return false;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    const FString ExteriorName=B->Floors.Num()==1?TEXT("SM_EnterableAnnex"):FString::Printf(TEXT("SM_EnterableShopHouse%c"),TCHAR('A'+B->Variant));
    auto* Facade=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Art/Buildings/%s.%s"),*ExteriorName,*ExteriorName));
    if(!Cube || !Facade) return false;
    TMap<FName,UMaterialInterface*> Materials;
    for(const auto& Box:B->Boxes)
    {
        if(Materials.Contains(Box.Material)) continue;
        auto* Mat=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Art/Materials/%s.%s"),*Box.Material.ToString(),*Box.Material.ToString()));
        if(!Mat) return false; Materials.Add(Box.Material,Mat);
    }
    Modify(); SetActorTransform(B->Transform());
    Tags.RemoveAll([](FName T){return T.ToString().StartsWith(TEXT("CC_Enterable_"));});
    Tags.Add(TEXT("CC_Enterable_Building")); Tags.Add(FName(*(TEXT("CC_Enterable_Id_")+B->Id.ToString())));
#if WITH_EDITOR
    SetActorLabel(B->Id.ToString());
#endif
    Exterior->SetStaticMesh(Facade); Exterior->SetRelativeRotation(FRotator(0,180,0));
    Exterior->SetRelativeScale3D(B->Floors.Num()==1?FVector(B->Size.X/650,B->Size.Y/500,1):FVector(B->Size.X/800,B->Size.Y/1150,1));
    for(const auto& C:Structure) if(C) { RemoveInstanceComponent(C.Get()); C->DestroyComponent(); }
    Structure.Reset();
    struct FGroup { FName Material; bool bSolid=true; TArray<FTransform> Instances; };
    TMap<FString,FGroup> Groups;
    for(const auto& Box:B->Boxes)
    {
        const bool Solid=Box.Kind!=TEXT("frame");
        const FString Key=Box.Material.ToString()+(Solid?TEXT("_Solid"):TEXT("_Visual"));
        auto& Group=Groups.FindOrAdd(Key); Group.Material=Box.Material; Group.bSolid=Solid;
        Group.Instances.Emplace(FRotator(0,Box.Yaw,0),Box.Center,Box.Size/100);
    }
    for(auto& Pair:Groups)
    {
        const FName ComponentName=MakeUniqueObjectName(this,UHierarchicalInstancedStaticMeshComponent::StaticClass(),FName(*Pair.Key));
        auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,ComponentName,RF_Transactional);
        C->SetupAttachment(BuildingRoot); C->SetMobility(EComponentMobility::Static); C->SetStaticMesh(Cube);
        C->SetMaterial(0,Materials.FindChecked(Pair.Value.Material)); C->SetCanEverAffectNavigation(false);
        C->SetCollisionProfileName(Pair.Value.bSolid?TEXT("BlockAll"):TEXT("NoCollision"));
        C->bAutoRebuildTreeOnInstanceChanges=false;
        C->AddInstances(Pair.Value.Instances,false,false,false);
        AddInstanceComponent(C); C->RegisterComponent(); C->BuildTreeIfOutdated(false,true); Structure.Add(C);
    }
    DoorProxies->ClearInstances(); ProxyDoors.Reset();
    if(auto* Wood=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CityCourier/Art/Materials/M_Wood.M_Wood"))) DoorProxies->SetMaterial(0,Wood);
    TArray<FTransform> DoorTransforms; DoorTransforms.Reserve(B->Doors.Num());
    for(int32 I=0;I<B->Doors.Num();++I) { DoorTransforms.Add(DoorProxyTransform(I,0)); ProxyDoors.Add(I); }
    DoorProxies->AddInstances(DoorTransforms,false,false,false);
    MarkPackageDirty(); return GetStructuralBoxCount()==B->Boxes.Num() && GetDoorProxyCount()==B->Doors.Num();
}
int32 ACourierBuildingActor::GetStructuralBoxCount() const
{ int32 Count=0; for(const auto& C:Structure) if(C) Count+=C->GetInstanceCount(); return Count; }
int32 ACourierBuildingActor::GetDoorProxyCount() const
{ return DoorProxies?DoorProxies->GetInstanceCount():0; }
TArray<FString> ACourierBuildingActor::GetPhysicalRoomErrors() const
{
    TArray<FString> Errors; const auto* B=GetDefinition();
    if(!B || !GetWorld()) { Errors.Add(TEXT("Building definition or world is unavailable")); return Errors; }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WholeCityRoomClearance),false);
    for(const auto& R:B->Rooms)
    {
        const FVector Foot=B->ToWorld(R.StandingPoint);
        if(GetWorld()->OverlapBlockingTestByChannel(Foot+FVector(0,0,90),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Params))
            Errors.Add(R.Id.ToString()+TEXT(": standing capsule is blocked"));
        FHitResult Hit;
        if(!GetWorld()->LineTraceSingleByChannel(Hit,Foot+FVector(0,0,5),Foot-FVector(0,0,10),ECC_Visibility,Params) || Hit.GetActor()!=this || Hit.Normal.Z<.7f)
            Errors.Add(R.Id.ToString()+TEXT(": matching structural floor is missing"));
    }
    return Errors;
}
FTransform ACourierBuildingActor::DoorProxyTransform(int32 Index,float Angle) const
{
    const auto* B=GetDefinition(); if(!B || !B->Doors.IsValidIndex(Index)) return FTransform::Identity;
    const auto& D=B->Doors[Index]; const FQuat Rotation=FRotator(0,D.ClosedYaw+Angle,0).Quaternion();
    return FTransform(Rotation,D.Hinge+Rotation.RotateVector(FVector(D.Width/2,0,D.Height/2)),FVector(D.Width/100,.05,D.Height/100));
}
bool ACourierBuildingActor::SetDoorProxy(int32 DoorIndex,float Angle,bool bVisible)
{
    const auto* B=GetDefinition(); if(!B || !B->Doors.IsValidIndex(DoorIndex) || !FMath::IsFinite(Angle)) return false;
    const int32 Instance=ProxyDoors.IndexOfByKey(DoorIndex);
    if(bVisible)
    {
        const FTransform T=DoorProxyTransform(DoorIndex,Angle);
        if(Instance!=INDEX_NONE) return DoorProxies->UpdateInstanceTransform(Instance,T,false,true,true);
        const int32 Added=DoorProxies->AddInstance(T,false); if(Added==INDEX_NONE) return false;
        if(Added!=ProxyDoors.Num()) return false; ProxyDoors.Add(DoorIndex); return true;
    }
    if(Instance==INDEX_NONE) return true;
    const bool Swap=DoorProxies->SupportsRemoveSwap(); if(!DoorProxies->RemoveInstance(Instance)) return false;
    if(Swap) ProxyDoors.RemoveAtSwap(Instance,1,EAllowShrinking::No); else ProxyDoors.RemoveAt(Instance,1,EAllowShrinking::No);
    return true;
}

void ACourierBuildingActor::EnsureEntranceBarriers()
{
    if(!EntryBarriers.IsEmpty() || !GetDefinition()) return;
    for(const auto& D:GetDefinition()->Doors) if(D.bExternal)
    {
        auto* C=NewObject<UBoxComponent>(this,NAME_None,RF_Transient);
        C->SetupAttachment(BuildingRoot); C->SetMobility(EComponentMobility::Movable);
        C->SetRelativeLocation(D.Hinge+FRotator(0,D.ClosedYaw,0).RotateVector(FVector(D.Width/2,0,D.Height/2)));
        C->SetRelativeRotation(FRotator(0,D.ClosedYaw,0)); C->SetBoxExtent(FVector(D.Width/2,5,D.Height/2));
        C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); C->SetCollisionResponseToAllChannels(ECR_Ignore);
        C->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Block); C->SetCanEverAffectNavigation(false);
        AddInstanceComponent(C); C->RegisterComponent(); EntryBarriers.Add(C);
    }
}
void ACourierBuildingActor::SetEntryLoading(bool bLoading)
{
    EnsureEntranceBarriers();
    for(const auto& C:EntryBarriers) C->SetCollisionResponseToChannel(ECC_Pawn,bLoading?ECR_Block:ECR_Ignore);
}
void ACourierBuildingActor::ReleaseDetails()
{
    for(const auto& C:Details) if(C) { RemoveInstanceComponent(C); C->DestroyComponent(); }
    Details.Reset();
}
bool ACourierBuildingActor::PrepareDetails()
{
    if(!Details.IsEmpty()) return true;
    const auto* B=GetDefinition(); if(!B) return false;
    if(!FPackageName::DoesPackageExist(TEXT("/Game/CityCourier/Art/Buildings/SM_CourierDoor"))) return false;
    TMap<FName,UStaticMesh*> Meshes;
    for(const FString Kind:{TEXT("Bed"),TEXT("Desk"),TEXT("Kitchen"),TEXT("Toilet"),TEXT("Shelf"),TEXT("Sofa"),TEXT("Counter")})
    {
        const FString Package=TEXT("/Game/CityCourier/Art/Buildings/SM_Interior")+Kind;
        if(!FPackageName::DoesPackageExist(Package)) return false;
        auto* M=LoadObject<UStaticMesh>(nullptr,*(Package+TEXT(".SM_Interior")+Kind)); if(!M) return false;
        Meshes.Add(FName(*Kind),M);
    }
    TMap<FName,TArray<FTransform>> Groups;
    for(const auto& R:B->Rooms)
    {
        FName Kind;
        if(R.Kind==TEXT("bedroom")) Kind=TEXT("Bed");
        else if(R.Kind==TEXT("living")) Kind=TEXT("Sofa");
        else if(R.Kind==TEXT("kitchen")) Kind=TEXT("Kitchen");
        else if(R.Kind==TEXT("bathroom")) Kind=TEXT("Toilet");
        else if(R.Kind==TEXT("storage")) Kind=TEXT("Shelf");
        else if(R.Kind==TEXT("office") || R.Kind==TEXT("meeting")) Kind=TEXT("Desk");
        else if(R.Kind==TEXT("shop") || R.Kind==TEXT("service")) Kind=TEXT("Counter");
        if(!Kind.IsNone())
        {
            const FBox Bounds=Meshes[Kind]->GetBoundingBox();
            const FVector FurnitureScale=B->Floors.Num()==1?FVector(.85,.75,1):FVector::OneVector;
            bool Placed=false;
            // Fit the real imported bounds into a corner, keeping the central standing
            // square and every graph segment clear for the player's capsule.
            for(float Yaw:{0.f,90.f}) for(int32 Corner=0;Corner<4 && !Placed;++Corner)
            {
                const FQuat Q=FRotator(0,Yaw,0).Quaternion(); const FBox Rotated=Bounds.TransformBy(FTransform(Q,FVector::ZeroVector,FurnitureScale));
                const FVector Ext=Rotated.GetExtent();
                const FVector Center((Corner&1)?R.Maximum.X-Ext.X-6:R.Minimum.X+Ext.X+6,
                    (Corner&2)?R.Maximum.Y-Ext.Y-6:R.Minimum.Y+Ext.Y+6,R.Minimum.Z-Rotated.Min.Z+Rotated.GetCenter().Z);
                FBox Foot(Center-FVector(Ext.X,Ext.Y,0),Center+FVector(Ext.X,Ext.Y,Ext.Z*2));
                if(Foot.Min.X<R.Minimum.X || Foot.Max.X>R.Maximum.X || Foot.Min.Y<R.Minimum.Y || Foot.Max.Y>R.Maximum.Y) continue;
                const FBox Expanded(FVector(Foot.Min.X-45,Foot.Min.Y-45,-1.e6),FVector(Foot.Max.X+45,Foot.Max.Y+45,1.e6));
                const FBox StandingClear(FVector(Foot.Min.X-60,Foot.Min.Y-60,-1.e6),FVector(Foot.Max.X+60,Foot.Max.Y+60,1.e6));
                if(StandingClear.IsInsideOrOn(R.StandingPoint)) continue;
                bool Blocked=false;
                for(const auto& E:B->Edges)
                {
                    const auto& A=B->Nodes[E.From]; const auto& Z=B->Nodes[E.To];
                    if(A.Floor!=R.Floor || (A.RoomId!=R.Id && Z.RoomId!=R.Id)) continue;
                    if(FMath::LineBoxIntersection(Expanded,A.Position,Z.Position,Z.Position-A.Position)) { Blocked=true; break; }
                }
                if(Blocked) continue;
                Groups.FindOrAdd(Kind).Emplace(Q,Center-Rotated.GetCenter(),FurnitureScale); Placed=true;
            }
            if(!Placed) { UE_LOG(LogTemp,Warning,TEXT("Cannot fit %s in %s: mesh %s, room %s -> %s"),*Kind.ToString(),*R.Id.ToString(),*Bounds.ToString(),*R.Minimum.ToString(),*R.Maximum.ToString()); ReleaseDetails(); return false; }
        }
        if(R.Kind!=TEXT("roof") && R.Kind!=TEXT("stairs"))
        {
            auto* Light=NewObject<UPointLightComponent>(this,NAME_None,RF_Transient);
            Light->SetupAttachment(BuildingRoot); Light->SetMobility(EComponentMobility::Movable);
            Light->SetRelativeLocation(R.StandingPoint+FVector(0,0,FMath::Min(240.0,R.Maximum.Z-R.Minimum.Z-30)));
            Light->SetIntensity(1200); Light->SetAttenuationRadius(420); Light->SetCastShadows(false);
            Light->SetLightColor(FLinearColor(1,.84,.65)); AddInstanceComponent(Light); Light->RegisterComponent(); Details.Add(Light);
        }
        if(!R.Unit.IsEmpty() || R.Kind==TEXT("hall"))
        {
            auto* Sign=NewObject<UTextRenderComponent>(this,NAME_None,RF_Transient);
            Sign->SetupAttachment(BuildingRoot); Sign->SetMobility(EComponentMobility::Movable);
            Sign->SetText(FText::FromString(FString::Printf(TEXT("%dF %s"),R.Floor,R.Kind==TEXT("hall" )?TEXT("Stairs / Exit"):*R.Unit)));
            Sign->SetWorldSize(13); Sign->SetTextRenderColor(FColor(245,224,170));
            Sign->SetRelativeLocation(FVector(R.Minimum.X+3,R.Minimum.Y+45,R.Minimum.Z+195));
            Sign->SetRelativeRotation(FRotator(0,0,0)); Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            AddInstanceComponent(Sign); Sign->RegisterComponent(); Details.Add(Sign);
        }
    }
    for(auto& G:Groups)
    {
        auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,NAME_None,RF_Transient);
        C->SetupAttachment(BuildingRoot); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(Meshes[G.Key]);
        C->SetCollisionProfileName(TEXT("BlockAllDynamic")); C->SetCanEverAffectNavigation(false);
        C->bAutoRebuildTreeOnInstanceChanges=false; C->AddInstances(G.Value,false,false,false);
        AddInstanceComponent(C); C->RegisterComponent(); C->BuildTreeIfOutdated(false,true); Details.Add(C);
    }
    return true;
}
