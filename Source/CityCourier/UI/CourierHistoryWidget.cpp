#include "UI/CourierHistoryWidget.h"
#include "UI/CourierUI.h"
#include "Components/ScrollBox.h"
#include "Player/CourierController.h"
#include "Profile/ProfileSubsystem.h"

void UCourierHistoryButton::SetEntry(UCourierHistoryWidget* Parent,FGuid Id)
{ Owner=Parent; EntryId=Id; OnClicked.AddDynamic(this,&UCourierHistoryButton::Open); }
void UCourierHistoryButton::Open() { if(Owner.IsValid()) Owner->OpenEntry(EntryId); }
void UCourierHistoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized(); auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Column;
    CourierUI::Add(Column,CourierUI::Text(WidgetTree,TEXT("订单历史"),32,CourierUI::Accent));
    UTextBlock* Label=nullptr; auto* Button=CourierUI::Button(WidgetTree,TEXT("返回接单页"),&Label); BackLabel=Label;
    Button->OnClicked.AddDynamic(this,&UCourierHistoryWidget::Back); CourierUI::Add(Column,Button);
    Rows=WidgetTree->ConstructWidget<UVerticalBox>(); CourierUI::Add(Column,Rows);
    RefreshFromState();
}
void UCourierHistoryWidget::ResetPage() { Selected.Reset(); LastCount=-1; RefreshFromState(); }
void UCourierHistoryWidget::Back()
{
    if(Selected.IsSet()) ResetPage();
    else if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) PC->CloseOrderHistory();
}
void UCourierHistoryWidget::OpenEntry(FGuid Id)
{
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); auto* P=PC?PC->GetProfileSystem():nullptr; if(!P) return;
    if(const auto* H=P->GetProfile().OrderHistory.FindByPredicate([&](const auto& E){return E.InstanceId==Id;})) { Selected=*H; Rebuild(); }
}
void UCourierHistoryWidget::RefreshFromState()
{
    if(!Rows || Selected.IsSet()) return;
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); auto* P=PC?PC->GetProfileSystem():nullptr;
    const int32 Count=P?P->GetProfile().OrderHistory.Num():0;
    const FGuid Newest=Count>0?P->GetProfile().OrderHistory[0].InstanceId:FGuid();
    if(Count!=LastCount || Newest!=LastNewest) { LastCount=Count; LastNewest=Newest; Rebuild(); }
}
void UCourierHistoryWidget::Rebuild()
{
    if(!Rows) return; Rows->ClearChildren();
    BackLabel->SetText(FText::FromString(Selected.IsSet()?TEXT("返回历史列表"):TEXT("返回接单页")));
    if(Selected.IsSet())
    {
        const auto& H=Selected.GetValue(); const auto LocalTime=H.SettledAtUtc+(FDateTime::Now()-FDateTime::UtcNow());
        auto* Names=CourierUI::Text(WidgetTree,H.MerchantName+TEXT(" → ")+H.DestinationName,26);
        Names->SetWrapTextAt(550); CourierUI::Add(Rows,Names);
        CourierUI::Add(Rows,CourierUI::Text(WidgetTree,FString::Printf(TEXT("%s\n单号 %s\n用时 %.0f秒 / 承诺 %.0f秒"),*LocalTime.ToString(TEXT("%Y-%m-%d %H:%M")),*H.InstanceId.ToString(EGuidFormats::Digits).Left(8),H.ElapsedSeconds,H.DeadlineSeconds),20,CourierUI::Muted));
        CourierUI::Add(Rows,CourierUI::Text(WidgetTree,FOrderHistoryRules::DescribeIncome(H).ToString(),24)); return;
    }
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); auto* P=PC?PC->GetProfileSystem():nullptr;
    if(!P || P->GetProfile().OrderHistory.IsEmpty())
    { CourierUI::Add(Rows,CourierUI::Text(WidgetTree,TEXT("还没有订单记录。\n历史从本版本开始记录，旧版累计收入与升级已保留。"),22,CourierUI::Muted)); return; }
    CourierUI::Add(Rows,CourierUI::Text(WidgetTree,TEXT("最近20单 · 点击查看明细\n查看历史时订单仍在计时。"),20,CourierUI::Muted));
    for(const auto& H:P->GetProfile().OrderHistory)
    {
        auto* Button=WidgetTree->ConstructWidget<UCourierHistoryButton>(); Button->SetEntry(this,H.InstanceId);
        const auto Time=H.SettledAtUtc+(FDateTime::Now()-FDateTime::UtcNow());
        auto* Text=CourierUI::Text(WidgetTree,FString::Printf(TEXT("%s → %s\n%s · %s\n%s"),*H.MerchantName,*H.DestinationName,*FOrderHistoryRules::DescribeOutcome(H).ToString(),*CourierUI::Money(H.PayoutCents),*Time.ToString(TEXT("%m-%d %H:%M"))),22);
        Text->SetWrapTextAt(530);
        Button->SetContent(CourierUI::Panel(WidgetTree,Text,FLinearColor(.07f,.12f,.14f),12)); CourierUI::Add(Rows,Button,6);
    }
}
