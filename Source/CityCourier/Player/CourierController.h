#pragma once
#include "GameFramework/PlayerController.h"
#include "Core/DeliveryTypes.h"
#include "CourierController.generated.h"
class ACourierCharacter;
UENUM(BlueprintType)
enum class ECourierInterfaceMode : uint8 { Gameplay, Phone, Map, Pause, Station, Settlement };

UCLASS()
class CITYCOURIER_API ACourierController : public APlayerController
{
    GENERATED_BODY()
public:
    ACourierController();
    void ShowActionFeedback(const FText& Message);
    FText GetActionFeedback() const;
    bool PreviewOffer(FGuid InstanceId);
    void CloseOfferPreview();
    const FDeliveryOffer* GetOfferPreview() const { return Preview.IsSet()?&Preview.GetValue():nullptr; }
    bool AcceptOffer(FGuid InstanceId);
    void OpenOrderHistory();
    void CloseOrderHistory();
    bool IsOrderHistoryOpen() const { return bHistoryOpen; }
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
    void OpenStation(class ARiderStation* Station);
    class ARiderStation* GetStation() const { return ActiveStation.Get(); }
    UFUNCTION(BlueprintPure) class URiderRecoveryComponent* GetRecovery() const { return Recovery; }
private:
    FText ActionFeedback;
    double FeedbackUntil=0;
    TOptional<FDeliveryOffer> Preview;
    bool bHistoryOpen=false;
    void RecoverStarted();
    void RecoverReleased();
    UPROPERTY(VisibleAnywhere) TObjectPtr<class URiderRecoveryComponent> Recovery;
    TWeakObjectPtr<class ARiderStation> ActiveStation;
    UPROPERTY() TObjectPtr<class UCourierHUDWidget> CourierHUD;
    UPROPERTY() TArray<TObjectPtr<class UInputAction>> InterfaceActions;
    UPROPERTY() TObjectPtr<class UInputMappingContext> Mapping;
    TWeakObjectPtr<ACourierCharacter> Courier;
    ECourierInterfaceMode InterfaceMode = ECourierInterfaceMode::Gameplay;
};
