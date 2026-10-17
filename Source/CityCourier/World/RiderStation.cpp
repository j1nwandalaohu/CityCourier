#include "World/RiderStation.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Vehicles/ScooterPawn.h"
#include "Profile/ProfileSubsystem.h"
ARiderStation::ARiderStation() { PrimaryActorTick.bCanEverTick=true; LocationKind=EDeliveryLocationKind::Station; }
void ARiderStation::Configure(UProfileSubsystem* Profile,AScooterPawn* Vehicle) { PlayerProfile=Profile; Bike=Vehicle; CancelCharge(); }
bool ARiderStation::ValidateService(ACourierCharacter* Courier) const
{
    FText Reason;
    return PlayerProfile && !PlayerProfile->NeedsReset() && Bike.IsValid() && Bike->GetSpeedKmh()<.1f &&
        !Bike->GetRider() && IsValid(Courier) && Courier->CanReachInteraction(this,GetInteractionLocation(),Reason,true);
}
bool ARiderStation::Interact(ACourierCharacter* Courier)
{
    FText Reason; if(!CanInteract(Courier,Reason)) return false;
    if(auto* PC=Cast<ACourierController>(Courier->GetController())) { PC->OpenStation(this); return true; } return false;
}
bool ARiderStation::BeginCharge(ACourierCharacter* Courier)
{
    if(IsCharging() || !ValidateService(Courier)) return false;
    ChargeSeconds=0; ChargingRider=Courier; Bike->ClearDriveInput(); return true;
}
void ARiderStation::CancelCharge() { ChargingRider.Reset(); ChargeSeconds=0; }
void ARiderStation::Tick(float Delta)
{
    Super::Tick(Delta); if(!IsCharging()) return;
    if(!ValidateService(ChargingRider.Get())) { CancelCharge(); return; }
    if(!FMath::IsFinite(Delta) || Delta<=0) return;
    ChargeSeconds+=Delta;
    if(ChargeSeconds>=5) { Bike->SetBatteryFraction(1); CancelCharge(); }
}
bool ARiderStation::TryPurchase(EUpgradeKind Kind,ACourierCharacter* Courier)
{
    if(!ValidateService(Courier) || IsCharging() || !PlayerProfile->TryPurchase(Kind)) return false;
    Bike->ApplyUpgrades(PlayerProfile->GetProfile()); return true;
}
