#pragma once
#include "Components/ActorComponent.h"
#include "RiderRecoveryComponent.generated.h"
UCLASS(ClassGroup=Movement,meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API URiderRecoveryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URiderRecoveryComponent();
    void Configure(class UCityDefinition* Definition,class AScooterPawn* Vehicle,class ACourierCharacter* Courier,class UDeliverySubsystem* Orders,class ARiderStation* RestStation);
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
    UFUNCTION(BlueprintCallable) void RecordSafeLocation(const FTransform& Transform);
    UFUNCTION(BlueprintCallable) void SetRecoverHeld(bool Held);
    UFUNCTION(BlueprintCallable) bool TryRecover();
    UFUNCTION(BlueprintCallable) bool TryRescue();
    UFUNCTION(BlueprintPure) bool CanRescue() const;
    UFUNCTION(BlueprintPure) float GetHoldProgress() const { return FMath::Clamp(HeldSeconds/2.f,0.f,1.f); }
private:
    bool ValidateLocation(const FTransform& Candidate,FTransform& VehicleTransform,FVector& FootPosition) const;
    bool FindDestination(bool StationOnly,FTransform& VehicleTransform,FVector& FootPosition) const;
    void Relocate(const FTransform& VehicleTransform,FVector FootPosition);
    UPROPERTY() TObjectPtr<UCityDefinition> City;
    UPROPERTY() TObjectPtr<UDeliverySubsystem> Delivery;
    TWeakObjectPtr<AScooterPawn> Bike;
    TWeakObjectPtr<ACourierCharacter> Rider;
    TWeakObjectPtr<ARiderStation> Station;
    FTransform SafeLocation;
    bool bHasSafe=false,bHeld=false,bUsedHold=false;
    float HeldSeconds=0,SampleSeconds=0;
};
