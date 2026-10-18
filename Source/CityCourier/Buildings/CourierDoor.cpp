#include "Buildings/CourierDoor.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ACourierDoor::ACourierDoor()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
    Pivot=CreateDefaultSubobject<USceneComponent>(TEXT("Hinge")); RootComponent=Pivot;
    Collision=CreateDefaultSubobject<UBoxComponent>(TEXT("LeafCollision")); Collision->SetupAttachment(Pivot);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeafVisual")); Visual->SetupAttachment(Collision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); Visual->SetStaticMesh(Cube.Object);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ACourierDoor::Configure(const FCourierDoorDefinition& D,const FTransform& BuildingTransform,const FCourierDoorState& Initial)
{
    Definition=D; State=Initial;
    bConfigurationValid=!D.Id.IsNone() && !D.Hinge.ContainsNaN() && !BuildingTransform.ContainsNaN() &&
        FMath::IsFinite(D.Width) && D.Width>=(D.bExternal?120:100) && FMath::IsFinite(D.Height) && D.Height>=220 &&
        FMath::IsFinite(D.ClosedYaw) && FMath::IsFinite(D.OpenAngle) && FMath::Abs(D.OpenAngle)>=1 && FMath::Abs(D.OpenAngle)<=180 &&
        FMath::IsFinite(Initial.Angle) && Initial.Angle>=FMath::Min(0.f,D.OpenAngle)-.01f && Initial.Angle<=FMath::Max(0.f,D.OpenAngle)+.01f;
    const FTransform SafeTransform=BuildingTransform.ContainsNaN()?FTransform::Identity:BuildingTransform;
    if(!bConfigurationValid)
    {
        State={}; Definition.Width=100; Definition.Height=220; Definition.OpenAngle=90;
        Definition.ClosedYaw=0; if(Definition.Hinge.ContainsNaN()) Definition.Hinge=FVector::ZeroVector;
    }
    ClosedRotation=SafeTransform.GetRotation()*FRotator(0,Definition.ClosedYaw,0).Quaternion();
    SetActorLocation(SafeTransform.TransformPosition(Definition.Hinge));
    Collision->SetBoxExtent(FVector(Definition.Width/2,2.5f,Definition.Height/2));
    Collision->SetRelativeLocation(FVector(Definition.Width/2,0,Definition.Height/2));
    Visual->SetRelativeScale3D(FVector(Definition.Width/100,.05,Definition.Height/100));
    if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CityCourier/Art/Materials/M_Wood.M_Wood"))) Visual->SetMaterial(0,Mat);
    bReady=bConfigurationValid;
    bMoving=bReady && !Initial.bBlocked && !FMath::IsNearlyEqual(State.Angle,State.bWantOpen?D.OpenAngle:0.f,.01f);
    ApplyAngle(); SetActorTickEnabled(bMoving);
}
void ACourierDoor::ApplyAngle()
{ SetActorRotation((ClosedRotation*FRotator(0,State.Angle,0).Quaternion()).Rotator()); }
FVector ACourierDoor::GetInteractionLocation() const
{ return GetActorLocation()+GetActorQuat().RotateVector(FVector(Definition.Width/2,0,85)); }
FText ACourierDoor::GetInteractionText() const
{
    if(!bReady) return FText::FromString(TEXT("室内准备中"));
    if(bMoving) return FText::FromString(TEXT("门正在开合"));
    return FText::FromString(State.bWantOpen?TEXT("E 关门"):TEXT("E 开门"));
}
bool ACourierDoor::CanInteract(ACourierCharacter* Courier,FText& Reason) const
{
    if(!IsValid(Courier)) { Reason=FText::FromString(TEXT("角色不可用")); return false; }
    if(!Courier->CanReachInteraction(this,GetInteractionLocation(),Reason)) return false;
    if(!bReady) { Reason=FText::FromString(TEXT("室内准备中，请稍候")); return false; }
    if(bMoving) { Reason=FText::FromString(TEXT("门正在开合")); return false; }
    return true;
}
FCourierActionResult ACourierDoor::TryToggle(ACourierCharacter* Courier)
{
    FText Reason; if(!CanInteract(Courier,Reason)) return {false,Reason};
    State.bWantOpen=!State.bWantOpen; State.bBlocked=false;
    bMoving=!FMath::IsNearlyEqual(State.Angle,State.bWantOpen?Definition.OpenAngle:0.f,.01f);
    SetActorTickEnabled(bMoving); OnDoorState.Broadcast(Definition.Id,State); return FCourierActionResult::Success();
}
bool ACourierDoor::Interact(ACourierCharacter* Courier)
{
    const auto Result=TryToggle(Courier);
    if(auto* PC=Courier?Cast<ACourierController>(Courier->GetController()):nullptr) PC->ShowActionFeedback(Result.Reason);
    return Result.bSucceeded;
}
bool ACourierDoor::IsAngularSliceBlocked(float From,float To) const
{
    if(!GetWorld()) return true;
    const float Middle=(From+To)/2;
    const FQuat Q=ClosedRotation*FRotator(0,Middle,0).Quaternion();
    // An OBB at the middle angle, expanded by the maximum arc displacement,
    // conservatively encloses the entire rotating leaf for this <=1 degree slice.
    const float Arc=Definition.Width*FMath::Sin(FMath::DegreesToRadians(FMath::Abs(To-From)/2));
    const FVector Extent(Definition.Width/2+Arc,2.5f+Arc,Definition.Height/2-.1f);
    const FVector Center=GetActorLocation()+Q.RotateVector(FVector(Definition.Width/2,0,Definition.Height/2));
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierDoorSweep),false,this);
    return GetWorld()->OverlapBlockingTestByChannel(Center,Q,ECC_Pawn,FCollisionShape::MakeBox(Extent),Params);
}
void ACourierDoor::AdvanceDoor(float Delta)
{
    if(!bMoving || !FMath::IsFinite(Delta) || Delta<=0 || (GetWorld() && GetWorld()->IsPaused())) return;
    const float Target=State.bWantOpen?Definition.OpenAngle:0;
    const float End=FMath::FInterpConstantTo(State.Angle,Target,Delta,FMath::Abs(Definition.OpenAngle)/.6f);
    const int32 Slices=FMath::Max(1,FMath::CeilToInt(FMath::Abs(End-State.Angle)));
    const float Start=State.Angle;
    for(int32 I=1;I<=Slices;++I)
    {
        const float Next=FMath::Lerp(Start,End,float(I)/Slices);
        if(IsAngularSliceBlocked(State.Angle,Next))
        {
            bMoving=false; State.bBlocked=true; SetActorTickEnabled(false);
            if(auto* PC=GetWorld()?Cast<ACourierController>(GetWorld()->GetFirstPlayerController()):nullptr) PC->ShowActionFeedback(FText::FromString(TEXT("门口有阻挡，请让开后重试")));
            OnDoorState.Broadcast(Definition.Id,State); return;
        }
        State.Angle=Next; ApplyAngle();
    }
    if(FMath::IsNearlyEqual(State.Angle,Target,.01f)) { State.Angle=Target; bMoving=false; SetActorTickEnabled(false); ApplyAngle(); }
    OnDoorState.Broadcast(Definition.Id,State);
}
void ACourierDoor::Tick(float Delta)
{ Super::Tick(Delta); AdvanceDoor(Delta); }
