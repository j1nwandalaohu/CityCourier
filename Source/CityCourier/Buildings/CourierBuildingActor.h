#pragma once
#include "GameFramework/Actor.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "CourierBuildingActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct CITYCOURIER_API FCourierPhysicalTraversalReport
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FString> Errors;
    UPROPERTY(BlueprintReadOnly) TArray<FName> VisitedRooms;
    UPROPERTY(BlueprintReadOnly) int32 VisitedNodes=0;
    UPROPERTY(BlueprintReadOnly) int32 OpenedDoors=0;
    UPROPERTY(BlueprintReadOnly) double WalkedMeters=0;
    UPROPERTY(BlueprintReadOnly) bool bReturnedToStreet=false;
};

/** Resident geometry never disappears when room details are evicted. */
UCLASS()
class CITYCOURIER_API ACourierBuildingActor : public AActor
{
    GENERATED_BODY()
public:
    ACourierBuildingActor();
    UFUNCTION(BlueprintCallable,Category="Courier|Buildings") bool ConfigureBuilding(UCityBuildingCatalog* InCatalog,int32 Index);
    UFUNCTION(BlueprintCallable,CallInEditor,Category="Courier|Buildings") bool RebuildStructure();
    UFUNCTION(BlueprintPure,Category="Courier|Buildings") int32 GetStructuralBoxCount() const;
    UFUNCTION(BlueprintPure,Category="Courier|Buildings") int32 GetDoorProxyCount() const;
    UFUNCTION(BlueprintCallable,Category="Courier|Buildings") TArray<FString> GetPhysicalRoomErrors() const;
    UFUNCTION(BlueprintCallable,Category="Courier|Buildings") FCourierPhysicalTraversalReport ValidatePhysicalTraversal(bool bWithDetails=true);
    const FCourierBuildingDefinition* GetDefinition() const;
    bool SetDoorProxy(int32 DoorIndex,float Angle,bool bVisible);
    bool PrepareDetails();
    void ReleaseDetails();
    void EnsureEntranceBarriers();
    void SetEntryLoading(bool bLoading);
    int32 GetDetailComponentCount() const { return Details.Num(); }
    FTransform DoorProxyTransform(int32 DoorIndex,float Angle) const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Courier|Buildings") TObjectPtr<UCityBuildingCatalog> Catalog;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Courier|Buildings") int32 BuildingIndex=INDEX_NONE;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> BuildingRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Exterior;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> DoorProxies;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Structure;
    UPROPERTY() TArray<int32> ProxyDoors;
    UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Details;
    UPROPERTY(Transient) TArray<TObjectPtr<class UBoxComponent>> EntryBarriers;
};
