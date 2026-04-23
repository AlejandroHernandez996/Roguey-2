#include "RogueyActionManager.h"
#include "RogueyActionNames.h"

#include "RogueyInteractable.h"
#include "RogueyMovementManager.h"
#include "RogueyPawn.h"
#include "RogueyPawnState.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Grid/RogueyPathfinder.h"
#include "Roguey/Combat/RogueyCombatManager.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Items/RogueyItemRow.h"
#include "Roguey/Items/RogueyItemType.h"
#include "Roguey/Items/RogueyLootDrop.h"
#include "Roguey/Skills/RogueyStat.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "EngineUtils.h"

void URogueyActionManager::Init(URogueyGridManager* InGrid, URogueyMovementManager* InMovement, URogueyCombatManager* InCombat)
{
	GridManager   = InGrid;
	MovementManager = InMovement;
	CombatManager = InCombat;
}

void URogueyActionManager::RogueyTick(int32 TickIndex)
{
	// ── Consume queue (eat/drink) — runs before movement/combat each tick ────
	for (auto& [Pawn, Slots] : PendingConsumeSlots)
	{
		if (IsValid(Pawn) && !Pawn->IsDead())
			ProcessConsumeQueue(Pawn, Slots);
	}
	PendingConsumeSlots.Empty();

	TickStatBuffs();

	// ── Main action loop ──────────────────────────────────────────────────────
	TArray<ARogueyPawn*> ToRemove;

	for (auto& [Pawn, Action] : PendingActions)
	{
		if (!IsValid(Pawn) || Pawn->IsDead())
		{
			ToRemove.Add(Pawn);
			continue;
		}

		switch (Action.Type)
		{
			case EActionType::Move:       TickMove(Pawn, Action, TickIndex);       break;
			case EActionType::AttackMove: TickAttackMove(Pawn, Action, TickIndex); break;
			case EActionType::Attack:     TickAttack(Pawn, Action, TickIndex);     break;
			case EActionType::TakeLoot:   TickTakeLoot(Pawn, Action, TickIndex);   break;
			default: break;
		}

		if (!Action.IsActive())
			ToRemove.Add(Pawn);
	}

	for (ARogueyPawn* Pawn : ToRemove)
		PendingActions.Remove(Pawn);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void URogueyActionManager::SetMoveAction(ARogueyPawn* Pawn, FIntPoint TargetTile, bool bRunning)
{
	if (!IsValid(Pawn) || !GridManager) return;

	FIntVector2 Target(TargetTile.X, TargetTile.Y);
	if (!GridManager->IsInBounds(Target)) return;

	FIntVector2 Start = Pawn->GetTileCoord();
	if (Start == Target) return;

	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Start, Target, Pawn->TileExtent);
	if (!Path.IsValid()) return;

	ClearAction(Pawn);

	MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);

	FRogueyPendingAction Action;
	Action.Type       = EActionType::Move;
	Action.TargetTile = TargetTile;
	PendingActions.Add(Pawn, Action);
}

void URogueyActionManager::SetActorAction(ARogueyPawn* Pawn, AActor* Target, FName ActionId)
{
	if (!IsValid(Pawn) || !IsValid(Target)) return;

	IRogueyInteractable* Interactable = Cast<IRogueyInteractable>(Target);
	if (!Interactable) return;

	// Validate the action is actually offered by this target
	TArray<FRogueyActionDef> Actions = Interactable->GetActions();
	if (!Actions.ContainsByPredicate([&](const FRogueyActionDef& A){ return A.ActionId == ActionId; })) return;

	if (ActionId == RogueyActions::Attack)
	{
		if (ARogueyPawn* TargetPawn = Cast<ARogueyPawn>(Target))
			SetAttackAction(Pawn, TargetPawn);
	}
	else if (ActionId == RogueyActions::Take)
	{
		if (ARogueyLootDrop* Drop = Cast<ARogueyLootDrop>(Target))
			SetTakeLootAction(Pawn, Drop);
	}
	else if (ActionId == RogueyActions::Examine)
	{
		FString Text = Interactable->GetExamineText();
		Pawn->ShowSpeechBubble(Text);
	}
}

void URogueyActionManager::SetAttackAction(ARogueyPawn* Pawn, ARogueyPawn* Target)
{
	if (!IsValid(Pawn) || !IsValid(Target) || Target->IsDead()) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::AttackMove;
	Action.TargetActor = Target;
	PendingActions.Add(Pawn, Action);

	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, Pawn->AttackRange, Pawn->bAttackCardinalOnly))
		RequestMoveTowardTarget(Pawn, Target);
}

bool URogueyActionManager::HasAction(ARogueyPawn* Pawn) const
{
	return PendingActions.Contains(Pawn);
}

EActionType URogueyActionManager::GetActionType(ARogueyPawn* Pawn) const
{
	if (const FRogueyPendingAction* Action = PendingActions.Find(Pawn))
		return Action->Type;
	return EActionType::None;
}

ARogueyPawn* URogueyActionManager::GetAttackTarget(const ARogueyPawn* Pawn) const
{
	if (!Pawn) return nullptr;
	if (const FRogueyPendingAction* Action = PendingActions.Find(const_cast<ARogueyPawn*>(Pawn)))
		if (Action->Type == EActionType::Attack || Action->Type == EActionType::AttackMove)
			return Cast<ARogueyPawn>(Action->TargetActor.Get());
	return nullptr;
}

void URogueyActionManager::ClearAction(ARogueyPawn* Pawn)
{
	if (FRogueyPendingAction* Existing = PendingActions.Find(Pawn))
	{
		if (Existing->Type == EActionType::Move || Existing->Type == EActionType::AttackMove)
			MovementManager->CancelMove(Pawn);
	}
	PendingActions.Remove(Pawn);
}

// ---------------------------------------------------------------------------
// Tick helpers
// ---------------------------------------------------------------------------

void URogueyActionManager::TickMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	// MovementManager drives the actual movement; we just watch for completion
	if (!MovementManager->HasPendingMove(Pawn))
		Action.Clear();
}

void URogueyActionManager::TickAttackMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Cast<ARogueyPawn>(Action.TargetActor.Get());
	if (!IsValid(Target) || Target->IsDead())
	{
		MovementManager->CancelMove(Pawn);
		Pawn->SetPawnState(EPawnState::Idle);
		Action.Clear();
		return;
	}

	// Red X stall: NPC footprint overlaps target's footprint and the target is doing a
	// non-move interaction. Players are never stalled — they can always walk out freely.
	if (!Pawn->IsPlayerControlled())
	{
		FIntVector2 PawnOrig = Pawn->GetTileCoord();
		FIntVector2 TargOrig = Target->GetTileCoord();
		bool bOverlap =
			PawnOrig.X                      < TargOrig.X + Target->TileExtent.X &&
			PawnOrig.X + Pawn->TileExtent.X > TargOrig.X &&
			PawnOrig.Y                      < TargOrig.Y + Target->TileExtent.Y &&
			PawnOrig.Y + Pawn->TileExtent.Y > TargOrig.Y;
		if (bOverlap)
		{
			EActionType TargetAction = GetActionType(Target);
			if (TargetAction == EActionType::Attack || TargetAction == EActionType::AttackMove)
			{
				MovementManager->CancelMove(Pawn);
				return;
			}
		}
	}

	if (IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, Pawn->AttackRange, Pawn->bAttackCardinalOnly))
	{
		MovementManager->CancelMove(Pawn);
		Action.Type = EActionType::Attack;
		TickAttack(Pawn, Action, TickIndex);
		return;
	}

	// Only re-path when target moved or the current path was cleared (e.g. blocked tile).
	FIntVector2 TargetCurrentTile = Target->GetTileCoord();
	if (Action.LastKnownTargetTile != TargetCurrentTile || !MovementManager->HasPendingMove(Pawn))
	{
		Action.LastKnownTargetTile = TargetCurrentTile;
		RequestMoveTowardTarget(Pawn, Target);
	}
}

void URogueyActionManager::TickAttack(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Cast<ARogueyPawn>(Action.TargetActor.Get());
	if (!IsValid(Target) || Target->IsDead())
	{
		Pawn->SetPawnState(EPawnState::Idle);
		Action.Clear();
		return;
	}

	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, Pawn->AttackRange, Pawn->bAttackCardinalOnly))
	{
		Action.Type = EActionType::AttackMove;
		RequestMoveTowardTarget(Pawn, Target);
		return;
	}

	int32 Damage = CombatManager->TryAttack(Pawn, Target, TickIndex);
	if (Damage < 0) return; // still on cooldown

	Pawn->SetPawnState(EPawnState::Attacking);
	Target->ReceiveHit(Damage, Pawn);

	if (Target->CurrentHP <= 0)
	{
		Target->SetPawnState(EPawnState::Dead);
		Pawn->SetPawnState(EPawnState::Idle);
		Action.Clear();
	}
}

void URogueyActionManager::RequestMoveTowardTarget(ARogueyPawn* Pawn, ARogueyPawn* Target)
{
	FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, Pawn->AttackRange, Pawn->bAttackCardinalOnly);
	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
	if (Path.IsValid())
		MovementManager->RequestMove(Pawn, MoveTemp(Path), true);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool URogueyActionManager::IsInAttackRange(FIntVector2 AOrigin, FIntPoint AExtent, FIntVector2 TOrigin, FIntPoint TExtent, int32 Range, bool bCardinalOnly)
{
	// Axis-aligned gap between the two rects (0 means edges touch or overlap on that axis)
	int32 gapX = FMath::Max(0, FMath::Max(AOrigin.X - (TOrigin.X + TExtent.X - 1),
	                                        TOrigin.X - (AOrigin.X + AExtent.X - 1)));
	int32 gapY = FMath::Max(0, FMath::Max(AOrigin.Y - (TOrigin.Y + TExtent.Y - 1),
	                                        TOrigin.Y - (AOrigin.Y + AExtent.Y - 1)));
	if (gapX == 0 && gapY == 0) return false; // rects overlap — can't attack from inside
	if (bCardinalOnly)
		return (gapX == 0 && gapY > 0 && gapY <= Range) || (gapY == 0 && gapX > 0 && gapX <= Range);
	return FMath::Max(gapX, gapY) <= Range;
}

FIntVector2 URogueyActionManager::FindBestAttackTile(FIntVector2 AOrigin, FIntPoint AExtent, FIntVector2 TOrigin, FIntPoint TExtent, int32 Range, bool bCardinalOnly) const
{
	FIntVector2 BestGoal = AOrigin; // fallback: stay put, pathfinder will fail gracefully
	int32 BestDist = INT_MAX;

	// All valid attacker origins are within Range of the target rect, accounting for attacker size
	for (int32 x = TOrigin.X - AExtent.X + 1 - Range; x <= TOrigin.X + TExtent.X - 1 + Range; x++)
	{
		for (int32 y = TOrigin.Y - AExtent.Y + 1 - Range; y <= TOrigin.Y + TExtent.Y - 1 + Range; y++)
		{
			FIntVector2 Candidate(x, y);
			if (!IsInAttackRange(Candidate, AExtent, TOrigin, TExtent, Range, bCardinalOnly)) continue;

			// Every tile in the attacker's footprint at this candidate origin must be valid
			bool bValid = true;
			for (int32 dx = 0; dx < AExtent.X && bValid; dx++)
				for (int32 dy = 0; dy < AExtent.Y && bValid; dy++)
				{
					FIntVector2 Foot(x + dx, y + dy);
					if (!GridManager->IsInBounds(Foot) || !GridManager->IsWalkable(Foot))
						bValid = false;
				}
			if (!bValid) continue;

			int32 D = ChebyshevDist(Candidate, AOrigin);
			if (D < BestDist) { BestDist = D; BestGoal = Candidate; }
		}
	}

	return BestGoal;
}

int32 URogueyActionManager::ChebyshevDist(FIntVector2 A, FIntVector2 B)
{
	return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
}

// ---------------------------------------------------------------------------
// Loot
// ---------------------------------------------------------------------------

void URogueyActionManager::SetTakeLootAction(ARogueyPawn* Pawn, ARogueyLootDrop* Drop)
{
	if (!IsValid(Pawn) || !IsValid(Drop)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::TakeLoot;
	Action.TargetActor = Drop;
	PendingActions.Add(Pawn, Action);

	if (Pawn->GetTileCoord() != Drop->LootTile)
	{
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), Drop->LootTile, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), true);
	}
}

void URogueyActionManager::TickTakeLoot(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyLootDrop* Drop = Cast<ARogueyLootDrop>(Action.TargetActor.Get());
	if (!IsValid(Drop))
	{
		MovementManager->CancelMove(Pawn);
		Action.Clear();
		return;
	}

	if (Pawn->GetTileCoord() == Drop->LootTile)
	{
		MovementManager->CancelMove(Pawn);
		Drop->TakeItem(Pawn);
		Action.Clear();
		return;
	}

	// Re-path if we've stopped moving (e.g. blocked)
	if (!MovementManager->HasPendingMove(Pawn))
	{
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), Drop->LootTile, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), true);
		else
			Action.Clear(); // unreachable
	}
}

// ---------------------------------------------------------------------------
// Consume queue
// ---------------------------------------------------------------------------

void URogueyActionManager::QueueConsume(ARogueyPawn* Pawn, int32 InvSlotIndex)
{
	if (!IsValid(Pawn)) return;
	PendingConsumeSlots.FindOrAdd(Pawn).Add(InvSlotIndex);
}

void URogueyActionManager::ProcessConsumeQueue(ARogueyPawn* Pawn, TArray<int32>& Slots)
{
	URogueyItemRegistry* Reg = URogueyItemRegistry::Get(Pawn);
	if (!Reg) return;

	// Reset slot flags for this tick
	Pawn->bFoodSlotUsed      = false;
	Pawn->bQuickFoodSlotUsed = false;
	Pawn->bPotionSlotUsed    = false;

	for (int32 SlotIdx : Slots)
	{
		if (!Pawn->Inventory.IsValidIndex(SlotIdx)) continue;
		FRogueyItem& InvItem = Pawn->Inventory[SlotIdx];
		if (InvItem.IsEmpty()) continue;

		const FRogueyItemRow* Row = Reg->FindItem(InvItem.ItemId);
		if (!Row) continue;

		if (Row->Type == ERogueyItemType::Food3Tick)
		{
			if (Pawn->bFoodSlotUsed) continue;
			Pawn->CurrentHP = FMath::Min(Pawn->CurrentHP + Row->HealAmount, Pawn->MaxHP);
			Pawn->OnRep_HP();
			Pawn->AttackCooldownTicks += 3;
			Pawn->bFoodSlotUsed = true;

			InvItem.Quantity--;
			if (InvItem.Quantity <= 0) InvItem = FRogueyItem();
		}
		else if (Row->Type == ERogueyItemType::FoodQuick)
		{
			if (Pawn->bQuickFoodSlotUsed) continue;
			Pawn->CurrentHP = FMath::Min(Pawn->CurrentHP + Row->HealAmount, Pawn->MaxHP);
			Pawn->OnRep_HP();
			Pawn->AttackCooldownTicks += 2;
			Pawn->bFoodSlotUsed      = true;
			Pawn->bQuickFoodSlotUsed = true;
			Pawn->bPotionSlotUsed    = true;

			InvItem.Quantity--;
			if (InvItem.Quantity <= 0) InvItem = FRogueyItem();
		}
		else if (Row->Type == ERogueyItemType::Potion)
		{
			if (Pawn->bPotionSlotUsed) continue;
			if (Row->StatBuffAmount <= 0 || Row->StatBuffDurationTicks <= 0) continue;

			// Apply or refresh the stat buff
			bool bFound = false;
			for (ARogueyPawn::FRogueyActiveStatBuff& Buff : Pawn->ActiveStatBuffs)
			{
				if (Buff.StatType == Row->StatBuffType)
				{
					// Refresh: remove old boost, apply new
					Pawn->StatPage.ModifyCurrent(Buff.StatType, -Buff.BoostAmount);
					Buff.BoostAmount     = Row->StatBuffAmount;
					Buff.TicksRemaining  = Row->StatBuffDurationTicks;
					Pawn->StatPage.ModifyCurrent(Buff.StatType, Buff.BoostAmount);
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				ARogueyPawn::FRogueyActiveStatBuff NewBuff;
				NewBuff.StatType       = Row->StatBuffType;
				NewBuff.BoostAmount    = Row->StatBuffAmount;
				NewBuff.TicksRemaining = Row->StatBuffDurationTicks;
				Pawn->ActiveStatBuffs.Add(NewBuff);
				Pawn->StatPage.ModifyCurrent(Row->StatBuffType, Row->StatBuffAmount);
			}

			Pawn->bPotionSlotUsed = true;

			// Consume one dose
			InvItem.Quantity--;
			if (InvItem.Quantity <= 0)
			{
				InvItem.ItemId   = Row->DepletedItemId.IsNone() ? FName() : Row->DepletedItemId;
				InvItem.Quantity = InvItem.ItemId.IsNone() ? 0 : 1;
				if (InvItem.ItemId.IsNone()) InvItem = FRogueyItem();
			}
		}
	}
}

void URogueyActionManager::TickStatBuffs()
{
	for (TActorIterator<ARogueyPawn> It(GetWorld()); It; ++It)
	{
		ARogueyPawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn->ActiveStatBuffs.IsEmpty()) continue;

		for (int32 i = Pawn->ActiveStatBuffs.Num() - 1; i >= 0; i--)
		{
			ARogueyPawn::FRogueyActiveStatBuff& Buff = Pawn->ActiveStatBuffs[i];
			Buff.TicksRemaining--;
			if (Buff.TicksRemaining <= 0)
			{
				// Restore: remove boost but don't go below base level
				if (FRogueyStat* Stat = Pawn->StatPage.Stats.Find(Buff.StatType))
					Stat->CurrentLevel = FMath::Max(Stat->CurrentLevel - Buff.BoostAmount, Stat->BaseLevel);
				Pawn->ActiveStatBuffs.RemoveAt(i);
			}
		}
	}
}
