#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "Buildings/CourierDoor.h"
#include "CourierBuildingSubsystem.generated.h"

class ACourierBuildingActor;
class ADeliveryLocation;
enum class ECourierBuildingDetailState:uint8 { Unloaded,Loading,Ready,Failed };
struct FCourierIndoorAddress
{
    int32 Building=INDEX_NONE;
    int32 Room=INDEX_NONE;
    int32 Floor=0;
    bool IsValid() const { return Building!=INDEX_NONE && Room!=INDEX_NONE; }
};

UCLASS()
class CITYCOURIER_API UCourierBuildingSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    bool Configure(UCityBuildingCatalog* Input);
    bool IsEnabled() const { return bEnabled; }
    const UCityBuildingCatalog* GetCatalog() const { return Catalog.Get(); }
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game || Type==EWorldType::PIE; }
    virtual bool IsTickable() const override { return !IsTemplate() && bEnabled; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCourierBuildingSubsystem,STATGROUP_Tickables); }
    virtual void Tick(float DeltaSeconds) override;
    virtual void Deinitialize() override;
    // Courier capsule center (88cm half height); explicit feet API for vehicles.
    FCourierIndoorAddress Locate(FVector Position) const;
    FCourierIndoorAddress LocateFeet(FVector Foot) const;
    void UpdateResidency(FVector Position,FName TargetBuilding=NAME_None);
    bool PrepareNext();
    ECourierBuildingDetailState GetDetailState(int32 Index) const;
    void RequestBuilding(int32 Index);
    ACourierDoor* ActivateDoor(FName DoorId);
    void RefreshDoors(FVector Position);
    void DeactivateFarDoors(FVector Position);
    TArray<ACourierDoor*> GetNearbyDoors(FVector Position,double Radius=500) const;
    TArray<AActor*> GetInteractionCandidates(FVector Position,double Radius=500) const;
    bool RegisterInteractable(AActor* Actor);
    void UnregisterInteractable(AActor* Actor);
    ADeliveryLocation* GetManagedLocation(FName LocationId) const;
    int32 GetReadyCount() const;
    int32 GetActiveDoorCount() const { return ActiveDoors.Num(); }
    double GetMaxPreparationMs() const { return MaxPreparationMs; }
    bool RegisterIndoorLocation(ADeliveryLocation* Location);
    void UnregisterIndoorLocation(ADeliveryLocation* Location);
    bool CanPrepareLocation(FName Id) const;
    bool CanUseLocation(FName Id,FVector Position,FText& Reason) const;
private:
    UPROPERTY() TObjectPtr<UCityBuildingCatalog> Catalog;
    UPROPERTY() TMap<int32,TObjectPtr<ACourierBuildingActor>> Actors;
    UPROPERTY() TMap<FName,TObjectPtr<ACourierDoor>> ActiveDoors;
    UPROPERTY() TMap<FName,TObjectPtr<ADeliveryLocation>> Locations;
    TMap<FName,FCourierDoorState> DoorStates;
    TMap<FName,FIntPoint> DoorLookup;
    TMap<FIntVector,TArray<FIntPoint>> DoorCells;
    TArray<TWeakObjectPtr<AActor>> OtherInteractables;
    TArray<ECourierBuildingDetailState> DetailStates;
    TArray<int32> PendingLoads;
    TSet<int32> Desired;
    FDelegateHandle SpawnHandle;
    FVector LastPosition=FVector::ZeroVector;
    float UpdateTime=0;
    double MaxPreparationMs=0;
    int32 EntryRequest=INDEX_NONE;
    bool bEnabled=false;
    void CacheActor(AActor* Actor);
    void BindLocations();
    void DeactivateDoor(FName Id);
    static FIntVector Cell(FVector Position);
};
