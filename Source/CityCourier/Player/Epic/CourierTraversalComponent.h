#pragma once
#include "Components/ActorComponent.h"
#include "Core/CourierActionResult.h"
#include "CourierTraversalComponent.generated.h"

USTRUCT()
struct FCourierTraversalCandidate
{
    GENERATED_BODY()
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<class UPrimitiveComponent> Component;
    FVector LocalEntry=FVector::ZeroVector;
    FVector LocalExit=FVector::ZeroVector;
    FTransform InitialTransform;
    bool bMantle=false;
    float Height=0;
    UPROPERTY() TObjectPtr<class UAnimMontage> Montage=nullptr;
};

UCLASS(ClassGroup=(Courier),meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API UCourierTraversalComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCourierTraversalComponent();
    bool FindCandidate(FCourierTraversalCandidate& OutCandidate) const;
    FCourierActionResult TryStart();
    void Cancel();
    bool IsActive() const { return bActive; }
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    bool LandingClear(const FVector& Feet) const;
    bool bActive=false;
    UPROPERTY() FCourierTraversalCandidate Candidate;
    float Elapsed=0;
    uint8 SavedMode=0;
    uint8 SavedCustomMode=0;
    bool bSavedOrientRotation=true;
};
