#include "UI/CourierPhoneWidget.h"
#include "UI/CourierUI.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "World/CityDefinition.h"
#include "Vehicles/RiderRecoveryComponent.h"
#include "Components/ScrollBox.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Vehicles/ScooterPawn.h"
#include "UI/CourierHistoryWidget.h"
void UCourierPhoneWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root=WidgetTree->ConstructWidget<UVerticalBox>(); auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Scroll->AddChild(Root); WidgetTree->RootWidget=CourierUI::Panel(WidgetTree,Scroll);
    WorkPage=WidgetTree->ConstructWidget<UVerticalBox>(); Root->AddChild(WorkPage); auto* Column=WorkPage.Get();
    History=CreateWidget<UCourierHistoryWidget>(GetOwningPlayer(),UCourierHistoryWidget::StaticClass()); Root->AddChild(History);
    CourierUI::Add(Column,CourierUI::Text(WidgetTree,TEXT("骑手工作台"),38,CourierUI::Accent),4);
    CourierUI::Add(Column,CourierUI::Text(WidgetTree,TEXT("街头骑手  /  同时配送一单"),20,CourierUI::Muted),4);
    Status=CourierUI::Text(WidgetTree,TEXT("")); CourierUI::Add(Column,Status,4);
    auto* HistoryButton=CourierUI::Button(WidgetTree,TEXT("订单历史 · 最近20单")); HistoryButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::OpenHistory); CourierUI::Add(Column,HistoryButton,4);
    auto* StationButton=CourierUI::Button(WidgetTree,TEXT("去骑手驿站")); StationButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::NavigateStation); CourierUI::Add(Column,StationButton,4);
    UTextBlock* NavLabel=nullptr; BikeButton=CourierUI::Button(WidgetTree,TEXT("找车"),&NavLabel); BikeLabel=NavLabel; BikeButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::NavigateBike); CourierUI::Add(Column,BikeButton,4);
    OrderButton=CourierUI::Button(WidgetTree,TEXT("恢复订单导航"),&NavLabel); OrderLabel=NavLabel; OrderButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::NavigateOrder); CourierUI::Add(Column,OrderButton,4);
    RefreshButton=CourierUI::Button(WidgetTree,TEXT("刷新附近订单")); RefreshButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::RefreshOffers); CourierUI::Add(Column,RefreshButton,4);
    DisplayedIds.SetNum(3);
    for(int32 I=0;I<3;++I)
    {
        auto* Card=WidgetTree->ConstructWidget<UVerticalBox>(); auto* T=CourierUI::Text(WidgetTree,TEXT(""),22); OfferText.Add(T); CourierUI::Add(Card,T,4);
        auto* B=CourierUI::Button(WidgetTree,TEXT("接下这一单")); CourierUI::Add(Card,B,4);
        if(I==0) B->OnClicked.AddDynamic(this,&UCourierPhoneWidget::AcceptFirst);
        if(I==1) B->OnClicked.AddDynamic(this,&UCourierPhoneWidget::AcceptSecond);
        if(I==2) B->OnClicked.AddDynamic(this,&UCourierPhoneWidget::AcceptThird);
        auto* PreviewButton=CourierUI::Button(WidgetTree,TEXT("预览路线")); CourierUI::Add(Card,PreviewButton,4);
        if(I==0) PreviewButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::PreviewFirst);
        if(I==1) PreviewButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::PreviewSecond);
        if(I==2) PreviewButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::PreviewThird);
        auto* P=CourierUI::Panel(WidgetTree,Card,FLinearColor(.07f,.12f,.14f),12); Cards.Add(P); CourierUI::Add(Column,P,6);
    }
    UTextBlock* Label=nullptr; CancelButton=CourierUI::Button(WidgetTree,TEXT("取消当前订单"),&Label); CancelText=Label;
    CancelButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::CancelOrder); CourierUI::Add(Column,CancelButton);
    RescueButton=CourierUI::Button(WidgetTree,TEXT("呼叫救援"),&Label); RescueText=Label;
    RescueButton->OnClicked.AddDynamic(this,&UCourierPhoneWidget::Rescue); CourierUI::Add(Column,RescueButton);
    CourierUI::Add(Column,CourierUI::Text(WidgetTree,TEXT("Tab 收起  ·  M 地图\n手机打开时仍会计时，车辆自动刹停。"),20,CourierUI::Muted),4); RefreshFromState();
}
void UCourierPhoneWidget::RefreshFromState()
{
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); auto* D=PC?PC->GetDeliverySystem():nullptr; if(!D || !Status || !D->GetCity()) return;
    const auto& S=D->GetState(); auto Name=[&](FName Id) { return D->GetLocationLabel(Id).ToString(); };
    const bool bHistory=PC->IsOrderHistoryOpen(); WorkPage->SetVisibility(bHistory?ESlateVisibility::Collapsed:ESlateVisibility::Visible); History->SetVisibility(bHistory?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(bHistory && !bWasHistory) History->ResetPage(); bWasHistory=bHistory;
    if(bHistory) { History->RefreshFromState(); return; }
    const FString IdleStatus=D->GetOffers().IsEmpty()?TEXT("路线暂不可用，请返回道路后刷新订单"):FString::Printf(TEXT("附近有 %d 个待接订单"),D->GetOffers().Num());
    Status->SetText(FText::FromString(S.IsActive()?FString::Printf(TEXT("%s\n%s → %s\n请先收起手机，再到门口按 E。"),S.Status==EDeliveryStatus::AwaitingPickup?TEXT("正在前往取餐"):TEXT("正在配送"),*Name(S.Offer.MerchantId),*Name(S.Offer.DestinationId)):IdleStatus));
    for(int32 I=0;I<Cards.Num();++I)
    {
        const bool Visible=!S.IsActive() && D->GetOffers().IsValidIndex(I); Cards[I]->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        DisplayedIds[I]=Visible?D->GetOffers()[I].InstanceId:FGuid();
        if(Visible) { const auto& O=D->GetOffers()[I]; OfferText[I]->SetText(FText::FromString(FString::Printf(TEXT("%s → %s\n%s · 限时 %.0f 秒\n取餐 %s + 送餐 %s = %s\n按刷新时位置估算"),*Name(O.MerchantId),*Name(O.DestinationId),*CourierUI::Money(O.BaseFeeCents),O.DeadlineSeconds,*CourierUI::Distance(O.PickupRoute.LengthMeters),*CourierUI::Distance(O.DeliveryRoute.LengthMeters),*CourierUI::Distance(O.PickupRoute.LengthMeters+O.DeliveryRoute.LengthMeters)))); }
    }
    if(!PC->GetActionFeedback().IsEmpty()) Status->SetText(FText::FromString(Status->GetText().ToString()+TEXT("\n")+PC->GetActionFeedback().ToString()));
    RefreshButton->SetVisibility(S.IsActive()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    OrderButton->SetIsEnabled(S.IsActive()); OrderLabel->SetText(FText::FromString(S.IsActive()?TEXT("恢复订单导航"):TEXT("订单导航 · 当前无订单")));
    const bool bBikeAvailable=D->GetScooter() && !D->GetScooter()->GetRider(); BikeButton->SetIsEnabled(bBikeAvailable);
    BikeLabel->SetText(FText::FromString(bBikeAvailable?TEXT("找车 · 导航到停放位置"):TEXT("找车 · 已骑车或车辆不可用")));
    CancelButton->SetVisibility(S.IsActive()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(!S.IsActive()) bConfirmCancel=false;
    CancelText->SetText(FText::FromString(bConfirmCancel?TEXT("确认取消：本单无收入"):TEXT("取消当前订单")));
    const bool RescueAvailable=PC->GetRecovery() && PC->GetRecovery()->CanRescue();
    RescueButton->SetVisibility(RescueAvailable?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(!RescueAvailable) bConfirmRescue=false;
    RescueText->SetText(FText::FromString(bConfirmRescue?TEXT("确认救援（活动单失败）"):TEXT("呼叫救援 · 免费回站充电")));
}
void UCourierPhoneWidget::Accept(int32 Index)
{
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer()))
        if(DisplayedIds.IsValidIndex(Index) && PC->AcceptOffer(DisplayedIds[Index])) { bConfirmCancel=false; PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay); }
    RefreshFromState();
}
void UCourierPhoneWidget::AcceptFirst(){ Accept(0); }
void UCourierPhoneWidget::AcceptSecond(){ Accept(1); }
void UCourierPhoneWidget::AcceptThird(){ Accept(2); }
void UCourierPhoneWidget::Preview(int32 Index)
{ if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) if(DisplayedIds.IsValidIndex(Index)) PC->PreviewOffer(DisplayedIds[Index]); RefreshFromState(); }
void UCourierPhoneWidget::PreviewFirst(){ Preview(0); }
void UCourierPhoneWidget::PreviewSecond(){ Preview(1); }
void UCourierPhoneWidget::PreviewThird(){ Preview(2); }
void UCourierPhoneWidget::RefreshOffers()
{
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) if(auto* D=PC->GetDeliverySystem())
    { const bool bOk=PC->GetPawn() && D->RefreshOffers(PC->GetPawn()->GetActorLocation()); PC->ShowActionFeedback(FText::FromString(bOk?TEXT("订单已刷新"):TEXT("无法刷新，请先完成当前订单并返回道路"))); }
    RefreshFromState();
}
void UCourierPhoneWidget::OpenHistory() { if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) PC->OpenOrderHistory(); RefreshFromState(); }
void UCourierPhoneWidget::Navigate(uint8 Mode)
{
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); auto* N=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>(); if(!PC || !N) return;
    const auto Result=N->SetNavigationMode(static_cast<ECourierNavigationMode>(Mode));
    if(Result.bSucceeded) { PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay); PC->ShowActionFeedback(FText::FromString(TEXT("导航已切换；活动订单仍在计时"))); }
    else PC->ShowActionFeedback(Result.Reason); RefreshFromState();
}
void UCourierPhoneWidget::NavigateStation(){ Navigate(static_cast<uint8>(ECourierNavigationMode::Station)); }
void UCourierPhoneWidget::NavigateBike(){ Navigate(static_cast<uint8>(ECourierNavigationMode::ParkedBike)); }
void UCourierPhoneWidget::NavigateOrder(){ Navigate(static_cast<uint8>(ECourierNavigationMode::Order)); }
void UCourierPhoneWidget::CancelOrder()
{
    if(!bConfirmCancel) { bConfirmCancel=true; RefreshFromState(); return; }
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) PC->GetDeliverySystem()->TryCancel(EDeliveryFailure::Cancelled);
}
void UCourierPhoneWidget::Rescue()
{
    if(!bConfirmRescue) { bConfirmRescue=true; RefreshFromState(); return; }
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer()))
        if(PC->GetRecovery()->TryRescue() && !PC->GetDeliverySystem()->GetState().IsActive() && PC->GetInterfaceMode()!=ECourierInterfaceMode::Settlement)
            PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
    bConfirmRescue=false; RefreshFromState();
}

