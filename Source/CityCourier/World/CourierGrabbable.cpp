#include "World/CourierGrabbable.h"
#include "Player/CourierCharacter.h"
#include "Player/Epic/CourierObjectInteractionComponent.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
ACourierGrabbable::ACourierGrabbable()
{
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrabbableBody")); SetRootComponent(Body);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Body->SetStaticMesh(Cube.Object); Body->SetRelativeScale3D(FVector(.35));
    Body->SetCollisionProfileName(TEXT("PhysicsActor")); Body->SetSimulatePhysics(true);
    Body->BodyInstance.SetMassOverride(10);
    Body->SetCanEverAffectNavigation(false);
}
void ACourierGrabbable::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Registry=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>()) Registry->RegisterInteractable(this);
}
void ACourierGrabbable::EndPlay(EEndPlayReason::Type Reason)
{
    if(auto* Registry=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>()) Registry->UnregisterInteractable(this);
    Super::EndPlay(Reason);
}
FVector ACourierGrabbable::GetInteractionLocation() const { return Body->GetComponentLocation(); }
bool ACourierGrabbable::CanInteract(ACourierCharacter* Courier,FText& Reason) const
{
    if(!Courier || !bCanGrab || Holder.IsValid() || !Body->IsSimulatingPhysics()) { Reason=FText::FromString(TEXT("物体暂时无法拿起")); return false; }
    if(Body->GetMass()>Courier->GetObjectInteraction()->MaximumMassKg) { Reason=FText::FromString(TEXT("物体太重，无法拿起")); return false; }
    return true;
}
bool ACourierGrabbable::Interact(ACourierCharacter* Courier) { return Courier && Courier->GetObjectInteraction()->TryGrab(Body).bSucceeded; }
