#pragma once
#include "GameFramework/Actor.h"
#include "Ambient/CitizenNavigation.h"
#include "Ambient/CitizenCrowdCoordination.h"
#include "Player/CourierStrikeReceiver.h"
#include "AmbientTrafficManager.generated.h"
UCLASS()
class CITYCOURIER_API AAmbientTrafficManager : public AActor
{
    GENERATED_BODY()
public:
    AAmbientTrafficManager();
    void Configure(class UCityDefinition* Definition);
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable) void UpdatePopulation(FVector RiderLocation);
    UFUNCTION(BlueprintPure) int32 GetActiveVehicleCount() const;
    UFUNCTION(BlueprintPure) int32 GetActivePedestrianCount() const;
    void QueueCitizenPath(const FCitizenPathRequest& Request);
    void NotifyCitizenAssault(class AAmbientPedestrian& Victim,const FCourierStrikeContext& Context);
    bool TryAcquireCitizenAttack(class AAmbientPedestrian& Citizen,AActor& Target);
    bool TryJoinCitizenFight(class AAmbientPedestrian& Citizen,AActor& Target);
    bool OwnsCitizenAttack(const class AAmbientPedestrian& Citizen,const AActor& Target) const;
    void ReleaseCitizenAttack(class AAmbientPedestrian& Citizen,bool bLeaveGroup=true);
private:
    void ProcessOneCitizenPath();
    TArray<FCitizenPathRequest> PathRequests;
    FCitizenCrowdCoordination Coordination;
    UPROPERTY() TObjectPtr<class UCityDefinition> City;
    UPROPERTY() TArray<TObjectPtr<class AAmbientVehicle>> Cars;
    UPROPERTY() TArray<TObjectPtr<class AAmbientPedestrian>> Walkers;
    float RefreshTime=0;
};
