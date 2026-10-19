#pragma once
#include "Ambient/AmbientVehicle.h"
#include "Player/CourierStrikeReceiver.h"
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
    virtual void SetPopulationActive(bool bActive) override;
    virtual bool ReceiveCourierStrike(FGuid InstanceId,const FVector& Origin,ECourierAction Action) override;
    UFUNCTION(BlueprintPure) ECitizenReaction GetReaction() const { return Reaction; }
    UFUNCTION(BlueprintPure) float GetReactionElapsed() const { return ReactionElapsed; }
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> Person;
private:
    void SetReaction(ECitizenReaction Next);
    void SetStandingCollision(bool bStanding);
    bool CanStandUp() const;
    ECitizenReaction Reaction=ECitizenReaction::Walking;
    float ReactionElapsed=0,CombatClock=0,LastLightTime=-100;
    TMap<FGuid,float> RecentStrikes;
};
