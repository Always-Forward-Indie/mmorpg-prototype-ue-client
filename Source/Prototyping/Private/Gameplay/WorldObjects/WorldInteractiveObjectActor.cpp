#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

AWorldInteractiveObjectActor::AWorldInteractiveObjectActor()
{
	// Tick is enabled only while a flash is active (toggled in StartFlash / Tick)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Mesh (assign in BP subclass or via DataTable)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRoot);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	// Interaction sphere — radius set from server data
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(200.f);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->SetHiddenInGame(true);
}

void AWorldInteractiveObjectActor::BeginPlay()
{
	Super::BeginPlay();

	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AWorldInteractiveObjectActor::OnInteractionSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AWorldInteractiveObjectActor::OnInteractionSphereEndOverlap);

	// Build MIDs from all mesh slots so we can set material parameters at runtime
	// (Opacity, EmissiveColor, TintColor, etc.) without touching the source assets.
	const int32 NumMats = MeshComponent->GetNumMaterials();
	CachedMIDs.SetNum(NumMats);
	for (int32 i = 0; i < NumMats; ++i)
	{
		UMaterialInterface* Mat = MeshComponent->GetMaterial(i);
		if (Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
			MeshComponent->SetMaterial(i, MID);
			CachedMIDs[i] = MID;
		}
	}
}

void AWorldInteractiveObjectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsFlashing) return;

	FlashElapsed += DeltaSeconds;
	const float T = FMath::Clamp(FlashElapsed / FMath::Max(FlashDuration, 0.01f), 0.f, 1.f);

	// Lerp EmissiveColor from FlashColour back to black as the flash fades
	const FLinearColor CurrentEmissive = FMath::Lerp(FlashColour, FLinearColor::Black, T);
	for (UMaterialInstanceDynamic* MID : CachedMIDs)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), CurrentEmissive);
		}
	}

	if (T >= 1.f)
	{
		bIsFlashing = false;
		SetActorTickEnabled(false);
	}
}

void AWorldInteractiveObjectActor::InitializeFromServerData(const FWorldObjectData& InData)
{
	ObjectData   = InData;
	CurrentState = InData.CurrentState;

	InteractionSphere->SetSphereRadius(InData.InteractionRadius);

	// Apply initial visual state
	ApplyStateVisuals(InData.CurrentState);

	UE_LOG(LogTemp, Log, TEXT("WIO Actor [%d] '%s' initialized — type=%d scope=%d state=%d radius=%.0f"),
		InData.ObjectId, *InData.Slug,
		static_cast<int32>(InData.ObjectType),
		static_cast<int32>(InData.Scope),
		static_cast<int32>(InData.CurrentState),
		InData.InteractionRadius);
}

// ─────────────────────────────────────────────────────────────────────────────
// State management
// ─────────────────────────────────────────────────────────────────────────────

void AWorldInteractiveObjectActor::SetObjectState(EWIOState NewState, int32 InRespawnSec)
{
	const EWIOState OldState = CurrentState;
	CurrentState = NewState;

	if (NewState == EWIOState::Depleted && InRespawnSec > 0)
	{
		StartRespawnTimer(InRespawnSec);
	}

	if (NewState == EWIOState::Active)
	{
		GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
		RespawnEndTime  = 0.0;
		RespawnTotalSec = 0;
	}

	// C++ handles all base visuals automatically
	ApplyStateVisuals(NewState);

	// Optional BP hook (for VFX/SFX additions — not required)
	if (OldState != NewState)
	{
		BP_OnStateChanged(NewState, OldState);
	}
}

float AWorldInteractiveObjectActor::GetRemainingRespawnTime() const
{
	if (RespawnEndTime <= 0.0) return 0.f;
	const double Remaining = RespawnEndTime - FPlatformTime::Seconds();
	return FMath::Max(0.f, static_cast<float>(Remaining));
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual helpers
// ─────────────────────────────────────────────────────────────────────────────

void AWorldInteractiveObjectActor::ApplyStateVisuals(EWIOState State)
{
	if (!MeshComponent) return;

	switch (State)
	{
	case EWIOState::Active:
	{
		MeshComponent->SetVisibility(true, true);
		// Restore full opacity and clear any depleted tint
		for (UMaterialInstanceDynamic* MID : CachedMIDs)
		{
			if (MID)
			{
				MID->SetScalarParameterValue(TEXT("Opacity"),     1.f);
				MID->SetVectorParameterValue(TEXT("TintColor"),   FLinearColor::White);
				MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
			}
		}
		break;
	}

	case EWIOState::Depleted:
	{
		MeshComponent->SetVisibility(true, true);
		// Reduce opacity and apply a grey tint to signal depleted state.
		// Materials need "Opacity" (scalar) and/or "TintColor" (vector) parameters.
		// If your materials don't expose these, set DepletedMaterialOverride instead.
		for (UMaterialInstanceDynamic* MID : CachedMIDs)
		{
			if (MID)
			{
				MID->SetScalarParameterValue(TEXT("Opacity"), DepletedOpacity);
				MID->SetVectorParameterValue(TEXT("TintColor"),
					FLinearColor(0.5f, 0.5f, 0.5f, DepletedOpacity));
			}
		}
		break;
	}

	case EWIOState::Disabled:
	{
		if (bHideWhenDisabled)
		{
			MeshComponent->SetVisibility(false, true);
		}
		break;
	}
	}
}

void AWorldInteractiveObjectActor::StartFlash(FLinearColor Colour)
{
	if (!MeshComponent || CachedMIDs.IsEmpty()) return;

	FlashColour  = Colour;
	FlashElapsed = 0.f;
	bIsFlashing  = true;

	// Apply full flash colour immediately; Tick will fade it back to black
	for (UMaterialInstanceDynamic* MID : CachedMIDs)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Colour);
		}
	}

	SetActorTickEnabled(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Interaction result callback
// ─────────────────────────────────────────────────────────────────────────────

void AWorldInteractiveObjectActor::OnInteractResultReceived(const FWIOInteractResult& Result)
{
	if (Result.bSuccess)
	{
		// For per-player objects, mark locally done and grey out the mesh
		if (ObjectData.Scope == EWIOScope::PerPlayer && ObjectData.ObjectType != EWIOObjectType::Examine)
		{
			bLocalPlayerDone = true;
			SetObjectState(EWIOState::Depleted);
		}

		// C++ auto-flash success tint, then notify BP for optional extras
		StartFlash(SuccessTintColour);
		BP_OnInteractionSuccess(Result);
	}
	else
	{
		// C++ auto-flash fail tint, then notify BP
		StartFlash(FailTintColour);
		BP_OnInteractionFailed(Result.ErrorCode);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Overlap detection
// ─────────────────────────────────────────────────────────────────────────────

void AWorldInteractiveObjectActor::OnInteractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ABasicPlayer* Player = Cast<ABasicPlayer>(OtherActor);
	if (!Player || !Player->IsLocallyControlled()) return;

	OnProximityChanged.Broadcast(this, true);
}

void AWorldInteractiveObjectActor::OnInteractionSphereEndOverlap(
	UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ABasicPlayer* Player = Cast<ABasicPlayer>(OtherActor);
	if (!Player || !Player->IsLocallyControlled()) return;

	OnProximityChanged.Broadcast(this, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Respawn timer
// ─────────────────────────────────────────────────────────────────────────────

void AWorldInteractiveObjectActor::StartRespawnTimer(int32 Seconds)
{
	RespawnTotalSec = Seconds;
	RespawnEndTime  = FPlatformTime::Seconds() + static_cast<double>(Seconds);

	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this,
		&AWorldInteractiveObjectActor::OnRespawnTimerComplete,
		static_cast<float>(Seconds), false);
}

void AWorldInteractiveObjectActor::OnRespawnTimerComplete()
{
	// Server will send the official Active state update shortly.
	// Fire the BP event so designers can prepare respawn VFX in advance.
	RespawnEndTime  = 0.0;
	RespawnTotalSec = 0;
	BP_OnRespawned();
}
