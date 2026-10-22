#pragma once
#include "GameFramework/Actor.h"
#include "Player/CourierInteractable.h"
#include "CourierGrabbable.generated.h"

UCLASS()
class CITYCOURIER_API ACourierGrabbable : public AActor,public ICourierInteractable
{
    GENERATED_BODY()
public:
    ACourierGrabbable();
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual bool CanInteract(ACourierCharacter* Courier,FText& OutReason) const override;
    virtual bool Interact(ACourierCharacter* Courier) override;
    virtual int32 GetInteractionPriority() const override { return 20; }
    virtual FVector GetInteractionLocation() const override;
    virtual FText GetInteractionText() const override { return FText::FromString(TEXT("E  拿起")); }
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Body;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bCanGrab=true;
    TWeakObjectPtr<ACourierCharacter> Holder;
};
