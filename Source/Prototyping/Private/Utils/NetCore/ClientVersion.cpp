#include "Utils/NetCore/ClientVersion.h"

namespace
{
bool ParseSemVer(const FString& Version, int32& OutMajor, int32& OutMinor, int32& OutPatch)
{
	OutMajor = OutMinor = OutPatch = 0;
	TArray<FString> Parts;
	Version.ParseIntoArray(Parts, TEXT("."));
	if (Parts.Num() != 3)
	{
		return false;
	}
	for (const FString& Part : Parts)
	{
		if (Part.IsEmpty() || !Part.IsNumeric())
		{
			return false;
		}
	}
	OutMajor = FCString::Atoi(*Parts[0]);
	OutMinor = FCString::Atoi(*Parts[1]);
	OutPatch = FCString::Atoi(*Parts[2]);
	return true;
}
}

bool FClientVersion::IsSemVer(const FString& Version)
{
	int32 Major = 0, Minor = 0, Patch = 0;
	return ParseSemVer(Version, Major, Minor, Patch);
}

int32 FClientVersion::Compare(const FString& A, const FString& B, bool& bOutValid)
{
	int32 AMajor = 0, AMinor = 0, APatch = 0;
	int32 BMajor = 0, BMinor = 0, BPatch = 0;
	bOutValid = ParseSemVer(A, AMajor, AMinor, APatch) && ParseSemVer(B, BMajor, BMinor, BPatch);
	if (!bOutValid)
	{
		return 0;
	}
	if (AMajor != BMajor) return AMajor < BMajor ? -1 : 1;
	if (AMinor != BMinor) return AMinor < BMinor ? -1 : 1;
	if (APatch != BPatch) return APatch < BPatch ? -1 : 1;
	return 0;
}

bool FClientVersion::IsCompatible(const FString& Version, const FString& MinVersion, const FString& MaxVersion)
{
	bool bMinValid = false, bMaxValid = false;
	if (Compare(Version, MinVersion, bMinValid) < 0 || !bMinValid)
	{
		return false;
	}
	if (Compare(Version, MaxVersion, bMaxValid) > 0 || !bMaxValid)
	{
		return false;
	}
	return true;
}
