#include "UI/CourierHUDWidget.h"
#include "UI/CourierUI.h"
#include "UI/CourierPhoneWidget.h"
#include "UI/CourierMapWidget.h"
#include "UI/CourierMenuWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "World/CityDefinition.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Vehicles/RiderRecoveryComponent.h"
void UCourierHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized(); SetIsFocusable(true);
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
    auto Place=[&](UWidget* W,FVector2D Anchor,FVector2D Align,FVector2D Position,FVector2D Size,int32 Z=0)
    { auto* S=Canvas->AddChildToCanvas(W); S->SetAnchors(FAnchors(Anchor.X,Anchor.Y)); S->SetAlignment(Align); S->SetPosition(Position); S->SetSize(Size); S->SetZOrder(Z); };
    auto* Goal=WidgetTree->ConstructWidget<UVerticalBox>();
    CourierUI::Add(Goal,CourierUI::Text(WidgetTree,TEXT("街头骑手 / 南城"),20,CourierUI::Accent),0);
    Objective=CourierUI::Text(WidgetTree,TEXT(""),30); CourierUI::Add(Goal,Objective);
    Time=CourierUI::Text(WidgetTree,TEXT(""),22); CourierUI::Add(Goal,Time,0);
    ObjectivePanel=CourierUI::Panel(WidgetTree,Goal); Place(ObjectivePanel,FVector2D(0,0),FVector2D(0,0),FVector2D(32,32),FVector2D(660,220));
    Balance=CourierUI::Text(WidgetTree,TEXT(""),30,CourierUI::Accent);
    Place(CourierUI::Panel(WidgetTree,Balance),FVector2D(1,0),FVector2D(1,0),FVector2D(-32,32),FVector2D(300,100));
    Vehicle=CourierUI::Text(WidgetTree,TEXT(""),26); Place(CourierUI::Panel(WidgetTree,Vehicle),FVector2D(1,1),FVector2D(1,1),FVector2D(-32,-72),FVector2D(300,160));
    Prompt=CourierUI::Text(WidgetTree,TEXT(""),26,CourierUI::Accent); Prompt->SetJustification(ETextJustify::Center);
    Place(Prompt,FVector2D(.5,1),FVector2D(.5,1),FVector2D(0,-145),FVector2D(700,45));
    SaveWarning=CourierUI::Text(WidgetTree,TEXT(""),22,CourierUI::Accent); SaveWarning->SetJustification(ETextJustify::Center);
    Place(SaveWarning,FVector2D(.5,0),FVector2D(.5,0),FVector2D(0,235),FVector2D(900,65));
    Controls=CourierUI::Text(WidgetTree,TEXT("WASD 移动 / 骑行  Shift 奔跑  E 交互 / 上下车  空格 刹车  长按 R 脱困  Tab 手机  M 地图  Esc 暂停"),20);
    Place(Controls,FVector2D(.5,1),FVector2D(.5,1),FVector2D(0,-22),FVector2D(1540,32));
    MiniMap=CreateWidget<UCourierMapWidget>(GetOwningPlayer(),UCourierMapWidget::StaticClass()); MiniMap->bMini=true;
    Place(MiniMap,FVector2D(0,1),FVector2D(0,1),FVector2D(32,-72),FVector2D(340,340));
    auto WidgetClass=[](const TCHAR* Name,UClass* Fallback)
    { auto* C=LoadClass<UUserWidget>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/UI/WBP_%s.WBP_%s_C"),Name,Name)); return C?C:Fallback; };
    Phone=CreateWidget<UCourierPhoneWidget>(GetOwningPlayer(),WidgetClass(TEXT("CourierPhone"),UCourierPhoneWidget::StaticClass()));
    Map=CreateWidget<UCourierMapWidget>(GetOwningPlayer(),WidgetClass(TEXT("CourierMap"),UCourierMapWidget::StaticClass()));
    Menu=CreateWidget<UCourierMenuWidget>(GetOwningPlayer(),WidgetClass(TEXT("CourierMenu"),UCourierMenuWidget::StaticClass()));
    Place(Phone,FVector2D(.5,.5),FVector2D(.5,.5),FVector2D(0,0),FVector2D(650,1000),10);
    Place(Map,FVector2D(.5,.5),FVector2D(.5,.5),FVector2D(0,0),FVector2D(1160,900),10);
    Place(Menu,FVector2D(.5,.5),FVector2D(.5,.5),FVector2D(0,0),FVector2D(820,850),10);
    ShowMode(ECourierInterfaceMode::Gameplay); RefreshFromState();
}
void UCourierHUDWidget::ShowMode(ECourierInterfaceMode Mode)
{
    if(!Phone || !Map || !Menu) return;
    const auto GameplayVisibility=Mode==ECourierInterfaceMode::Gameplay?ESlateVisibility::Visible:ESlateVisibility::Collapsed;
    ObjectivePanel->SetVisibility(GameplayVisibility); Controls->SetVisibility(GameplayVisibility); Prompt->SetVisibility(GameplayVisibility); SaveWarning->SetVisibility(GameplayVisibility);
    Phone->SetVisibility(Mode==ECourierInterfaceMode::Phone?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Map->SetVisibility(Mode==ECourierInterfaceMode::Map?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const bool ShowMenu=Mode==ECourierInterfaceMode::Pause || Mode==ECourierInterfaceMode::Settlement || Mode==ECourierInterfaceMode::Station;
    Menu->SetVisibility(ShowMenu?ESlateVisibility::Visible:ESlateVisibility::Collapsed); Menu->ResetDialog();
    RefreshFromState();
}
void UCourierHUDWidget::NativeTick(const FGeometry& G,float Delta)
{ Super::NativeTick(G,Delta); RefreshTime+=Delta; if(RefreshTime>=.1) { RefreshTime=0; RefreshFromState(); } }
FReply UCourierHUDWidget::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& Event)
{
    if(auto* PC=Cast<ACourierController>(GetOwningPlayer()))
    {
        if(Event.GetKey()==EKeys::Tab) { PC->TogglePhone(); return FReply::Handled(); }
        if(Event.GetKey()==EKeys::M) { PC->ToggleMap(); return FReply::Handled(); }
        if(Event.GetKey()==EKeys::Escape) { PC->TogglePauseMenu(); return FReply::Handled(); }
    }
    return Super::NativeOnPreviewKeyDown(G,Event);
}
void UCourierHUDWidget::RefreshFromState()
{
    auto* PC=Cast<ACourierController>(GetOwningPlayer()); if(!PC || !Objective) return;
    auto* D=PC->GetDeliverySystem(); auto* P=GetGameInstance()->GetSubsystem<UProfileSubsystem>(); if(!D || !P) return;
    Balance->SetText(FText::FromString(CourierUI::Money(P->GetProfile().BalanceCents)));
    const auto& S=D->GetState(); FString Goal=TEXT("按 Tab 打开手机，接下第一单"); FString Clock=TEXT("从休息站出发，熟悉这座街区。");
    if(S.IsActive() && D->GetCity())
    {
        const auto* L=D->GetCity()->FindLocation(D->GetTargetId());
        Goal=(S.Status==EDeliveryStatus::AwaitingPickup?FString(TEXT("取餐 · ")):FString(TEXT("送达 · ")))+(L?L->DisplayName.ToString():TEXT(""));
        const bool Late=S.ElapsedSeconds>=S.Offer.DeadlineSeconds;
        const int32 Remaining=FMath::CeilToInt(S.Offer.DeadlineSeconds+(Late?60:0)-S.ElapsedSeconds);
        Clock=FString::Printf(TEXT("%s %02d:%02d    餐品完整度 %d%%"),Late?TEXT("宽限期"):TEXT("剩余"),Remaining/60,Remaining%60,S.IntegrityPercent);
        if(const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()) if(!Nav->GetActiveRoute().bReachable) Clock+=TEXT("  请回到道路");
    }
    Objective->SetText(FText::FromString(Goal)); Time->SetText(FText::FromString(Clock));
    AScooterPawn* Bike=nullptr; for(TActorIterator<AScooterPawn> It(GetWorld());It;++It) { Bike=*It; break; }
    Vehicle->SetText(FText::FromString(Bike?FString::Printf(TEXT("%.0f km/h\n电量 %.0f%% %s"),Bike->GetSpeedKmh(),Bike->GetBatteryFraction()*100,Bike->GetBatteryFraction()<.2?TEXT("· 低"):TEXT("")):TEXT("步行")));
    Prompt->SetText(PC->GetCourier()?(PC->GetCourier()->IsMounted()?FText::FromString(TEXT("E 下车（速度不高于 3 km/h）")):PC->GetCourier()->GetInteractionPrompt()):FText::GetEmpty());
    if(!PC->GetActionFeedback().IsEmpty()) Prompt->SetText(PC->GetActionFeedback());
    if(PC->GetRecovery() && PC->GetRecovery()->GetHoldProgress()>0)
        Prompt->SetText(FText::FromString(FString::Printf(TEXT("保持按住 R · 脱困 %.0f%%（订单耗时 +5秒）"),PC->GetRecovery()->GetHoldProgress()*100)));
    SaveWarning->SetText(FText::FromString(P->NeedsReset()?TEXT("存档无法读取，请在暂停菜单选择恢复方式。"):P->HasUnsavedChanges()?TEXT("本次进度未保存 · 按 Esc 重试保存"):P->GetLoadResult()==EProfileLoadResult::RecoveredBackup?TEXT("已从备份恢复进度"):TEXT("")));
    if(Phone && Phone->IsVisible()) Phone->RefreshFromState(); if(Menu && Menu->IsVisible()) Menu->RefreshFromState();
}

