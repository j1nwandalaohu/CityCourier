#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Ambient/AmbientVehicle.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierActionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/CapsuleComponent.h"

namespace
{
struct FVehicleTraversalFixture : FTestCourierWorld
{
    AAmbientVehicle* Car;
    ACourierCharacter* Rider;
    UCharacterMovementComponent* Movement;
    FVehicleTraversalFixture(FVector RiderPosition=FVector(290,0,90))
    {
        Box(FVector(0,0,-50),FVector(20000,20000,50));
        Car=World->SpawnActor<AAmbientVehicle>();
        Car->SetPatrol({FVector(0,0,70),FVector(2200,0,70),FVector(2200,4000,70),FVector(0,4000,70)});
        Car->SetPopulationActive(true);
        Rider=World->SpawnActor<ACourierCharacter>(RiderPosition,FRotator(0,180,0));
        Movement=Rider->GetCharacterMovement();Movement->bRunPhysicsWithNoController=true;
        World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
        Movement->SetMovementMode(MOVE_Walking);
    }
    void Step(bool bDrive=false,FVector Input=FVector::ZeroVector)
    {
        if(bDrive)Car->Tick(1.f/60);
        if(!Input.IsZero())Rider->AddMovementInput(Input);
        Movement->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        Rider->GetActions()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
    }
    void Land()
    {
        Movement->SetMovementMode(MOVE_Falling);
        for(int32 I=0;I<120 && !Movement->IsMovingOnGround();++I)Step();
        for(int32 I=0;I<12;++I)Step();
    }
    double FeetZ() const {return Rider->GetActorLocation().Z-Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleSurfaceTest,"CityCourier.VehicleTraversal.AuthoredSurfaces",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVehicleSurfaceTest::RunTest(const FString&)
{
    FTestCourierWorld F;auto* Car=F.World->SpawnActor<AAmbientVehicle>(FVector(0,0,70),FRotator::ZeroRotator);
    for(const auto& Sample:TArray<FVector>{FVector(150,0,115),FVector(0,0,167),FVector(-180,0,100)})
    {
        FHitResult Hit;
        TestTrue(TEXT("Character query hits the car surface"),F.World->LineTraceSingleByChannel(Hit,FVector(Sample.X,Sample.Y,400),FVector(Sample.X,Sample.Y,0),ECC_Pawn));
        AddInfo(FString::Printf(TEXT("Surface x=%.0f expected %.0f actual %.2f"),Sample.X,Sample.Z,Hit.ImpactPoint.Z));
        TestTrue(TEXT("Hood, roof and rear deck match their authored mesh heights"),Hit.GetActor()==Car && FMath::IsNearlyEqual(Hit.ImpactPoint.Z,Sample.Z,.5));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleTwoJumpTest,"CityCourier.VehicleTraversal.GroundHoodRoof",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVehicleTwoJumpTest::RunTest(const FString&)
{
    FVehicleTraversalFixture F;
    TestTrue(TEXT("Ground jump at the front of the car is accepted"),F.Rider->TryJumpWithFeedback().bSucceeded);
    TestTrue(TEXT("Nearby hood receives enough initial lift"),F.Movement->Velocity.Z>490);
    TestEqual(TEXT("Hood assistance does not change ordinary jump settings"),F.Movement->JumpZVelocity,420.f);
    for(int32 I=0;I<100;++I){F.Step(false,FVector(-1,0,0));if(F.Movement->IsMovingOnGround())break;}
    F.Rider->ClearMovementInput();for(int32 I=0;I<12;++I)F.Step();
    AddInfo(FString::Printf(TEXT("First landing %s feet %.2f base %s"),*F.Rider->GetActorLocation().ToString(),F.FeetZ(),*GetNameSafe(F.Rider->GetMovementBase())));
    if(!TestTrue(TEXT("The first physical jump lands on the hood"),F.Movement->IsMovingOnGround() && FMath::IsNearlyEqual(F.FeetZ(),117.15,3.)))return false;
    TestTrue(TEXT("Second jump from hood is accepted after landing"),F.Rider->TryJumpWithFeedback().bSucceeded);
    for(int32 I=0;I<100;++I)
    {
        if(F.Rider->GetActorLocation().X>10)F.Step(false,FVector(-1,0,0));
        else {F.Rider->ClearMovementInput();F.Step();}
        if(F.Movement->IsMovingOnGround())break;
    }
    AddInfo(FString::Printf(TEXT("Roof landing %s feet %.2f base %s"),*F.Rider->GetActorLocation().ToString(),F.FeetZ(),*GetNameSafe(F.Rider->GetMovementBase())));
    TestTrue(TEXT("The second physical jump lands on the roof"),F.Movement->IsMovingOnGround() && FMath::IsNearlyEqual(F.FeetZ(),169.15,3.));
    TestTrue(TEXT("Roof becomes the native movement base"),F.Rider->GetMovementBase() && F.Rider->GetMovementBase()->GetOwner()==F.Car);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehiclePassengerTest,"CityCourier.VehicleTraversal.CarriesStandingPassenger",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVehiclePassengerTest::RunTest(const FString&)
{
    for(double X:{150.,0.})
    {
        FVehicleTraversalFixture F(FVector(X,0,320));F.Land();
        if(!TestTrue(TEXT("Passenger acquires the vehicle through a real landing"),F.Rider->GetMovementBase() && F.Rider->GetMovementBase()->GetOwner()==F.Car))continue;
        const FVector Relative=F.Car->GetActorTransform().InverseTransformPosition(F.Rider->GetActorLocation());
        for(int32 I=0;I<480;++I)F.Step(true);
        const FVector After=F.Car->GetActorTransform().InverseTransformPosition(F.Rider->GetActorLocation());
        AddInfo(FString::Printf(TEXT("Passenger x=%.0f car=%s yaw=%.2f local drift=%.3f speed=%.2f"),X,*F.Car->GetActorLocation().ToString(),F.Car->GetActorRotation().Yaw,FVector::Distance(Relative,After),F.Car->GetSpeedCm()));
        TestTrue(TEXT("Standing passenger does not become a traffic obstacle"),F.Car->GetActorLocation().Size2D()>1000);
        TestTrue(TEXT("Car actually turns while occupied"),FMath::Abs(F.Car->GetActorRotation().Yaw)>20);
        TestTrue(TEXT("Native movement base carries translation and rotation without drift"),FVector::Distance(Relative,After)<3 && F.Movement->IsMovingOnGround());
        TestTrue(TEXT("Jump off the moving vehicle"),F.Rider->TryJumpWithFeedback().bSucceeded);
        TestTrue(TEXT("Jump clears base while retaining forward vehicle momentum"),F.Movement->IsFalling() && !F.Rider->GetMovementBase() && F.Movement->Velocity.Size2D()>50);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleOrdinaryJumpTest,"CityCourier.VehicleTraversal.OrdinaryJumpUnchanged",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVehicleOrdinaryJumpTest::RunTest(const FString&)
{
    FVehicleTraversalFixture F(FVector(1000,1000,90));
    TestTrue(TEXT("Ordinary ground jump is accepted"),F.Rider->TryJumpWithFeedback().bSucceeded);
    TestEqual(TEXT("No hood assistance away from a car"),F.Movement->Velocity.Z,420.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleTurningCollisionTest,"CityCourier.VehicleTraversal.TurningStillBlocksRoadPedestrians",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVehicleTurningCollisionTest::RunTest(const FString&)
{
    FVehicleTraversalFixture F(FVector(290,0,90));
    F.Car->SetPatrol({FVector(0,0,70),FVector(0,6000,70)});
    F.Car->SetActorRotation(FRotator::ZeroRotator);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VehicleTurningPassengerTest),false,F.Car);
    bool bPenetrated=false;
    for(int32 I=0;I<180;++I)
    {
        F.Car->Tick(1.f/60);
        bPenetrated|=F.World->OverlapBlockingTestByChannel(F.Car->GetActorLocation(),F.Car->GetActorQuat(),ECC_Pawn,
            FCollisionShape::MakeBox(FVector(204,87,64)),Query);
    }
    TestFalse(TEXT("Turning car must not sweep through a road pedestrian while its heading catches up"),bPenetrated);
    return true;
}
#endif
