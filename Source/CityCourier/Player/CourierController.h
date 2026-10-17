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
    virtual void SetupInputComponent() override;
    UFUNCTION(Exec,BlueprintCallable) bool CourierOrder(int32 Number);
    UFUNCTION(BlueprintPure) class UDeliverySubsystem* GetDeliverySystem() const;
    ACourierCharacter* GetCourier() const { return Courier.Get(); }
    void SetInterfaceMode(ECourierInterfaceMode Mode);
    ECourierInterfaceMode GetInterfaceMode() const { return InterfaceMode; }
private:
    void DevFirst() { CourierOrder(1); }
    void DevSecond() { CourierOrder(2); }
    void DevThird() { CourierOrder(3); }
    void DevCancel();
    UPROPERTY() TObjectPtr<class UInputMappingContext> Mapping;
    TWeakObjectPtr<ACourierCharacter> Courier;
    ECourierInterfaceMode InterfaceMode = ECourierInterfaceMode::Gameplay;
};
