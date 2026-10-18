#include "UI/CourierMenuWidget.h"
#include "UI/CourierUI.h"
#include "UI/CourierSettings.h"
#include "Components/Slider.h"
#include "Components/HorizontalBox.h"
#include "Player/CourierController.h"
#include "Delivery/DeliverySubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "World/RiderStation.h"
#include "Profile/ProfileRules.h"
void UCourierMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized(); auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=CourierUI::Panel(WidgetTree,Column);
    Title=CourierUI::Text(WidgetTree,TEXT(""),36,CourierUI::Accent); CourierUI::Add(Column,Title);
    Body=CourierUI::Text(WidgetTree,TEXT(""),24); CourierUI::Add(Column,Body,14);
    SaveMessage=CourierUI::Text(WidgetTree,TEXT(""),22,CourierUI::Accent); CourierUI::Add(Column,SaveMessage);
    Settings=WidgetTree->ConstructWidget<UVerticalBox>(); CourierUI::Add(Column,Settings);
    CourierUI::Add(Settings,CourierUI::Text(WidgetTree,TEXT("音量"),22,CourierUI::Muted));
    Volume=WidgetTree->ConstructWidget<USlider>(); Volume->SetValue(GetGameInstance()->GetSubsystem<UProfileSubsystem>()->GetProfile().MasterVolume);
    Volume->OnMouseCaptureEnd.AddDynamic(this,&UCourierMenuWidget::CommitVolume); Volume->OnControllerCaptureEnd.AddDynamic(this,&UCourierMenuWidget::CommitVolume); CourierUI::Add(Settings,Volume,14);
    CourierUI::Add(Settings,CourierUI::Text(WidgetTree,TEXT("画质"),22,CourierUI::Muted));
    auto* Quality=WidgetTree->ConstructWidget<UHorizontalBox>(); CourierUI::Add(Settings,Quality);
    auto* Low=CourierUI::Button(WidgetTree,TEXT("低")); auto* Mid=CourierUI::Button(WidgetTree,TEXT("中")); auto* High=CourierUI::Button(WidgetTree,TEXT("高"));
    Quality->AddChild(Low); Quality->AddChild(Mid); Quality->AddChild(High);
    Low->OnClicked.AddDynamic(this,&UCourierMenuWidget::QualityLow); Mid->OnClicked.AddDynamic(this,&UCourierMenuWidget::QualityMedium); High->OnClicked.AddDynamic(this,&UCourierMenuWidget::QualityHigh);
    StationActions=WidgetTree->ConstructWidget<UVerticalBox>(); CourierUI::Add(Column,StationActions);
    UTextBlock* ChargeLabel=nullptr; auto* ChargeButton=CourierUI::Button(WidgetTree,TEXT("免费充电 · 5秒"),&ChargeLabel); ChargeText=ChargeLabel;
    ChargeButton->OnClicked.AddDynamic(this,&UCourierMenuWidget::Charge); CourierUI::Add(StationActions,ChargeButton);
    for(int32 I=0;I<3;++I)
    {
        UTextBlock* T=nullptr; auto* B=CourierUI::Button(WidgetTree,TEXT("升级"),&T); UpgradeTexts.Add(T); CourierUI::Add(StationActions,B);
        if(I==0) B->OnClicked.AddDynamic(this,&UCourierMenuWidget::UpgradePower);
        if(I==1) B->OnClicked.AddDynamic(this,&UCourierMenuWidget::UpgradeHandling);
        if(I==2) B->OnClicked.AddDynamic(this,&UCourierMenuWidget::UpgradeBattery);
    }
    auto* Continue=CourierUI::Button(WidgetTree,TEXT("返回街区")); Continue->OnClicked.AddDynamic(this,&UCourierMenuWidget::Resume); CourierUI::Add(Column,Continue,14);
    RetryButton=CourierUI::Button(WidgetTree,TEXT("重试保存")); RetryButton->OnClicked.AddDynamic(this,&UCourierMenuWidget::RetrySave); CourierUI::Add(Column,RetryButton);
    UTextBlock* Label=nullptr; ResetButton=CourierUI::Button(WidgetTree,TEXT("创建新进度"),&Label); ResetText=Label;
    ResetButton->OnClicked.AddDynamic(this,&UCourierMenuWidget::ResetSave); CourierUI::Add(Column,ResetButton);
    auto* Exit=CourierUI::Button(WidgetTree,TEXT("退出游戏"),&Label); QuitText=Label; Exit->OnClicked.AddDynamic(this,&UCourierMenuWidget::Quit); CourierUI::Add(Column,Exit);
    RefreshFromState();
}
void UCourierMenuWidget::RefreshFromState()
{
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); if(!PC || !Title) return;
    const auto* P=GetGameInstance()->GetSubsystem<UProfileSubsystem>(); const bool Settlement=PC->GetInterfaceMode()==ECourierInterfaceMode::Settlement;
    auto* Station=PC->GetInterfaceMode()==ECourierInterfaceMode::Station?PC->GetStation():nullptr;
    Title->SetText(FText::FromString(Station?TEXT("南城骑手驿站"):Settlement?TEXT("配送结算"):TEXT("休息一下")));
    FString Description=TEXT("游戏已暂停。返回街区后继续计时。");
    if(Settlement)
    {
        const auto& Result=PC->GetDeliverySystem()->GetLastSettlement();
        Description=FOrderHistoryRules::DescribeIncome(Result.History).ToString()+TEXT("\n可在手机的订单历史中再次查看。");
    }
    if(Station)
    {
        Description=TEXT("车辆停在入口12米内，停车后可充电和升级。\n玩家需留在站点2米内；订单仍在计时。");
        const FString CurrentMessage=Station->GetServiceMessage().IsEmpty()?ServiceMessage:Station->GetServiceMessage().ToString();
        if(!CurrentMessage.IsEmpty()) Description+=TEXT("\n")+CurrentMessage;
        ChargeText->SetText(FText::FromString(Station->IsCharging()?FString::Printf(TEXT("充电 %.0f%% · 点击取消"),Station->GetChargeProgress()*100):TEXT("免费充电 · 5秒")));
        const int32 Levels[]={P->GetProfile().PowerLevel,P->GetProfile().HandlingLevel,P->GetProfile().BatteryLevel};
        const TCHAR* Names[]={TEXT("动力"),TEXT("操控"),TEXT("电池")};
        for(int32 I=0;I<3;++I)
        {
            const int64 Cost=FProfileRules::GetUpgradeCost(static_cast<EUpgradeKind>(I),Levels[I]);
            const FString Label=Cost<0?FString::Printf(TEXT("%s · 已满级"),Names[I]):FString::Printf(TEXT("%s%s %d → %d级 · %s"),PendingUpgrade==I?TEXT("确认购买 "):TEXT(""),Names[I],Levels[I],Levels[I]+1,*CourierUI::Money(Cost));
            UpgradeTexts[I]->SetText(FText::FromString(Label));
        }
    }
    if(P->NeedsReset()) Description=P->GetLastError()+TEXT("\n创建新进度需要确认，余额与升级将从零开始。");
    if(bConfirmQuit) Description=TEXT("确认退出游戏？\n进行中的订单将被丢弃，重启回到休息站。\n已保存的收入和升级会保留。")+(bExitWarned?FString(TEXT("\n本次进度仍未保存，继续退出会丢失未保存内容。")):FString());
    Body->SetText(FText::FromString(Description));
    SaveMessage->SetText(FText::FromString(P->HasUnsavedChanges()?TEXT("本次进度未保存，请重试。"):P->GetLoadResult()==EProfileLoadResult::RecoveredBackup?TEXT("已从备份恢复进度。"):FString::Printf(TEXT("余额 %s  ·  当前画质：%s"),*CourierUI::Money(P->GetProfile().BalanceCents),P->GetProfile().QualityLevel==0?TEXT("低"):P->GetProfile().QualityLevel==1?TEXT("中"):TEXT("高"))));
    Settings->SetVisibility(!Station && !Settlement && !P->NeedsReset() && !bConfirmQuit?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    StationActions->SetVisibility(Station && !P->NeedsReset() && !bConfirmQuit?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    RetryButton->SetVisibility(P->HasUnsavedChanges() && !P->NeedsReset()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    ResetButton->SetVisibility(P->NeedsReset()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    ResetText->SetText(FText::FromString(bConfirmReset?TEXT("确认创建：余额与升级归零"):TEXT("创建新进度")));
    QuitText->SetText(FText::FromString(bConfirmQuit?TEXT("确认退出游戏"):TEXT("退出游戏")));
}
void UCourierMenuWidget::Resume()
{
    if(bConfirmQuit || bConfirmReset) { ResetDialog(); RefreshFromState(); return; }
    if(!GetGameInstance()->GetSubsystem<UProfileSubsystem>()->NeedsReset()) if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) PC->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
}
void UCourierMenuWidget::RetrySave() { GetGameInstance()->GetSubsystem<UProfileSubsystem>()->SaveProfile(); RefreshFromState(); }
void UCourierMenuWidget::ResetSave()
{
    if(!bConfirmReset) { bConfirmReset=true; RefreshFromState(); return; }
    GetGameInstance()->GetSubsystem<UProfileSubsystem>()->ResetProfileAfterConfirmation(); bConfirmReset=false; RefreshFromState();
}
void UCourierMenuWidget::Quit()
{
    if(!bConfirmQuit) { bConfirmQuit=true; RefreshFromState(); return; }
    auto* P=GetGameInstance()->GetSubsystem<UProfileSubsystem>();
    if(P->HasUnsavedChanges() && !P->NeedsReset() && !P->SaveProfile() && !bExitWarned) { bExitWarned=true; RefreshFromState(); return; }
    UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);
}
void UCourierMenuWidget::CommitVolume()
{ auto* P=GetGameInstance()->GetSubsystem<UProfileSubsystem>(); P->UpdateSettings(Volume->GetValue(),P->GetProfile().QualityLevel); UCourierSettings::Apply(P->GetProfile()); }
void UCourierMenuWidget::SetQuality(int32 Level)
{ auto* P=GetGameInstance()->GetSubsystem<UProfileSubsystem>(); P->UpdateSettings(P->GetProfile().MasterVolume,Level); UCourierSettings::Apply(P->GetProfile()); RefreshFromState(); }
void UCourierMenuWidget::QualityLow(){ SetQuality(0); }
void UCourierMenuWidget::QualityMedium(){ SetQuality(1); }
void UCourierMenuWidget::QualityHigh(){ SetQuality(2); }
void UCourierMenuWidget::Charge()
{
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) if(auto* S=PC->GetStation())
    {
        PendingUpgrade=-1;
        if(S->IsCharging()) { S->CancelCharge(); ServiceMessage=TEXT("已取消充电，电量保持不变。"); }
        else { const auto R=S->BeginChargeWithFeedback(PC->GetCourier()); ServiceMessage=R.bSucceeded?TEXT("充电需5秒，完成后电量为100%。"):R.Reason.ToString(); }
    }
    RefreshFromState();
}
void UCourierMenuWidget::Purchase(int32 Kind)
{
    if(PendingUpgrade!=Kind) { PendingUpgrade=Kind; RefreshFromState(); return; }
    PendingUpgrade=-1;
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) if(auto* S=PC->GetStation())
    { const auto R=S->TryPurchaseWithFeedback(static_cast<EUpgradeKind>(Kind),PC->GetCourier()); ServiceMessage=R.bSucceeded?TEXT("升级成功，车辆性能已更新。"):R.Reason.ToString(); }
    RefreshFromState();
}
void UCourierMenuWidget::UpgradePower(){ Purchase(0); }
void UCourierMenuWidget::UpgradeHandling(){ Purchase(1); }
void UCourierMenuWidget::UpgradeBattery(){ Purchase(2); }

