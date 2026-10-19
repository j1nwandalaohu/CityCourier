#pragma once
#include "GameFramework/Actor.h"
#include "World/RoadGraph.h"
#include "PackagedValidationDriver.generated.h"

// Explicit opt-in Development executable instrumentation. Never spawned in Shipping.
UCLASS()
class CITYCOURIER_API APackagedValidationDriver:public AActor
{
    GENERATED_BODY()
public:
    APackagedValidationDriver();
    virtual void BeginPlay()override;
    virtual void Tick(float DeltaSeconds)override;
private:
    enum class EStage:uint8 { Warmup,Performance,Drive,Walk,ReturnBike,Charging,Cruise,FinishWait,Reload,Preview };
    void BeginTrip(FName Location);
    bool FollowRoad(float DeltaSeconds);
    bool WalkTo(FVector Target);
    void NextOrder();
    void Finish(const FString& Error=FString());
    void WriteStatus(const FString& Filename,const FString& Error=FString());
    void Capture(const FString& Name);
    void RunInterfaceChecks(double Now);
    void RunFeatureChecks(double Now);
    void RunCharacterActionChecks(double Now);
    void RunInteriorChecks(double Now,float DeltaSeconds);
    void RunInteriorPerformance(double Now);
    bool FollowInteriorJourney();
    bool BeginInteriorJourney(FVector Destination);
    FRouteResult InteriorRoute;
    TArray<FVector> InteriorTargets;
    TSet<FString> InteriorVisitedLayouts;
    int32 InteriorStep=0,InteriorIndex=0,InteriorTargetIndex=0,InteriorRepeats=0;
    double InteriorWait=0;
    bool bInteriorOfferPrepared=false;
    FName InteriorDeliveryTarget;
    FVector InteriorLastPosition=FVector::ZeroVector;
    double InteriorLastMoveTime=0;
    double FeatureWorldTime=0,FeatureWaitStart=0;
    UPROPERTY() TObjectPtr<class ACourierController> PC;
    UPROPERTY() TObjectPtr<class ACourierCharacter> Rider;
    UPROPERTY() TObjectPtr<class AScooterPawn> Bike;
    UPROPERTY() TObjectPtr<class ARiderStation> Station;
    UPROPERTY() TObjectPtr<class UDeliverySubsystem> Delivery;
    UPROPERTY() TObjectPtr<class UProfileSubsystem> Profile;
    UPROPERTY() TObjectPtr<class UCityDefinition> City;
    EStage Stage=EStage::Warmup;
    FName Target;
    TArray<FVector> Points;
    int32 PointIndex=0,OrderIndex=0;
    int32 InterfaceCheck=0;
    int32 InitialCompleted=0,InitialFailed=0;
    double StartTime=0,PreviousTime=0,StageTime=0,LastStatus=0,MinimumSeconds=1200,PerformanceStart=0;
    bool bFinished=false,bPerfDone=false,bFinalizeAtStation=false,bCapturedPerformance=false;
    FString Output,Frames,Events,Mode;
    double DistanceMeters=0;
    FVector LastPosition;
};
