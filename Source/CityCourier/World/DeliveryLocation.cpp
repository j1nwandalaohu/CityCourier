#include "World/DeliveryLocation.h"
#include "Player/CourierCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Delivery/DeliverySubsystem.h"
#include "Buildings/CourierBuildingSubsystem.h"

ADeliveryLocation::ADeliveryLocation()
{
    InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint")); RootComponent = InteractionPoint;
    Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker")); Marker->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Marker->SetStaticMesh(Mesh.Object); Marker->SetRelativeScale3D(FVector(1.5,1.5,0.03));
    Marker->SetRelativeLocation(FVector(0,0,5)); Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector ADeliveryLocation::GetInteractionLocation() const { return InteractionPoint->GetComponentLocation(); }
void ADeliveryLocation::BeginPlay()
{ Super::BeginPlay(); if(auto* D=GetWorld()->GetSubsystem<UDeliverySubsystem>()) D->RegisterLocation(this); }
void ADeliveryLocation::EndPlay(const EEndPlayReason::Type Reason)
{ if(auto* D=GetWorld()->GetSubsystem<UDeliverySubsystem>()) D->UnregisterLocation(this); Super::EndPlay(Reason); }
bool ADeliveryLocation::CanInteract(ACourierCharacter* Courier, FText& OutReason) const
{
    OutReason=FText::GetEmpty();
    if(LocationKind!=EDeliveryLocationKind::Station && !bActiveOrderTarget)
    { OutReason=FText::FromString(TEXT("这里不是当前订单的取送餐点")); return false; }
    if(IsValid(Courier)) if(const auto* S=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>(); S && S->IsEnabled())
        if(!S->CanUseLocation(LocationId,Courier->GetActorLocation(),OutReason)) return false;
    return IsValid(Courier) && Courier->CanReachInteraction(this, GetInteractionLocation(), OutReason);
}
bool ADeliveryLocation::Interact(ACourierCharacter* Courier)
{
    FText Reason;
    return CanInteract(Courier, Reason) && OnInteraction.IsBound() && OnInteraction.Execute(Courier);
}
