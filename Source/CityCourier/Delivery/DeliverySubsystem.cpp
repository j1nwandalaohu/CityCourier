#include "Delivery/DeliverySubsystem.h"
#include "Core/DeliveryRules.h"
#include "Profile/ProfileSubsystem.h"
#include "World/CityDefinition.h"
#include "World/DeliveryLocation.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

bool UDeliverySubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type == EWorldType::Game || Type == EWorldType::PIE; }
bool UDeliverySubsystem::Configure(UCityDefinition* Definition,UProfileSubsystem* PlayerProfile,AScooterPawn* Vehicle)
{
    if (bConfigured || !IsValid(Definition) || !IsValid(PlayerProfile) || !IsValid(Vehicle)) return false;
    TArray<FString> Errors; if (!Definition->Validate(Errors)) return false;
    City = Definition; Profile = PlayerProfile; Scooter = Vehicle; bConfigured = true; LastClock = ReadClock();
    Vehicle->ApplyUpgrades(Profile->GetProfile());
    Profile->OnProfileChanged.AddWeakLambda(this,[this]() { if (Scooter.IsValid()) Scooter->ApplyUpgrades(Profile->GetProfile()); });
    Vehicle->OnEffectiveImpact.AddUObject(this,&UDeliverySubsystem::HandleImpact);
    for (TActorIterator<ADeliveryLocation> It(GetWorld()); It; ++It)
    {
        if (It->LocationKind == EDeliveryLocationKind::Station) continue;
        const FName Id = It->LocationId;
        const auto Kind = It->LocationKind;
        It->OnInteraction.BindWeakLambda(this,[this,Id,Kind](ACourierCharacter* Courier)
        { return Kind == EDeliveryLocationKind::Merchant ? TryPickup(Id,Courier) : TryDeliver(Id,Courier); });
    }
    RefreshOffers(CurrentRiderLocation()); return true;
}
double UDeliverySubsystem::ReadClock() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (TestClock) return TestClock();
#endif
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
}
void UDeliverySubsystem::SynchronizeClock()
{
    const double Now = ReadClock();
    if (!FMath::IsFinite(Now)) return;
    FDeliveryRules::Advance(State,FMath::Max(0.0,Now-LastClock)); LastClock = Now;
    if (State.Status == EDeliveryStatus::Failed) FinishOnce();
}
void UDeliverySubsystem::Tick(float DeltaSeconds) { SynchronizeClock(); }
FVector UDeliverySubsystem::CurrentRiderLocation() const
{
    if (GetWorld()) if (const auto* PC = GetWorld()->GetFirstPlayerController())
        if (const APawn* Pawn = PC->GetPawn()) return Pawn->GetActorLocation();
    return LastRiderLocation;
}
bool UDeliverySubsystem::RefreshOffers(FVector RiderLocation)
{
    if (!bConfigured || State.IsActive() || RiderLocation.ContainsNaN()) return false;
    LastRiderLocation = RiderLocation;
    TArray<FDeliveryOffer> Candidates;
    for (const auto& Fee : City->Fees)
    {
        const auto ToShop = City->FindRouteFrom(RiderLocation,Fee.MerchantId);
        const auto* Shop = City->FindLocation(Fee.MerchantId); const auto* Customer = City->FindLocation(Fee.DestinationId);
        if (!Shop || !Customer || !ToShop.bReachable) continue;
        const auto ToCustomer = City->FindRouteBetween(*Shop,*Customer); if (!ToCustomer.bReachable) continue;
        Candidates.Add({FGuid::NewGuid(),Fee.MerchantId,Fee.DestinationId,Fee.FeeCents,
            FDeliveryRules::CalculateDeadline(ToShop.LengthMeters+ToCustomer.LengthMeters)});
    }
    Offers.Reset();
    while (Offers.Num() < 3 && !Candidates.IsEmpty())
    {
        const int32 Index = FMath::RandRange(0,Candidates.Num()-1);
        Offers.Add(Candidates[Index]); Candidates.RemoveAtSwap(Index);
    }
    OnDeliveryChanged.Broadcast(); return Offers.Num() == 3;
}
bool UDeliverySubsystem::TryAccept(FGuid InstanceId)
{
    SynchronizeClock();
    if (!bConfigured || Profile->NeedsReset() || Finished.Contains(InstanceId)) return false;
    const auto* Offer = Offers.FindByPredicate([&](const FDeliveryOffer& O) { return O.InstanceId == InstanceId; });
    if (!Offer || !FDeliveryRules::Accept(State,*Offer)) return false;
    LastClock = ReadClock(); UpdateTargets(); OnDeliveryChanged.Broadcast(); return true;
}
ADeliveryLocation* UDeliverySubsystem::FindLocationActor(FName Id) const
{
    for (TActorIterator<ADeliveryLocation> It(GetWorld()); It; ++It) if (It->LocationId == Id) return *It;
    return nullptr;
}
bool UDeliverySubsystem::ValidateCourier(FName Id,ACourierCharacter* Courier) const
{
    if (!IsValid(Courier) || Courier->GetWorld() != GetWorld() || Courier->IsMounted()) return false;
    auto* Location = FindLocationActor(Id); FText Reason;
    return Location && Courier->CanReachInteraction(Location,Location->GetInteractionLocation(),Reason);
}
bool UDeliverySubsystem::TryPickup(FName Merchant,ACourierCharacter* Courier)
{
    SynchronizeClock();
    if (!ValidateCourier(Merchant,Courier) || !FDeliveryRules::Pickup(State,Merchant)) return false;
    LastRiderLocation = Courier->GetActorLocation(); UpdateTargets(); OnDeliveryChanged.Broadcast(); return true;
}
bool UDeliverySubsystem::TryDeliver(FName Destination,ACourierCharacter* Courier)
{
    SynchronizeClock(); int64 Payout = 0;
    if (!ValidateCourier(Destination,Courier) || !FDeliveryRules::Deliver(State,Destination,Payout)) return false;
    LastRiderLocation = Courier->GetActorLocation(); FinishOnce(Payout); return true;
}
bool UDeliverySubsystem::TryCancel(EDeliveryFailure Reason)
{
    SynchronizeClock(); if (!FDeliveryRules::Cancel(State,Reason)) return false;
    FinishOnce(); return true;
}
void UDeliverySubsystem::ApplyRecoveryPenalty()
{
    SynchronizeClock(); if (!State.IsActive()) return;
    FDeliveryRules::Advance(State,5); if (!State.IsActive()) FinishOnce(); else OnDeliveryChanged.Broadcast();
}
void UDeliverySubsystem::HandleImpact(float ClosingSpeed)
{
    SynchronizeClock();
    if (!Scooter.IsValid() || !Scooter->GetRider() || !Scooter->GetRider()->IsMounted()) return;
    const int32 Before = State.IntegrityPercent; FDeliveryRules::ApplyImpact(State,ClosingSpeed);
    if (State.Status == EDeliveryStatus::Failed) FinishOnce();
    else if (Before != State.IntegrityPercent) OnDeliveryChanged.Broadcast();
}
FName UDeliverySubsystem::GetTargetId() const
{
    return State.Status == EDeliveryStatus::AwaitingPickup ? State.Offer.MerchantId :
        State.Status == EDeliveryStatus::Delivering ? State.Offer.DestinationId : NAME_None;
}
void UDeliverySubsystem::UpdateTargets()
{
    const FName Target = GetTargetId();
    for (TActorIterator<ADeliveryLocation> It(GetWorld()); It; ++It) It->bActiveOrderTarget = !Target.IsNone() && It->LocationId == Target;
}
void UDeliverySubsystem::FinishOnce(int64 PayoutCents)
{
    if ((State.Status != EDeliveryStatus::Completed && State.Status != EDeliveryStatus::Failed) || Finished.Contains(State.Offer.InstanceId)) return;
    Finished.Add(State.Offer.InstanceId);
    LastSettlement = {State.Offer.InstanceId,State.Status == EDeliveryStatus::Completed,PayoutCents};
    Profile->ApplySettlement(LastSettlement); UpdateTargets(); RefreshOffers(CurrentRiderLocation());
    OnDeliveryFinished.Broadcast(LastSettlement);
}
