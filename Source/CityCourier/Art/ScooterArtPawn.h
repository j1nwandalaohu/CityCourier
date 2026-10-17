#pragma once
#include "Vehicles/ScooterPawn.h"
#include "ScooterArtPawn.generated.h"

UCLASS()
class CITYCOURIER_API AScooterArtPawn : public AScooterPawn
{
    GENERATED_BODY()
public:
    AScooterArtPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> BodyArt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> FrontWheelArt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> RearWheelArt;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<USceneComponent>> RiderAnchors;
};
