#pragma once
#include "UObject/Interface.h"
#include "CourierInteractable.generated.h"
class ACourierCharacter;

UINTERFACE(MinimalAPI, NotBlueprintable)
class UCourierInteractable : public UInterface { GENERATED_BODY() };
class CITYCOURIER_API ICourierInteractable
{
    GENERATED_BODY()
public:
    virtual bool CanInteract(ACourierCharacter* Courier, FText& OutReason) const = 0;
    virtual bool Interact(ACourierCharacter* Courier) = 0;
    virtual int32 GetInteractionPriority() const = 0;
    virtual FVector GetInteractionLocation() const = 0;
    virtual FText GetInteractionText() const { return FText::FromString(TEXT("E  Interact")); }
};
