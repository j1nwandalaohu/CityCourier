#pragma once
#include "Animation/AnimInstance.h"
#include "Player/CourierActionTimeline.h"
#include "CourierAnimInstance.generated.h"
UCLASS()
class CITYCOURIER_API UCourierAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    static constexpr int32 RequiredEpicGraphVersion=11;
    // Stored in the generated class defaults; old saved graphs retain zero.
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Courier|Epic") int32 EpicGraphVersion=0;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Courier|Epic") bool bUseEpicGraph=false;
    UFUNCTION(BlueprintPure) bool IsEpicAnimationEnabled() const { return bUseEpicGraph; }
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") FVector WorldVelocity=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") FVector WorldAcceleration=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") int32 EpicPostureIndex=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") bool bUseAirDatabase=false;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") bool bUseSprintDatabase=false;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") float JogPlayRate=1;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") float SprintPlayRate=1;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Epic") float LocomotionLegWeight=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") FTransform LeftHandGoal;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") FTransform RightHandGoal;
    // Foot translation is a component-space surface point; rotation is the
    // surface tilt relative to component up, applied to the incoming foot pose.
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") FTransform LeftFootGoal;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") FTransform RightFootGoal;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") float LeftHandAlpha=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") float RightHandAlpha=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") float LeftFootAlpha=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") float RightFootAlpha=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") float ContactPelvisOffset=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Contact") float ContactEnabled=1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Courier") TArray<TObjectPtr<class UAnimSequence>> Clips;
    UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> PostureClips;
    static int32 SelectClip(float Speed,bool bMounted) { return bMounted?3:Speed<8?0:Speed<520?1:2; }
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeInitializeAnimation() override;
    UPROPERTY(BlueprintReadOnly,Category="Courier") float GroundSpeed=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier") bool bRiding=false;
    UPROPERTY(BlueprintReadOnly,Category="Courier") float SteeringAmount=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier") int32 AnimationIndex=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Actions") bool bAirborne=false;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Actions") ECourierAction CurrentAction=ECourierAction::None;
    UPROPERTY(BlueprintReadOnly,Category="Courier|Actions") float ActionElapsed=0;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
