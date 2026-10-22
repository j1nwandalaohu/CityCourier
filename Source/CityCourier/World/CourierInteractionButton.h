#pragma once
#include "GameFramework/Actor.h"
#include "Player/CourierInteractable.h"
#include "CourierInteractionButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCourierButtonPressed,ACourierCharacter*,Courier);
UCLASS()
class CITYCOURIER_API ACourierInteractionButton : public AActor,public ICourierInteractable
{
    GENERATED_BODY()
public:
    ACourierInteractionButton();
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual bool CanInteract(ACourierCharacter* Courier,FText& OutReason) const override;
    virtual bool Interact(ACourierCharacter* Courier) override;
    virtual int32 GetInteractionPriority() const override { return 20; }
    virtual FVector GetInteractionLocation() const override;
    virtual FText GetInteractionText() const override { return FText::FromString(TEXT("E  按下按钮")); }
    virtual bool GetInteractionPose(const ACourierCharacter* Courier,FCourierInteractionPose& OutPose) const override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class USceneComponent> HandContact;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bUseLeftHand=false;
    UPROPERTY(BlueprintAssignable) FCourierButtonPressed OnPressed;
    int32 GetPressCount() const { return PressCount; }
private:
    friend class UCourierObjectInteractionComponent;
    bool SubmitPress(ACourierCharacter* Courier);
    int32 PressCount=0;
};
