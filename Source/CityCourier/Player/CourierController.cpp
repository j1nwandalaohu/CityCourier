#include "Player/CourierController.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerInput.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "UI/CourierHUDWidget.h"
#include "UI/CourierSettings.h"
#include "Engine/GameInstance.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "World/RiderStation.h"
#include "Vehicles/RiderRecoveryComponent.h"

ACourierController::ACourierController() { Recovery=CreateDefaultSubobject<URiderRecoveryComponent>(TEXT("Recovery")); }
void ACourierController::ShowActionFeedback(const FText& Message)
{ ActionFeedback=Message; FeedbackUntil=GetWorld()->GetTimeSeconds()+3; }
FText ACourierController::GetActionFeedback() const
{ return GetWorld() && GetWorld()->GetTimeSeconds()<FeedbackUntil?ActionFeedback:FText::GetEmpty(); }
void ACourierController::RecoverStarted() { if(InterfaceMode==ECourierInterfaceMode::Gameplay) Recovery->SetRecoverHeld(true); }
void ACourierController::RecoverReleased() { Recovery->SetRecoverHeld(false); }
void ACourierController::OpenStation(ARiderStation* Station) { ActiveStation=Station; SetInterfaceMode(ECourierInterfaceMode::Station); }

void ACourierController::SetupInputComponent()
{
    Super::SetupInputComponent();
    auto* E=Cast<UEnhancedInputComponent>(InputComponent); if(!E) return;
    auto Action=[&](const TCHAR* Name) { auto* A=LoadObject<UInputAction>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Input/IA_%s.IA_%s"),Name,Name)); if(A) InterfaceActions.Add(A); return A; };
    if(auto* A=Action(TEXT("Phone"))) E->BindAction(A,ETriggerEvent::Started,this,&ACourierController::TogglePhone);
    if(auto* A=Action(TEXT("Map"))) E->BindAction(A,ETriggerEvent::Started,this,&ACourierController::ToggleMap);
    if(auto* A=Action(TEXT("Pause"))) E->BindAction(A,ETriggerEvent::Started,this,&ACourierController::TogglePauseMenu);
    if(auto* A=Action(TEXT("Recover")))
    {
        E->BindAction(A,ETriggerEvent::Started,this,&ACourierController::RecoverStarted);
        E->BindAction(A,ETriggerEvent::Completed,this,&ACourierController::RecoverReleased);
        E->BindAction(A,ETriggerEvent::Canceled,this,&ACourierController::RecoverReleased);
    }
}
bool ACourierController::CourierOrder(int32 Number)
{
    auto* D = GetWorld()->GetSubsystem<UDeliverySubsystem>();
    return D && D->GetOffers().IsValidIndex(Number-1) && AcceptOffer(D->GetOffers()[Number-1].InstanceId);
}
bool ACourierController::AcceptOffer(FGuid Id)
{
    auto* D=GetDeliverySystem();
    const auto Result=D?D->TryAcceptWithFeedback(Id):FCourierActionResult::Failure(TEXT("订单已失效，请刷新后重试"));
    if(!Result.bSucceeded) { ShowActionFeedback(Result.Reason); return false; }
    Preview.Reset(); ShowActionFeedback(FText::GetEmpty()); return true;
}
bool ACourierController::PreviewOffer(FGuid Id)
{
    FDeliveryOffer Offer; auto* D=GetDeliverySystem();
    if(!D || !D->GetOfferPreview(Id,Offer))
    { ShowActionFeedback(FText::FromString(TEXT("订单已失效或正在配送，请返回接单页"))); return false; }
    auto* Nav=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>();
    if(!Nav || !Nav->IsRouteClear(Offer.PickupRoute.Points) || !Nav->IsRouteClear(Offer.DeliveryRoute.Points))
    { ShowActionFeedback(FText::FromString(TEXT("路线暂不可用，请返回道路后刷新订单"))); return false; }
    Preview=Offer; SetInterfaceMode(ECourierInterfaceMode::Map); return true;
}
void ACourierController::CloseOfferPreview() { Preview.Reset(); SetInterfaceMode(ECourierInterfaceMode::Phone); }
void ACourierController::OpenOrderHistory() { SetInterfaceMode(ECourierInterfaceMode::Phone); bHistoryOpen=true; }
void ACourierController::CloseOrderHistory() { bHistoryOpen=false; SetInterfaceMode(ECourierInterfaceMode::Phone); }
UDeliverySubsystem* ACourierController::GetDeliverySystem() const
{ return GetWorld()->GetSubsystem<UDeliverySubsystem>(); }
UProfileSubsystem* ACourierController::GetProfileSystem() const
{ return GetGameInstance()?GetGameInstance()->GetSubsystem<UProfileSubsystem>():nullptr; }
void ACourierController::TogglePhone() { SetInterfaceMode(InterfaceMode==ECourierInterfaceMode::Phone?ECourierInterfaceMode::Gameplay:ECourierInterfaceMode::Phone); }
void ACourierController::ToggleMap()
{ if(Preview.IsSet()) { CloseOfferPreview(); return; } SetInterfaceMode(InterfaceMode==ECourierInterfaceMode::Map?ECourierInterfaceMode::Gameplay:ECourierInterfaceMode::Map); }
void ACourierController::TogglePauseMenu() { SetInterfaceMode(InterfaceMode==ECourierInterfaceMode::Pause?ECourierInterfaceMode::Gameplay:ECourierInterfaceMode::Pause); }

void ACourierController::BeginPlay()
{
    Super::BeginPlay();
    Mapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CityCourier/Input/IMC_Courier.IMC_Courier"));
    if (auto* Local = GetLocalPlayer())
        if (auto* Inputs = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            if (Mapping) Inputs->AddMappingContext(Mapping, 0);
    SetControlRotation(FRotator(-12, 0, 0)); SetInterfaceMode(ECourierInterfaceMode::Gameplay);
    if(GetLocalPlayer())
    {
        auto* Class=LoadClass<UCourierHUDWidget>(nullptr,TEXT("/Game/CityCourier/UI/WBP_CourierHUD.WBP_CourierHUD_C"));
        CourierHUD=CreateWidget<UCourierHUDWidget>(this,Class?Class:UCourierHUDWidget::StaticClass()); CourierHUD->AddToViewport();
        auto* Profile=GetGameInstance()->GetSubsystem<UProfileSubsystem>(); UCourierSettings::Apply(Profile->GetProfile());
        GetDeliverySystem()->OnDeliveryFinished.AddWeakLambda(this,[this](const FSettlement&) { SetInterfaceMode(ECourierInterfaceMode::Settlement); });
        if(Profile->NeedsReset()) SetInterfaceMode(ECourierInterfaceMode::Pause);
    }
}
void ACourierController::OnPossess(APawn* NewPawn)
{
    Super::OnPossess(NewPawn);
    if (auto* Rider = Cast<ACourierCharacter>(NewPawn)) Courier = Rider;
}
void ACourierController::SetInterfaceMode(ECourierInterfaceMode Mode)
{
    if(GetGameInstance()) if(const auto* P=GetGameInstance()->GetSubsystem<UProfileSubsystem>())
        if(P->NeedsReset()) Mode=ECourierInterfaceMode::Pause;
    if (PlayerInput) PlayerInput->FlushPressedKeys();
    if(ActiveStation.IsValid() && Mode!=ECourierInterfaceMode::Station) { ActiveStation->CancelCharge(); ActiveStation.Reset(); }
    if(Recovery) Recovery->SetRecoverHeld(false);
    InterfaceMode = Mode;
    if(Mode!=ECourierInterfaceMode::Map) Preview.Reset();
    if(Mode!=ECourierInterfaceMode::Phone) bHistoryOpen=false;
    if (auto* Scooter = Cast<AScooterPawn>(GetPawn())) Scooter->ClearDriveInput();
    if (auto* Local=GetLocalPlayer()) if (auto* Inputs=Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        FModifyContextOptions Options; Options.bIgnoreAllPressedKeysUntilRelease=true;
        Inputs->RequestRebuildControlMappings(Options,EInputMappingRebuildType::RebuildWithFlush);
    }
    if (Courier.IsValid()) Courier->SetInputSuppressed(Mode != ECourierInterfaceMode::Gameplay,Mode==ECourierInterfaceMode::Pause);
    SetPause(Mode == ECourierInterfaceMode::Pause);
    bShowMouseCursor = Mode != ECourierInterfaceMode::Gameplay;
    ResetIgnoreLookInput(); ResetIgnoreMoveInput();
    if (Mode == ECourierInterfaceMode::Gameplay) SetInputMode(FInputModeGameOnly());
    else
    {
        SetIgnoreLookInput(true); SetIgnoreMoveInput(true);
        FInputModeGameAndUI InputMode; InputMode.SetHideCursorDuringCapture(false); if(CourierHUD) InputMode.SetWidgetToFocus(CourierHUD->TakeWidget()); SetInputMode(InputMode);
    }
    if(CourierHUD) CourierHUD->ShowMode(Mode);
}
