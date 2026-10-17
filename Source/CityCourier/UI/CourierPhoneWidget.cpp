#include "UI/CourierPhoneWidget.h"
#include "UI/CourierUI.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "World/CityDefinition.h"
void UCourierPhoneWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=CourierUI::Panel(WidgetTree,Column);
    CourierUI::Add(Column,CourierUI::Text(WidgetTree,TEXT("骑手工作台"),38,CourierUI::Accent),4);
    CourierUI::Add(Column,CourierUI::Text(WidgetTree,TEXT("街头骑手  /  同时配送一单"),20,CourierUI::Muted),4);
    Status=CourierUI::Text(WidgetTree,TEXT("")); CourierUI::Add(Column,Status,4);
    for(int32 I=0;I<3;++I)
    {
        auto* Card=WidgetTree->ConstructWidget<UVerticalBox>(); auto* T=CourierUI::Text(WidgetTree,TEXT(""),22); OfferText.Add(T); CourierUI::Add(Card,T,4);
        auto* B=CourierUI::Button(WidgetTree,TEXT("接下这一单")); CourierUI::Add(Card,B,4);
        if(I==0) B->OnClicked.AddDynamic(this,&UCourierPhoneWidget::AcceptFirst);
        if(I==1) B->OnClicked.AddDynamic(this,&UCourierPhoneWidget::AcceptSecond);
        if(I==2) B->OnClicked.AddDynamic(this,&UCourierPhoneWidget::AcceptThird);
        auto* P=CourierUI::Panel(WidgetTree,Card,FLinearColor(.07f,.12f,.14f),12); Cards.Add(P); CourierUI::Add(Column,P,6);
    }
    UTextBlock* Label=nullptr; CancelButton=CourierUI::Button(WidgetTree,TEXT("取消当前订单"),&Label); CancelText=Label;
    CancelButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::CancelOrder); CourierUI::Add(Column,CancelButton);
    CourierUI::Add(Column,CourierUI::Text(WidgetTree,TEXT("Tab 收起  ·  M 地图\n手机打开时仍会计时，车辆自动刹停。"),20,CourierUI::Muted),4); RefreshFromState();
}
void UCourierPhoneWidget::RefreshFromState()
{
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); auto* D=PC?PC->GetDeliverySystem():nullptr; if(!D || !Status || !D->GetCity()) return;
    const auto& S=D->GetState(); auto Name=[&](FName Id) { const auto* L=D->GetCity()->FindLocation(Id); return L?L->DisplayName.ToString():Id.ToString(); };
    Status->SetText(FText::FromString(S.IsActive()?FString::Printf(TEXT("%s\n%s → %s\n请先收起手机，再到门口按 E。"),S.Status==EDeliveryStatus::AwaitingPickup?TEXT("正在前往取餐"):TEXT("正在配送"),*Name(S.Offer.MerchantId),*Name(S.Offer.DestinationId)):TEXT("附近有 3 个待接订单")));
    for(int32 I=0;I<Cards.Num();++I)
    {
        const bool Visible=!S.IsActive() && D->GetOffers().IsValidIndex(I); Cards[I]->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        if(Visible) { const auto& O=D->GetOffers()[I]; OfferText[I]->SetText(FText::FromString(FString::Printf(TEXT("%s → %s\n%s     限时 %.0f 秒"),*Name(O.MerchantId),*Name(O.DestinationId),*CourierUI::Money(O.BaseFeeCents),O.DeadlineSeconds))); }
    }
    CancelButton->SetVisibility(S.IsActive()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(!S.IsActive()) bConfirmCancel=false;
    CancelText->SetText(FText::FromString(bConfirmCancel?TEXT("确认取消：本单无收入"):TEXT("取消当前订单")));
}
void UCourierPhoneWidget::Accept(int32 Index)
{
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer()))
        if(PC->CourierOrder(Index+1)) { bConfirmCancel=false; PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay); }
    RefreshFromState();
}
void UCourierPhoneWidget::AcceptFirst(){ Accept(0); }
void UCourierPhoneWidget::AcceptSecond(){ Accept(1); }
void UCourierPhoneWidget::AcceptThird(){ Accept(2); }
void UCourierPhoneWidget::CancelOrder()
{
    if(!bConfirmCancel) { bConfirmCancel=true; RefreshFromState(); return; }
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) PC->GetDeliverySystem()->TryCancel(EDeliveryFailure::Cancelled);
}

