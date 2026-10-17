#pragma once
#include "Player/CourierCharacter.h"
#include "CourierArtCharacter.generated.h"

/** Visual-only extension: movement, interaction and possession remain in the gameplay base. */
UCLASS()
class CITYCOURIER_API ACourierArtCharacter : public ACourierCharacter
{
    GENERATED_BODY()
public:
    ACourierArtCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
};
