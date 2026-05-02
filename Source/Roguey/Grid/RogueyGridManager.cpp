#include "RogueyGridManager.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/World/RogueyObject.h"

void URogueyGridManager::Init(int32 Width, int32 Height)
{
	// Caller must unregister all actors before reinitialising — Init only resets tile topology.
	ensure(ActorLocations.IsEmpty());
	ActorLocations.Empty();
	TileOccupancy.Empty();

	GridDimensions = FIntVector2(Width, Height);
	Grid.Init(Width, Height);
}

void URogueyGridManager::LoadChunkTiles(FIntPoint ChunkCoord, const FRogueyGrid& ChunkGrid)
{
	const int32 OffX = ChunkCoord.X * ChunkSize;
	const int32 OffY = ChunkCoord.Y * ChunkSize;

	for (int32 lx = 0; lx < ChunkSize; lx++)
	{
		for (int32 ly = 0; ly < ChunkSize; ly++)
		{
			const FIntVector2 WorldTile(OffX + lx, OffY + ly);
			FRogueyTile NewTile;
			if (const FRogueyTile* Src = ChunkGrid.Tiles.Find(FIntVector2(lx, ly)))
				NewTile = *Src;
			Grid.Tiles.Add(WorldTile, NewTile);
		}
	}
}

void URogueyGridManager::UnloadChunkTiles(FIntPoint ChunkCoord)
{
	const int32 OffX = ChunkCoord.X * ChunkSize;
	const int32 OffY = ChunkCoord.Y * ChunkSize;

	for (int32 lx = 0; lx < ChunkSize; lx++)
		for (int32 ly = 0; ly < ChunkSize; ly++)
			Grid.Tiles.Remove(FIntVector2(OffX + lx, OffY + ly));
}

void URogueyGridManager::RogueyTick(int32 TickIndex)
{
	// Movement and pathfinding will be processed here
}

static FIntPoint GetPawnExtent(const AActor* Actor)
{
	if (const ARogueyPawn* Pawn = Cast<ARogueyPawn>(Actor))
		return FIntPoint(FMath::Max(1, Pawn->TileExtent.X), FMath::Max(1, Pawn->TileExtent.Y));
	if (const ARogueyObject* Obj = Cast<ARogueyObject>(Actor))
		return FIntPoint(FMath::Max(1, Obj->TileExtent.X), FMath::Max(1, Obj->TileExtent.Y));
	return FIntPoint(1, 1);
}

void URogueyGridManager::RegisterActor(AActor* Actor, FIntVector2 Coord)
{
	if (!Actor || !Grid.IsInBounds(Coord)) return;
	ActorLocations.Add(Actor, Coord);
	FIntPoint Extent = GetPawnExtent(Actor);
	for (int32 dx = 0; dx < Extent.X; dx++)
		for (int32 dy = 0; dy < Extent.Y; dy++)
			TileOccupancy.Add(FIntVector2(Coord.X + dx, Coord.Y + dy), Actor);
}

void URogueyGridManager::UnregisterActor(AActor* Actor)
{
	if (!Actor) return;
	if (const FIntVector2* OldCoord = ActorLocations.Find(Actor))
	{
		FIntPoint Extent = GetPawnExtent(Actor);
		for (int32 dx = 0; dx < Extent.X; dx++)
			for (int32 dy = 0; dy < Extent.Y; dy++)
				TileOccupancy.Remove(FIntVector2(OldCoord->X + dx, OldCoord->Y + dy));
	}
	ActorLocations.Remove(Actor);
}

void URogueyGridManager::MoveActor(AActor* Actor, FIntVector2 NewCoord)
{
	if (!Actor || !Grid.IsInBounds(NewCoord)) return;
	FIntPoint Extent = GetPawnExtent(Actor);
	if (const FIntVector2* OldCoord = ActorLocations.Find(Actor))
		for (int32 dx = 0; dx < Extent.X; dx++)
			for (int32 dy = 0; dy < Extent.Y; dy++)
				TileOccupancy.Remove(FIntVector2(OldCoord->X + dx, OldCoord->Y + dy));
	ActorLocations.FindOrAdd(Actor) = NewCoord;
	for (int32 dx = 0; dx < Extent.X; dx++)
		for (int32 dy = 0; dy < Extent.Y; dy++)
			TileOccupancy.Add(FIntVector2(NewCoord.X + dx, NewCoord.Y + dy), Actor);
}

FIntVector2 URogueyGridManager::GetActorTile(const AActor* Actor) const
{
	// TObjectPtr key requires an explicit TObjectPtr for correct hash lookup in editor builds.
	if (const FIntVector2* Coord = ActorLocations.Find(TObjectPtr<AActor>(const_cast<AActor*>(Actor))))
		return *Coord;
	return FIntVector2(INT32_MIN, INT32_MIN);
}

AActor* URogueyGridManager::GetActorAtTile(FIntVector2 Coord) const
{
	if (const TObjectPtr<AActor>* Found = TileOccupancy.Find(Coord))
		return Found->Get();
	return nullptr;
}

bool URogueyGridManager::IsActorRegistered(const AActor* Actor) const
{
	return ActorLocations.Contains(TObjectPtr<AActor>(const_cast<AActor*>(Actor)));
}

bool URogueyGridManager::IsWalkable(FIntVector2 Coord) const
{
	return Grid.IsWalkable(Coord);
}

bool URogueyGridManager::IsOccupiedByBlocker(FIntVector2 Coord) const
{
	const TObjectPtr<AActor>* Found = TileOccupancy.Find(Coord);
	if (!Found || !Found->Get()) return false;
	// Pawns respect their bBlocksMovement flag; non-Pawn actors (objects) always block.
	const ARogueyPawn* Pawn = Cast<ARogueyPawn>(Found->Get());
	return !Pawn || Pawn->bBlocksMovement;
}

bool URogueyGridManager::CanMove(FIntVector2 From, FIntVector2 To) const
{
	return Grid.CanMove(From, To) && !IsOccupiedByBlocker(To);
}

bool URogueyGridManager::CanMoveTo(FIntVector2 From, FIntVector2 To, FIntPoint Extent) const
{
	// Origin tile's standard passability + diagonal anti-cutting
	if (!Grid.CanMove(From, To)) return false;

	// Multi-tile actors never move diagonally — sweeping a footprint through a corner is unsound
	if (Extent.X > 1 || Extent.Y > 1)
	{
		if (FMath::Abs(To.X - From.X) != 0 && FMath::Abs(To.Y - From.Y) != 0) return false;
	}

	// All footprint tiles at the destination must be in bounds, walkable, and unblocked
	for (int32 dx = 0; dx < Extent.X; dx++)
		for (int32 dy = 0; dy < Extent.Y; dy++)
		{
			FIntVector2 Tile(To.X + dx, To.Y + dy);
			if (!Grid.IsInBounds(Tile) || !Grid.IsWalkable(Tile)) return false;
			if (IsOccupiedByBlocker(Tile)) return false;
		}
	return true;
}

bool URogueyGridManager::CanActorMoveTo(const AActor* Actor, FIntVector2 NewOrigin) const
{
	if (!Actor) return false;
	FIntVector2 OldOrigin = GetActorTile(Actor);

	FIntPoint Extent(1, 1);
	if (const ARogueyPawn* Pawn = Cast<ARogueyPawn>(Actor))
		Extent = FIntPoint(FMath::Max(1, Pawn->TileExtent.X), FMath::Max(1, Pawn->TileExtent.Y));

	// Multi-tile actors only move cardinally — consistent with pathfinder
	if (Extent.X > 1 || Extent.Y > 1)
	{
		if (FMath::Abs(NewOrigin.X - OldOrigin.X) != 0 && FMath::Abs(NewOrigin.Y - OldOrigin.Y) != 0)
			return false;
	}

	// Origin tile passability + diagonal anti-cutting
	if (!Grid.CanMove(OldOrigin, NewOrigin)) return false;

	// Every tile in the new footprint must be in bounds, walkable, and unblocked by others
	for (int32 dx = 0; dx < Extent.X; dx++)
	{
		for (int32 dy = 0; dy < Extent.Y; dy++)
		{
			FIntVector2 Tile(NewOrigin.X + dx, NewOrigin.Y + dy);
			if (!Grid.IsInBounds(Tile) || !Grid.IsWalkable(Tile)) return false;
			if (const TObjectPtr<AActor>* Found = TileOccupancy.Find(Tile))
				if (Found->Get() != Actor)
					if (const ARogueyPawn* OccPawn = Cast<ARogueyPawn>(Found->Get()))
						if (OccPawn->bBlocksMovement) return false;
		}
	}
	return true;
}

bool URogueyGridManager::IsAdjacent(FIntVector2 A, FIntVector2 B) const
{
	return FMath::Abs(A.X - B.X) <= 1 && FMath::Abs(A.Y - B.Y) <= 1 && A != B;
}

ETileType URogueyGridManager::GetTileType(FIntVector2 Coord) const
{
	if (const FRogueyTile* T = Grid.Tiles.Find(Coord))
		return T->TileType;
	return ETileType::Free;
}

bool URogueyGridManager::HasLineOfSight(FIntVector2 From, FIntVector2 To) const
{
	if (From == To) return true;

	int32 X0 = From.X, Y0 = From.Y;
	const int32 X1  = To.X,  Y1  = To.Y;
	const int32 DX  = FMath::Abs(X1 - X0);
	const int32 DY  = FMath::Abs(Y1 - Y0);
	const int32 SX  = (X0 < X1) ? 1 : -1;
	const int32 SY  = (Y0 < Y1) ? 1 : -1;
	int32 Err = DX - DY;

	// Walk from From toward To via Bresenham's, checking each intermediate tile.
	// The From tile (attacker) and To tile (target) are not tested.
	while (X0 != X1 || Y0 != Y1)
	{
		const int32 E2 = 2 * Err;
		if (E2 > -DY) { Err -= DY; X0 += SX; }
		if (E2 <  DX) { Err += DX; Y0 += SY; }

		if (X0 == X1 && Y0 == Y1) return true; // arrived at target — clear

		const FIntVector2 Cur(X0, Y0);
		if (!Grid.IsWalkable(Cur))                        return false; // Blocked/Water tile
		if (IsOccupiedByBlocker(Cur))                     return false; // solid world object or pawn
		if (GetTileType(Cur) == ETileType::Wall)           return false; // building wall tile
	}

	return true;
}

bool URogueyGridManager::IsInBounds(FIntVector2 Coord) const
{
	return Grid.IsInBounds(Coord);
}

FVector URogueyGridManager::TileToWorld(FIntVector2 Coord) const
{
	return FVector(Coord.X * TileSize + TileSize * 0.5f, Coord.Y * TileSize + TileSize * 0.5f, 0.f);
}

FIntVector2 URogueyGridManager::WorldToTile(FVector WorldPos) const
{
	return FIntVector2(FMath::FloorToInt(WorldPos.X / TileSize), FMath::FloorToInt(WorldPos.Y / TileSize));
}
