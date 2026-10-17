#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourierArtBuilder.generated.h"
UCLASS()
class CITYCOURIER_API UCourierArtBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Courier|Editor") static bool BuildAnimationBlueprint(class UAnimBlueprint* Blueprint);
};
