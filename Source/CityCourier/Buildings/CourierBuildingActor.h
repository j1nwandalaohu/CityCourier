#pragma once
#include "GameFramework/Actor.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "CourierBuildingActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

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
    const FCourierBuildingDefinition* GetDefinition() const;
    bool SetDoorProxy(int32 DoorIndex,float Angle,bool bVisible);
    FTransform DoorProxyTransform(int32 DoorIndex,float Angle) const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Courier|Buildings") TObjectPtr<UCityBuildingCatalog> Catalog;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Courier|Buildings") int32 BuildingIndex=INDEX_NONE;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> BuildingRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Exterior;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> DoorProxies;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Structure;
    UPROPERTY() TArray<int32> ProxyDoors;
};
