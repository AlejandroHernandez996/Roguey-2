#include "RogueyGridManager.h"

void URogueyGridManager::Init(int32 Width, int32 Height)
{
	GridDimensions = FIntVector2(Width, Height);
	Grid.Init(Width, Height);
}

void URogueyGridManager::RogueyTick(int32 TickIndex)
{
	// Movement and pathfinding will be processed here
}

void URogueyGridManager::RegisterActor(AActor* Actor, FIntVector2 Coord)
{
	if (!Actor || !Grid.IsInBounds(Coord)) return;
	ActorLocations.Add(Actor, Coord);
}

void URogueyGridManager::UnregisterActor(AActor* Actor)
{
	if (!Actor) return;
	ActorLocations.Remove(Actor);
}

void URogueyGridManager::MoveActor(AActor* Actor, FIntVector2 NewCoord)
{
	if (!Actor || !Grid.IsInBounds(NewCoord)) return;
	ActorLocations.FindOrAdd(Actor) = NewCoord;
}

FIntVector2 URogueyGridManager::GetActorTile(const AActor* Actor) const
{
	if (const FIntVector2* Coord = ActorLocations.Find(Actor))
	{
		return *Coord;
	}
	return FIntVector2(-1, -1);
}

bool URogueyGridManager::IsActorRegistered(const AActor* Actor) const
{
	return ActorLocations.Contains(Actor);
}

bool URogueyGridManager::CanMove(FIntVector2 From, FIntVector2 To) const
{
	return Grid.CanMove(From, To);
}

bool URogueyGridManager::IsAdjacent(FIntVector2 A, FIntVector2 B) const
{
	return FMath::Abs(A.X - B.X) <= 1 && FMath::Abs(A.Y - B.Y) <= 1 && A != B;
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
