#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "World/RoadGraph.h"
#include "DeliveryNavigationSubsystem.generated.h"
class UDeliverySubsystem;
UCLASS()
class CITYCOURIER_API UDeliveryNavigationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game || Type==EWorldType::PIE; }
    virtual bool IsTickable() const override { return !IsTemplate() && Delivery!=nullptr; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDeliveryNavigationSubsystem,STATGROUP_Tickables); }
    virtual void Tick(float DeltaSeconds) override;
    void Configure(UDeliverySubsystem* System) { Delivery=System; }
    const FRouteResult& GetActiveRoute() const { return Route; }
    bool RefreshRoute(FVector RiderLocation);
    void UpdateNavigation(FVector RiderLocation,double DeltaSeconds);
    double DistanceFromRoute(FVector Position) const;
    bool IsRouteClear(const TArray<FVector>& Points) const;
private:
    UPROPERTY() TObjectPtr<UDeliverySubsystem> Delivery;
    FRouteResult Route;
    FName Target;
    double CheckSeconds=0;
};
