#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourierEpicArtBuilder.generated.h"

UCLASS()
class CITYCOURIER_API UCourierEpicArtBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Courier|Epic|Editor")
    static bool EnableOnPlayer(class UCourierEpicAnimationConfig* Config,class UBlueprint* PlayerBlueprint,const FString& Fingerprint,FText& OutReason);
    UFUNCTION(BlueprintCallable,Category="Courier|Epic|Editor")
    static TArray<FString> GetNotifyClasses(class UAnimSequenceBase* Animation);
    UFUNCTION(BlueprintCallable,Category="Courier|Epic|Editor")
    static bool BuildPhysicsAsset(class UCourierEpicAnimationConfig* Config,FText& OutReason);
    UFUNCTION(BlueprintCallable,Category="Courier|Epic|Editor")
    static bool BuildRetargetAssets(class UCourierEpicAnimationConfig* Config,FText& OutReason);
    UFUNCTION(BlueprintCallable,Category="Courier|Epic|Editor")
    static bool BuildPoseSearchDatabases(class UCourierEpicAnimationConfig* Config,FText& OutReason);
    UFUNCTION(BlueprintCallable,Category="Courier|Epic|Editor")
    static bool ReuseRetargetedAssets(class UCourierEpicAnimationConfig* Config,const TArray<class UAnimationAsset*>& Assets);
    UFUNCTION(BlueprintCallable,Category="Courier|Epic|Editor")
    static bool BuildAnimationBlueprint(class UCourierEpicAnimationConfig* Config,class UAnimBlueprint* Blueprint,FText& OutReason);
};
