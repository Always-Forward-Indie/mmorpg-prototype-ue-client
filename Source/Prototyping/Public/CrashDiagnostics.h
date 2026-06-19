#pragma once

#include "CoreMinimal.h"

PROTOTYPING_API DECLARE_LOG_CATEGORY_EXTERN(LogCrashDiag, Log, All);

extern PROTOTYPING_API const TCHAR* GActiveCrashGuardName;

struct FCrashGuard
{
	FCrashGuard(const TCHAR* InName)
		: PreviousName(GActiveCrashGuardName)
	{
		GActiveCrashGuardName = InName;
#if !UE_BUILD_SHIPPING
		UE_LOG(LogCrashDiag, Log, TEXT("[CrashGuard] > %s"), InName);
#endif
	}

	~FCrashGuard()
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogCrashDiag, Log, TEXT("[CrashGuard] < %s"),
			GActiveCrashGuardName ? GActiveCrashGuardName : TEXT("<null>"));
#endif
		GActiveCrashGuardName = PreviousName;
	}

private:
	const TCHAR* PreviousName;
};

#define CRASH_GUARD(Name) FCrashGuard UE_JOIN(__crashGuard__, __LINE__)(TEXT(Name))
