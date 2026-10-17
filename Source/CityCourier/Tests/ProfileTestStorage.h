#pragma once
#include "Profile/ProfileRepository.h"

// Faults occur at the storage boundary; failed writes never replace old bytes.
class FTestProfileStorage : public IProfileStorage
{
public:
    TMap<FString, TArray<uint8>> Files;
    TArray<TArray<uint8>> Preserved;
    FString FailRead, FailWrite, FailReplace;
    bool bFailPreserve = false;
    virtual bool Exists(const FString& Name) const override { return Files.Contains(Name); }
    virtual bool Read(const FString& Name, TArray<uint8>& Bytes) override
    {
        if (Name == FailRead || !Files.Contains(Name)) return false;
        Bytes = Files[Name]; return true;
    }
    virtual bool WriteAtomic(const FString& Name, const TArray<uint8>& Bytes) override
    {
        if (Name == FailWrite || Name == FailReplace) return false;
        Files.Add(Name, Bytes); return true;
    }
    virtual bool Preserve(const FString& Name) override
    {
        if (bFailPreserve || !Files.Contains(Name)) return false;
        Preserved.Add(Files[Name]); Files.Remove(Name); return true;
    }
};
