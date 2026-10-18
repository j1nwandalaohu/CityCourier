#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/CourierMapWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierMapTurnTest,"CityCourier.Navigation.MapPreservesTurnDirection",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCourierMapTurnTest::RunTest(const FString& Parameters)
{
    // Unreal positive yaw is a right turn; screen Y increases downwards.
    // The displayed path must turn clockwise for right and anticlockwise for left.
    for(bool Mini:{false,true})
    {
        const FVector2D Size=Mini?FVector2D(340,340):FVector2D(1160,900);
        const double Margin=Mini?18:80;
        const FVector Origin(800,-11550,90);
        auto Project=[&](FVector P){return UCourierMapWidget::ProjectWorldPosition(P,Size,Margin);};
        for(double Yaw:{0.,45.,90.,135.,180.,225.,270.,315.})
        {
            const FVector Junction=Origin+FRotator(0,Yaw,0).Vector()*1000;
            const FVector2D Before=Project(Junction)-Project(Origin);
            const FVector2D Left=Project(Junction+FRotator(0,Yaw-90,0).Vector()*1000)-Project(Junction);
            const FVector2D Right=Project(Junction+FRotator(0,Yaw+90,0).Vector()*1000)-Project(Junction);
            TestTrue(*FString::Printf(TEXT("%s left stays left at yaw %.0f"),Mini?TEXT("Mini"):TEXT("Full"),Yaw),FVector2D::CrossProduct(Before,Left)<0);
            TestTrue(*FString::Printf(TEXT("%s right stays right at yaw %.0f"),Mini?TEXT("Mini"):TEXT("Full"),Yaw),FVector2D::CrossProduct(Before,Right)>0);
            TestTrue(TEXT("Turning retains map distance scale"),FMath::IsNearlyEqual(Before.Size(),Left.Size(),.0001));
        }
        TestTrue(TEXT("World origin stays at map center"),Project(FVector::ZeroVector).Equals(Size*.5));
        TestTrue(TEXT("Height does not change map position"),Project(Origin).Equals(Project(Origin+FVector(0,0,500))));
    }
    return true;
}
#endif
