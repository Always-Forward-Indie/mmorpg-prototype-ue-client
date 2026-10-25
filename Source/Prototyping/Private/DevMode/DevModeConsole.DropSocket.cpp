#include "DevMode/DevModeConsoleCommands.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"

// ============================================================================
// devmode.dropsocket chunk
// A2 reconnect hook: closes the chunk socket so the mid-session drop
// detector (worker flag + poll state check) and the bounded reconnect run
// exactly like on a real network drop. Offline DevMode has no live socket,
// so there it is a safe no-op with a log line. Never affects prod: this
// object only exists when DevMode is enabled.
// ============================================================================
void UDevModeConsoleCommands::Cmd_DropSocket(const TArray<FString>& Args) const
{
    if (Args.Num() < 1 || !Args[0].Equals(TEXT("chunk"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogGameplay, Warning, TEXT("Usage: devmode.dropsocket chunk"));
        return;
    }
    if (!GameInstance)
    {
        return;
    }
    UNetworkManager* Net = GameInstance->GetNetworkManager();
    if (!Net)
    {
        UE_LOG(LogGameplay, Warning, TEXT("devmode.dropsocket: no NetworkManager"));
        return;
    }
    Net->DropChunkSocketForTest();
    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_DROPSOCKET chunk requested"));
}
