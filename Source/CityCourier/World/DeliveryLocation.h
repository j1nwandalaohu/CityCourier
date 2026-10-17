#pragma once
#include "GameFramework/Actor.h"
#include "World/CityDefinition.h"
#include "DeliveryLocation.generated.h"

UCLASS()
class CITYCOURIER_API ADeliveryLocation : public AActor
{
    GENERATED_BODY()
public:
    ADeliveryLocation();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LocationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDeliveryLocationKind LocationKind = EDeliveryLocationKind::Destination;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UCityDefinition> CityDefinition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> InteractionPoint;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Marker;
};
