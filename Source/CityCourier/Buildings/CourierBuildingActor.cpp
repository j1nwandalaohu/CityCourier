#include "Buildings/CourierBuildingActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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
