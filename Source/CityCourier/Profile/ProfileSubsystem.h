#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "Profile/ProfileRepository.h"
#include "ProfileSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnProfileChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnProfileSaveFailed, const FString&);

UCLASS()
class CITYCOURIER_API UProfileSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    EProfileLoadResult LoadProfile();
    bool ResetProfileAfterConfirmation();
    bool ApplySettlement(const FSettlement& Settlement);
    bool TryPurchase(EUpgradeKind Kind);
    UFUNCTION(BlueprintCallable) bool SaveProfile();
    bool UpdateSettings(float Volume, int32 Quality);
    const FPlayerProfile& GetProfile() const { return Profile; }
    const FString& GetLastError() const { return LastError; }
    UFUNCTION(BlueprintPure) bool HasUnsavedChanges() const { return bDirty; }
    bool NeedsReset() const { return bNeedsReset; }
    EProfileLoadResult GetLoadResult() const { return LoadResult; }
    FOnProfileChanged OnProfileChanged;
    FOnProfileSaveFailed OnSaveFailed;
#if WITH_DEV_AUTOMATION_TESTS
    void SetStorageForTests(TSharedRef<IProfileStorage> Storage);
#endif
private:
    FPlayerProfile Profile;
    TUniquePtr<FProfileRepository> Repository;
    TSet<FGuid> SettledInstances;
    FString LastError;
    EProfileLoadResult LoadResult = EProfileLoadResult::NewProfile;
    bool bLoaded = false, bNeedsReset = false, bDirty = false;
    void Changed();
};
