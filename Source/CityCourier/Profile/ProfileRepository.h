#pragma once
#include "Profile/PlayerProfile.h"

class CITYCOURIER_API IProfileStorage
{
public:
    virtual ~IProfileStorage() = default;
    virtual bool Read(const FString& Name, TArray<uint8>& Bytes) = 0;
    virtual bool Exists(const FString& Name) const = 0;
    virtual bool WriteAtomic(const FString& Name, const TArray<uint8>& Bytes) = 0;
    virtual bool Preserve(const FString& Name) = 0;
};

class CITYCOURIER_API FFileProfileStorage : public IProfileStorage
{
public:
    explicit FFileProfileStorage(const FString& Directory);
    virtual bool Read(const FString& Name, TArray<uint8>& Bytes) override;
    virtual bool Exists(const FString& Name) const override;
    virtual bool WriteAtomic(const FString& Name, const TArray<uint8>& Bytes) override;
    virtual bool Preserve(const FString& Name) override;
private:
    FString Root;
    FString Resolve(const FString& Name) const;
};

class CITYCOURIER_API FProfileRepository
{
public:
    static const FString PrimaryName;
    static const FString BackupName;
    static const FString ResetMarkerName;
    explicit FProfileRepository(TSharedRef<IProfileStorage> InStorage) : Storage(InStorage) {}
    EProfileLoadResult Load(FPlayerProfile& Profile, FString& OutError,bool* bOutMigrated=nullptr);
    bool Save(const FPlayerProfile& Profile, FString& OutError);
private:
    TSharedRef<IProfileStorage> Storage;
    bool PreserveInvalid(const FString& Name, FString& OutError);
};
