#pragma once
#include "GameFramework/Actor.h"
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
private:
    UPROPERTY() TObjectPtr<class UCityDefinition> City;
    UPROPERTY() TArray<TObjectPtr<class AAmbientVehicle>> Cars;
    UPROPERTY() TArray<TObjectPtr<class AAmbientPedestrian>> Walkers;
    float RefreshTime=0;
};
