#include "World/CourierInteractionButton.h"
#include "Player/CourierCharacter.h"
#include "Player/Epic/CourierObjectInteractionComponent.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
ACourierInteractionButton::ACourierInteractionButton()
{
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonBody")); SetRootComponent(Body);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Body->SetStaticMesh(Cube.Object); Body->SetRelativeScale3D(FVector(.12,.25,.25));
    Body->SetCollisionProfileName(TEXT("BlockAll")); Body->SetCanEverAffectNavigation(false);
    HandContact=CreateDefaultSubobject<USceneComponent>(TEXT("HandContact")); HandContact->SetupAttachment(Body);
    HandContact->SetAbsolute(false,false,true); HandContact->SetRelativeLocation(FVector(-65,0,0));
}
void ACourierInteractionButton::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Registry=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>()) Registry->RegisterInteractable(this);
}
void ACourierInteractionButton::EndPlay(EEndPlayReason::Type Reason)
{
    if(auto* Registry=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>()) Registry->UnregisterInteractable(this);
    Super::EndPlay(Reason);
}
FVector ACourierInteractionButton::GetInteractionLocation() const { return HandContact->GetComponentLocation(); }
bool ACourierInteractionButton::CanInteract(ACourierCharacter* Courier,FText& Reason) const
{
    if(!Courier || !bEnabled) { Reason=FText::FromString(TEXT("按钮暂时不可用")); return false; }
    if(!Courier->GetObjectInteraction()->CanReachHand(GetInteractionLocation(),bUseLeftHand)) { Reason=FText::FromString(TEXT("请再靠近按钮")); return false; }
    return true;
}
bool ACourierInteractionButton::Interact(ACourierCharacter* Courier) { return Courier && Courier->GetObjectInteraction()->TryPress(this).bSucceeded; }
bool ACourierInteractionButton::GetInteractionPose(const ACourierCharacter* Courier,FCourierInteractionPose& Pose) const
{
    Pose={};
    if(bUseLeftHand) { Pose.LeftHandWorld=HandContact->GetComponentTransform(); Pose.LeftWeight=1; }
    else { Pose.RightHandWorld=HandContact->GetComponentTransform(); Pose.RightWeight=1; }
    return true;
}
bool ACourierInteractionButton::SubmitPress(ACourierCharacter* Courier)
{
    FText Reason; if(!CanInteract(Courier,Reason)) return false;
    ++PressCount; OnPressed.Broadcast(Courier); return true;
}
