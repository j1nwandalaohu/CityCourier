#pragma once
#include "GameFramework/Pawn.h"
#include "Player/CourierInteractable.h"
#include "Profile/PlayerProfile.h"
#include "InputActionValue.h"
#include "ScooterPawn.generated.h"
class ACourierCharacter;
class UScooterMovementComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnScooterImpact, float);
DECLARE_MULTICAST_DELEGATE(FOnScooterLowBattery);

UCLASS()
class CITYCOURIER_API AScooterPawn : public APawn, public ICourierInteractable
{
    GENERATED_BODY()
public:
    AScooterPawn();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual UPawnMovementComponent* GetMovementComponent() const override;
    UScooterMovementComponent* GetScooterMovement() const { return Movement; }
    UFUNCTION(BlueprintCallable) bool TryMount(ACourierCharacter* Courier);
    UFUNCTION(BlueprintCallable) bool TryDismount();
    UFUNCTION(BlueprintCallable) void SetDriveInput(float Throttle, float Steering, bool bBrake);
    void ClearDriveInput();
    UFUNCTION(BlueprintPure) float GetSpeedKmh() const;
    UFUNCTION(BlueprintPure) float GetBatteryFraction() const { return BatteryFraction; }
    void SetBatteryFraction(float Fraction);
    void ApplyUpgrades(const FPlayerProfile& Profile);
    ACourierCharacter* GetRider() const { return Rider.Get(); }
    void ConsumeTravel(double DistanceMeters);
    void UpdateVisualDrive(float Steering, float Pitch, float DeltaSeconds);
    virtual bool CanInteract(ACourierCharacter* Courier, FText& OutReason) const override;
    virtual bool Interact(ACourierCharacter* Courier) override { return TryMount(Courier); }
    virtual int32 GetInteractionPriority() const override { return 10; }
    virtual FVector GetInteractionLocation() const override { return GetActorLocation(); }
    virtual FText GetInteractionText() const override { return FText::FromString(TEXT("E  骑上电动车")); }
    FOnScooterImpact OnEffectiveImpact;
    FOnScooterLowBattery OnLowBattery;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UBoxComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UCameraComponent> FollowCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> PrototypeParts;
private:
    bool FindDismountLocation(FVector& OutLocation) const;
    void Drive(const FInputActionValue& Value);
    void StopDrive();
    void Look(const FInputActionValue& Value);
    void BrakeStart();
    void BrakeStop();
    void DismountPressed();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UScooterMovementComponent> Movement;
    UPROPERTY() TArray<TObjectPtr<class UInputAction>> InputActions;
    TWeakObjectPtr<ACourierCharacter> Rider;
    float BatteryFraction = 1;
    int32 BatteryLevel = 0;
    FVector2D DriveAxis = FVector2D::ZeroVector;
    bool bBrakeHeld = false;
    double LastLookTime = -10;
};
