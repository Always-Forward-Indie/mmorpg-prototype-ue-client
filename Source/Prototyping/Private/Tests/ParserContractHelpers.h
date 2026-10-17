#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// Shared scaffolding for the MMO.Protocol contract specs (roadmap P0.1).
// JSON text -> FJsonObject/Array via engine API only — no game logic here,
// so specs stay honest (they pin production JSONParser calls, never mirror
// them). Single source of truth: all contract specs include this header
// instead of redefining the helpers.

inline TSharedPtr<FJsonObject> ParserContract_ParseObj(const FString& Json)
{
    TSharedPtr<FJsonObject> Out;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    FJsonSerializer::Deserialize(Reader, Out);
    return Out;
}

inline TArray<TSharedPtr<FJsonValue>> ParserContract_ParseArrayField(const FString& Json, const FString& FieldName)
{
    TSharedPtr<FJsonObject> Root = ParserContract_ParseObj(Json);
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root.IsValid() && Root->TryGetArrayField(*FieldName, Arr) && Arr)
    {
        return *Arr;
    }
    return {};
}
