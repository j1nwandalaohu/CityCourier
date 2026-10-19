#pragma once
#include "GameFramework/Actor.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "Player/CourierInteractable.h"
#include "Core/CourierActionResult.h"
#include "CourierDoor.generated.h"

USTRUCT()
struct CITYCOURIER_API FCourierDoorState
{
    GENERATED_BODY()
    UPROPERTY() float Angle=0;
    UPROPERTY() bool bWantOpen=false;
    UPROPERTY() bool bBlocked=false;
};
UCLASS()
class CITYCOURIER_API ACourierDoor : public AActor, public ICourierInteractable
{
    GENERATED_BODY()
public:
    ACourierDoor();
    void Configure(const FCourierDoorDefinition& Definition,const FTransform& BuildingTransform,const FCourierDoorState& Initial={});
    virtual void Tick(float DeltaSeconds) override;
    void AdvanceDoor(float DeltaSeconds);
    FCourierActionResult TryToggle(ACourierCharacter* Courier);
    virtual bool CanInteract(ACourierCharacter* Courier,FText& Reason) const override;
    virtual bool Interact(ACourierCharacter* Courier) override;
    virtual int32 GetInteractionPriority() const override { return 50; }
    virtual FVector GetInteractionLocation() const override;
    virtual FText GetInteractionText() const override;
    const FCourierDoorState& GetDoorState() const { return State; }
    const FCourierDoorDefinition& GetDefinition() const { return Definition; }
    bool IsMoving() const { return bMoving; }
    void SetReady(bool bValue) { bReady=bValue && bConfigurationValid; }
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDoorState,FName,const FCourierDoorState&);
    FOnDoorState OnDoorState;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Pivot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UBoxComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Visual;
private:
    UPROPERTY() FCourierDoorDefinition Definition;
    UPROPERTY() FCourierDoorState State;
    bool bMoving=false,bReady=false,bConfigurationValid=false;
    FQuat ClosedRotation=FQuat::Identity;
    void ApplyAngle();
    bool IsAngularSliceBlocked(float From,float To) const;
};
