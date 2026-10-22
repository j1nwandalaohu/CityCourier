#pragma once
#include "Components/ActorComponent.h"
#include "CourierPhysicalReactionComponent.generated.h"

UCLASS(ClassGroup=(Courier),meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API UCourierPhysicalReactionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCourierPhysicalReactionComponent();
    bool ApplyLocalImpulse(FName Bone,FVector Impulse);
    void Cancel();
    bool IsActive() const { return !SavedBodies.IsEmpty(); }
    float GetImpulseUsed() const { return ImpulseUsed; }
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere,Category="Reaction") float MaximumImpulse=2500;
    UPROPERTY(EditAnywhere,Category="Reaction") float RecoverySeconds=.45f;
private:
    struct FSavedBody { FName Name; bool bSimulating; float Blend; };
    TArray<FSavedBody> SavedBodies;
    TWeakObjectPtr<class USkeletalMeshComponent> ActiveMesh;
    UPROPERTY(Transient) TObjectPtr<class UPhysicalAnimationComponent> PhysicalAnimation;
    float Elapsed=0,ImpulseUsed=0;
    ECollisionEnabled::Type SavedCollision=ECollisionEnabled::NoCollision;
    bool bSavedBlendPhysics=false;
    FCollisionResponseContainer SavedResponses;
};
