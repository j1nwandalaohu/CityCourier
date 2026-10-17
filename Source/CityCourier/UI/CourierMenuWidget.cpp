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
    Title->SetText(FText::FromString(Settlement?TEXT("配送结算"):TEXT("休息一下")));
    FString Description=TEXT("游戏已暂停。返回街区后继续计时。");
    if(Settlement)
    {
        const auto& S=PC->GetDeliverySystem()->GetState(); const auto& Result=PC->GetDeliverySystem()->GetLastSettlement();
        const int64 IntegrityFee=S.Offer.BaseFeeCents*S.IntegrityPercent/100;
        const bool Late=S.ElapsedSeconds>=S.Offer.DeadlineSeconds;
        Description=FString::Printf(TEXT("%s\n基础配送费      %s\n餐品完整度      %d%%（折减 %s）\n时间调整         %s\n实际收入         %s"),Result.bCompleted?TEXT("送达成功，辛苦了！"):TEXT("本单未完成"),*CourierUI::Money(S.Offer.BaseFeeCents),S.IntegrityPercent,*CourierUI::Money(S.Offer.BaseFeeCents-IntegrityFee),Late?TEXT("超时，收入按六折计算"):TEXT("准时，无折减"),*CourierUI::Money(Result.PayoutCents));
        if(!Result.bCompleted) Description+=TEXT("\n失败订单不扣余额；可以接下一单。");
    }
    if(P->NeedsReset()) Description=TEXT("主存档和备份都无法读取。\n损坏文件已保留，请选择创建新进度或退出。\n创建新进度后，余额与升级将从零开始。");
    if(bConfirmQuit) Description=TEXT("确认退出游戏？\n进行中的订单将被丢弃，重启回到休息站。\n已保存的收入和升级会保留。")+(bExitWarned?FString(TEXT("\n本次进度仍未保存，继续退出会丢失未保存内容。")):FString());
    Body->SetText(FText::FromString(Description));
    SaveMessage->SetText(FText::FromString(P->HasUnsavedChanges()?TEXT("本次进度未保存，请重试。"):P->GetLoadResult()==EProfileLoadResult::RecoveredBackup?TEXT("已从备份恢复进度。"):FString::Printf(TEXT("余额 %s  ·  当前画质：%s"),*CourierUI::Money(P->GetProfile().BalanceCents),P->GetProfile().QualityLevel==0?TEXT("低"):P->GetProfile().QualityLevel==1?TEXT("中"):TEXT("高"))));
    Settings->SetVisibility(!Settlement && !P->NeedsReset() && !bConfirmQuit?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
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

