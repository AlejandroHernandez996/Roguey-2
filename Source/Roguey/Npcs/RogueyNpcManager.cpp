#include "RogueyNpcManager.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "RogueyNpc.h"
#include "Roguey/RogueyCharacter.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Core/RogueyMovementManager.h"
#include "Roguey/Core/RogueyActionManager.h"
#include "Roguey/Core/RogueyConstants.h"

void URogueyNpcManager::Init(URogueyGridManager* InGrid, URogueyMovementManager* InMovement, URogueyActionManager* InAction)
{
	GridManager     = InGrid;
	MovementManager = InMovement;
	ActionManager   = InAction;
}

void URogueyNpcManager::RogueyTick(int32 TickIndex)
{
	for (TActorIterator<ARogueyNpc> It(GetWorld()); It; ++It)
	{
		ARogueyNpc* Npc = *It;
		if (!IsValid(Npc) || Npc->IsDead()) continue;
		TickNpc(Npc, TickIndex);
	}
}

// ---------------------------------------------------------------------------
// Per-NPC dispatcher
// ---------------------------------------------------------------------------

void URogueyNpcManager::TickNpc(ARogueyNpc* Npc, int32 TickIndex)
{
	if (Npc->Behavior == ENpcBehavior::Friendly) return;

	// Always drain LastAttacker — leaving it set causes target-switching and bounce loops.
	if (Npc->LastAttacker.IsValid())
	{
		ARogueyPawn* Attacker = Npc->LastAttacker.Get();
		Npc->LastAttacker.Reset();

		// Only react in Idle: mid-combat hits don't change the target, and a returning
		// NPC should finish walking home before it can be re-engaged.
		if (Npc->AiState == ENpcAiState::Idle && IsValid(Attacker) && !Attacker->IsDead())
		{
			Npc->AggroTarget = Attacker;
			Npc->AiState     = ENpcAiState::Combat;
			if (Npc->Behavior != ENpcBehavior::Passive)
				ActionManager->SetAttackAction(Npc, Attacker);
			// Passive: TickCombat will initiate the flee
		}
	}

	switch (Npc->AiState)
	{
		case ENpcAiState::Idle:      TickIdle(Npc, TickIndex);      break;
		case ENpcAiState::Combat:    TickCombat(Npc, TickIndex);    break;
		case ENpcAiState::Returning: TickReturning(Npc, TickIndex); break;
	}

#if ENABLE_DRAW_DEBUG
	DrawNpcDebug(Npc);
#endif
}

// ---------------------------------------------------------------------------
// State ticks
// ---------------------------------------------------------------------------

void URogueyNpcManager::TickIdle(ARogueyNpc* Npc, int32 TickIndex)
{
	// Aggressive NPCs scan for targets every idle tick
	if (Npc->Behavior == ENpcBehavior::Aggressive)
	{
		if (ARogueyPawn* Target = FindClosestPlayerInRadius(Npc, Npc->AggroRadius))
		{
			Npc->AggroTarget = Target;
			Npc->AiState     = ENpcAiState::Combat;
			ActionManager->SetAttackAction(Npc, Target);
			return;
		}
	}

	// Normal wander — pause between steps to look natural
	if (Npc->WanderCooldown > 0)
	{
		Npc->WanderCooldown--;
		return;
	}

	if (!MovementManager->HasPendingMove(Npc))
	{
		FIntVector2 Dest = PickWanderTile(Npc);
		if (Dest != Npc->GetTileCoord())
			ActionManager->SetMoveAction(Npc, FIntPoint(Dest.X, Dest.Y), false);
		Npc->WanderCooldown = FMath::RandRange(2, 5);
	}
}

void URogueyNpcManager::TickCombat(ARogueyNpc* Npc, int32 TickIndex)
{
	ARogueyPawn* Target = Npc->AggroTarget.Get();

	const bool bTargetGone = !IsValid(Target) || Target->IsDead();
	const bool bLeashed    = ChebyshevDist(Npc->GetTileCoord(), Npc->SpawnTile) > Npc->LeashRadius;

	if (bTargetGone || bLeashed)
	{
		Npc->AggroTarget.Reset();
		ActionManager->ClearAction(Npc);
		Npc->AiState = ENpcAiState::Returning;
		return;
	}

	if (Npc->Behavior == ENpcBehavior::Passive)
	{
		// Flee: pick a new tile each time movement ends so the NPC keeps running
		if (!MovementManager->HasPendingMove(Npc))
		{
			FIntVector2 FleeTile = PickFleeTile(Npc, Target->GetTileCoord());
			ActionManager->SetMoveAction(Npc, FIntPoint(FleeTile.X, FleeTile.Y), true);
		}
		return;
	}

	// Aggressive / Defensive: ActionManager handles re-pathing each tick once set
	if (!ActionManager->HasAction(Npc))
		ActionManager->SetAttackAction(Npc, Target);
}

void URogueyNpcManager::TickReturning(ARogueyNpc* Npc, int32 TickIndex)
{
	if (Npc->GetTileCoord() == Npc->SpawnTile)
	{
		// Home — full heal, then immediately scan for new targets in the same tick
		// so Aggressive NPCs don't do even one idle-walk step with players in range.
		Npc->CurrentHP      = Npc->MaxHP;
		Npc->WanderCooldown = FMath::RandRange(1, 3);
		Npc->AiState        = ENpcAiState::Idle;
		TickIdle(Npc, TickIndex);
		return;
	}

	if (!MovementManager->HasPendingMove(Npc))
		ActionManager->SetMoveAction(Npc, FIntPoint(Npc->SpawnTile.X, Npc->SpawnTile.Y), true);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FIntVector2 URogueyNpcManager::PickWanderTile(const ARogueyNpc* Npc) const
{
	static constexpr int32 WanderRange = 3;

	float Angle  = FMath::RandRange(0.f, 2.f * PI);
	float Radius = FMath::RandRange(0.f, (float)WanderRange);

	FIntVector2 Current = Npc->GetTileCoord();
	FIntVector2 Candidate(
		Current.X + FMath::RoundToInt(FMath::Cos(Angle) * Radius),
		Current.Y + FMath::RoundToInt(FMath::Sin(Angle) * Radius)
	);

	return GridManager->IsInBounds(Candidate) ? Candidate : Current;
}

FIntVector2 URogueyNpcManager::PickFleeTile(const ARogueyNpc* Npc, FIntVector2 ThreatTile) const
{
	FIntVector2 Current = Npc->GetTileCoord();

	// Vector pointing away from the threat
	int32 dx = Current.X - ThreatTile.X;
	int32 dy = Current.Y - ThreatTile.Y;

	// All 8 directions — keep only those with positive dot product against flee vector
	// (i.e. they point "away"). Random selection among valid directions produces
	// natural-looking zig-zag rather than a straight-line sprint.
	const FIntVector2 Dirs[8] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
	const int32 FleeDistance  = FMath::RandRange(4, 6);

	TArray<FIntVector2> Candidates;
	for (const FIntVector2& Dir : Dirs)
	{
		if (Dir.X * dx + Dir.Y * dy <= 0) continue; // points toward or sideways — skip

		FIntVector2 Dest(Current.X + Dir.X * FleeDistance, Current.Y + Dir.Y * FleeDistance);
		if (GridManager->IsInBounds(Dest))
			Candidates.Add(Dest);
	}

	if (Candidates.IsEmpty()) return Npc->SpawnTile;

	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

ARogueyPawn* URogueyNpcManager::FindClosestPlayerInRadius(const ARogueyNpc* Npc, int32 RadiusTiles) const
{
	ARogueyPawn* Closest = nullptr;
	int32 BestDist = INT_MAX;

	for (TActorIterator<ARogueyCharacter> It(GetWorld()); It; ++It)
	{
		ARogueyCharacter* Player = *It;
		if (!IsValid(Player) || Player->IsDead()) continue;

		int32 Dist = ChebyshevDist(Npc->GetTileCoord(), Player->GetTileCoord());
		if (Dist <= RadiusTiles && Dist < BestDist)
		{
			BestDist = Dist;
			Closest  = Player;
		}
	}

	return Closest;
}

int32 URogueyNpcManager::ChebyshevDist(FIntVector2 A, FIntVector2 B)
{
	return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
}

#if ENABLE_DRAW_DEBUG
void URogueyNpcManager::DrawNpcDebug(ARogueyNpc* Npc) const
{
	const UWorld* World = Npc->GetWorld();
	if (!World) return;

	const float Duration  = RogueyConstants::GameTickInterval + 0.1f;
	const float TSize     = RogueyConstants::TileSize;
	const float Half      = TSize * 0.5f;
	const float BoxHalfZ  = 5.f;
	const float Thickness = 3.f;

	// Spawn tile world centre (leash + idle radii are anchored here)
	FVector SpawnPos(
		Npc->SpawnTile.X * TSize + Half,
		Npc->SpawnTile.Y * TSize + Half,
		Npc->GetActorLocation().Z
	);

	// NPC world position projected flat (aggro radius moves with the NPC)
	FVector NpcFlat(Npc->GetActorLocation().X, Npc->GetActorLocation().Y, SpawnPos.Z);

	// +0.5 tile so the box edge falls on the tile boundary, not the tile centre
	auto HalfExt = [&](int32 R) { return FVector((R + 0.5f) * TSize, (R + 0.5f) * TSize, BoxHalfZ); };

	// Leash radius — blue, anchored at spawn
	DrawDebugBox(World, SpawnPos, HalfExt(Npc->LeashRadius), FColor::Blue, false, Duration, 0, Thickness);

	// Aggro radius — red, follows NPC
	DrawDebugBox(World, NpcFlat,  HalfExt(Npc->AggroRadius), FColor::Red,  false, Duration, 0, Thickness);

	// Tile footprint — single outer border at terrain level (actor Z minus hover height)
	{
		const float LineZ = Npc->GetActorLocation().Z - RogueyConstants::PawnHoverHeight + 8.f;
		FIntVector2 Orig = Npc->GetTileCoord();
		FVector BL( Orig.X                        * TSize,  Orig.Y                        * TSize, LineZ);
		FVector BR((Orig.X + Npc->TileExtent.X)   * TSize,  Orig.Y                        * TSize, LineZ);
		FVector TR((Orig.X + Npc->TileExtent.X)   * TSize, (Orig.Y + Npc->TileExtent.Y)  * TSize, LineZ);
		FVector TL( Orig.X                        * TSize, (Orig.Y + Npc->TileExtent.Y)  * TSize, LineZ);
		DrawDebugLine(World, BL, BR, FColor::White, false, Duration, 0, Thickness);
		DrawDebugLine(World, BR, TR, FColor::White, false, Duration, 0, Thickness);
		DrawDebugLine(World, TR, TL, FColor::White, false, Duration, 0, Thickness);
		DrawDebugLine(World, TL, BL, FColor::White, false, Duration, 0, Thickness);
	}

	// ── State text ──────────────────────────────────────────────────────────
	const TCHAR* BehaviorStr = TEXT("?");
	switch (Npc->Behavior)
	{
		case ENpcBehavior::Aggressive: BehaviorStr = TEXT("Aggressive"); break;
		case ENpcBehavior::Defensive:  BehaviorStr = TEXT("Defensive");  break;
		case ENpcBehavior::Passive:    BehaviorStr = TEXT("Passive");    break;
	}

	const TCHAR* StateStr  = TEXT("?");
	FColor        StateColor = FColor::White;
	switch (Npc->AiState)
	{
		case ENpcAiState::Idle:      StateStr = TEXT("Idle");      StateColor = FColor::Green;  break;
		case ENpcAiState::Combat:    StateStr = TEXT("Combat");    StateColor = FColor::Red;    break;
		case ENpcAiState::Returning: StateStr = TEXT("Returning"); StateColor = FColor::Yellow; break;
	}

	const TCHAR* ActionStr = TEXT("None");
	switch (ActionManager->GetActionType(Npc))
	{
		case EActionType::Move:       ActionStr = TEXT("Move");       break;
		case EActionType::Attack:     ActionStr = TEXT("Attack");     break;
		case EActionType::AttackMove: ActionStr = TEXT("AttackMove"); break;
		default: break;
	}

	FString DebugText = FString::Printf(TEXT("%s\n%s\n%s"), BehaviorStr, StateStr, ActionStr);
	DrawDebugString(World, Npc->GetActorLocation() + FVector(0.f, 0.f, 200.f),
	                DebugText, nullptr, StateColor, Duration);
}
#endif
