#include "Gameplay/Teleport/TeleportManager.h"
#include "Prototyping.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/TeleportRule.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTeleportManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
	networkManager = InNetworkManager;
	gameInstance = InGameInstance;
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportManager: Initialize called with null parameters"));
	}
}

void UTeleportManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
	if (!World)
	{
		CancelActiveTransition();
	}
}

ABasicPlayer* UTeleportManager::FindPlayer(int32 CharacterId) const
{
	if (!gameInstance || !IsValid(gameInstance))
	{
		return nullptr;
	}
	return gameInstance->GetPlayerByCharacterId(CharacterId);
}

ABasicPlayer* UTeleportManager::FindPlayerByCharacterId(int32 CharacterId) const
{
	return FindPlayer(CharacterId);
}

int32 UTeleportManager::ResolveCharacterId(int32 BodyCharacterId, int32 ClientId) const
{
	if (BodyCharacterId > 0)
	{
		return BodyCharacterId;
	}
	if (ClientId > 0 && gameInstance && IsValid(gameInstance))
	{
		if (const FClientDataStruct* Stored = gameInstance->ConnectedPlayers.Find(ClientId))
		{
			return Stored->characterData.characterId;
		}
	}
	return 0;
}

void UTeleportManager::ApplyServerTeleport(const FTeleportTargetData& Target, bool bIsRespawn)
{
	ABasicPlayer* Player = FindPlayer(Target.CharacterId);
	if (!Player || !IsValid(Player))
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportManager: player not found for charId=%d req=%s"),
			Target.CharacterId, *Target.RequestIdEcho);
		return;
	}

	const FVector Dest(Target.X, Target.Y, Target.Z);
	const float Yaw = FTeleportRule::NormalizeYaw(Target.Yaw);

	if (!FTeleportRule::IsValidTarget(Target.X, Target.Y, Target.Z, Target.Yaw))
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportManager: rejected insane target (%.0f, %.0f, %.0f yaw=%.0f) charId=%d req=%s"),
			Target.X, Target.Y, Target.Z, Target.Yaw, Target.CharacterId, *Target.RequestIdEcho);
		return;
	}

	const bool bIsLocal = !Player->GetIsOtherClient()
		&& gameInstance && Target.CharacterId == gameInstance->GetCurrentCharacterID();

	if (!bIsLocal)
	{
		// Remote view: immediate snap through the shared single source of
		// truth (no fade, no input lock — not our pawn). Revive path is
		// idempotent, so skill-teleport packets on living remotes are safe.
		FString SnapError;
		if (!Player->SnapToAuthoritativePosition(Dest, Yaw, SnapError))
		{
			UE_LOG(LogConnection, Error, TEXT("TeleportManager: remote snap failed charId=%d: %s"), Target.CharacterId, *SnapError);
			return;
		}
		if (bIsRespawn)
		{
			if (Target.bHasVitals)
			{
				Player->ApplyRespawnVitals(Target.HealthCurrent, Target.HealthMax, Target.ManaCurrent, Target.ManaMax);
			}
			Player->SetDead_Implementation(false);
		}
		return;
	}

	// Local pawn: full cover transition. A stacked teleport cancels the
	// previous one (timers cleared, new fade starts) instead of stranding
	// the player under black.
	if (bTransitionActive)
	{
		UE_LOG(LogConnection, Warning, TEXT("TeleportManager: stacked teleport charId=%d req=%s — restarting cover"),
			Target.CharacterId, *Target.RequestIdEcho);
		ClearTimers();
		if (ActivePlayer && IsValid(ActivePlayer) && ActivePlayer != Player)
		{
			ActivePlayer->FinishTeleportReveal();
		}
	}

	bTransitionActive = true;
	ActivePlayer = Player;
	ActiveTarget = Target;
	bActiveIsRespawn = bIsRespawn;
	ActiveReason = bIsRespawn ? TEXT("respawn") : TEXT("teleport");

	BeginCoverTransition(Player, ActiveReason);

	if (!worldContext || !IsValid(worldContext))
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportManager: no world for cover timers charId=%d — revealing immediately"), Target.CharacterId);
		ExecuteCoveredSnap();
		ExecuteReveal();
		return;
	}

	FTimerManager& TM = worldContext->GetTimerManager();
	TM.SetTimer(SnapTimerHandle, this, &UTeleportManager::ExecuteCoveredSnap, FTeleportRule::FadeOutSec, false);
	TM.SetTimer(SafetyTimerHandle, this, &UTeleportManager::ExecuteSafetyUnlock, FTeleportRule::RevealSafetySec, false);
}

void UTeleportManager::ApplyLocalTeleport(ABasicPlayer* Player, const FVector& Dest, float Yaw, const FString& Reason)
{
	if (!Player || !IsValid(Player))
	{
		UE_LOG(LogGameplay, Error, TEXT("TeleportManager: ApplyLocalTeleport null player"));
		return;
	}
	if (!FTeleportRule::IsValidTarget(Dest.X, Dest.Y, Dest.Z, Yaw))
	{
		UE_LOG(LogGameplay, Error, TEXT("TeleportManager: rejected insane local target (%.0f, %.0f, %.0f)"), Dest.X, Dest.Y, Dest.Z);
		return;
	}

	FTeleportTargetData Target;
	Target.CharacterId = Player->GetPlayerCharacterID();
	Target.X = Dest.X;
	Target.Y = Dest.Y;
	Target.Z = Dest.Z;
	Target.Yaw = FTeleportRule::NormalizeYaw(Yaw);

	if (Player->GetIsOtherClient())
	{
		FString SnapError;
		Player->SnapToAuthoritativePosition(Dest, Target.Yaw, SnapError);
		return;
	}

	if (bTransitionActive)
	{
		ClearTimers();
		if (ActivePlayer && IsValid(ActivePlayer) && ActivePlayer != Player)
		{
			ActivePlayer->FinishTeleportReveal();
		}
	}

	bTransitionActive = true;
	ActivePlayer = Player;
	ActiveTarget = Target;
	bActiveIsRespawn = false;
	ActiveReason = Reason;

	BeginCoverTransition(Player, Reason);

	if (!worldContext || !IsValid(worldContext))
	{
		ExecuteCoveredSnap();
		ExecuteReveal();
		return;
	}

	FTimerManager& TM = worldContext->GetTimerManager();
	TM.SetTimer(SnapTimerHandle, this, &UTeleportManager::ExecuteCoveredSnap, FTeleportRule::FadeOutSec, false);
	TM.SetTimer(SafetyTimerHandle, this, &UTeleportManager::ExecuteSafetyUnlock, FTeleportRule::RevealSafetySec, false);
}

void UTeleportManager::BeginCoverTransition(ABasicPlayer* Player, const FString& Reason)
{
	Player->BeginTeleportCover(Reason);
}

void UTeleportManager::ExecuteCoveredSnap()
{
	if (!bTransitionActive || !ActivePlayer || !IsValid(ActivePlayer))
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportManager: snap fired without an active player"));
		bTransitionActive = false;
		return;
	}

	const FVector Dest(ActiveTarget.X, ActiveTarget.Y, ActiveTarget.Z);
	const float DistCm = FVector::Dist2D(ActivePlayer->GetActorLocation(), Dest);

	FString SnapError;
	if (!ActivePlayer->SnapToAuthoritativePosition(Dest, FTeleportRule::NormalizeYaw(ActiveTarget.Yaw), SnapError))
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportManager: covered snap failed charId=%d req=%s: %s"),
			ActiveTarget.CharacterId, *ActiveTarget.RequestIdEcho, *SnapError);
	}

	if (bActiveIsRespawn)
	{
		// Vitals BEFORE revive unlocks movement, so the pawn never runs on
		// corpse values. Missing vitals (old servers, skill teleports) fall
		// back to the bJustRevived stats_update snap.
		if (ActiveTarget.bHasVitals)
		{
			ActivePlayer->ApplyRespawnVitals(ActiveTarget.HealthCurrent, ActiveTarget.HealthMax, ActiveTarget.ManaCurrent, ActiveTarget.ManaMax);
		}
		else
		{
			UE_LOG(LogConnection, Verbose, TEXT("TeleportManager: no vitals in respawn charId=%d — waiting for stats_update"), ActiveTarget.CharacterId);
		}
		ActivePlayer->SetDead_Implementation(false);
	}

	UE_LOG(LogConnection, Log, TEXT("TeleportManager: charId=%d %s to (%.0f, %.0f, %.0f) req=%s"),
		ActiveTarget.CharacterId, *ActiveReason, Dest.X, Dest.Y, Dest.Z, *ActiveTarget.RequestIdEcho);

	if (worldContext && IsValid(worldContext))
	{
		worldContext->GetTimerManager().SetTimer(RevealTimerHandle, this,
			&UTeleportManager::ExecuteReveal, FTeleportRule::HoldForDistance(DistCm), false);
	}
	else
	{
		ExecuteReveal();
	}
}

void UTeleportManager::ExecuteReveal()
{
	if (!bTransitionActive)
	{
		return;
	}
	bTransitionActive = false;
	LastRevealTimeSec = FPlatformTime::Seconds();

	UE_LOG(LogConnection, Log, TEXT("TeleportManager: reveal charId=%d (%s)"),
		ActiveTarget.CharacterId, *ActiveReason);

	if (ActivePlayer && IsValid(ActivePlayer))
	{
		ActivePlayer->FinishTeleportReveal();
		SendTeleportAck(ActiveTarget.CharacterId,
			ActivePlayer->GetActorLocation(),
			ActivePlayer->GetActorRotation().Yaw,
			ActiveTarget.RequestIdEcho, ActiveReason);
	}

	ClearTimers();
	ActivePlayer = nullptr;
}

void UTeleportManager::ExecuteSafetyUnlock()
{
	if (!bTransitionActive)
	{
		return;
	}
	UE_LOG(LogConnection, Error, TEXT("TeleportManager: reveal safety timeout charId=%d req=%s — force-unlocking"),
		ActiveTarget.CharacterId, *ActiveTarget.RequestIdEcho);
	bTransitionActive = false;
	if (ActivePlayer && IsValid(ActivePlayer))
	{
		ActivePlayer->FinishTeleportReveal();
	}
	ClearTimers();
	ActivePlayer = nullptr;
}

void UTeleportManager::CancelActiveTransition()
{
	ClearTimers();
	bTransitionActive = false;
	ActivePlayer = nullptr;
}

bool UTeleportManager::ShouldApplyCorrection(int32 CharacterId, const FVector& CorrPos) const
{
	ABasicPlayer* Player = FindPlayer(CharacterId);
	if (!Player || !IsValid(Player) || Player->GetIsOtherClient())
	{
		return true;
	}

	// A correction landing mid-cover races the snap at fade peak — it is
	// stale history by construction. The server drops such moves now; this
	// covers residual races without touching genuine drift.
	if (bTransitionActive)
	{
		UE_LOG(LogConnection, Warning, TEXT("TeleportManager: ignoring correction during cover charId=%d"), CharacterId);
		return false;
	}

	// Post-reveal tail: a far correction right after the reveal is the same
	// stale history (pre-teleport spot). Small nudges still apply.
	if (FPlatformTime::Seconds() - LastRevealTimeSec < FTeleportRule::CorrectionTailSec
		&& FVector::Dist2D(Player->GetActorLocation(), CorrPos) >= FTeleportRule::CoverThresholdCm)
	{
		UE_LOG(LogConnection, Warning, TEXT("TeleportManager: ignoring stale post-reveal correction charId=%d"), CharacterId);
		return false;
	}

	return true;
}

void UTeleportManager::ClearTimers()
{
	UWorld* World = (worldContext && IsValid(worldContext)) ? worldContext : nullptr;
	if (!World && ActivePlayer && IsValid(ActivePlayer))
	{
		World = ActivePlayer->GetWorld();
	}
	if (World)
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(SnapTimerHandle);
		TM.ClearTimer(RevealTimerHandle);
		TM.ClearTimer(SafetyTimerHandle);
	}
}

void UTeleportManager::SendTeleportAck(int32 CharacterId, const FVector& AppliedPos, float AppliedYaw, const FString& RequestIdEcho, const FString& Reason)
{
	if (!networkManager || !gameInstance)
	{
		return;
	}

	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID())));
	HeaderData.Add(TEXT("hash"), MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientData().hash)));

	BodyData.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(CharacterId)));
	BodyData.Add(TEXT("requestIdEcho"), MakeShareable(new FJsonValueString(RequestIdEcho)));
	BodyData.Add(TEXT("x"), MakeShareable(new FJsonValueNumber(AppliedPos.X)));
	BodyData.Add(TEXT("y"), MakeShareable(new FJsonValueNumber(AppliedPos.Y)));
	BodyData.Add(TEXT("z"), MakeShareable(new FJsonValueNumber(AppliedPos.Z)));
	BodyData.Add(TEXT("rotZ"), MakeShareable(new FJsonValueNumber(static_cast<double>(AppliedYaw))));
	BodyData.Add(TEXT("reason"), MakeShareable(new FJsonValueString(Reason)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("teleportAck"),
		HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	networkManager->SendDataToChunkServer(JsonString);
	UE_LOG(LogConnection, Log, TEXT("TeleportManager: teleportAck sent charId=%d req=%s reason=%s"),
		CharacterId, *RequestIdEcho, *Reason);
}
