#pragma once
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Core/CourierActionResult.h"
#include "CourierCharacter.generated.h"

UCLASS()
class CITYCOURIER_API ACourierCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ACourierCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    UFUNCTION(BlueprintCallable) bool TryInteract();
    FCourierActionResult TryInteractWithFeedback();
    void RefreshInteraction();
    bool CanReachInteraction(const AActor* Target, FVector Point, FText& Reason, bool bAllowSuppressed=false) const;
    AActor* GetInteractionTarget() const { return InteractionTarget.Get(); }
    const FText& GetInteractionPrompt() const { return InteractionPrompt; }
    bool IsMounted() const { return bMounted; }
    void SetMounted(bool bValue) { bMounted = bValue; ClearMovementInput(); RefreshInteraction(); }
    void SetInputSuppressed(bool bValue) { bInputSuppressed = bValue; ClearMovementInput(); RefreshInteraction(); }
    void ClearMovementInput();
    void SetSprinting(bool bValue);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UCameraComponent> FollowCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> PrototypeParts;
private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void SprintStart();
    void SprintStop();
    void InteractPressed();
    UPROPERTY() TArray<TObjectPtr<class UInputAction>> InputActions;
    TWeakObjectPtr<AActor> InteractionTarget;
    FText InteractionPrompt;
    bool bMounted = false;
    bool bInputSuppressed = false;
    float InteractionRefreshTime = 0;
};
