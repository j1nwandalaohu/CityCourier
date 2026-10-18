#include "Profile/ProfileRepository.h"
#include "Profile/ProfileRules.h"
#include "Profile/ProfileSaveGame.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

const FString FProfileRepository::PrimaryName = TEXT("CourierProfile.sav");
const FString FProfileRepository::BackupName = TEXT("CourierProfile.bak");
const FString FProfileRepository::ResetMarkerName = TEXT("CourierProfile.reset-required");

FFileProfileStorage::FFileProfileStorage(const FString& Directory)
    : Root(FPaths::ConvertRelativePathToFull(Directory)) {}

FString FFileProfileStorage::Resolve(const FString& Name) const
{
    if (Name != FProfileRepository::PrimaryName && Name != FProfileRepository::BackupName &&
        Name != FProfileRepository::ResetMarkerName) return {};
    return FPaths::Combine(Root, Name);
}

bool FFileProfileStorage::Exists(const FString& Name) const
{
    const FString Path = Resolve(Name);
    return !Path.IsEmpty() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
}

bool FFileProfileStorage::Read(const FString& Name, TArray<uint8>& Bytes)
{
    const FString Path = Resolve(Name);
    if (Path.IsEmpty()) return false;
    const int64 Size = FPlatformFileManager::Get().GetPlatformFile().FileSize(*Path);
    return Size >= 0 && Size <= 65548 && FFileHelper::LoadFileToArray(Bytes, *Path);
}

bool FFileProfileStorage::WriteAtomic(const FString& Name, const TArray<uint8>& Bytes)
{
    const FString Path = Resolve(Name);
    if (Path.IsEmpty() || Bytes.Num() > 65548) return false;
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    if (!Files.CreateDirectoryTree(*Root)) return false;
    const FString Temp = Path + TEXT(".tmp-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TUniquePtr<IFileHandle> Handle(Files.OpenWrite(*Temp));
    if (!Handle) return false;
    const bool bWritten = Handle->Write(Bytes.GetData(), Bytes.Num()) && Handle->Flush(true);
    Handle.Reset();
    TArray<uint8> Verified;
    bool bSuccess = bWritten && FFileHelper::LoadFileToArray(Verified, *Temp) && Verified == Bytes;
#if PLATFORM_WINDOWS
    // Same-directory Windows rename with replacement leaves the old destination intact on failure.
    if (bSuccess) bSuccess = ::MoveFileExW(*Temp, *Path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    bSuccess = false; // This release supports Windows only; never fall back to delete-then-move.
#endif
    if (!bSuccess) Files.DeleteFile(*Temp);
    return bSuccess;
}

bool FFileProfileStorage::Preserve(const FString& Name)
{
    const FString Path = Resolve(Name);
    if (Path.IsEmpty()) return false;
    const FString Kept = Path + TEXT(".preserved-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S-")) +
        FGuid::NewGuid().ToString(EGuidFormats::Digits);
    return FPlatformFileManager::Get().GetPlatformFile().MoveFile(*Kept, *Path);
}

bool FProfileRepository::PreserveInvalid(const FString& Name, FString& OutError)
{
    // Persist the reset requirement before moving originals, so restarting cannot silently create a new profile.
    if (!Storage->WriteAtomic(ResetMarkerName, TArray<uint8>{1}) || !Storage->Preserve(Name))
    {
        OutError = TEXT("无法保留原存档，请检查磁盘权限；原文件未被覆盖。"); return false;
    }
    return true;
}

EProfileLoadResult FProfileRepository::Load(FPlayerProfile& Profile, FString& OutError,bool* bOutMigrated)
{
    if(bOutMigrated) *bOutMigrated=false;
    OutError.Empty(); bool bProblem = Storage->Exists(ResetMarkerName);
    for (const FString& Name : {PrimaryName, BackupName})
    {
        if (!Storage->Exists(Name)) continue;
        bProblem = true; TArray<uint8> Bytes; FPlayerProfile Candidate;
        if (!Storage->Read(Name, Bytes))
        {
            OutError = TEXT("无法读取存档；请检查磁盘权限后重试。");
            return EProfileLoadResult::NeedsReset;
        }
        const bool bDecoded=UProfileSaveGame::Decode(Bytes,Candidate);
        if(bDecoded && Candidate.Version>2)
        { OutError=TEXT("存档来自更新版本，原文件未改动。请使用兼容版本，或确认后创建新进度。"); return EProfileLoadResult::NeedsReset; }
        bool bMigrated=false;
        if (bDecoded && FProfileRules::UpgradeToCurrent(Candidate,bMigrated))
        {
            if(bOutMigrated) *bOutMigrated=bMigrated;
            Profile = Candidate;
            if (Name == BackupName) OutError = TEXT("已恢复上一个有效备份。");
            return Name == PrimaryName ? EProfileLoadResult::Loaded : EProfileLoadResult::RecoveredBackup;
        }
        if (!PreserveInvalid(Name, OutError)) return EProfileLoadResult::NeedsReset;
    }
    if (bProblem)
    {
        OutError = TEXT("存档损坏或版本不兼容，原文件已保留。确认后可建立新资料。");
        return EProfileLoadResult::NeedsReset;
    }
    Profile = FPlayerProfile{}; return EProfileLoadResult::NewProfile;
}

bool FProfileRepository::Save(const FPlayerProfile& Profile, FString& OutError)
{
    OutError.Empty(); TArray<uint8> Bytes; FPlayerProfile Verified;
    if (!FProfileRules::Validate(Profile) || !UProfileSaveGame::Encode(Profile, Bytes) ||
        !UProfileSaveGame::Decode(Bytes, Verified) || !(Verified == Profile))
    {
        OutError = TEXT("资料校验失败，未保存。"); return false;
    }
    if (Storage->Exists(PrimaryName))
    {
        TArray<uint8> Old; FPlayerProfile Previous;
        if (!Storage->Read(PrimaryName, Old)) { OutError = TEXT("无法读取旧存档，取消覆盖。"); return false; }
        if (UProfileSaveGame::Decode(Old, Previous) && FProfileRules::ValidateSupported(Previous))
        {
            if (!Storage->WriteAtomic(BackupName, Old)) { OutError = TEXT("备份失败，本次进度未保存。"); return false; }
        }
        else if (!PreserveInvalid(PrimaryName, OutError)) return false;
    }
    if (!Storage->WriteAtomic(PrimaryName, Bytes)) { OutError = TEXT("本次进度未保存，请检查剩余空间与写入权限。"); return false; }
    TArray<uint8> Saved;
    if (!Storage->Read(PrimaryName, Saved) || Saved != Bytes) { OutError = TEXT("存档写后校验失败，请重试保存。"); return false; }
    return true;
}
