#include "Ambient/AmbientPedestrian.h"
#include "Ambient/CitizenAnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
AAmbientPedestrian::AAmbientPedestrian()
{
    bPedestrian=true;MaxSpeed=130;Acceleration=180;Deceleration=500;
    Body->SetBoxExtent(FVector(28,28,85));Visual->SetVisibility(false);
    Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    // Swept fists see recipients as overlaps, retaining walls as blockers.
    Body->SetCollisionResponseToChannel(ECC_Visibility,ECR_Overlap);
    for(auto* Shape:{Chassis.Get(),Hood.Get(),Roof.Get()})Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Person=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Walker"));Person->SetupAttachment(Body);
    Person->SetRelativeLocation(FVector(0,0,-90));Person->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Modern portrait FBX faces +Y, while patrol/fall clearance use actor +X.
    Person->SetRelativeRotation(FRotator(0,-90,0));
}
void AAmbientPedestrian::BeginPlay()
{
    Super::BeginPlay();
    Person->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CityCourier/Art/Ambient/CharMorph/SK_CitizenCharMorph.SK_CitizenCharMorph")));
    Person->SetAnimInstanceClass(UCitizenAnimInstance::StaticClass());
    Person->AddTickPrerequisiteActor(this);
}
void AAmbientPedestrian::Tick(float DeltaSeconds)
{
    if(!IsPopulationActive() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0)return;
    CombatClock+=DeltaSeconds;
    float Remaining=FMath::Min(DeltaSeconds,10.f);
    while(Remaining>SMALL_NUMBER)
    {
        if(Reaction==ECitizenReaction::Walking){Super::Tick(Remaining);break;}
        const float Duration=Reaction==ECitizenReaction::Staggering?29.f/60.f:
            Reaction==ECitizenReaction::Falling?1.05f:Reaction==ECitizenReaction::Down?2.f:2.7f;
        const float Step=FMath::Min(Remaining,FMath::Max(0.f,Duration-ReactionElapsed));
        ReactionElapsed+=Step;Remaining-=Step;
        if(ReactionElapsed+KINDA_SMALL_NUMBER<Duration)break;
        if(Reaction==ECitizenReaction::Staggering || Reaction==ECitizenReaction::GettingUp)
            SetReaction(ECitizenReaction::Walking);
        else if(Reaction==ECitizenReaction::Falling)SetReaction(ECitizenReaction::Down);
        else if(CanStandUp())SetReaction(ECitizenReaction::GettingUp);
        else {ReactionElapsed=Duration;break;} // Retry clearance without restarting the down pose.
    }
}
void AAmbientPedestrian::SetStandingCollision(bool bStanding)
{
    // Counter-moving the mesh keeps its world-space floor origin unchanged.
    const double Ground=GetActorLocation().Z+Person->GetRelativeLocation().Z;
    // A downed body is no longer a standing obstacle for the player's capsule.
    // Expanding the horizontal box must not depenetrate the rider onto its top.
    Body->SetCollisionResponseToChannel(ECC_Pawn,bStanding?ECR_Block:ECR_Ignore);
    Body->SetBoxExtent(bStanding?FVector(28,28,85):FVector(90,34,22));
    const double Height=bStanding?90.:24.;
    Person->SetRelativeLocation(FVector(0,0,-Height));
    FVector Position=GetActorLocation();Position.Z=Ground+Height;
    SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
    Body->ComponentVelocity=FVector::ZeroVector;
}
bool AAmbientPedestrian::CanStandUp() const
{
    const FVector Floor=Person->GetComponentLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CitizenStandClearance),false,this);
    return !GetWorld()->OverlapBlockingTestByChannel(Floor+FVector(0,0,90),GetActorQuat(),ECC_Pawn,
        FCollisionShape::MakeBox(FVector(28,28,85)),Query);
}
void AAmbientPedestrian::SetReaction(ECitizenReaction Next)
{
    Reaction=Next;ReactionElapsed=0;Speed=0;Body->ComponentVelocity=FVector::ZeroVector;
    if(Next==ECitizenReaction::Down)SetStandingCollision(false);
    else if(Next==ECitizenReaction::GettingUp || Next==ECitizenReaction::Walking)SetStandingCollision(true);
}
void AAmbientPedestrian::SetPopulationActive(bool bActive)
{
    if(!bActive)
    {
        SetReaction(ECitizenReaction::Walking);
        RecentStrikes.Empty();CombatClock=0;LastLightTime=-100;
    }
    Super::SetPopulationActive(bActive);
}
bool AAmbientPedestrian::ReceiveCourierStrike(FGuid InstanceId,const FVector& Origin,ECourierAction Action)
{
    if(!IsPopulationActive() || !InstanceId.IsValid() || Origin.ContainsNaN()
        || (Action!=ECourierAction::LightL && Action!=ECourierAction::LightR && Action!=ECourierAction::Heavy)
        || FVector::DistSquared2D(Origin,GetActorLocation())>FMath::Square(190.)
        || FMath::Abs(Origin.Z-GetActorLocation().Z)>150
        || Reaction==ECitizenReaction::Falling || Reaction==ECitizenReaction::Down || Reaction==ECitizenReaction::GettingUp
        || RecentStrikes.Contains(InstanceId))return false;
    for(auto It=RecentStrikes.CreateIterator();It;++It)if(CombatClock-It.Value()>10.f)It.RemoveCurrent();
    RecentStrikes.Add(InstanceId,CombatClock);
    const bool bKnockDown=Action==ECourierAction::Heavy || CombatClock-LastLightTime<=1.2f;
    LastLightTime=bKnockDown?-100:CombatClock;
    if(bKnockDown)
    {
        const FVector Facing=(Origin-GetActorLocation()).GetSafeNormal2D();
        const FQuat Rotation=Facing.IsNearlyZero()?GetActorQuat():Facing.Rotation().Quaternion();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CitizenFallClearance),false,this);
        // Falling poses extend beyond the standing capsule. Confined spaces
        // keep a standing hit reaction instead of folding the body into a wall.
        const bool bBlocked=GetWorld()->OverlapAnyTestByObjectType(Person->GetComponentLocation()+FVector(0,0,24),
            Rotation,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeBox(FVector(90,34,22)),Query);
        if(bBlocked)SetReaction(ECitizenReaction::Staggering);
        else {SetActorRotation(Rotation);SetReaction(ECitizenReaction::Falling);}
    }
    else if(Reaction!=ECitizenReaction::Staggering)SetReaction(ECitizenReaction::Staggering);
    return true;
}
