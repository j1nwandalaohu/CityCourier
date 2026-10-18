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
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual bool CanInteract(ACourierCharacter* Courier, FText& OutReason) const override;
    virtual bool Interact(ACourierCharacter* Courier) override;
    virtual int32 GetInteractionPriority() const override { return bActiveOrderTarget ? 100 : (LocationKind == EDeliveryLocationKind::Station ? 20 : 0); }
    virtual FVector GetInteractionLocation() const override;
    virtual FText GetInteractionText() const override
    { return FText::FromString(LocationKind==EDeliveryLocationKind::Station?TEXT("E  休息站"):bActiveOrderTarget?(LocationKind==EDeliveryLocationKind::Merchant?TEXT("E  取餐"):TEXT("E  交付外卖")):TEXT("配送地点")); }
    DECLARE_DELEGATE_RetVal_OneParam(bool, FOnLocationInteraction, ACourierCharacter*);
    FOnLocationInteraction OnInteraction;
    bool bActiveOrderTarget = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LocationId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FName BuildingId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FName RoomId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector StreetAnchor=FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDeliveryLocationKind LocationKind = EDeliveryLocationKind::Destination;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UCityDefinition> CityDefinition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> InteractionPoint;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Marker;
};
