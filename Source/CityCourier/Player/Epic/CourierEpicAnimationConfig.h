#pragma once
#include "Engine/DataAsset.h"
#include "CourierEpicAnimationConfig.generated.h"

class USkeletalMesh;
class USkeleton;
class UAnimMontage;
class UPoseSearchDatabase;
class UControlRig;
class UPhysicsAsset;
class UAnimInstance;
class UIKRetargeter;
class UAnimSequence;

UCLASS(BlueprintType)
class CITYCOURIER_API UCourierEpicAnimationConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> TargetMesh;
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> SourceMesh;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UIKRetargeter> Retargeter;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<TSoftObjectPtr<class UAnimationAsset>> SourceAnimations;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TSoftObjectPtr<UAnimSequence>> RetargetedSequences;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TSoftObjectPtr<UAnimMontage>> RetargetedMontages;
#endif
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UPoseSearchDatabase> LocomotionDatabase;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UPoseSearchDatabase> SprintDatabase;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float JogReferenceSpeed=505;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float SprintReferenceSpeed=706;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UPoseSearchDatabase> AirDatabase;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<class UChooserTable> DatabaseChooser;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UAnimMontage> VaultMontage;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UAnimMontage> MantleMontage;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UAnimSequence> StandingLanding;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UAnimSequence> MovingLanding;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftClassPtr<UControlRig> ContactRig;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UPhysicsAsset> PhysicsAsset;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftClassPtr<UAnimInstance> AnimationClass;
    // Set only by the final editor validation, never by the asset creation step.
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString ValidationFingerprint;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="80")) float MinimumTraversalHeight=80;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="50")) float MaximumVaultHeight=100;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="100",ClampMax="110")) float MaximumMantleHeight=110;
    bool ValidateFor(const USkeletalMesh* Mesh,FText& OutReason) const;
};
