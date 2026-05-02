#include "RogueyForestDirector.h"

#include "RogueyChunkManager.h"
#include "RogueyPortal.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Engine/DataTable.h"

void URogueyForestDirector::Init(ARogueyGameMode* InGameMode)
{
	GameMode = InGameMode;
}

void URogueyForestDirector::BeginRun()
{
	Credits        = 0.f;
	bBossSpawned   = false;
	bBossDefeated  = false;
	bPortalSpawned = false;
	BossDeathTick  = -1;
	BossChunkCoord = FIntPoint(0, 0);
	SpawnedNpcs.Reset();
	BossNpc        = nullptr;

	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	CachedPoolTable = Settings ? Settings->DirectorPoolTable.LoadSynchronous() : nullptr;
	if (!CachedPoolTable)
		UE_LOG(LogTemp, Warning, TEXT("ForestDirector: DirectorPoolTable not assigned in Project Settings → Roguey → Data Tables"));
}

void URogueyForestDirector::EndRun()
{
	SpawnedNpcs.Reset();
	BossNpc         = nullptr;
	CachedPoolTable = nullptr;
}

// ── Main tick ─────────────────────────────────────────────────────────────────

void URogueyForestDirector::RogueyTick(int32 TickIndex)
{
	if (!IsValid(GameMode) || !IsValid(GameMode->ChunkManager)) return;
	if (!GameMode->ChunkManager->IsForestRunActive()) return;

	const bool bWasBossAlive = bBossSpawned && !bBossDefeated;
	PruneDead();

	if (bWasBossAlive && bBossDefeated)
		BossDeathTick = TickIndex;

	const int32 ThreatTier = URogueyChunkManager::ThreatToTier(GameMode->ChunkManager->GetForestThreatTick());

	AccrueCredits(ThreatTier);
	TrySpendCredits(ThreatTier);

	if (!bBossSpawned)
		TrySpawnBoss();

	if (bBossDefeated && !bPortalSpawned && BossDeathTick >= 0 && TickIndex - BossDeathTick >= BossPortalDelay)
		SpawnEscapePortal();
}

// ── Credits ───────────────────────────────────────────────────────────────────

void URogueyForestDirector::AccrueCredits(int32 ThreatTier)
{
	// Tier 0: +0.2/tick → rat (cost 5) every ~25 ticks (15s), cap in ~150 ticks
	// Tier 4: +0.6/tick → wolf (cost 6) every ~10 ticks (6s), cap in ~50 ticks
	Credits = FMath::Min(CreditCap, Credits + 0.2f + ThreatTier * 0.1f);
}

bool URogueyForestDirector::TrySpendCredits(int32 ThreatTier)
{
	// Tier 0: 1  Tier 1: 2  Tier 2: 3  Tier 3: 4  Tier 4: 5
	const int32 LiveCap = 1 + ThreatTier;
	if (CountLiveDirectorNpcs() >= LiveCap) return false;
	if (!CachedPoolTable) return false;

	struct FCandidate { FName NpcTypeId; float Weight; int32 Cost; };
	TArray<FCandidate> Pool;
	float TotalWeight = 0.f;

	CachedPoolTable->ForeachRow<FRogueyDirectorPoolRow>(TEXT("TrySpendCredits"),
		[&](const FName&, const FRogueyDirectorPoolRow& Row)
		{
			if (ThreatTier < Row.MinThreatTier || ThreatTier > Row.MaxThreatTier) return;
			if (Row.DirectorCost <= 0) return;
			if (Credits < static_cast<float>(Row.DirectorCost)) return;
			Pool.Add({ Row.NpcTypeId, Row.Weight, Row.DirectorCost });
			TotalWeight += Row.Weight;
		});

	if (Pool.IsEmpty()) return false;

	float Roll = FMath::FRand() * TotalWeight;
	const FCandidate* Picked = nullptr;
	for (const FCandidate& C : Pool)
	{
		Roll -= C.Weight;
		if (Roll <= 0.f) { Picked = &C; break; }
	}
	if (!Picked) Picked = &Pool.Last();

	FIntVector2 SpawnTile;
	if (!FindSpawnTile(SpawnTile)) return false;

	Credits -= static_cast<float>(Picked->Cost);
	SpawnDirectorNpc(Picked->NpcTypeId, SpawnTile);
	return true;
}

// ── Spawn helpers ─────────────────────────────────────────────────────────────

bool URogueyForestDirector::FindSpawnTile(FIntVector2& OutTile) const
{
	if (!IsValid(GameMode) || !IsValid(GameMode->GridManager)) return false;

	TArray<FIntVector2> PlayerTiles;
	for (FConstPlayerControllerIterator It = GameMode->GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC) continue;
		const ARogueyPawn* Pawn = Cast<ARogueyPawn>(PC->GetPawn());
		if (!Pawn || Pawn->IsDead()) continue;
		PlayerTiles.Add(FIntVector2(Pawn->TilePosition.X, Pawn->TilePosition.Y));
	}
	if (PlayerTiles.IsEmpty()) return false;

	const FIntVector2 PlayerTile = PlayerTiles[FMath::RandRange(0, PlayerTiles.Num() - 1)];

	for (int32 Attempt = 0; Attempt < 30; Attempt++)
	{
		const float Angle = FMath::FRand() * 2.f * PI;
		const int32 Dist  = FMath::RandRange(SpawnMinDist, SpawnMaxDist);
		const FIntVector2 Candidate(
			PlayerTile.X + FMath::RoundToInt(FMath::Cos(Angle) * Dist),
			PlayerTile.Y + FMath::RoundToInt(FMath::Sin(Angle) * Dist));
		if (GameMode->GridManager->IsWalkable(Candidate)
			&& !GameMode->GridManager->IsOccupiedByBlocker(Candidate))
		{
			OutTile = Candidate;
			return true;
		}
	}
	return false;
}

void URogueyForestDirector::SpawnDirectorNpc(FName NpcTypeId, FIntVector2 SpawnTile)
{
	if (!IsValid(GameMode)) return;
	ARogueyNpc* Npc = GameMode->SpawnNpc(SpawnTile, NpcTypeId);
	if (!IsValid(Npc)) return;
	SpawnedNpcs.Add(Npc);
	// Register with chunk manager so UnloadChunk destroys this NPC when its chunk streams out.
	// Without this, Director NPCs outlive their chunk and permanently fill the live-NPC cap.
	if (IsValid(GameMode->ChunkManager))
		GameMode->ChunkManager->RegisterChunkActor(SpawnTile, Npc);
}

void URogueyForestDirector::PruneDead()
{
	SpawnedNpcs.RemoveAll([](const TObjectPtr<ARogueyNpc>& N)
	{
		return !IsValid(N) || N->IsDead();
	});

	if (bBossSpawned && !bBossDefeated && (!IsValid(BossNpc) || BossNpc->IsDead()))
		bBossDefeated = true;
}

int32 URogueyForestDirector::CountLiveDirectorNpcs() const
{
	int32 Count = 0;
	for (const TObjectPtr<ARogueyNpc>& N : SpawnedNpcs)
		if (IsValid(N) && !N->IsDead()) Count++;
	return Count;
}

// ── Boss lifecycle ────────────────────────────────────────────────────────────

void URogueyForestDirector::TrySpawnBoss()
{
	if (!IsValid(GameMode) || !IsValid(GameMode->ChunkManager) || !IsValid(GameMode->GridManager)) return;

	const TMap<FIntPoint, EForestBiomeType>& LoadedBiomes = GameMode->ChunkManager->GetLoadedChunkBiomes();
	for (const TTuple<FIntPoint, EForestBiomeType>& Pair : LoadedBiomes)
	{
		if (Pair.Value != EForestBiomeType::BossArena) continue;

		BossChunkCoord = Pair.Key;
		const int32 CS = URogueyGridManager::ChunkSize;
		const FIntVector2 Center(BossChunkCoord.X * CS + CS / 2, BossChunkCoord.Y * CS + CS / 2);

		// Spiral out from center to find a walkable tile
		bool bFoundSpawn = false;
		FIntVector2 SpawnTile(0, 0);
		for (int32 R = 0; R <= 5 && !bFoundSpawn; R++)
		{
			for (int32 DX = -R; DX <= R && !bFoundSpawn; DX++)
			{
				for (int32 DY = -R; DY <= R && !bFoundSpawn; DY++)
				{
					if (R > 0 && FMath::Abs(DX) != R && FMath::Abs(DY) != R) continue;
					const FIntVector2 C(Center.X + DX, Center.Y + DY);
					if (GameMode->GridManager->IsWalkable(C) && !GameMode->GridManager->IsOccupiedByBlocker(C))
					{
						SpawnTile = C;
						bFoundSpawn = true;
					}
				}
			}
		}
		if (!bFoundSpawn) return;

		ARogueyNpc* Boss = GameMode->SpawnNpc(SpawnTile, TEXT("forest_boss"));
		if (!IsValid(Boss))
		{
			UE_LOG(LogTemp, Warning, TEXT("ForestDirector: SpawnNpc('forest_boss') failed at tile (%d,%d) — check DT_Npcs import and NpcClass in GameMode BP"), SpawnTile.X, SpawnTile.Y);
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("ForestDirector: boss spawned at chunk (%d,%d), tile (%d,%d)"), BossChunkCoord.X, BossChunkCoord.Y, SpawnTile.X, SpawnTile.Y);
		BossNpc      = Boss;
		bBossSpawned = true;
		return;
	}
}

void URogueyForestDirector::SpawnEscapePortal()
{
	if (!IsValid(GameMode) || !IsValid(GameMode->GridManager)) return;

	const int32 CS = URogueyGridManager::ChunkSize;
	const FIntVector2 Center(BossChunkCoord.X * CS + CS / 2, BossChunkCoord.Y * CS + CS / 2);

	bool bFoundPortal = false;
	FIntVector2 PortalTile(0, 0);
	for (int32 R = 0; R <= 5 && !bFoundPortal; R++)
	{
		for (int32 DX = -R; DX <= R && !bFoundPortal; DX++)
		{
			for (int32 DY = -R; DY <= R && !bFoundPortal; DY++)
			{
				if (R > 0 && FMath::Abs(DX) != R && FMath::Abs(DY) != R) continue;
				const FIntVector2 C(Center.X + DX, Center.Y + DY);
				if (GameMode->GridManager->IsWalkable(C) && !GameMode->GridManager->IsOccupiedByBlocker(C))
				{
					PortalTile = C;
					bFoundPortal = true;
				}
			}
		}
	}
	if (!bFoundPortal) return;

	FVector WorldPos = GameMode->GridManager->TileToWorld(PortalTile);

	ARogueyPortal* Portal = GameMode->GetWorld()->SpawnActorDeferred<ARogueyPortal>(
		ARogueyPortal::StaticClass(), FTransform(WorldPos), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Portal)) return;
	Portal->PortalName  = NSLOCTEXT("Roguey", "EscapePortalName", "Escape Portal");
	Portal->ExamineDesc = TEXT("A portal back to safety. The forest boss has been slain.");
	Portal->NextAreaId  = NAME_None; // empty NextAreaId → TriggerVictory
	Portal->FinishSpawning(FTransform(WorldPos));

	bPortalSpawned = true;
}
