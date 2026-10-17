#pragma once
#include "Ambient/AmbientVehicle.h"
#include "AmbientPedestrian.generated.h"
UCLASS()
class CITYCOURIER_API AAmbientPedestrian : public AAmbientVehicle
{
    GENERATED_BODY()
public:
    AAmbientPedestrian();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> Person;
};
