#include "UI/CourierMapWidget.h"
#include "UI/CourierUI.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Vehicles/ScooterPawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "World/CityDefinition.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "World/DeliveryLocation.h"
FVector2D UCourierMapWidget::ProjectWorldPosition(const FVector& P,const FVector2D& Size,double Margin)
{
    const double Scale=FMath::Min(Size.X-2*Margin,Size.Y-2*Margin)/60000;
    // Unreal +X forward is map up, +Y right is map right. Flipping only Y
    // mirrors the world and reverses every left/right turn on the map.
    return FVector2D(Size.X*.5+P.Y*Scale,Size.Y*.5-P.X*Scale);
}
void UCourierMapWidget::NativeOnInitialized() { Super::NativeOnInitialized(); WidgetTree->RootWidget=WidgetTree->ConstructWidget<UCanvasPanel>(); }
int32 UCourierMapWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
    const FVector2D Size=G.GetLocalSize(); const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    FSlateDrawElement::MakeBox(Out,++Layer,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,CourierUI::Ink);
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); const auto* D=PC?PC->GetDeliverySystem():nullptr; if(!D || !D->GetCity()) return Layer;
    const auto* City=D->GetCity(); const float Margin=bMini?18:80;
    auto Point=[&](FVector P) { return ProjectWorldPosition(P,Size,Margin); };
    auto Text=[&](FVector2D At,const FString& Value,int32 FontSize,FLinearColor Color)
    { FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(400,50),FSlateLayoutTransform(At)),Value,CourierUI::Font(FontSize),ESlateDrawEffect::None,Color); };
    const auto* Preview=bMini?nullptr:PC->GetOfferPreview();
    const auto* Interiors=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>();
    const auto Address=Interiors && PC->GetPawn()?Interiors->Locate(PC->GetPawn()->GetActorLocation()):FCourierIndoorAddress{};
    if(Address.IsValid() && !Preview)
    {
        const auto& B=Interiors->GetCatalog()->Buildings[Address.Building];
        const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>();
        FBox WorldBounds(EForceInit::ForceInit);
        for(int32 X:{-1,1}) for(int32 Y:{-1,1}) WorldBounds+=B.ToWorld(FVector(X*B.Size.X/2,Y*B.Size.Y/2,0));
        const FVector Extent=WorldBounds.GetSize();
        const double Scale=FMath::Min((Size.X-50)/Extent.Y,(Size.Y-(bMini?80:160))/Extent.X);
        auto IndoorPoint=[&](FVector P){const FVector Delta=P-B.Origin; return FVector2D(Size.X/2+Delta.Y*Scale,Size.Y/2+20-Delta.X*Scale);};
        for(const auto& R:B.Rooms) if(R.Floor==Address.Floor)
        {
            TArray<FVector2D> Corners;
            for(const FVector P:{FVector(R.Minimum.X,R.Minimum.Y,0),FVector(R.Maximum.X,R.Minimum.Y,0),FVector(R.Maximum.X,R.Maximum.Y,0),FVector(R.Minimum.X,R.Maximum.Y,0),FVector(R.Minimum.X,R.Minimum.Y,0)}) Corners.Add(IndoorPoint(B.ToWorld(P)));
            const bool Target=Nav && Nav->GetActiveRoute().TargetRoom==R.Id;
            FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Corners,ESlateDrawEffect::None,Target?CourierUI::Accent:FLinearColor(.36f,.48f,.5f),true,Target?3:1);
            const FVector2D Center=IndoorPoint(B.ToWorld(R.StandingPoint));
            if(R.Kind==TEXT("stairs")) Text(Center,TEXT("楼梯 ↑↓"),bMini?11:17,CourierUI::Accent);
            else if(!bMini && R.Kind!=TEXT("hall")) Text(Center,R.Doorplate()+TEXT(" ")+R.Label.ToString(),15,Target?CourierUI::Accent:CourierUI::Muted);
        }
        const double FloorZ=B.Floors[Address.Floor-1].Elevation+B.Origin.Z;
        if(Nav)
        {
            const auto& Route=Nav->GetActiveRoute();
            for(int32 I=1;I<Route.Points.Num();++I)
            {
                const FVector A=Route.Points[I-1],Z=Route.Points[I];
                if(FMath::Abs(A.Z-FloorZ)>80 || FMath::Abs(Z.Z-FloorZ)>80 || B.LocateRoom((A+Z)/2+FVector(0,0,90))==INDEX_NONE) continue;
                FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),{IndoorPoint(A),IndoorPoint(Z)},ESlateDrawEffect::None,CourierUI::Accent,true,bMini?2:4);
            }
        }
        for(const auto& Door:B.Doors) if(FMath::Abs(Door.Hinge.Z+B.Origin.Z-FloorZ)<50)
        {
            const FVector2D P=IndoorPoint(B.ToWorld(Door.Hinge+FRotator(0,Door.ClosedYaw,0).RotateVector(FVector(Door.Width/2,0,0))));
            FSlateDrawElement::MakeBox(Out,Layer+3,G.ToPaintGeometry(FVector2D(4,4),FSlateLayoutTransform(P-FVector2D(2,2))),Brush,ESlateDrawEffect::None,CourierUI::Accent);
        }
        const FVector2D Player=IndoorPoint(PC->GetPawn()->GetActorLocation());
        FSlateDrawElement::MakeBox(Out,Layer+4,G.ToPaintGeometry(FVector2D(8,8),FSlateLayoutTransform(Player-FVector2D(4,4))),Brush,ESlateDrawEffect::None,FLinearColor::White);
        Text(FVector2D(12,8),FString::Printf(TEXT("%d号楼 · %d楼"),Address.Building+1,Address.Floor),bMini?16:28,FLinearColor::White);
        if(Nav) Text(FVector2D(12,Size.Y-(bMini?24:60)),Nav->GetTargetLabel().ToString(),bMini?11:20,CourierUI::Accent);
        if(!bMini) Text(FVector2D(12,Size.Y-32),TEXT("金色：门 / 路线   ↑↓：楼梯   白色：你   M 返回"),18,CourierUI::Muted);
        return Layer+5;
    }
    for(const auto& E:City->Roads.Edges)
    {
        const auto* A=City->Roads.FindNode(E.From); const auto* B=City->Roads.FindNode(E.To); if(!A || !B) continue;
        FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),{Point(A->Position),Point(B->Position)},ESlateDrawEffect::None,FLinearColor(.22f,.33f,.35f),true,bMini?2:5);
    }
    auto DrawRoute=[&](const FRouteResult& Route,FLinearColor Color)
    {
        if(!Route.bReachable) return;
        TArray<FVector2D> Points; for(const auto& P:Route.Points) Points.Add(Point(P));
        if(Points.Num()>1) FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,bMini?3:6);
    };
    if(Preview) { DrawRoute(Preview->PickupRoute,CourierUI::Accent); DrawRoute(Preview->DeliveryRoute,FLinearColor(.3f,.8f,.8f)); }
    else if(const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()) DrawRoute(Nav->GetActiveRoute(),CourierUI::Accent);
    TArray<FSlateRect> LabelRects;
    auto Marker=[&](const FCourierLocationDefinition& L,FLinearColor Color)
    {
        FVector Position=L.Position;
        if(Interiors && Interiors->IsEnabled()) if(const auto* Actor=Interiors->GetManagedLocation(L.LocationId)) Position=Actor->GetActorLocation();
        const FVector2D P=Point(Position); const float R=bMini?3:6;
        FSlateDrawElement::MakeBox(Out,Layer+3,G.ToPaintGeometry(FVector2D(R*2,R*2),FSlateLayoutTransform(P-FVector2D(R,R))),Brush,ESlateDrawEffect::None,Color);
        if(!bMini)
        {
            // Full addresses belong in the selected journey header. Short map
            // labels use measured bounds so the station/bike cluster stays legible.
            const FString Label=L.DisplayName.ToString();
            const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(FStringView(Label),CourierUI::Font(16));
            for(int32 Attempt=0;Attempt<32;++Attempt)
            {
                const int32 Row=Attempt/2;
                const float Offset=(Row/2+1)*26*(Row%2?1:-1);
                FVector2D At=P+FVector2D(Attempt%2?-Extent.X-10:10,Offset);
                At.X=FMath::Clamp(At.X,20.,FMath::Max(20.,Size.X-Extent.X-20));
                At.Y=FMath::Clamp(At.Y,140.,Size.Y-130);
                const FSlateRect Bounds(At.X-4,At.Y-3,At.X+Extent.X+4,At.Y+Extent.Y+3);
                if(LabelRects.ContainsByPredicate([&](const auto& Other){return FSlateRect::DoRectanglesIntersect(Bounds,Other);})) continue;
                LabelRects.Add(Bounds); Text(At,Label,16,Color); break;
            }
        }
    };
    for(const auto& L:City->Merchants) Marker(L,CourierUI::Accent);
    for(const auto& L:City->Destinations) Marker(L,FLinearColor(.3f,.8f,.8f));
    Marker(City->Station,FLinearColor(.55f,.9f,.45f));
    if(auto* Bike=D->GetScooter())
    { FCourierLocationDefinition L; L.Position=Bike->GetActorLocation(); L.DisplayName=FText::FromString(TEXT("电动车")); Marker(L,FLinearColor(.8f,.5f,1)); }
    if(const APawn* Pawn=PC->GetPawn())
    {
        const FVector2D P=Point(Pawn->GetActorLocation());
        FSlateDrawElement::MakeBox(Out,Layer+4,G.ToPaintGeometry(FVector2D(10,10),FSlateLayoutTransform(P-FVector2D(5,5))),Brush,ESlateDrawEffect::None,FLinearColor::White);
    }
    Text(FVector2D(18,12),bMini?TEXT("南城街区  ·  M 地图"):Preview?TEXT("接单路线预览 / 尚未接单"):TEXT("南城街区 / 配送导航"),bMini?18:30,FLinearColor::White);
    if(Preview)
    {
        Text(FVector2D(30,82),TEXT("取餐：")+D->GetLocationLabel(Preview->MerchantId).ToString(),18,CourierUI::Accent);
        Text(FVector2D(30,110),TEXT("送餐：")+D->GetLocationLabel(Preview->DestinationId).ToString()+TEXT(" · 含上楼与开门"),18,FLinearColor(.3f,.8f,.8f));
        Text(FVector2D(30,Size.Y-78),FString::Printf(TEXT("金色取餐 %s + 青色送餐 %s = 总路程 %s"),*CourierUI::Distance(Preview->PickupRoute.LengthMeters),*CourierUI::Distance(Preview->DeliveryRoute.LengthMeters),*CourierUI::Distance(Preview->PickupRoute.LengthMeters+Preview->DeliveryRoute.LengthMeters)),20,FLinearColor::White);
    }
    else if(!bMini) if(const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>())
        Text(FVector2D(30,82),Nav->GetTargetLabel().ToString(),18,CourierUI::Accent);
    if(!bMini) Text(FVector2D(30,Size.Y-45),TEXT("绿色：驿站   紫色：车辆   白色：你   M 返回   Esc 暂停"),19,CourierUI::Muted);
    // Interactive controls are HUD siblings above this custom-painted widget.
    return Layer+5;
}

