#include "Buildings/CourierInteriorFinish.h"
#include "Buildings/CourierBuildingActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#if WITH_EDITOR
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#endif

UCourierInteriorFinish* UCourierInteriorFinish::Load()
{
    const TCHAR* Package=TEXT("/Game/CityCourier/Art/Interiors/DA_InteriorFinish");
    return FPackageName::DoesPackageExist(Package)?LoadObject<UCourierInteriorFinish>(nullptr,TEXT("/Game/CityCourier/Art/Interiors/DA_InteriorFinish.DA_InteriorFinish")):nullptr;
}
bool UCourierInteriorFinish::Matches(const UCityBuildingCatalog* Catalog) const
{
    if(!Catalog || DoorMeshes.Num()!=3 || (!Catalog->SourceDigest.IsEmpty() && CanonicalDigest!=Catalog->SourceDigest)) return false;
    for(const auto& Mesh:DoorMeshes) if(!Mesh) return false;
    for(const auto& B:Catalog->Buildings) if(!Buildings.Contains(B.Id)) return false;
    return true;
}
bool UCourierInteriorFinish::IsAvailable(FName BuildingId) const
{
    const auto* B=Buildings.Find(BuildingId); if(!B || B->Groups.IsEmpty() || DoorMeshes.Num()!=3) return false;
    for(const auto& Mesh:DoorMeshes) if(!Mesh || !FPackageName::DoesPackageExist(Mesh->GetOutermost()->GetName())) return false;
    for(const auto& G:B->Groups) if(!G.Mesh || G.Transforms.IsEmpty() || !FPackageName::DoesPackageExist(G.Mesh->GetOutermost()->GetName())) return false;
    return true;
}
int32 UCourierInteriorFinish::DoorStyle(const FCourierBuildingDefinition& B,const FCourierDoorDefinition& Door)
{
    const int32 I=B.FindRoom(Door.ToRoom); const auto* R=B.Rooms.IsValidIndex(I)?&B.Rooms[I]:nullptr;
    if(R && R->Floor==1 && (B.Kind==TEXT("shop") || R->Kind==TEXT("shop") || R->Kind==TEXT("service"))) return 0;
    return B.Kind==TEXT("office")?2:1;
}
bool UCourierInteriorFinish::ImportJsonFile(const FString& Filename,UCityBuildingCatalog* Catalog)
{
#if WITH_EDITOR
    FString Text; TSharedPtr<FJsonObject> Root;
    if(!Catalog || !FFileHelper::LoadFileToString(Text,*Filename) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root) return false;
    const TArray<TSharedPtr<FJsonValue>>* Records;
    if(!Root->TryGetArrayField(TEXT("buildings"),Records) || Records->Num()!=Catalog->Buildings.Num()) return false;
    TMap<FName,FCourierFinishBuilding> Imported;
    for(const auto& Value:*Records)
    {
        auto Record=Value->AsObject(); if(!Record) return false;
        FString Id; if(!Record->TryGetStringField(TEXT("id"),Id)) return false;
        FName Key(*Id); if(Catalog->FindBuilding(Key)==INDEX_NONE || Imported.Contains(Key)) return false;
        const TArray<TSharedPtr<FJsonValue>>* Groups; if(!Record->TryGetArrayField(TEXT("groups"),Groups) || Groups->IsEmpty()) return false;
        FCourierFinishBuilding B;
        for(const auto& GroupValue:*Groups)
        {
            auto Group=GroupValue->AsObject(); FString MeshName; bool Solid;
            const TArray<TSharedPtr<FJsonValue>>* Transforms;
            if(!Group || !Group->TryGetStringField(TEXT("mesh"),MeshName) || !MeshName.StartsWith(TEXT("SM_IF_")) || MeshName.Contains(TEXT("/")) ||
                !Group->TryGetBoolField(TEXT("solid"),Solid) || !Group->TryGetArrayField(TEXT("transforms"),Transforms) || Transforms->IsEmpty()) return false;
            FCourierFinishGroup G; G.bSolid=Solid;
            G.Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/CityCourier/Art/Interiors/")+MeshName+TEXT(".")+MeshName)); if(!G.Mesh) return false;
            for(const auto& Transform:*Transforms)
            {
                const auto& A=Transform->AsArray(); if(A.Num()!=9) return false;
                double N[9]; for(int32 J=0;J<9;++J) if(!A[J]->TryGetNumber(N[J]) || !FMath::IsFinite(N[J])) return false;
                if(N[6]<=0 || N[7]<=0 || N[8]<=0) return false;
                G.Transforms.Emplace(FRotator(N[3],N[4],N[5]),FVector(N[0],N[1],N[2]),FVector(N[6],N[7],N[8]));
            }
            B.Groups.Add(MoveTemp(G));
        }
        Imported.Add(Key,MoveTemp(B));
    }
    TArray<TObjectPtr<UStaticMesh>> Leaves;
    for(const TCHAR* Kind:{TEXT("Shop"),TEXT("Residential"),TEXT("Office")})
    {
        const FString Name=FString(TEXT("SM_IF_Door"))+Kind;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/CityCourier/Art/Interiors/")+Name+TEXT(".")+Name)); if(!Mesh) return false; Leaves.Add(Mesh);
    }
    Modify(); Buildings=MoveTemp(Imported); DoorMeshes=MoveTemp(Leaves); CanonicalDigest=Catalog->SourceDigest; MarkPackageDirty(); return true;
#else
    return false;
#endif
}
bool UCourierInteriorFinish::Apply(ACourierBuildingActor* Actor) const
{
    const auto* B=Actor?Actor->GetDefinition():nullptr;
    if(!B || !Matches(Actor->Catalog) || !IsAvailable(B->Id)) return false;
    const auto* Art=Buildings.Find(B->Id); if(!Art) return false;
    for(const auto& Group:Art->Groups) if(!Group.Mesh || Group.Transforms.IsEmpty()) return false;
    for(const auto& Group:Art->Groups)
    {
        auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor,NAME_None,RF_Transient);
        C->SetupAttachment(Actor->BuildingRoot); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(Group.Mesh);
        C->SetCollisionProfileName(Group.bSolid?TEXT("BlockAllDynamic"):TEXT("NoCollision")); C->SetCanEverAffectNavigation(false);
        C->bAutoRebuildTreeOnInstanceChanges=false; C->AddInstances(Group.Transforms,false,false,false);
        Actor->AddInstanceComponent(C); C->RegisterComponent(); C->BuildTreeIfOutdated(false,true); Actor->Details.Add(C);
        if(Group.Mesh->GetFName()!=TEXT("SM_IF_LightPanel")) continue;
        for(const auto& T:Group.Transforms)
        {
            auto* Light=NewObject<URectLightComponent>(Actor,NAME_None,RF_Transient);
            Light->SetupAttachment(Actor->BuildingRoot); Light->SetMobility(EComponentMobility::Movable);
            // Light +X points out through fixture local -Z, including wall lamps.
            Light->SetRelativeLocation(T.TransformPosition(FVector(0,0,-1.5)));
            Light->SetRelativeRotation(FRotationMatrix::MakeFromXY(T.TransformVectorNoScale(-FVector::UpVector),T.TransformVectorNoScale(FVector::ForwardVector)).Rotator());
            // The city uses a fixed gameplay exposure. A full 1800 lm per small
            // room clipped plaster and furniture to white, especially where
            // ceiling and landing fixtures overlap. Keep use-specific contrast.
            const float Lumens=B->Kind==TEXT("office")?300.f:B->Kind==TEXT("shop")?240.f:180.f;
            Light->SetIntensityUnits(ELightUnits::Lumens); Light->SetIntensity(Lumens); Light->SetAttenuationRadius(650);
            Light->SetSourceWidth(90); Light->SetSourceHeight(14); Light->SetCastShadows(true);
            Light->MaxDrawDistance=2500; Light->MaxDistanceFadeRange=500;
            Light->SetUseTemperature(true); Light->SetTemperature(B->Kind==TEXT("shop")?4500:B->Kind==TEXT("office")?5000:3800);
            Actor->AddInstanceComponent(Light); Light->RegisterComponent(); Actor->Details.Add(Light);
        }
    }
    return true;
}
