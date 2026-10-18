#include "UI/CourierMapWidget.h"
#include "UI/CourierUI.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Vehicles/ScooterPawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "World/CityDefinition.h"
void UCourierMapWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized(); auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
}
int32 UCourierMapWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
    const FVector2D Size=G.GetLocalSize(); const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    FSlateDrawElement::MakeBox(Out,++Layer,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,CourierUI::Ink);
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); const auto* D=PC?PC->GetDeliverySystem():nullptr; if(!D || !D->GetCity()) return Layer;
    const auto* City=D->GetCity(); const float Margin=bMini?18:80; const double Scale=FMath::Min(Size.X-2*Margin,Size.Y-2*Margin)/60000;
    auto Point=[&](FVector P) { return FVector2D(Size.X*.5+P.X*Scale,Size.Y*.5-P.Y*Scale); };
    auto Text=[&](FVector2D At,const FString& Value,int32 FontSize,FLinearColor Color)
    { FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(400,50),FSlateLayoutTransform(At)),Value,CourierUI::Font(FontSize),ESlateDrawEffect::None,Color); };
    for(const auto& E:City->Roads.Edges)
    {
        const auto* A=City->Roads.FindNode(E.From); const auto* B=City->Roads.FindNode(E.To); if(!A || !B) continue;
        FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),{Point(A->Position),Point(B->Position)},ESlateDrawEffect::None,FLinearColor(.22f,.33f,.35f),true,bMini?2:5);
    }
    const auto* Preview=bMini?nullptr:PC->GetOfferPreview();
    auto DrawRoute=[&](const FRouteResult& Route,FLinearColor Color)
    {
        if(!Route.bReachable) return;
        TArray<FVector2D> Points; for(const auto& P:Route.Points) Points.Add(Point(P));
        if(Points.Num()>1) FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,bMini?3:6);
    };
    if(Preview) { DrawRoute(Preview->PickupRoute,CourierUI::Accent); DrawRoute(Preview->DeliveryRoute,FLinearColor(.3f,.8f,.8f)); }
    else if(const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()) DrawRoute(Nav->GetActiveRoute(),CourierUI::Accent);
    auto Marker=[&](const FCourierLocationDefinition& L,FLinearColor Color)
    {
        const FVector2D P=Point(L.Position); const float R=bMini?3:6;
        FSlateDrawElement::MakeBox(Out,Layer+3,G.ToPaintGeometry(FVector2D(R*2,R*2),FSlateLayoutTransform(P-FVector2D(R,R))),Brush,ESlateDrawEffect::None,Color);
        if(!bMini) Text(P+FVector2D(10,-24),L.DisplayName.ToString(),18,Color);
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
        const auto* M=City->FindLocation(Preview->MerchantId); const auto* Dst=City->FindLocation(Preview->DestinationId);
        Text(FVector2D(30,58),FString::Printf(TEXT("%s → %s · 按刷新时位置估算"),M?*M->DisplayName.ToString():TEXT("商家"),Dst?*Dst->DisplayName.ToString():TEXT("收货点")),20,CourierUI::Muted);
        Text(FVector2D(30,Size.Y-78),FString::Printf(TEXT("金色取餐 %s + 青色送餐 %s = 总路程 %s"),*CourierUI::Distance(Preview->PickupRoute.LengthMeters),*CourierUI::Distance(Preview->DeliveryRoute.LengthMeters),*CourierUI::Distance(Preview->PickupRoute.LengthMeters+Preview->DeliveryRoute.LengthMeters)),20,FLinearColor::White);
    }
    if(!bMini) Text(FVector2D(30,Size.Y-45),TEXT("绿色：驿站   紫色：车辆   白色：你   M 返回   Esc 暂停"),19,CourierUI::Muted);
    // Interactive controls are HUD siblings above this custom-painted widget.
    return Layer+5;
}

