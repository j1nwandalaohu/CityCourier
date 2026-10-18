#include "Player/CourierAnimInstance.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
#include "Player/CourierActionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
void UCourierAnimInstance::NativeUpdateAnimation(float Delta)
{
    Super::NativeUpdateAnimation(Delta);
    auto* Rider=Cast<ACourierCharacter>(TryGetPawnOwner()); if(!Rider) return;
    bRiding=Rider->IsMounted(); GroundSpeed=Rider->GetVelocity().Size2D(); SteeringAmount=0;
    bAirborne=Rider->GetCharacterMovement()->IsFalling();
    CurrentAction=Rider->GetActions()->GetTimeline().GetAction(); ActionElapsed=Rider->GetActions()->GetTimeline().GetElapsed();
    if(auto* Bike=Cast<AScooterPawn>(Rider->GetAttachParentActor()))
    { GroundSpeed=Bike->GetSpeedKmh()/ .036f; SteeringAmount=FMath::Clamp(-Bike->VisualRoot->GetRelativeRotation().Roll/12.f,-1.f,1.f); }
    AnimationIndex=bRiding?3:GroundSpeed>500?2:GroundSpeed>10?1:0;
}
