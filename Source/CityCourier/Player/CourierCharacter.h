#pragma once
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Core/CourierActionResult.h"
#include "Player/CourierStrikeReceiver.h"
#include "CourierCharacter.generated.h"

UCLASS()
class CITYCOURIER_API ACourierCharacter : public ACharacter,public ICourierStrikeReceiver
{
    GENERATED_BODY()
public:
    ACourierCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode,uint8 PreviousCustomMode=0) override;
    virtual void OnJumped_Implementation() override;
    FCourierActionResult TryJumpWithFeedback();
    bool CanPerformGroundAction(FText& Reason,bool bAllowAttack=false) const;
    class UCourierActionComponent* GetActions() const { return Actions; }
    void UpdateActionMovement();
    virtual bool ReceiveCourierStrike(FGuid InstanceId,const FVector& Origin,ECourierAction Action) override;
    UFUNCTION(BlueprintCallable) bool TryInteract();
    FCourierActionResult TryInteractWithFeedback();
    void RefreshInteraction();
    bool CanReachInteraction(const AActor* Target, FVector Point, FText& Reason, bool bAllowSuppressed=false) const;
    AActor* GetInteractionTarget() const { return InteractionTarget.Get(); }
    const FText& GetInteractionPrompt() const { return InteractionPrompt; }
    bool IsMounted() const { return bMounted; }
    void SetMounted(bool bValue);
    void SetInputSuppressed(bool bValue,bool bPreserveAction=false);
    void ClearMovementInput();
    void SetSprinting(bool bValue);
    void SetCrouching(bool bValue);
    UFUNCTION(BlueprintCallable) bool SetProne(bool bValue);
    UFUNCTION(BlueprintPure) bool IsProne() const { return bProne; }
    virtual void OnStartCrouch(float HeightAdjust,float ScaledHeightAdjust) override;
    virtual void OnEndCrouch(float HeightAdjust,float ScaledHeightAdjust) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UCameraComponent> FollowCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> PrototypeParts;
private:
    TArray<AActor*> InteractionCandidates() const;
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void SprintStart();
    void SprintStop();
    void CrouchStart();
    void CrouchStop();
    void ProneStart();
    void ProneStop();
    bool HasProneClearance(FVector Location,FRotator Rotation,FVector Displacement=FVector::ZeroVector) const;
    void InteractPressed();
    void JumpPressed();
    void JumpReleased();
    void LightPressed();
    void HeavyPressed();
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UCourierActionComponent> Actions;
    UPROPERTY() TArray<TObjectPtr<class UInputAction>> InputActions;
    TWeakObjectPtr<AActor> InteractionTarget;
    FText InteractionPrompt;
    bool bMounted = false;
    bool bInputSuppressed = false;
    bool bSprintHeld = false;
    bool bCrouchHeld = false;
    bool bProne = false;
    bool bProneHeld = false;
    float InteractionRefreshTime = 0;
};
