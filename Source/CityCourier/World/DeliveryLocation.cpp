#include "World/DeliveryLocation.h"
#include "Player/CourierCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADeliveryLocation::ADeliveryLocation()
{
    InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint")); RootComponent = InteractionPoint;
    Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker")); Marker->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Marker->SetStaticMesh(Mesh.Object); Marker->SetRelativeScale3D(FVector(1.5,1.5,0.03));
    Marker->SetRelativeLocation(FVector(0,0,5)); Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector ADeliveryLocation::GetInteractionLocation() const { return InteractionPoint->GetComponentLocation(); }
bool ADeliveryLocation::CanInteract(ACourierCharacter* Courier, FText& OutReason) const
{
    return IsValid(Courier) && Courier->CanReachInteraction(this, GetInteractionLocation(), OutReason);
}
bool ADeliveryLocation::Interact(ACourierCharacter* Courier)
{
    FText Reason;
    return CanInteract(Courier, Reason) && OnInteraction.IsBound() && OnInteraction.Execute(Courier);
}
