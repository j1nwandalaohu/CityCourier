#include "Player/CourierAnimInstance.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
void UCourierAnimInstance::NativeUpdateAnimation(float Delta)
{
    Super::NativeUpdateAnimation(Delta);
    auto* Rider=Cast<ACourierCharacter>(TryGetPawnOwner()); if(!Rider) return;
    bRiding=Rider->IsMounted(); GroundSpeed=Rider->GetVelocity().Size2D(); SteeringAmount=0;
    if(auto* Bike=Cast<AScooterPawn>(Rider->GetAttachParentActor()))
    { GroundSpeed=Bike->GetSpeedKmh()/ .036f; SteeringAmount=FMath::Clamp(-Bike->VisualRoot->GetRelativeRotation().Roll/12.f,-1.f,1.f); }
    AnimationIndex=bRiding?3:GroundSpeed>500?2:GroundSpeed>10?1:0;
}
