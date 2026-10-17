#include "Profile/ProfileSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

namespace { constexpr uint32 ProfileMagic = 0x43505231; constexpr int32 HeaderSize = 12; }

bool UProfileSaveGame::Encode(const FPlayerProfile& Profile, TArray<uint8>& Bytes)
{
    auto* Save = NewObject<UProfileSaveGame>(); Save->Snapshot = Profile;
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save, Payload) || Payload.Num() > 65536) return false;
    const uint32 Header[] = {ProfileMagic, static_cast<uint32>(Payload.Num()), FCrc::MemCrc32(Payload.GetData(), Payload.Num())};
    Bytes.SetNumUninitialized(HeaderSize);
    FMemory::Memcpy(Bytes.GetData(), Header, HeaderSize); Bytes.Append(Payload);
    return true;
}

bool UProfileSaveGame::Decode(const TArray<uint8>& Bytes, FPlayerProfile& Profile)
{
    // Check framing and checksum before passing possibly damaged bytes to UE serialization.
    if (Bytes.Num() <= HeaderSize || Bytes.Num() > 65536 + HeaderSize) return false;
    uint32 Header[3]; FMemory::Memcpy(Header, Bytes.GetData(), HeaderSize);
    if (Header[0] != ProfileMagic || Header[1] != static_cast<uint32>(Bytes.Num() - HeaderSize) ||
        Header[2] != FCrc::MemCrc32(Bytes.GetData() + HeaderSize, Bytes.Num() - HeaderSize)) return false;
    TArray<uint8> Payload; Payload.Append(Bytes.GetData() + HeaderSize, Bytes.Num() - HeaderSize);
    auto* Save = Cast<UProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload));
    if (!Save) return false;
    Profile = Save->Snapshot; return true;
}
