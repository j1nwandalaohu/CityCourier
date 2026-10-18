#pragma once
#include "GameFramework/SaveGame.h"
#include "Profile/PlayerProfile.h"
#include "ProfileSaveGame.generated.h"

UCLASS()
class CITYCOURIER_API UProfileSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) FPlayerProfile Snapshot;
    static bool Encode(const FPlayerProfile& Profile, TArray<uint8>& Bytes);
    static bool Decode(const TArray<uint8>& Bytes, FPlayerProfile& Profile);
    static bool HasNewerEnvelope(const TArray<uint8>& Bytes);
};
