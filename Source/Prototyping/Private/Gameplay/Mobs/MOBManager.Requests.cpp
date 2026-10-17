// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "CrashDiagnostics.h"

void UMOBManager::SendGetMobData(const FClientDataStruct& ClientData)
{
	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Add the MOB ID to the body data
	TSharedPtr<FJsonValueNumber> MOBID = MakeShareable(new FJsonValueNumber(1));
	BodyData.Add("mobId", MOBID);

	FString JsonString = JSONParser::SerializeJson("getMOBData", HeaderData, BodyData);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

// Update mob health based on combat result
void UMOBManager::UpdateMobHealth(const FCombatResultData& ResultData)
{
	if (!worldContext || !worldContext->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("MOBManager: Cannot update mob health - no valid world context"));
		return;
	}

	// Find the mob in the world by ID
	FString MobUidStr = FString::FromInt(ResultData.TargetId);
	
	if (MOBExists(worldContext, FName(MobUidStr)))
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(worldContext, FName(MobUidStr), FoundActors);

		if (FoundActors.Num() > 0)
		{
			ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);
			if (MOB)
			{
				// Update the mob's health
				MOB->SetMOBCurrentHealth(ResultData.RemainingHealth);
				
				// Set the damaged flag
				MOB->SetMobIsDamaged(ResultData.bIsDamaged);
				
				// If the mob died, handle death
				if (ResultData.bTargetDied)
				{
					MOB->SetMOBIsDead(true);
					
					// Trigger visual death animation/effect
					MOB->Die();
				}
				
				// Force update UI to show new health
				MOB->ForceUpdateUI();
				
				// Notify game instance about mob health update
				if (gameInstance)
				{
					gameInstance->UpdateMobHealth(
						ResultData.TargetId,
						ResultData.RemainingHealth,
						ResultData.RemainingMana,
						ResultData.bTargetDied,
						ResultData.bIsDamaged,
						ResultData.DamageDealt
					);
				}
				
				UE_LOG(LogTemp, Warning, TEXT("MOB %s health updated: %d, IsDead: %s"),
					*MOB->GetMobName(), MOB->GetMOBCurrentHealth(), ResultData.bTargetDied ? TEXT("True") : TEXT("False"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Mob with ID %d not found for health update"), ResultData.TargetId);
	}
}

// Spawn a MOB
