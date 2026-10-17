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
    // Gameplay attaches the capsule at z=95 above the bike root; compensate only
    // the visual mesh so the seated pelvis meets the 79cm saddle.
    GetMesh()->SetRelativeLocation(IsMounted() ? FVector(14,0,-127) : FVector(0,0,-88));
}
