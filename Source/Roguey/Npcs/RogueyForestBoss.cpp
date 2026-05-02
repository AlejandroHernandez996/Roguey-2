#include "RogueyForestBoss.h"

#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Core/RogueyConstants.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/RogueyCharacter.h"
#include "Roguey/World/RogueySpikeTileOverlay.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ARogueyForestBoss::ARogueyForestBoss()
{
	// Safe default — DT_Npcs row will override this in BeginPlay.
	TileExtent = FIntPoint(4, 4);

	// Placeholder body — cube spanning the 4×4 tile footprint (400×400×200 UU).
	// TilePosition is the top-left corner; offset (200,200,0) centres the mesh in the footprint.
	// Replace by assigning a real mesh in a BP_RogueyForestBoss subclass.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetRootComponent());
	BodyMesh->SetRelativeLocation(FVector(200.f, 200.f, 0.f));
	BodyMesh->SetRelativeScale3D(FVector(4.f, 4.f, 2.f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
		BodyMesh->SetStaticMesh(CubeFinder.Object);
}

void ARogueyForestBoss::BeginPlay()
{
	Super::BeginPlay();
	// SpawnTile is set by ARogueyNpc::BeginPlay before we get here.
	BossCenter = SpawnTile;

	if (HasAuthority())
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		SpikeOverlay = GetWorld()->SpawnActor<ARogueySpikeTileOverlay>(
			ARogueySpikeTileOverlay::StaticClass(), FTransform::Identity, Params);
	}
}

void ARogueyForestBoss::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SpikeOverlay))
		SpikeOverlay->Destroy();
	Super::EndPlay(EndPlayReason);
}

// ── Interactable overrides ────────────────────────────────────────────────────

TArray<FRogueyActionDef> ARogueyForestBoss::GetActions() const
{
	if (!bBossActive)
	{
		return {
			{ RogueyActions::Offer,   NSLOCTEXT("Roguey", "ActionOffer",   "Offer")   },
			{ RogueyActions::Examine, NSLOCTEXT("Roguey", "ActionExamine", "Examine") },
		};
	}
	return Super::GetActions();
}

bool ARogueyForestBoss::OnItemOffered(FName ItemId, int32 Quantity, ARogueyPawn* Offerer)
{
	if (bBossActive) return false;
	if (ItemId != FName("oak_logs")) return false;

	OfferCount += Quantity;

	if (Offerer)
	{
		if (OfferCount < OfferGoal)
		{
			Offerer->PostGameMessage(
				FString::Printf(TEXT("You offer an oak log. (%d/%d)"), OfferCount, OfferGoal),
				RogueyChat::Game);
		}
		else
		{
			Offerer->PostGameMessage(
				TEXT("The Ancient Forest Guardian awakens with fury!"),
				RogueyChat::Game);
			bBossActive = true;
			Behavior    = ENpcBehavior::Aggressive;
			AiState     = ENpcAiState::Idle;
		}
	}
	return true;
}

// ── Boss abilities tick ────────────────────────────────────────────────────────

void ARogueyForestBoss::TickBossAbilities(int32 TickIndex)
{
	if (!bBossActive) return;

	AdvanceSpikes(TickIndex);
	DealSpikeDamage(TickIndex);

	if (TickIndex - LastSpikesTick >= SpikeInterval)
	{
		// Target a random living player
		for (TActorIterator<ARogueyCharacter> It(GetWorld()); It; ++It)
		{
			ARogueyCharacter* Player = *It;
			if (!ARogueyPawn::IsAlive(Player)) continue;
			PlaceSpikeCross(Player->GetTileCoord(), TickIndex);
			LastSpikesTick = TickIndex;
			break;
		}
	}

	// Push current spike state to the replicated overlay so all clients see phase colours.
	if (IsValid(SpikeOverlay))
	{
		TArray<FReplicatedSpike> SpikeData;
		SpikeData.Reserve(SpikeMap.Num());
		for (const auto& Pair : SpikeMap)
		{
			FReplicatedSpike& S = SpikeData.AddDefaulted_GetRef();
			S.Tile  = FIntPoint(Pair.Key.X, Pair.Key.Y);
			S.Phase = Pair.Value.Phase;
		}
		SpikeOverlay->UpdateSpikes(SpikeData);
	}
}

// ── Spike helpers ─────────────────────────────────────────────────────────────

void ARogueyForestBoss::PlaceSpikeCross(FIntVector2 Target, int32 TickIndex)
{
	// Center + 4 arms in cardinal directions
	TArray<FIntVector2> Tiles;
	Tiles.Reserve(1 + SpikeArmLen * 4);
	Tiles.Add(Target);
	for (int32 i = 1; i <= SpikeArmLen; i++)
	{
		Tiles.Add(FIntVector2(Target.X + i, Target.Y));
		Tiles.Add(FIntVector2(Target.X - i, Target.Y));
		Tiles.Add(FIntVector2(Target.X, Target.Y + i));
		Tiles.Add(FIntVector2(Target.X, Target.Y - i));
	}

	for (const FIntVector2& T : Tiles)
	{
		FSpikeTileState& State = SpikeMap.FindOrAdd(T);
		State.Phase          = 1;
		State.ActivationTick = TickIndex;
	}
}

void ARogueyForestBoss::AdvanceSpikes(int32 TickIndex)
{
	for (auto It = SpikeMap.CreateIterator(); It; ++It)
	{
		FSpikeTileState& S = It->Value;
		const int32 Elapsed = TickIndex - S.ActivationTick;

		// Phase 1 → 2 at elapsed=1, phase 2 → 3 at elapsed=2 (one tick per phase)
		S.Phase = FMath::Min(3, 1 + Elapsed);

		// Remove after phase 3 has lasted SpikeRedTicks ticks
		if (Elapsed >= 2 + SpikeRedTicks)
			It.RemoveCurrent();
	}
}

void ARogueyForestBoss::DealSpikeDamage(int32 TickIndex)
{
	if (SpikeMap.IsEmpty()) return;

	for (TActorIterator<ARogueyCharacter> It(GetWorld()); It; ++It)
	{
		ARogueyCharacter* Player = *It;
		if (!ARogueyPawn::IsAlive(Player)) continue;

		const FSpikeTileState* Spike = SpikeMap.Find(Player->GetTileCoord());
		if (!Spike || Spike->Phase < 3) continue;

		Player->CurrentHP = FMath::Max(0, Player->CurrentHP - SpikeDamage);
		Player->OnRep_HP();
		if (Player->CurrentHP <= 0)
			Player->SetPawnState(EPawnState::Dead);
		Player->ReceiveHit(SpikeDamage, nullptr);
	}
}
