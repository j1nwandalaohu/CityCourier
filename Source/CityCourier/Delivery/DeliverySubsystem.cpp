#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
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
        RegisterLocation(*It);
    RefreshOffers(CurrentRiderLocation()); return true;
}
void UDeliverySubsystem::RegisterLocation(ADeliveryLocation* Location)
{
    if(!IsValid(Location) || Location->GetWorld()!=GetWorld() || Location->LocationKind==EDeliveryLocationKind::Station) return;
    if(City) Location->CityDefinition=City;
    const FName Id=Location->LocationId; const auto Kind=Location->LocationKind;
    Location->OnInteraction.BindWeakLambda(this,[this,Id,Kind](ACourierCharacter* Courier)
    { return Kind==EDeliveryLocationKind::Merchant?TryPickup(Id,Courier):TryDeliver(Id,Courier); });
    Location->bActiveOrderTarget=Id==GetTargetId();
}
void UDeliverySubsystem::UnregisterLocation(ADeliveryLocation* Location)
{ if(IsValid(Location) && Location->LocationKind!=EDeliveryLocationKind::Station) { Location->OnInteraction.Unbind(); Location->bActiveOrderTarget=false; } }
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
        FDeliveryOffer Offer{FGuid::NewGuid(),Fee.MerchantId,Fee.DestinationId,Fee.FeeCents,
            FDeliveryRules::CalculateDeadline(ToShop.LengthMeters+ToCustomer.LengthMeters)};
        Offer.EstimateOrigin=RiderLocation; Offer.PickupRoute=ToShop; Offer.DeliveryRoute=ToCustomer;
        if(!AreRoutesClear(Offer)) continue;
        Candidates.Add(MoveTemp(Offer));
    }
    Offers.Reset();
    while (Offers.Num() < 3 && !Candidates.IsEmpty())
    {
        const int32 Index = FMath::RandRange(0,Candidates.Num()-1);
        Offers.Add(Candidates[Index]); Candidates.RemoveAtSwap(Index);
    }
    OnDeliveryChanged.Broadcast(); return Offers.Num() == 3;
}
AScooterPawn* UDeliverySubsystem::GetScooter() const { return Scooter.Get(); }
bool UDeliverySubsystem::GetOfferPreview(FGuid InstanceId,FDeliveryOffer& OutOffer) const
{
    OutOffer={}; if(State.IsActive()) return false;
    const auto* Found=Offers.FindByPredicate([&](const FDeliveryOffer& O){return O.InstanceId==InstanceId;});
    if(!Found || !Found->PickupRoute.bReachable || !Found->DeliveryRoute.bReachable) return false;
    OutOffer=*Found; return true;
}
bool UDeliverySubsystem::TryAccept(FGuid InstanceId)
{ return TryAcceptWithFeedback(InstanceId).bSucceeded; }
bool UDeliverySubsystem::AreRoutesClear(const FDeliveryOffer& Offer) const
{
    const auto* Navigation=GetWorld()->GetSubsystem<UDeliveryNavigationSubsystem>();
    return Navigation && Navigation->IsRouteClear(Offer.PickupRoute.Points) && Navigation->IsRouteClear(Offer.DeliveryRoute.Points);
}
FCourierActionResult UDeliverySubsystem::TryAcceptWithFeedback(FGuid InstanceId)
{
    SynchronizeClock();
    if(State.IsActive()) return FCourierActionResult::Failure(TEXT("请先完成当前订单"));
    if (!bConfigured || Profile->NeedsReset() || Finished.Contains(InstanceId)) return FCourierActionResult::Failure(TEXT("订单已失效，请刷新后重试"));
    const auto* Offer = Offers.FindByPredicate([&](const FDeliveryOffer& O) { return O.InstanceId == InstanceId; });
    if(!Offer) return FCourierActionResult::Failure(TEXT("订单已失效，请刷新后重试"));
    if(!AreRoutesClear(*Offer)) return FCourierActionResult::Failure(TEXT("路线暂不可用，请返回道路后刷新订单"));
    if(!FDeliveryRules::Accept(State,*Offer)) return FCourierActionResult::Failure(TEXT("订单已失效，请刷新后重试"));
    LastClock = ReadClock(); UpdateTargets(); OnDeliveryChanged.Broadcast(); return FCourierActionResult::Success();
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
    const auto* Merchant=City->FindLocation(State.Offer.MerchantId); const auto* Destination=City->FindLocation(State.Offer.DestinationId);
    LastSettlement.History=FOrderHistoryRules::MakeEntry(State,PayoutCents,Merchant?Merchant->DisplayName.ToString():State.Offer.MerchantId.ToString(),
        Destination?Destination->DisplayName.ToString():State.Offer.DestinationId.ToString(),FDateTime::UtcNow());
    Profile->ApplySettlement(LastSettlement); UpdateTargets(); RefreshOffers(CurrentRiderLocation());
    OnDeliveryFinished.Broadcast(LastSettlement);
}
