#pragma once
#include "CoreMinimal.h"
#include "CourierActionTimeline.generated.h"

UENUM(BlueprintType)
enum class ECourierAction : uint8 { None, LightL, LightR, Heavy, HitFront, HitBack };

struct FActionSlice
{
    ECourierAction Action;
    FGuid InstanceId;
    double Start, End;
};

// Gameplay clock. Animation notifications never open the authoritative hit window.
class CITYCOURIER_API FCourierActionTimeline
{
public:
    bool RequestAttack(bool bHeavy);
    TArray<FActionSlice> Advance(double Delta);
    void Cancel();
    void ReceiveHit(bool bFront);
    void ClearQueuedInput() { Queued=ECourierAction::None; }
    ECourierAction GetAction() const { return Action; }
    double GetElapsed() const { return Elapsed; }
    FGuid GetInstanceId() const { return InstanceId; }
    bool IsAttacking() const { return Action==ECourierAction::LightL || Action==ECourierAction::LightR || Action==ECourierAction::Heavy; }
    bool IsReacting() const { return Action==ECourierAction::HitFront || Action==ECourierAction::HitBack; }
private:
    void Start(ECourierAction Next);
    ECourierAction Action=ECourierAction::None, Queued=ECourierAction::None;
    double Elapsed=0;
    FGuid InstanceId;
};
