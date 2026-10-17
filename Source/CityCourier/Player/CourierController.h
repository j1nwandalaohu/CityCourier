#pragma once
#include "GameFramework/PlayerController.h"
#include "CourierController.generated.h"
class ACourierCharacter;
UENUM(BlueprintType)
enum class ECourierInterfaceMode : uint8 { Gameplay, Phone, Map, Pause, Station, Settlement };

UCLASS()
class CITYCOURIER_API ACourierController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* NewPawn) override;
    ACourierCharacter* GetCourier() const { return Courier.Get(); }
    void SetInterfaceMode(ECourierInterfaceMode Mode);
    ECourierInterfaceMode GetInterfaceMode() const { return InterfaceMode; }
private:
    UPROPERTY() TObjectPtr<class UInputMappingContext> Mapping;
    TWeakObjectPtr<ACourierCharacter> Courier;
    ECourierInterfaceMode InterfaceMode = ECourierInterfaceMode::Gameplay;
};
