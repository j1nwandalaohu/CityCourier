#pragma once
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "CourierGameMode.generated.h"

UCLASS()
class CITYCOURIER_API ACourierGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACourierGameMode();
    virtual void StartPlay() override;
};
