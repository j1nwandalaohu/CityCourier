#include "Profile/ProfileSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace { constexpr uint32 LegacyMagic = 0x43505231; constexpr uint32 ProfileMagic = 0x43505232; constexpr int32 HeaderSize = 12; }

bool UProfileSaveGame::Encode(const FPlayerProfile& Profile, TArray<uint8>& Bytes)
{
    auto* Save = NewObject<UProfileSaveGame>(); Save->Snapshot = Profile;
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save, Payload) || Payload.Num() > 65536) return false;
    const uint32 Header[] = {Profile.Version==1?LegacyMagic:ProfileMagic, static_cast<uint32>(Payload.Num()), FCrc::MemCrc32(Payload.GetData(), Payload.Num())};
    Bytes.SetNumUninitialized(HeaderSize);
    FMemory::Memcpy(Bytes.GetData(), Header, HeaderSize); Bytes.Append(Payload);
    return true;
}

bool UProfileSaveGame::HasNewerEnvelope(const TArray<uint8>& Bytes)
{
    if(Bytes.Num()<static_cast<int32>(sizeof(uint32))) return false;
    uint32 Magic=0; FMemory::Memcpy(&Magic,Bytes.GetData(),sizeof(Magic));
    // Unsupported formats cannot safely be interpreted using today's length/CRC layout.
    return (Magic&0xffffff00u)==(ProfileMagic&0xffffff00u) && Magic>ProfileMagic;
}
bool UProfileSaveGame::Decode(const TArray<uint8>& Bytes, FPlayerProfile& Profile)
{
    // Check framing and checksum before passing possibly damaged bytes to UE serialization.
    if (Bytes.Num() <= HeaderSize || Bytes.Num() > 65536 + HeaderSize) return false;
    uint32 Header[3]; FMemory::Memcpy(Header, Bytes.GetData(), HeaderSize);
    if ((Header[0] != ProfileMagic && Header[0] != LegacyMagic) || Header[1] != static_cast<uint32>(Bytes.Num() - HeaderSize) ||
        Header[2] != FCrc::MemCrc32(Bytes.GetData() + HeaderSize, Bytes.Num() - HeaderSize)) return false;
    TArray<uint8> Payload; Payload.Append(Bytes.GetData() + HeaderSize, Bytes.Num() - HeaderSize);
    auto* Save = Cast<UProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload));
    if (!Save) return false;
    if(Header[0]==LegacyMagic)
    {
        // UE delta serialization omitted v1's default Version field. Restore its original
        // default before reading tagged properties; an explicitly stored version still wins.
        Save->Snapshot=FPlayerProfile{}; Save->Snapshot.Version=1;
        auto Reader=UGameplayStatics::StripSaveGameHeader(Payload);
        FObjectAndNameAsStringProxyArchive Archive(Reader,true); Save->Serialize(Archive);
        if(Archive.IsError()) return false;
    }
    Profile = Save->Snapshot; return true;
}
