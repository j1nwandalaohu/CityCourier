#pragma once
#include "Engine/DataAsset.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "CourierInteriorFinish.generated.h"

class UStaticMesh;
class ACourierBuildingActor;

USTRUCT()
struct FCourierFinishGroup
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY() bool bSolid=false;
    UPROPERTY() TArray<FTransform> Transforms;
};

USTRUCT()
struct FCourierFinishBuilding
{
    GENERATED_BODY()
    UPROPERTY() TArray<FCourierFinishGroup> Groups;
};

/** Separate cooked art overlay: canonical rooms, routes and doors remain authoritative. */
UCLASS(BlueprintType)
class CITYCOURIER_API UCourierInteriorFinish : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY() FString CanonicalDigest;
    UPROPERTY() TMap<FName,FCourierFinishBuilding> Buildings;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> DoorMeshes;
    UFUNCTION(BlueprintCallable,Category="Courier|Art") bool ImportJsonFile(const FString& Filename,UCityBuildingCatalog* Catalog);
    static UCourierInteriorFinish* Load();
    static int32 DoorStyle(const FCourierBuildingDefinition& B,const FCourierDoorDefinition& Door);
    bool Apply(ACourierBuildingActor* Actor) const;
    bool Matches(const UCityBuildingCatalog* Catalog) const;
    bool IsAvailable(FName BuildingId) const;
};
