#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "World/RoadGraph.h"
#include "Core/CourierActionResult.h"
#include "DeliveryNavigationSubsystem.generated.h"
class UDeliverySubsystem;
enum class ECourierNavigationMode : uint8 { Order, Station, ParkedBike };
UCLASS()
class CITYCOURIER_API UDeliveryNavigationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    FCourierActionResult SetNavigationMode(ECourierNavigationMode NewMode);
    ECourierNavigationMode GetNavigationMode() const { return Mode; }
    FText GetTargetLabel() const;
    double GetRemainingMeters() const { return RemainingMeters; }
    FVector GetTargetPosition() const { return TargetPosition; }
    bool HasArrived() const { return bArrived; }
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game || Type==EWorldType::PIE; }
    virtual bool IsTickable() const override { return !IsTemplate() && Delivery!=nullptr; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDeliveryNavigationSubsystem,STATGROUP_Tickables); }
    virtual void Tick(float DeltaSeconds) override;
    void Configure(UDeliverySubsystem* System);
    const FRouteResult& GetActiveRoute() const { return Route; }
    bool RefreshRoute(FVector RiderLocation);
    void UpdateNavigation(FVector RiderLocation,double DeltaSeconds);
    double DistanceFromRoute(FVector Position) const;
    bool IsRouteClear(const TArray<FVector>& Points) const;
    bool IsRouteClear(const FRouteResult& Candidate) const;
private:
    FName ResolveTarget() const;
    void UpdateRemaining(FVector Position);
    FVector RiderPosition() const;
    ECourierNavigationMode Mode=ECourierNavigationMode::Order;
    FGuid ObservedOrder;
    FVector TargetPosition=FVector::ZeroVector;
    double RemainingMeters=0;
    UPROPERTY() TObjectPtr<UDeliverySubsystem> Delivery;
    FRouteResult Route;
    FName Target;
    double CheckSeconds=0;
    FName OriginRoom;
    bool bArrived=false;
};
