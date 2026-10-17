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
    UFUNCTION(BlueprintCallable) bool CourierOrder(int32 Number);
    UFUNCTION(BlueprintPure) class UDeliverySubsystem* GetDeliverySystem() const;
    UFUNCTION(BlueprintPure) class UProfileSubsystem* GetProfileSystem() const;
    ACourierCharacter* GetCourier() const { return Courier.Get(); }
    UFUNCTION(BlueprintCallable) void SetInterfaceMode(ECourierInterfaceMode Mode);
    UFUNCTION(BlueprintCallable) void TogglePhone();
    UFUNCTION(BlueprintCallable) void ToggleMap();
    UFUNCTION(BlueprintCallable) void TogglePauseMenu();
    ECourierInterfaceMode GetInterfaceMode() const { return InterfaceMode; }
private:
    UPROPERTY() TObjectPtr<class UCourierHUDWidget> CourierHUD;
    UPROPERTY() TArray<TObjectPtr<class UInputAction>> InterfaceActions;
    UPROPERTY() TObjectPtr<class UInputMappingContext> Mapping;
    TWeakObjectPtr<ACourierCharacter> Courier;
    ECourierInterfaceMode InterfaceMode = ECourierInterfaceMode::Gameplay;
};
