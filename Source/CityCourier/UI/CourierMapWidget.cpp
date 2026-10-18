#include "UI/CourierMapWidget.h"
#include "UI/CourierUI.h"
#include "Components/CanvasPanel.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "World/CityDefinition.h"
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
    Layer=Super::NativePaint(Args,G,Culling,Out,Layer,Style,bEnabled);
    const FVector2D Size=G.GetLocalSize(); const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    FSlateDrawElement::MakeBox(Out,++Layer,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,CourierUI::Ink);
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); const auto* D=PC?PC->GetDeliverySystem():nullptr; if(!D || !D->GetCity()) return Layer;
    const auto* City=D->GetCity(); const float Margin=bMini?18:80;
    auto Point=[&](FVector P) { return ProjectWorldPosition(P,Size,Margin); };
    auto Text=[&](FVector2D At,const FString& Value,int32 FontSize,FLinearColor Color)
    { FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(400,50),FSlateLayoutTransform(At)),Value,CourierUI::Font(FontSize),ESlateDrawEffect::None,Color); };
    for(const auto& E:City->Roads.Edges)
    {
        const auto* A=City->Roads.FindNode(E.From); const auto* B=City->Roads.FindNode(E.To); if(!A || !B) continue;
        FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),{Point(A->Position),Point(B->Position)},ESlateDrawEffect::None,FLinearColor(.22f,.33f,.35f),true,bMini?2:5);
    }
    if(const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>())
    {
        TArray<FVector2D> Points; for(const auto& P:Nav->GetActiveRoute().Points) Points.Add(Point(P));
        if(Points.Num()>1) FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,CourierUI::Accent,true,bMini?3:6);
    }
    auto Marker=[&](const FCourierLocationDefinition& L,FLinearColor Color)
    {
        const FVector2D P=Point(L.Position); const float R=bMini?3:6;
        FSlateDrawElement::MakeBox(Out,Layer+3,G.ToPaintGeometry(FVector2D(R*2,R*2),FSlateLayoutTransform(P-FVector2D(R,R))),Brush,ESlateDrawEffect::None,Color);
        if(!bMini) Text(P+FVector2D(10,-24),L.DisplayName.ToString(),18,Color);
    };
    for(const auto& L:City->Merchants) Marker(L,CourierUI::Accent);
    for(const auto& L:City->Destinations) Marker(L,FLinearColor(.3f,.8f,.8f));
    Marker(City->Station,FLinearColor(.55f,.9f,.45f));
    if(const APawn* Pawn=PC->GetPawn())
    {
        const FVector2D P=Point(Pawn->GetActorLocation());
        FSlateDrawElement::MakeBox(Out,Layer+4,G.ToPaintGeometry(FVector2D(10,10),FSlateLayoutTransform(P-FVector2D(5,5))),Brush,ESlateDrawEffect::None,FLinearColor::White);
    }
    Text(FVector2D(18,12),bMini?TEXT("南城街区  ·  M 地图"):TEXT("南城街区 / 配送导航"),bMini?18:30,FLinearColor::White);
    if(!bMini) Text(FVector2D(30,Size.Y-48),TEXT("金色：商户 / 路线    青色：收餐点    绿色：休息站    白色：你    M 返回"),19,CourierUI::Muted);
    return Layer+5;
}

