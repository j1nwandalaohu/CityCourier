#pragma once
#include "GameFramework/Actor.h"
#include "World/CityDefinition.h"
#include "Player/CourierInteractable.h"
#include "DeliveryLocation.generated.h"

UCLASS()
class CITYCOURIER_API ADeliveryLocation : public AActor, public ICourierInteractable
{
    GENERATED_BODY()
public:
    ADeliveryLocation();
    virtual bool CanInteract(ACourierCharacter* Courier, FText& OutReason) const override;
    virtual bool Interact(ACourierCharacter* Courier) override;
    virtual int32 GetInteractionPriority() const override { return bActiveOrderTarget ? 100 : (LocationKind == EDeliveryLocationKind::Station ? 20 : 0); }
    virtual FVector GetInteractionLocation() const override;
    DECLARE_DELEGATE_RetVal_OneParam(bool, FOnLocationInteraction, ACourierCharacter*);
    FOnLocationInteraction OnInteraction;
    bool bActiveOrderTarget = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LocationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDeliveryLocationKind LocationKind = EDeliveryLocationKind::Destination;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UCityDefinition> CityDefinition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> InteractionPoint;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Marker;
};
