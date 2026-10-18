#include "Art/CourierArtCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ACourierArtCharacter::ACourierArtCharacter()
{
    GetMesh()->SetRelativeLocation(FVector(0,0,-88));
    GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}
void ACourierArtCharacter::BeginPlay()
{
    Super::BeginPlay();
    if(GetMesh()->GetSkeletalMeshAsset())
        for(const auto& Part:PrototypeParts) Part->SetVisibility(false,true);
}
void ACourierArtCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Compensate for the gameplay seat attachment so the art stays at the
    // authored height even when the capsule attachment offset changes.
    GetMesh()->SetRelativeLocation(IsMounted() ? FVector(14,0,-32-GetRootComponent()->GetRelativeLocation().Z) : FVector(0,0,-88));
}
