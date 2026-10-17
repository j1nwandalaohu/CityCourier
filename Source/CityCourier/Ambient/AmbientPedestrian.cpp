#include "Ambient/AmbientPedestrian.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
AAmbientPedestrian::AAmbientPedestrian()
{
    bPedestrian=true;MaxSpeed=130;Acceleration=180;Deceleration=500;
    Body->SetBoxExtent(FVector(28,28,85));Visual->SetVisibility(false);
    Person=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Walker"));Person->SetupAttachment(Body);
    Person->SetRelativeLocation(FVector(0,0,-90));Person->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AAmbientPedestrian::BeginPlay()
{
    Super::BeginPlay();
    Person->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CityCourier/Art/Ambient/SK_Citizen.SK_Citizen")));
    Person->PlayAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/CityCourier/Art/Characters/A_Walk.A_Walk")),true);
}
void AAmbientPedestrian::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(auto* Animation=Person->GetSingleNodeInstance())Animation->SetPlayRate(Speed/160.f);
}
