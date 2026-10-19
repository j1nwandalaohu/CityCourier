#include "UI/CourierNavigationArrowWidget.h"
#include "UI/CourierUI.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Delivery/DeliverySubsystem.h"
#include "Player/CourierController.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/CanvasPanel.h"
#include "Camera/PlayerCameraManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

void UCourierNavigationArrowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    WidgetTree->RootWidget=WidgetTree->ConstructWidget<UCanvasPanel>();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UCourierNavigationArrowWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta);
    const auto* PC=Cast<ACourierController>(GetOwningPlayer());
    const auto* Nav=GetWorld()?GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>():nullptr;
    const APawn* Pawn=PC?PC->GetPawn():nullptr;
    bHasTarget=PC && Pawn && Nav && PC->GetInterfaceMode()==ECourierInterfaceMode::Gameplay && !Nav->GetTargetLabel().IsEmpty();
    if(!bHasTarget){bHeadingInitialized=false;return;}
    bArrived=Nav->HasArrived();bReachable=Nav->GetActiveRoute().bReachable;
    Title=Nav->GetNavigationMode()==ECourierNavigationMode::Station?TEXT("返回驿站"):Nav->GetNavigationMode()==ECourierNavigationMode::ParkedBike?TEXT("寻找电动车"):
        PC->GetDeliverySystem()->GetState().Status==EDeliveryStatus::AwaitingPickup?TEXT("取餐导航"):TEXT("送餐导航");
    Distance=CourierUI::Distance(Nav->GetRemainingMeters());
    FVector Ground=Pawn->GetActorLocation();
    if(const auto* Character=Cast<ACharacter>(Pawn))Ground.Z-=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const double ViewYaw=PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraRotation().Yaw:PC->GetControlRotation().Yaw;
    Cue=FNavigationGuidance::FromRoute(Nav->GetActiveRoute(),Ground,ViewYaw);
    if(Cue.bHasDirection)
    {
        Heading=bHeadingInitialized?FMath::UnwindDegrees(Heading+FMath::FindDeltaAngleDegrees(Heading,Cue.HeadingDegrees)*(1-FMath::Exp(-12.*FMath::Max(0.f,Delta)))):Cue.HeadingDegrees;
        bHeadingInitialized=true;
    }
    if(bArrived)Instruction=TEXT("已到达 · 按 E 交互");
    else if(!bReachable || !Cue.bHasDirection)Instruction=TEXT("请返回道路");
    else if(Cue.bUpstairs)Instruction=TEXT("上楼 · 沿楼梯前进");
    else if(Cue.bDownstairs)Instruction=TEXT("下楼 · 沿楼梯前进");
    else if(FMath::Abs(Cue.HeadingDegrees)>150)Instruction=TEXT("目标路线在后方");
    else Instruction=TEXT("沿箭头方向前进");
}
int32 UCourierNavigationArrowWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
    Layer=Super::NativePaint(Args,G,Culling,Out,Layer,Style,bEnabled);
    if(!bHasTarget)return Layer;
    const FVector2D Size=G.GetLocalSize();
    static const FSlateRoundedBoxBrush Panel(FLinearColor::White,14.f);
    FSlateDrawElement::MakeBox(Out,++Layer,G.ToPaintGeometry(),&Panel,ESlateDrawEffect::None,FLinearColor(.025f,.04f,.05f,.84f));
    auto Text=[&](double Y,const FString& Value,int32 FontSize,FLinearColor Color)
    {
        const auto Font=CourierUI::Font(FontSize);
        const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(FStringView(Value),Font);
        FSlateDrawElement::MakeText(Out,Layer+2,G.ToPaintGeometry(Extent,FSlateLayoutTransform(FVector2D((Size.X-Extent.X)/2,Y))),Value,Font,ESlateDrawEffect::None,Color);
    };
    Text(10,Title,18,CourierUI::Muted);
    Text(137,bArrived?FString(TEXT("已到达")):bReachable?Distance:FString(TEXT("路线不可用")),23,bArrived?FLinearColor(.5f,.9f,.6f):FLinearColor::White);
    Text(168,Instruction,15,CourierUI::Muted);
    if(!bReachable || bArrived || !Cue.bHasDirection)return Layer+3;

    // A small extruded mesh projected into Slate: real 3D vertices and face
    // shading, without a second scene capture or dependence on world lighting.
    const FVector2D Center(Size.X/2,83);
    const double Scale=49;
    const double Angle=FMath::DegreesToRadians(Heading),C=FMath::Cos(Angle),S=FMath::Sin(Angle);
    auto Rotate=[&](FVector P){return FVector(P.X*C+P.Y*S,-P.X*S+P.Y*C,P.Z);};
    auto Project=[&](FVector P){return Center+FVector2D(P.X*Scale,(-P.Y*.66-P.Z*.75)*Scale);};
    const TArray<FVector> Rim={{0,1.1,.12},{.57,.16,.12},{.23,.16,.12},{.23,-.88,.12},{-.23,-.88,.12},{-.23,.16,.12},{-.57,.16,.12}};
    struct FFace {FVector A,B,C;double Depth;FLinearColor Color;};
    TArray<FFace> Faces;
    auto Face=[&](FVector A,FVector B,FVector Z,bool Top)
    {
        A=Rotate(A);B=Rotate(B);Z=Rotate(Z);
        FVector N=FVector::CrossProduct(B-A,Z-A).GetSafeNormal();
        if(Top && N.Z<0)N=-N;
        const double Light=.48+.52*FMath::Max(0.,FVector::DotProduct(N,FVector(-.3,-.4,.85).GetSafeNormal()));
        Faces.Add({A,B,Z,((A.Y+B.Y+Z.Y)*.75-(A.Z+B.Z+Z.Z)*.66)/3,CourierUI::Accent*Light});
        Faces.Last().Color.A=1;
    };
    const FVector Ridge(0,.16,.36),Tail(0,-.88,.36);
    Face(Rim[6],Rim[0],Ridge,true);Face(Rim[0],Rim[1],Ridge,true);
    Face(Rim[5],Ridge,Tail,true);Face(Rim[5],Tail,Rim[4],true);
    Face(Ridge,Rim[2],Rim[3],true);Face(Ridge,Rim[3],Tail,true);
    for(int32 I=0;I<Rim.Num();++I)
    {
        const FVector A=Rim[I],B=Rim[(I+1)%Rim.Num()],BottomA(A.X,A.Y,-.15),BottomB(B.X,B.Y,-.15);
        Face(A,BottomA,B,false);Face(B,BottomA,BottomB,false);
    }
    Faces.Sort([](const FFace& A,const FFace& B){return A.Depth>B.Depth;});
    TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
    for(const FFace& F:Faces)
        for(FVector P:{F.A,F.B,F.C})
        {
            Indices.Add(Vertices.Num());
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(Project(P)),FVector2f(.5f,.5f),F.Color.ToFColor(true)));
        }
    const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
    FSlateDrawElement::MakeCustomVerts(Out,Layer+1,Resource,Vertices,Indices,nullptr,0,0);
    if(Cue.bUpstairs || Cue.bDownstairs)Text(109,Cue.bUpstairs?TEXT("↑ 上楼"):TEXT("↓ 下楼"),16,CourierUI::Accent);
    return Layer+3;
}
