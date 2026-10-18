#include "World/RiderStation.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Vehicles/ScooterPawn.h"
#include "Profile/ProfileSubsystem.h"
#include "Profile/ProfileRules.h"
ARiderStation::ARiderStation() { PrimaryActorTick.bCanEverTick=true; LocationKind=EDeliveryLocationKind::Station; }
void ARiderStation::Configure(UProfileSubsystem* Profile,AScooterPawn* Vehicle) { PlayerProfile=Profile; Bike=Vehicle; CancelCharge(); }
FCourierActionResult ARiderStation::CheckService(ACourierCharacter* Courier) const
{
    if(!PlayerProfile || PlayerProfile->NeedsReset()) return FCourierActionResult::Failure(TEXT("资料不可用，请先处理存档问题"));
    if(!Bike.IsValid()) return FCourierActionResult::Failure(TEXT("车辆不可用"));
    if(!IsValid(Courier)) return FCourierActionResult::Failure(TEXT("骑手不可用"));
    FText Reason;
    if(!Courier->CanReachInteraction(this,GetInteractionLocation(),Reason,true)) return {false,Reason};
    if(Bike->GetRider()) return FCourierActionResult::Failure(TEXT("请先将车辆停好并下车"));
    if(Bike->GetSpeedKmh()>=.1f) return FCourierActionResult::Failure(TEXT("请先将车辆停稳"));
    if(FVector::Distance(Bike->GetActorLocation(),GetInteractionLocation())>1200) return FCourierActionResult::Failure(TEXT("请将车辆停在驿站入口 12 米内"));
    return FCourierActionResult::Success();
}
bool ARiderStation::Interact(ACourierCharacter* Courier)
{
    FText Reason; if(!CanInteract(Courier,Reason)) return false;
    if(auto* PC=Cast<ACourierController>(Courier->GetController())) { PC->OpenStation(this); return true; } return false;
}
bool ARiderStation::BeginCharge(ACourierCharacter* Courier)
{ return BeginChargeWithFeedback(Courier).bSucceeded; }
FCourierActionResult ARiderStation::BeginChargeWithFeedback(ACourierCharacter* Courier)
{
    const auto Check=CheckService(Courier); if(!Check.bSucceeded) { ServiceMessage=Check.Reason; return Check; }
    if(IsCharging()) return FCourierActionResult::Failure(TEXT("正在充电，请稍候或取消"));
    ServiceMessage=FText::FromString(TEXT("充电需5秒，完成后电量为100%。"));
    ChargeSeconds=0; ChargingRider=Courier; Bike->ClearDriveInput(); return FCourierActionResult::Success();
}
void ARiderStation::CancelCharge() { ChargingRider.Reset(); ChargeSeconds=0; ServiceMessage=FText::GetEmpty(); }
void ARiderStation::Tick(float Delta)
{
    Super::Tick(Delta); if(!IsCharging()) return;
    const auto Check=CheckService(ChargingRider.Get());
    if(!Check.bSucceeded) { CancelCharge(); ServiceMessage=FText::FromString(TEXT("充电已停止：")+Check.Reason.ToString()); return; }
    if(!FMath::IsFinite(Delta) || Delta<=0) return;
    ChargeSeconds+=Delta;
    if(ChargeSeconds>=5) { Bike->SetBatteryFraction(1); CancelCharge(); ServiceMessage=FText::FromString(TEXT("充电完成，电量100%。")); }
}
bool ARiderStation::TryPurchase(EUpgradeKind Kind,ACourierCharacter* Courier)
{ return TryPurchaseWithFeedback(Kind,Courier).bSucceeded; }
FCourierActionResult ARiderStation::TryPurchaseWithFeedback(EUpgradeKind Kind,ACourierCharacter* Courier)
{
    ServiceMessage=FText::GetEmpty();
    auto Check=CheckService(Courier); if(!Check.bSucceeded) return Check;
    if(IsCharging()) return FCourierActionResult::Failure(TEXT("正在充电，请完成或取消后升级"));
    const auto& P=PlayerProfile->GetProfile();
    if(Kind!=EUpgradeKind::Power && Kind!=EUpgradeKind::Handling && Kind!=EUpgradeKind::Battery) return FCourierActionResult::Failure(TEXT("升级项目不可用"));
    const int32 Level=Kind==EUpgradeKind::Power?P.PowerLevel:Kind==EUpgradeKind::Handling?P.HandlingLevel:P.BatteryLevel;
    const int64 Cost=FProfileRules::GetUpgradeCost(Kind,Level);
    if(Cost<0) return FCourierActionResult::Failure(TEXT("该项目已满级"));
    if(P.BalanceCents<Cost) return {false,FText::FromString(FString::Printf(TEXT("余额不足：需要 ¥%.2f，当前 ¥%.2f"),Cost/100.0,P.BalanceCents/100.0))};
    if(!PlayerProfile->TryPurchase(Kind)) return FCourierActionResult::Failure(TEXT("购买失败，请检查资料状态"));
    Bike->ApplyUpgrades(PlayerProfile->GetProfile()); return FCourierActionResult::Success();
}
