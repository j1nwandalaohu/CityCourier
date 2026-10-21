#pragma once
#include "CoreMinimal.h"
#include "CitizenBehavior.generated.h"

UENUM(BlueprintType)
enum class ECitizenPersonality : uint8 { Cautious, Ordinary, Aggressive };
UENUM(BlueprintType)
enum class ECitizenBehaviorState : uint8
{ Patrol, AssessThreat, Confront, Chase, Search, Fight, Flee, Cower, Disengage, ReturnToPatrol };
UENUM(BlueprintType)
enum class ECitizenExitReason : uint8
{ None, TargetInvalid, Timeout, LeashExceeded, LostTarget, Unreachable, Safe, Recycled };

USTRUCT(BlueprintType)
struct FCitizenBehaviorConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) ECitizenPersonality Personality=ECitizenPersonality::Ordinary;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Courage=.65f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float FightChance=.85f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bCanChase=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float ChaseSeconds=20;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float LeashCm=3500;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float RetreatPressure=70;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float ApproachSpeed=240;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float ChaseSpeed=380;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float AttackDistance=135;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float LeaveFightDistance=180;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float AttackAngle=35;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float MemorySeconds=3;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float SearchSeconds=5;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float SafeDistance=2000;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float SafeSeconds=4;
    static FCitizenBehaviorConfig ForPersonality(ECitizenPersonality Personality);
};

struct FCitizenPerception
{
    bool bTargetValid=false,bTargetVisible=false,bTargetMounted=false,bBodyAvailable=true;
    bool bAttackReady=false,bPathFailed=false,bAtPatrol=false;
    float TargetDistanceCm=MAX_flt,TargetAngleDeg=180;
    FVector VisibleTargetPosition=FVector::ZeroVector,SelfPosition=FVector::ZeroVector;
};

/** Decision state owns no actors and never queries a hidden target's position. */
struct FCitizenBehavior
{
    FCitizenBehaviorConfig Config;
    ECitizenBehaviorState State=ECitizenBehaviorState::Patrol;
    ECitizenExitReason ExitReason=ECitizenExitReason::None;
    FVector EventOrigin=FVector::ZeroVector,LastSeenPosition=FVector::ZeroVector;
    float Pressure=0,IncidentAge=0,StateAge=0,UnseenAge=0,SafeAge=0;
    bool bHasThreat=false,bDirectThreat=false;
    void Reset(int32 Seed,const FCitizenBehaviorConfig& InConfig);
    void RegisterThreat(FGuid EventId,FVector Origin,bool bHeavy,bool bDirect);
    void Warn();
    void Advance(float Delta,const FCitizenPerception& Perception);
    void Disengage(ECitizenExitReason Reason);
    void SetState(ECitizenBehaviorState Next);
private:
    FRandomStream Random;
    FGuid LastEvent;
    bool bWillFight=false,bWarningOnly=false;
    float AssessSeconds=.5f;
};
