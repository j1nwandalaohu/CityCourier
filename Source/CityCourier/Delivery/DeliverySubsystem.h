#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "Core/DeliveryTypes.h"
#include "Profile/PlayerProfile.h"
#include "Core/CourierActionResult.h"
#include "DeliverySubsystem.generated.h"
class UCityDefinition;
class UProfileSubsystem;
class AScooterPawn;
class ACourierCharacter;
class ADeliveryLocation;
DECLARE_MULTICAST_DELEGATE(FOnDeliveryChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDeliveryFinished, const FSettlement&);

UCLASS()
class CITYCOURIER_API UDeliverySubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void Tick(float DeltaSeconds) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDeliverySubsystem,STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate() && bConfigured; }
    bool Configure(UCityDefinition* Definition,UProfileSubsystem* PlayerProfile,AScooterPawn* Vehicle);
    void RegisterLocation(ADeliveryLocation* Location);
    void UnregisterLocation(ADeliveryLocation* Location);
    bool RefreshOffers(FVector RiderLocation);
    const TArray<FDeliveryOffer>& GetOffers() const { return Offers; }
    bool TryAccept(FGuid InstanceId);
    FCourierActionResult TryAcceptWithFeedback(FGuid InstanceId);
    bool GetOfferPreview(FGuid InstanceId,FDeliveryOffer& OutOffer) const;
    AScooterPawn* GetScooter() const;
    UFUNCTION(BlueprintCallable) bool TryPickup(FName Merchant,ACourierCharacter* Courier);
    UFUNCTION(BlueprintCallable) bool TryDeliver(FName Destination,ACourierCharacter* Courier);
    bool TryCancel(EDeliveryFailure Reason);
    void ApplyRecoveryPenalty();
    const FDeliveryState& GetState() const { return State; }
    UFUNCTION(BlueprintPure) FName GetTargetId() const;
    UFUNCTION(BlueprintPure) double GetDeadlineSeconds() const { return State.Offer.DeadlineSeconds; }
    UFUNCTION(BlueprintPure) ADeliveryLocation* FindLocationActor(FName Id) const;
    const FSettlement& GetLastSettlement() const { return LastSettlement; }
    UCityDefinition* GetCity() const { return City; }
    FOnDeliveryChanged OnDeliveryChanged;
    FOnDeliveryFinished OnDeliveryFinished;
#if WITH_DEV_AUTOMATION_TESTS
    void SetClockForTests(TFunction<double()> Clock) { TestClock = MoveTemp(Clock); }
#endif
private:
    double ReadClock() const;
    void SynchronizeClock();
    void FinishOnce(int64 PayoutCents = 0);
    void UpdateTargets();
    bool ValidateCourier(FName Id,ACourierCharacter* Courier) const;
    void HandleImpact(float ClosingSpeed);
    FVector CurrentRiderLocation() const;
    bool AreRoutesClear(const FDeliveryOffer& Offer) const;
    UPROPERTY() TObjectPtr<UCityDefinition> City;
    UPROPERTY() TObjectPtr<UProfileSubsystem> Profile;
    TWeakObjectPtr<AScooterPawn> Scooter;
    TArray<FDeliveryOffer> Offers;
    FDeliveryState State;
    FSettlement LastSettlement;
    TSet<FGuid> Finished;
    FVector LastRiderLocation = FVector::ZeroVector;
    double LastClock = 0;
    bool bConfigured = false;
#if WITH_DEV_AUTOMATION_TESTS
    TFunction<double()> TestClock;
#endif
};
