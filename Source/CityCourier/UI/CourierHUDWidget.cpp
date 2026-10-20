#include "UI/CourierHUDWidget.h"
#include "UI/CourierUI.h"
#include "UI/CourierPhoneWidget.h"
#include "UI/CourierMapWidget.h"
#include "UI/CourierNavigationArrowWidget.h"
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
    ObjectivePanel=CourierUI::Panel(WidgetTree,Goal); Place(ObjectivePanel,FVector2D(0,0),FVector2D(0,0),FVector2D(32,32),FVector2D(740,290));
    Balance=CourierUI::Text(WidgetTree,TEXT(""),30,CourierUI::Accent);
    Place(CourierUI::Panel(WidgetTree,Balance),FVector2D(1,0),FVector2D(1,0),FVector2D(-32,32),FVector2D(300,100));
    Vehicle=CourierUI::Text(WidgetTree,TEXT(""),26); Place(CourierUI::Panel(WidgetTree,Vehicle),FVector2D(1,1),FVector2D(1,1),FVector2D(-32,-72),FVector2D(300,160));
    Prompt=CourierUI::Text(WidgetTree,TEXT(""),26,CourierUI::Accent); Prompt->SetJustification(ETextJustify::Center);
    Place(Prompt,FVector2D(.5,1),FVector2D(.5,1),FVector2D(0,-145),FVector2D(700,45));
    SaveWarning=CourierUI::Text(WidgetTree,TEXT(""),22,CourierUI::Accent); SaveWarning->SetJustification(ETextJustify::Center);
    Place(SaveWarning,FVector2D(.5,0),FVector2D(.5,0),FVector2D(0,330),FVector2D(900,65));
    Controls=CourierUI::Text(WidgetTree,TEXT("WASD 移动/骑行  左Shift 趴下/匍匐  Ctrl 蹲下  右Shift 奔跑  E 交互  空格 跳跃/刹车  鼠标左右键 出拳  长按 R 脱困  Tab 手机  M 地图  Esc 暂停"),16);
    Place(Controls,FVector2D(.5,1),FVector2D(.5,1),FVector2D(0,-22),FVector2D(1740,32));
    MiniMap=CreateWidget<UCourierMapWidget>(GetOwningPlayer(),UCourierMapWidget::StaticClass()); MiniMap->bMini=true;
    MiniMap->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(MiniMap,FVector2D(0,1),FVector2D(0,1),FVector2D(32,-72),FVector2D(340,340));
    NavigationArrow=CreateWidget<UCourierNavigationArrowWidget>(GetOwningPlayer(),UCourierNavigationArrowWidget::StaticClass(),TEXT("NavigationArrow"));
    Place(NavigationArrow,FVector2D(0,0),FVector2D(.5,0),FVector2D(1180,28),FVector2D(260,198),1);
    auto WidgetClass=[](const TCHAR* Name,UClass* Fallback)
    { auto* C=LoadClass<UUserWidget>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/UI/WBP_%s.WBP_%s_C"),Name,Name)); return C?C:Fallback; };
    Phone=CreateWidget<UCourierPhoneWidget>(GetOwningPlayer(),WidgetClass(TEXT("CourierPhone"),UCourierPhoneWidget::StaticClass()));
    Map=CreateWidget<UCourierMapWidget>(GetOwningPlayer(),WidgetClass(TEXT("CourierMap"),UCourierMapWidget::StaticClass()));
    Menu=CreateWidget<UCourierMenuWidget>(GetOwningPlayer(),WidgetClass(TEXT("CourierMenu"),UCourierMenuWidget::StaticClass()));
    Place(Phone,FVector2D(.5,.5),FVector2D(.5,.5),FVector2D(0,0),FVector2D(650,1000),10);
    Place(Map,FVector2D(.5,.5),FVector2D(.5,.5),FVector2D(0,0),FVector2D(1160,900),10);
    UTextBlock* BackText=nullptr;
    MapBackButton=CourierUI::Button(WidgetTree,TEXT("返回街区"),&BackText); MapBackLabel=BackText;
    MapBackButton->OnClicked.AddDynamic(this,&UCourierHUDWidget::ReturnFromMap);
    Place(MapBackButton,FVector2D(.5,.5),FVector2D(1,0),FVector2D(560,-430),FVector2D(200,55),20);
    Place(Menu,FVector2D(.5,.5),FVector2D(.5,.5),FVector2D(0,0),FVector2D(820,850),10);
    ShowMode(ECourierInterfaceMode::Gameplay); RefreshFromState();
}
void UCourierHUDWidget::ShowMode(ECourierInterfaceMode Mode)
{
    if(!Phone || !Map || !Menu) return;
    const auto GameplayVisibility=Mode==ECourierInterfaceMode::Gameplay?ESlateVisibility::Visible:ESlateVisibility::Collapsed;
    ObjectivePanel->SetVisibility(GameplayVisibility); Controls->SetVisibility(GameplayVisibility); Prompt->SetVisibility(GameplayVisibility); SaveWarning->SetVisibility(GameplayVisibility);
    if(NavigationArrow)NavigationArrow->SetVisibility(Mode==ECourierInterfaceMode::Gameplay?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    Phone->SetVisibility(Mode==ECourierInterfaceMode::Phone?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Map->SetVisibility(Mode==ECourierInterfaceMode::Map?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    MapBackButton->SetVisibility(Mode==ECourierInterfaceMode::Map?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const auto* PC=Cast<ACourierController>(GetOwningPlayer());
    MapBackLabel->SetText(FText::FromString(PC && PC->GetOfferPreview()?TEXT("返回接单页"):TEXT("返回街区")));
    const bool ShowMenu=Mode==ECourierInterfaceMode::Pause || Mode==ECourierInterfaceMode::Settlement || Mode==ECourierInterfaceMode::Station;
    Menu->SetVisibility(ShowMenu?ESlateVisibility::Visible:ESlateVisibility::Collapsed); Menu->ResetDialog();
    RefreshFromState();
}
void UCourierHUDWidget::ReturnFromMap()
{ if(auto* PC=Cast<ACourierController>(GetOwningPlayer())) { if(PC->GetOfferPreview()) PC->CloseOfferPreview(); else PC->ToggleMap(); } }
void UCourierHUDWidget::NativeTick(const FGeometry& G,float Delta)
{
    Super::NativeTick(G,Delta);
    if(NavigationArrow)if(auto* ArrowSlot=Cast<UCanvasPanelSlot>(NavigationArrow->Slot))
    {
        const double Width=G.GetLocalSize().X,Gap=Width-1104;
        ArrowSlot->SetPosition(Gap>=170?FVector2D(772+Gap/2,28):FVector2D(Width/2,340));
        ArrowSlot->SetSize(FVector2D(Gap>=170?FMath::Min(260.,Gap-12):240.,198));
    }
    RefreshTime+=Delta;if(RefreshTime>=.1){RefreshTime=0;RefreshFromState();}
}
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
        Goal=(S.Status==EDeliveryStatus::AwaitingPickup?FString(TEXT("取餐 · ")):FString(TEXT("送达 · ")))+D->GetLocationLabel(D->GetTargetId()).ToString();
        const bool Late=S.ElapsedSeconds>=S.Offer.DeadlineSeconds;
        const int32 Remaining=FMath::CeilToInt(S.Offer.DeadlineSeconds+(Late?60:0)-S.ElapsedSeconds);
        Clock=FString::Printf(TEXT("%s %02d:%02d    餐品完整度 %d%%"),Late?TEXT("宽限期"):TEXT("剩余"),Remaining/60,Remaining%60,S.IntegrityPercent);
        if(const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()) if(!Nav->GetActiveRoute().bReachable) Clock+=TEXT("  请回到道路");
    }
    if(const auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>()) if(!Nav->GetTargetLabel().IsEmpty())
    {
        const FString RouteInfo=Nav->GetActiveRoute().bReachable?(Nav->HasArrived()?FString(TEXT("已到达，请按 E 交互")):CourierUI::Distance(Nav->GetRemainingMeters())):FString(TEXT("无法规划路线，请返回道路"));
        Clock+=TEXT("\n导航：")+Nav->GetTargetLabel().ToString()+TEXT(" · ")+RouteInfo;
        if(Nav->GetActiveRoute().bReachable && !Nav->HasArrived() && PC->GetPawn())
        {
            const FVector Delta=Nav->GetTargetPosition()-PC->GetPawn()->GetActorLocation();
            const TCHAR* NS=Delta.Y>100?TEXT("北"):Delta.Y<-100?TEXT("南"):TEXT("");
            const TCHAR* EW=Delta.X>100?TEXT("东"):Delta.X<-100?TEXT("西"):TEXT("");
            Clock+=FString::Printf(TEXT("（目标在%s%s）"),EW,NS);
        }
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

