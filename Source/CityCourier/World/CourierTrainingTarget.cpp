#include "World/CourierTrainingTarget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"

ACourierTrainingTarget::ACourierTrainingTarget()
{
    PrimaryActorTick.bCanEverTick=true;
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("TargetBody")); RootComponent=Body;
    Body->SetBoxExtent(FVector(20,25,55)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Body->SetCollisionResponseToChannel(ECC_Visibility,ECR_Overlap);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetVisual")); Visual->SetupAttachment(Body);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Visual->SetStaticMesh(Cube.Object); Visual->SetRelativeScale3D(FVector(.4,.5,1.1)); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("TargetLabel")); Label->SetupAttachment(Body);
    Label->SetRelativeLocation(FVector(0,0,80)); Label->SetWorldSize(18); Label->SetText(FText::FromString(TEXT("TRAINING: 0")));
}
bool ACourierTrainingTarget::ReceiveCourierStrike(FGuid Id,const FVector& Origin,ECourierAction Action)
{
    if(!Id.IsValid() || Received.Contains(Id) || Origin.ContainsNaN() ||
        (Action!=ECourierAction::LightL && Action!=ECourierAction::LightR && Action!=ECourierAction::Heavy)) return false;
    Received.Add(Id); ++HitCount; if(ReactionRemaining<=0) ReactionRemaining=.4f;
    Label->SetText(FText::FromString(FString::Printf(TEXT("TRAINING: %d"),HitCount)));
    return true;
}
void ACourierTrainingTarget::Tick(float Delta)
{
    Super::Tick(Delta); ReactionRemaining=FMath::Max(0.f,ReactionRemaining-Delta);
    Visual->SetRelativeRotation(FRotator(FMath::Sin(ReactionRemaining/.4f*PI)*12,0,0));
}
