#pragma once
#include "Ambient/AmbientVehicle.h"
#include "Player/CourierStrikeReceiver.h"
#include "Ambient/CitizenBehavior.h"
#include "Ambient/CitizenCombat.h"
#include "Ambient/CitizenNavigation.h"
#include "AmbientPedestrian.generated.h"
UENUM(BlueprintType)
enum class ECitizenReaction : uint8 { Walking, Staggering, Falling, Down, GettingUp };
UCLASS()
class CITYCOURIER_API AAmbientPedestrian : public AAmbientVehicle, public ICourierStrikeReceiver
{
    GENERATED_BODY()
public:
    AAmbientPedestrian();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void SetPatrol(const TArray<FVector>& Points) override;
    virtual void SetPopulationActive(bool bActive) override;
    virtual bool ReceiveCourierStrike(FGuid InstanceId,const FVector& Origin,ECourierAction Action) override;
    virtual bool ReceiveCourierStrikeContext(const FCourierStrikeContext& Context) override;
    void InitializeCitizen(class AAmbientTrafficManager* Manager,int32 Slot);
    void ReceiveWitnessEvent(const struct FCitizenWitnessEvent& Event);
    void ApplyCitizenPath(const FCitizenPathResult& Result);
    void ResetCitizenRuntime();
    AActor* GetThreat() const { return Threat.Get(); }
    const FCitizenBehavior& GetBehavior() const { return Behavior; }
    const FCitizenCombat& GetCombat() const { return Combat; }
    uint32 GetActivationGeneration() const { return ActivationGeneration; }
    bool CanHoldAttack() const;
    bool CanParticipateInFight() const;
    bool IsReadyForAttackToken() const;
    bool IsBodyAvailable() const { return Reaction==ECitizenReaction::Walking; }
    UPROPERTY(EditAnywhere,Category="Citizen|Behavior") bool bOverrideBehaviorConfig=false;
    UPROPERTY(EditAnywhere,Category="Citizen|Behavior") FCitizenBehaviorConfig BehaviorConfig;
    UFUNCTION(BlueprintPure) ECitizenReaction GetReaction() const { return Reaction; }
    UFUNCTION(BlueprintPure) float GetReactionElapsed() const { return ReactionElapsed; }
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> Person;
private:
    friend struct FCitizenNavigation;
    friend class AAmbientTrafficManager;
    void AdvanceBehavior(float Delta);
    FCitizenPerception Sense() const;
    bool CanSeeTarget(const AActor* Target) const;
    bool CanAttackTarget(const AActor* Target) const;
    float CloseFightDistance() const;
    void MoveBehavior(float Delta);
    void RequestPath(FVector Goal);
    FVector SelectFleeGoal();
    FVector PatrolReturnPoint() const;
    void AdvanceCombat(float Delta);
    void TraceCounterpunch(const FCitizenAttackSlice& Slice,uint32 Generation);
    void CancelCombat(bool bLeaveGroup=false);
    void ObserveBump(AActor* Actor);
    void SetReaction(ECitizenReaction Next);
    void SetStandingCollision(bool bStanding);
    bool CanStandUp() const;
    ECitizenReaction Reaction=ECitizenReaction::Walking;
    float ReactionElapsed=0,CombatClock=0,LastLightTime=-100;
    TMap<FGuid,float> RecentStrikes;
    TWeakObjectPtr<AActor> Threat,BumpTarget;
    TWeakObjectPtr<class AAmbientTrafficManager> Crowd;
    FCitizenBehavior Behavior;
    FCitizenCombat Combat;
    FCitizenNavigation Navigation;
    FRandomStream Random;
    uint32 ActivationGeneration=0,PathSerial=0;
    float SenseAge=0,FleeGoalAge=0,NextBumpSample=0,BumpWindow=0;
    int32 BumpCount=0,CrowdSlot=0;
    bool bNextLeft=true,bHitThisAttack=false,bHasFleeGoal=false;
    FVector FleeGoal=FVector::ZeroVector;
    TArray<TPair<FVector,float>> RejectedFleeGoals;
    TArray<TPair<FVector,float>> RejectedReturnPoints;
    int32 PathRequestCount=0;
};
