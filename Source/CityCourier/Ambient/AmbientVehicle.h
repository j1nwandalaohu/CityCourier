#pragma once
#include "GameFramework/Actor.h"
#include "AmbientVehicle.generated.h"
UCLASS()
class CITYCOURIER_API AAmbientVehicle : public AActor
{
    GENERATED_BODY()
public:
    AAmbientVehicle();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable) void SetPatrol(const TArray<FVector>& Points);
    UFUNCTION(BlueprintCallable) void SetPopulationActive(bool bActive);
    UFUNCTION(BlueprintPure) float GetSpeedCm() const { return Speed; }
    bool IsPopulationActive() const { return bPopulationActive; }
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Visual;
protected:
    float MaxSpeed=700,Acceleration=140,Deceleration=400;
    float Speed=0;
    bool bPopulationActive=false;
    bool bPedestrian=false;
    TArray<FVector> Patrol;
    int32 NextPoint=1;
};
