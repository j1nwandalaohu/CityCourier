#pragma once
#include "World/DeliveryLocation.h"
#include "Profile/PlayerProfile.h"
#include "Core/CourierActionResult.h"
#include "RiderStation.generated.h"
UCLASS()
class CITYCOURIER_API ARiderStation : public ADeliveryLocation
{
    GENERATED_BODY()
public:
    FCourierActionResult CheckService(ACourierCharacter* Courier) const;
    FCourierActionResult BeginChargeWithFeedback(ACourierCharacter* Courier);
    FCourierActionResult TryPurchaseWithFeedback(EUpgradeKind Kind,ACourierCharacter* Courier);
    FText GetServiceMessage() const { return ServiceMessage; }
    ARiderStation();
    void Configure(class UProfileSubsystem* Profile,class AScooterPawn* Vehicle);
    virtual void Tick(float DeltaSeconds) override;
    virtual bool Interact(ACourierCharacter* Courier) override;
    UFUNCTION(BlueprintCallable) bool BeginCharge(ACourierCharacter* Courier);
    UFUNCTION(BlueprintCallable) void CancelCharge();
    bool TryPurchase(EUpgradeKind Kind,ACourierCharacter* Courier);
    UFUNCTION(BlueprintPure) bool IsCharging() const { return ChargingRider.IsValid(); }
    UFUNCTION(BlueprintPure) float GetChargeProgress() const { return ChargeSeconds/5.f; }
private:
    FText ServiceMessage;
    UPROPERTY() TObjectPtr<UProfileSubsystem> PlayerProfile;
    TWeakObjectPtr<AScooterPawn> Bike;
    TWeakObjectPtr<ACourierCharacter> ChargingRider;
    float ChargeSeconds=0;
};
