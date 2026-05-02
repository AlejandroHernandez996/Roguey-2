#include "RogueyActionManager.h"
#include "RogueyConstants.h"
#include "RogueyActionNames.h"
#include "Roguey/Trade/RogueyTradeManager.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/RogueyCharacter.h"
#include "RogueyConstants.h"
#include "Roguey/World/RogueyPortal.h"
#include "Roguey/World/RogueyObject.h"
#include "Roguey/World/RogueyBankObject.h"
#include "Roguey/RogueyGameInstance.h"
#include "Roguey/World/RogueyObjectRegistry.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/Npcs/RogueyNpcRegistry.h"
#include "Roguey/RogueyPlayerController.h"

#include "RogueyInteractable.h"
#include "RogueyMovementManager.h"
#include "RogueyPawn.h"
#include "RogueyPawnState.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Grid/RogueyPathfinder.h"
#include "Roguey/Combat/RogueyCombatManager.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Items/RogueyShopRegistry.h"
#include "Roguey/Items/RogueyItemRow.h"
#include "Roguey/Items/RogueyItemType.h"
#include "Roguey/Items/RogueyLootDrop.h"
#include "Roguey/Items/RogueyUseCombinationRegistry.h"
#include "Roguey/Combat/RogueySpellRegistry.h"
#include "Roguey/Combat/RogueySpellCombinationRegistry.h"
#include "Roguey/Skills/RogueyStat.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "Roguey/Skills/RogueySkillRecipeRegistry.h"
#include "Roguey/Passives/RogueyPassiveRegistry.h"
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

	// ── Inventory-mutation queue — runs after consume/stat-buffs, before movement ──
	for (auto& [Pawn, Ops] : PendingInvOps)
	{
		if (IsValid(Pawn) && !Pawn->IsDead())
			ProcessInvOpQueue(Pawn, Ops);
	}
	PendingInvOps.Empty();

	// ── Main action loop ──────────────────────────────────────────────────────
	// Snapshot keys first: TickEnterMove→TryEnter→ResetArea calls ClearAction for all
	// pawns, which removes entries from PendingActions mid-iteration and crashes the TMap.
	TArray<ARogueyPawn*> Keys;
	PendingActions.GetKeys(Keys);

	for (ARogueyPawn* Pawn : Keys)
	{
		// Reset the replacement flag so only ClearAction calls from within THIS pawn's handler
		// can set it and suppress the write-back below.
		PawnsReplacedThisTick.Remove(Pawn);

		FRogueyPendingAction* ActionPtr = PendingActions.Find(Pawn);
		if (!ActionPtr) continue; // removed by an earlier handler (e.g. ResetArea)

		if (!IsValid(Pawn) || Pawn->IsDead())
		{
			PendingActions.Remove(Pawn);
			continue;
		}

		// Copy so that tick handlers which mutate PendingActions (ResetArea calls ClearAction
		// for every pawn) don't leave us with a dangling reference into the map.
		FRogueyPendingAction Action = *ActionPtr;

		switch (Action.Type)
		{
			case EActionType::Move:            TickMove(Pawn, Action, TickIndex);            break;
			case EActionType::AttackMove:      TickAttackMove(Pawn, Action, TickIndex);      break;
			case EActionType::Attack:          TickAttack(Pawn, Action, TickIndex);          break;
			case EActionType::TakeLoot:        TickTakeLoot(Pawn, Action, TickIndex);        break;
			case EActionType::TalkMove:        TickTalkMove(Pawn, Action, TickIndex);        break;
			case EActionType::TradeMove:       TickTradeMove(Pawn, Action, TickIndex);       break;
			case EActionType::PlayerTradeMove: TickPlayerTradeMove(Pawn, Action, TickIndex); break;
			case EActionType::GatherMove:      TickGatherMove(Pawn, Action, TickIndex);      break;
			case EActionType::Gather:          TickGather(Pawn, Action, TickIndex);          break;
			case EActionType::EnterMove:       TickEnterMove(Pawn, Action, TickIndex);       break;
			case EActionType::FollowMove:      TickFollowMove(Pawn, Action, TickIndex);      break;
			case EActionType::BankMove:        TickBankMove(Pawn, Action, TickIndex);        break;
			case EActionType::BankViaNpcMove:  TickBankViaNpcMove(Pawn, Action, TickIndex);  break;
			case EActionType::UseOnActorMove:  TickUseOnActorMove(Pawn, Action, TickIndex);  break;
			case EActionType::UseOnObject:     TickUseOnObject(Pawn, Action, TickIndex);     break;
			case EActionType::CraftMove:       TickCraftMove(Pawn, Action, TickIndex);       break;
			case EActionType::SkillCraft:      TickSkillCraft(Pawn, Action, TickIndex);      break;
			default: break;
		}

		// Write back mutations (Type transitions, TicksRemaining, LastKnownTargetTile, etc.).
		// Skip if ClearAction was called for this pawn during the handler — a Set*Action call
		// replaced the entry and we must not clobber the new action with the stale local copy.
		if (!PawnsReplacedThisTick.Contains(Pawn))
		{
			if (FRogueyPendingAction* Current = PendingActions.Find(Pawn))
			{
				*Current = Action;
				if (!Current->IsActive())
					PendingActions.Remove(Pawn);
			}
		}
	}
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

void URogueyActionManager::SetActorAction(ARogueyPawn* Pawn, AActor* Target, FName ActionId, bool bRunning, int32 InvSlot)
{
	if (!IsValid(Pawn) || !IsValid(Target)) return;

	// UseOn bypasses GetActions() validation — any interactable can be targeted; the combination
	// registry does its own validation inside TickUseOnActorMove.
	if (ActionId == RogueyActions::UseOn)
	{
		SetUseOnActorAction(Pawn, Target, InvSlot, bRunning);
		return;
	}

	IRogueyInteractable* Interactable = Cast<IRogueyInteractable>(Target);
	if (!Interactable) return;

	// Validate the action is actually offered by this target
	TArray<FRogueyActionDef> Actions = Interactable->GetActions();
	if (!Actions.ContainsByPredicate([&](const FRogueyActionDef& A){ return A.ActionId == ActionId; }))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetActorAction: '%s' rejected — not offered by %s"), *ActionId.ToString(), *Target->GetClass()->GetName());
		return;
	}

	if (ActionId == RogueyActions::Attack)
	{
		if (ARogueyPawn* TargetPawn = Cast<ARogueyPawn>(Target))
			SetAttackAction(Pawn, TargetPawn, bRunning);
	}
	else if (ActionId == RogueyActions::Take)
	{
		if (ARogueyLootDrop* Drop = Cast<ARogueyLootDrop>(Target))
			SetTakeLootAction(Pawn, Drop, bRunning);
	}
	else if (ActionId == RogueyActions::Examine)
	{
		FString Text = Interactable->GetExamineText();
		Pawn->PostGameMessage(Text, RogueyChat::Examine);
	}
	else if (ActionId == RogueyActions::TalkTo)
	{
		if (ARogueyNpc* Npc = Cast<ARogueyNpc>(Target))
			SetTalkAction(Pawn, Npc, bRunning);
	}
	else if (ActionId == RogueyActions::Bank)
	{
		if (ARogueyNpc* Npc = Cast<ARogueyNpc>(Target))
		{
			UE_LOG(LogTemp, Log, TEXT("SetActorAction: Bank dispatched to SetBankViaNpcAction, NpcTypeId=%s, Behavior=%d"), *Npc->NpcTypeId.ToString(), (int32)Npc->Behavior);
			SetBankViaNpcAction(Pawn, Npc, bRunning);
		}
		else
			UE_LOG(LogTemp, Warning, TEXT("SetActorAction: Bank action but target is not ARogueyNpc (class=%s)"), *Target->GetClass()->GetName());
	}
	else if (ActionId == RogueyActions::Trade)
	{
		if (ARogueyNpc* Npc = Cast<ARogueyNpc>(Target))
			SetTradeAction(Pawn, Npc, bRunning);
		else if (ARogueyCharacter* Other = Cast<ARogueyCharacter>(Target))
			SetPlayerTradeAction(Pawn, Other);
	}
	else if (ActionId == RogueyActions::Enter)
	{
		if (ARogueyPortal* Portal = Cast<ARogueyPortal>(Target))
			SetEnterPortalAction(Pawn, Portal);
	}
	else if (ActionId == RogueyActions::Gather)
	{
		if (ARogueyObject* Object = Cast<ARogueyObject>(Target))
			SetGatherAction(Pawn, Object, bRunning);
	}
	else if (ActionId == RogueyActions::Follow)
	{
		if (ARogueyPawn* TargetPawn = Cast<ARogueyPawn>(Target))
			SetFollowAction(Pawn, TargetPawn);
	}
	else if (ActionId == RogueyActions::OpenBank)
	{
		if (ARogueyBankObject* Bank = Cast<ARogueyBankObject>(Target))
			SetBankAction(Pawn, Bank, bRunning);
	}
	else if (ActionId == RogueyActions::Craft)
	{
		if (ARogueyObject* Object = Cast<ARogueyObject>(Target))
			SetCraftAction(Pawn, Object, bRunning);
	}
	else if (ActionId == RogueyActions::Offer)
	{
		// Find the first oak log in inventory and walk to offer it
		int32 Slot = -1;
		for (int32 i = 0; i < Pawn->Inventory.Num(); i++)
			if (!Pawn->Inventory[i].IsEmpty() && Pawn->Inventory[i].ItemId == FName("oak_logs"))
				{ Slot = i; break; }
		if (Slot >= 0)
			SetUseOnActorAction(Pawn, Target, Slot, bRunning);
		else
			Pawn->PostGameMessage(TEXT("You need oak logs to awaken the guardian."), RogueyChat::Game);
	}
}

void URogueyActionManager::SetAttackAction(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Target) || Target->IsDead()) return;

	ClearAction(Pawn); // clears AttackTarget

	FRogueyPendingAction Action;
	Action.Type        = EActionType::AttackMove;
	Action.TargetActor = Target;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	Pawn->AttackTarget = Target;

	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, Pawn->AttackRange, Pawn->bAttackCardinalOnly))
		RequestMoveTowardTarget(Pawn, Target, bRunning);
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
		switch (Existing->Type)
		{
		case EActionType::Move:
		case EActionType::AttackMove:
		case EActionType::GatherMove:
		case EActionType::TalkMove:
		case EActionType::TradeMove:
		case EActionType::PlayerTradeMove:
		case EActionType::EnterMove:
		case EActionType::FollowMove:
		case EActionType::BankMove:
		case EActionType::BankViaNpcMove:
		case EActionType::UseOnActorMove:
		case EActionType::SpellCastMove:
		case EActionType::CraftMove:
			MovementManager->CancelMove(Pawn);
			break;
		default:
			break;
		}
	}
	PendingActions.Remove(Pawn);
	PawnsReplacedThisTick.Add(Pawn);
	if (IsValid(Pawn)) Pawn->AttackTarget = nullptr;
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
	if (!ARogueyPawn::IsAlive(Target))
	{
		MovementManager->CancelMove(Pawn);
		Pawn->SetPawnState(EPawnState::Idle);
		Pawn->AttackTarget = nullptr;
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
		// Ranged/magic also require an unobstructed line of sight before committing to Attack.
		// Melee (range=1) is always clear — the gap check already ensures adjacency with no tiles between.
		const bool bHasLoS = Pawn->AttackRange <= 1
			|| GridManager->HasLineOfSight(Pawn->GetTileCoord(), Target->GetTileCoord());

		if (bHasLoS)
		{
			MovementManager->CancelMove(Pawn);
			Action.Type = EActionType::Attack;
			TickAttack(Pawn, Action, TickIndex);
			return;
		}
		// In range but LoS blocked — fall through and re-path to find a clear angle
	}

	// Only re-path when target moved or the current path was cleared (e.g. blocked tile).
	FIntVector2 TargetCurrentTile = Target->GetTileCoord();
	if (Action.LastKnownTargetTile != TargetCurrentTile || !MovementManager->HasPendingMove(Pawn))
	{
		Action.LastKnownTargetTile = TargetCurrentTile;
		RequestMoveTowardTarget(Pawn, Target, Action.bRunning);
	}
}

void URogueyActionManager::TickAttack(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Cast<ARogueyPawn>(Action.TargetActor.Get());
	if (!ARogueyPawn::IsAlive(Target))
	{
		Pawn->SetPawnState(EPawnState::Idle);
		Pawn->AttackTarget = nullptr;
		Action.Clear();
		return;
	}

	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, Pawn->AttackRange, Pawn->bAttackCardinalOnly))
	{
		Action.Type = EActionType::AttackMove;
		RequestMoveTowardTarget(Pawn, Target, Action.bRunning);
		return;
	}

	// Safety guard: LoS may have been blocked since AttackMove last evaluated it (target/wall moved).
	if (Pawn->AttackRange > 1 && !GridManager->HasLineOfSight(Pawn->GetTileCoord(), Target->GetTileCoord()))
	{
		Action.Type = EActionType::AttackMove;
		Action.LastKnownTargetTile = FIntVector2(-1, -1); // force repath
		return;
	}

	if (CombatManager->TryCombatAttack(Pawn, Target, TickIndex))
		Pawn->SetPawnState(EPawnState::Attacking);
}

void URogueyActionManager::RequestMoveTowardTarget(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning)
{
	FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, Pawn->AttackRange, Pawn->bAttackCardinalOnly);
	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
	if (Path.IsValid())
		MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);
}

bool URogueyActionManager::IsAdjacent(const ARogueyPawn* A, const ARogueyPawn* B) const
{
	return IsInAttackRange(A->GetTileCoord(), A->TileExtent, B->GetTileCoord(), B->TileExtent, 1, false);
}

void URogueyActionManager::MoveToAdjacentTarget(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning)
{
	FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, Target->GetTileCoord(), Target->TileExtent, 1, false);
	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
	if (Path.IsValid())
		MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);
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
	FIntVector2 BestGoal(-1, -1); // sentinel: no reachable adjacent tile found; caller should retry next tick
	int32 BestDist = INT_MAX;

	// All valid attacker origins are within Range of the target rect, accounting for attacker size
	for (int32 x = TOrigin.X - AExtent.X + 1 - Range; x <= TOrigin.X + TExtent.X - 1 + Range; x++)
	{
		for (int32 y = TOrigin.Y - AExtent.Y + 1 - Range; y <= TOrigin.Y + TExtent.Y - 1 + Range; y++)
		{
			FIntVector2 Candidate(x, y);
			if (!IsInAttackRange(Candidate, AExtent, TOrigin, TExtent, Range, bCardinalOnly)) continue;

			// Every tile in the attacker's footprint at this candidate origin must be walkable
			// and unoccupied — otherwise the pathfinder can't actually reach it.
			bool bValid = true;
			for (int32 dx = 0; dx < AExtent.X && bValid; dx++)
				for (int32 dy = 0; dy < AExtent.Y && bValid; dy++)
				{
					FIntVector2 Foot(x + dx, y + dy);
					if (!GridManager->IsInBounds(Foot) || !GridManager->IsWalkable(Foot) || GridManager->IsOccupiedByBlocker(Foot))
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

bool URogueyActionManager::PawnHasTool(const ARogueyPawn* Pawn, FName ItemId)
{
	if (!Pawn || ItemId.IsNone()) return true; // nothing required
	for (const FRogueyItem& Item : Pawn->Inventory)
		if (Item.ItemId == ItemId) return true;
	for (const auto& [Slot, Item] : Pawn->Equipment)
		if (Item.ItemId == ItemId) return true;
	return false;
}

static const TCHAR* GatheringSkillName(ERogueyStatType Skill)
{
	switch (Skill)
	{
	case ERogueyStatType::Woodcutting: return TEXT("Woodcutting");
	case ERogueyStatType::Mining:      return TEXT("Mining");
	case ERogueyStatType::Fishing:     return TEXT("Fishing");
	case ERogueyStatType::Fletching:   return TEXT("Fletching");
	case ERogueyStatType::Smithing:    return TEXT("Smithing");
	default:                           return TEXT("Gathering");
	}
}

// ---------------------------------------------------------------------------
// Talk-to
// ---------------------------------------------------------------------------

void URogueyActionManager::SetTalkAction(ARogueyPawn* Pawn, ARogueyPawn* Target, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Target)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::TalkMove;
	Action.TargetActor = Target;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	if (!IsAdjacent(Pawn, Target))
		MoveToAdjacentTarget(Pawn, Target, bRunning);
}

void URogueyActionManager::TickTalkMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Cast<ARogueyPawn>(Action.TargetActor.Get());
	if (!IsValid(Target))
	{
		MovementManager->CancelMove(Pawn);
		Action.Clear();
		return;
	}

	if (!IsAdjacent(Pawn, Target))
	{
		FIntVector2 TargetCurrentTile = Target->GetTileCoord();
		if (Action.LastKnownTargetTile != TargetCurrentTile || !MovementManager->HasPendingMove(Pawn))
		{
			Action.LastKnownTargetTile = TargetCurrentTile;
			MoveToAdjacentTarget(Pawn, Target, Action.bRunning);
		}
		return;
	}

	// Adjacent — open dialogue on the owning client
	MovementManager->CancelMove(Pawn);
	Action.Clear();

	if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController()))
	{
		URogueyNpcRegistry* NpcReg = URogueyNpcRegistry::Get(Pawn);
		const ARogueyNpc*   Npc    = Cast<ARogueyNpc>(Target);
		const FRogueyNpcRow* Row   = (NpcReg && Npc) ? NpcReg->FindNpc(Npc->NpcTypeId) : nullptr;
		if (Row && !Row->DialogueStartNodeId.IsNone())
			PC->Client_OpenDialogue(Row->DialogueStartNodeId, Row->NpcName);
	}
}

// ---------------------------------------------------------------------------
// Trade
// ---------------------------------------------------------------------------

void URogueyActionManager::SetTradeAction(ARogueyPawn* Pawn, ARogueyNpc* Target, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Target)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::TradeMove;
	Action.TargetActor = Target;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	if (!IsAdjacent(Pawn, Target))
		MoveToAdjacentTarget(Pawn, Target, bRunning);
}

void URogueyActionManager::TickTradeMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyNpc* Target = Cast<ARogueyNpc>(Action.TargetActor.Get());
	if (!IsValid(Target))
	{
		MovementManager->CancelMove(Pawn);
		Action.Clear();
		return;
	}

	if (!IsAdjacent(Pawn, Target))
	{
		FIntVector2 TargetCurrentTile = Target->GetTileCoord();
		if (Action.LastKnownTargetTile != TargetCurrentTile || !MovementManager->HasPendingMove(Pawn))
		{
			Action.LastKnownTargetTile = TargetCurrentTile;
			MoveToAdjacentTarget(Pawn, Target, Action.bRunning);
		}
		return;
	}

	// Adjacent — open shop on the owning client
	MovementManager->CancelMove(Pawn);
	Action.Clear();

	if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController()))
		PC->Client_OpenShop(Target->NpcTypeId);
}

// ── Player-to-player trade ──────────────────────────────────────────────────

void URogueyActionManager::SetPlayerTradeAction(ARogueyPawn* Pawn, ARogueyPawn* Target)
{
	if (!IsValid(Pawn) || !IsValid(Target) || Pawn == Target) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::PlayerTradeMove;
	Action.TargetActor = Target;
	PendingActions.Add(Pawn, Action);

	if (!IsAdjacent(Pawn, Target))
		MoveToAdjacentTarget(Pawn, Target);
}

void URogueyActionManager::TickPlayerTradeMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Cast<ARogueyPawn>(Action.TargetActor.Get());
	if (!ARogueyPawn::IsAlive(Target))
	{
		MovementManager->CancelMove(Pawn);
		Action.Clear();
		return;
	}

	if (!IsAdjacent(Pawn, Target))
	{
		FIntVector2 TargetTile = Target->GetTileCoord();
		if (Action.LastKnownTargetTile != TargetTile || !MovementManager->HasPendingMove(Pawn))
		{
			Action.LastKnownTargetTile = TargetTile;
			MoveToAdjacentTarget(Pawn, Target);
		}
		return;
	}

	// Adjacent — resolve: confirm if Target already sent a request to Pawn, otherwise request
	MovementManager->CancelMove(Pawn);
	Action.Clear();

	if (ARogueyGameMode* GM = Cast<ARogueyGameMode>(GetOuter()))
	{
		if (GM->TradeManager)
		{
			if (GM->TradeManager->HasPendingRequestFrom(Target, Pawn))
				GM->TradeManager->ConfirmTrade(Pawn, Target);
			else
				GM->TradeManager->RequestTrade(Pawn, Target);
		}
	}
}

// ---------------------------------------------------------------------------
// Loot
// ---------------------------------------------------------------------------

void URogueyActionManager::SetTakeLootAction(ARogueyPawn* Pawn, ARogueyLootDrop* Drop, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Drop)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::TakeLoot;
	Action.TargetActor = Drop;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	if (Pawn->GetTileCoord() != Drop->LootTile)
	{
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), Drop->LootTile, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);
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
			MovementManager->RequestMove(Pawn, MoveTemp(Path), Action.bRunning);
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
			Pawn->FoodCooldownPenalty += 3;
			Pawn->bFoodSlotUsed = true;

			Pawn->PostGameMessage(
				FString::Printf(TEXT("You eat the %s. It heals %d HP."), *Row->DisplayName, Row->HealAmount),
				RogueyChat::Consume);

			InvItem.Quantity--;
			if (InvItem.Quantity <= 0) InvItem = FRogueyItem();
		}
		else if (Row->Type == ERogueyItemType::FoodQuick)
		{
			if (Pawn->bQuickFoodSlotUsed) continue;
			Pawn->CurrentHP = FMath::Min(Pawn->CurrentHP + Row->HealAmount, Pawn->MaxHP);
			Pawn->OnRep_HP();
			Pawn->FoodCooldownPenalty += 2;
			Pawn->bFoodSlotUsed      = true;
			Pawn->bQuickFoodSlotUsed = true;
			Pawn->bPotionSlotUsed    = true;

			Pawn->PostGameMessage(
				FString::Printf(TEXT("You eat the %s. It heals %d HP."), *Row->DisplayName, Row->HealAmount),
				RogueyChat::Consume);

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

			Pawn->SyncStatPage();
			Pawn->bPotionSlotUsed = true;

			Pawn->PostGameMessage(
				FString::Printf(TEXT("You drink some %s."), *Row->DisplayName),
				RogueyChat::Consume);

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

void URogueyActionManager::QueueInvOp(ARogueyPawn* Pawn, FPendingInvOp Op)
{
	if (!IsValid(Pawn)) return;
	PendingInvOps.FindOrAdd(Pawn).Add(Op);
}

void URogueyActionManager::ProcessInvOpQueue(ARogueyPawn* Pawn, TArray<FPendingInvOp>& Ops)
{
	ARogueyGameMode* GM = Cast<ARogueyGameMode>(GetOuter());

	for (const FPendingInvOp& Op : Ops)
	{
		switch (Op.OpType)
		{
		// ── Swap ────────────────────────────────────────────────────────────────
		case EInvOpType::SwapSlots:
			if (Pawn->Inventory.IsValidIndex(Op.SlotA) && Pawn->Inventory.IsValidIndex(Op.SlotB))
				Pawn->Inventory.Swap(Op.SlotA, Op.SlotB);
			break;

		// ── Equip ───────────────────────────────────────────────────────────────
		case EInvOpType::EquipFromInventory:
		{
			if (!Pawn->Inventory.IsValidIndex(Op.SlotA) || Pawn->Inventory[Op.SlotA].IsEmpty()) break;
			URogueyItemRegistry* Reg = URogueyItemRegistry::Get(Pawn);
			if (!Reg) break;
			const FRogueyItemRow* Row = Reg->FindItem(Pawn->Inventory[Op.SlotA].ItemId);
			if (!Row || !Row->IsEquippable()) break;
			EEquipmentSlot Slot = Row->GetEquipSlot();
			FRogueyItem ItemToEquip = Pawn->Inventory[Op.SlotA];
			if (Row->bStackable && Pawn->Equipment.Contains(Slot) && Pawn->Equipment[Slot].ItemId == ItemToEquip.ItemId)
			{
				Pawn->Equipment[Slot].Quantity += ItemToEquip.Quantity;
				Pawn->Inventory[Op.SlotA] = FRogueyItem();
			}
			else
			{
				Pawn->Inventory[Op.SlotA] = Pawn->Equipment.Contains(Slot) ? Pawn->Equipment[Slot] : FRogueyItem();
				Pawn->Equipment.Add(Slot, ItemToEquip);
			}
			Pawn->RecalcEquipmentBonuses();
			break;
		}

		// ── Unequip ─────────────────────────────────────────────────────────────
		case EInvOpType::UnequipToInventory:
			if (Pawn->Equipment.Contains(Op.EquipSlot) && !Pawn->Equipment[Op.EquipSlot].IsEmpty())
			{
				const FRogueyItem ItemToReturn = Pawn->Equipment[Op.EquipSlot];
				if (Pawn->TryAddItem(ItemToReturn))
				{
					Pawn->Equipment.Remove(Op.EquipSlot);
					Pawn->RecalcEquipmentBonuses();
				}
			}
			break;

		// ── Bank deposit ─────────────────────────────────────────────────────────
		case EInvOpType::BankDeposit:
		{
			if (!Pawn->Inventory.IsValidIndex(Op.SlotA) || Pawn->Inventory[Op.SlotA].IsEmpty()) break;
			ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController());
			URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>();
			if (!PC || !GI) break;
			TArray<FRogueyItem>& Bank = GI->GetOrCreateBank(URogueyGameInstance::GetPlayerKey(PC));
			const FRogueyItem Item = Pawn->Inventory[Op.SlotA];
			URogueyItemRegistry* Reg = URogueyItemRegistry::Get(Pawn);
			const FRogueyItemRow* Row = Reg ? Reg->FindItem(Item.ItemId) : nullptr;
			bool bStacked = false;
			if (Row && Row->bStackable)
				for (FRogueyItem& BankSlot : Bank)
					if (BankSlot.ItemId == Item.ItemId)
					{
						BankSlot.Quantity += Item.Quantity;
						bStacked = true;
						break;
					}
			if (!bStacked)
			{
				bool bPlaced = false;
				for (FRogueyItem& BankSlot : Bank)
					if (BankSlot.IsEmpty()) { BankSlot = Item; bPlaced = true; break; }
				if (!bPlaced) break; // bank full
			}
			Pawn->Inventory[Op.SlotA] = FRogueyItem();
			PC->Client_UpdateBank(Bank);
			break;
		}

		// ── Bank withdraw ────────────────────────────────────────────────────────
		case EInvOpType::BankWithdraw:
		{
			ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController());
			URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>();
			if (!PC || !GI) break;
			TArray<FRogueyItem>& Bank = GI->GetOrCreateBank(URogueyGameInstance::GetPlayerKey(PC));
			if (!Bank.IsValidIndex(Op.SlotA) || Bank[Op.SlotA].IsEmpty()) break;
			FRogueyItem& BankSlot = Bank[Op.SlotA];
			const int32 WithdrawQty = FMath::Min(Op.Quantity, BankSlot.Quantity);
			URogueyItemRegistry* Reg = URogueyItemRegistry::Get(Pawn);
			const FRogueyItemRow* Row = Reg ? Reg->FindItem(BankSlot.ItemId) : nullptr;
			bool bDone = false;
			if (Row && Row->bStackable)
				for (int32 i = 0; i < Pawn->Inventory.Num(); i++)
					if (Pawn->Inventory[i].ItemId == BankSlot.ItemId)
					{
						Pawn->Inventory[i].Quantity += WithdrawQty;
						BankSlot.Quantity -= WithdrawQty;
						if (BankSlot.Quantity <= 0) BankSlot = FRogueyItem();
						PC->Client_UpdateBank(Bank);
						bDone = true;
						break;
					}
			if (!bDone)
			{
				int32 InvSlot = -1;
				for (int32 i = 0; i < Pawn->Inventory.Num(); i++)
					if (Pawn->Inventory[i].IsEmpty()) { InvSlot = i; break; }
				if (InvSlot < 0) break;
				Pawn->Inventory[InvSlot] = FRogueyItem(BankSlot.ItemId, WithdrawQty);
				BankSlot.Quantity -= WithdrawQty;
				if (BankSlot.Quantity <= 0) BankSlot = FRogueyItem();
				PC->Client_UpdateBank(Bank);
			}
			break;
		}

		// ── Shop buy ─────────────────────────────────────────────────────────────
		case EInvOpType::BuyShopItem:
		{
			if (Op.Quantity <= 0 || !GM) break;
			URogueyShopRegistry* ShopReg = URogueyShopRegistry::Get(Pawn);
			URogueyItemRegistry*  ItemReg = URogueyItemRegistry::Get(Pawn);
			if (!ShopReg || !ItemReg) break;
			const FRogueyItemRow* ItemRow = ItemReg->FindItem(Op.NameB);
			if (!ItemRow) break;
			FName  RowName;
			int32  UnitPrice = 0;
			for (const FRogueyShopRow& R : ShopReg->GetShopItems(Op.NameA))
				if (R.ItemId == Op.NameB) { RowName = R.RowName; UnitPrice = R.Price; break; }
			if (RowName.IsNone()) break;
			int32 TotalCost = UnitPrice * Op.Quantity;
			int32 CoinsSlot = -1;
			for (int32 i = 0; i < Pawn->Inventory.Num(); i++)
				if (Pawn->Inventory[i].ItemId == FName("coins") && Pawn->Inventory[i].Quantity >= TotalCost)
					{ CoinsSlot = i; break; }
			if (CoinsSlot < 0) break;
			bool bHasSpace = false;
			if (ItemRow->bStackable) {
				for (const FRogueyItem& S : Pawn->Inventory)
					if (S.ItemId == Op.NameB || S.IsEmpty()) { bHasSpace = true; break; }
			} else {
				for (const FRogueyItem& S : Pawn->Inventory)
					if (S.IsEmpty()) { bHasSpace = true; break; }
			}
			if (!bHasSpace) break;
			if (!GM->TryConsumeShopStock(RowName, Op.Quantity)) break;
			Pawn->Inventory[CoinsSlot].Quantity -= TotalCost;
			if (Pawn->Inventory[CoinsSlot].Quantity <= 0) Pawn->Inventory[CoinsSlot] = FRogueyItem();
			if (ItemRow->bStackable) {
				for (FRogueyItem& S : Pawn->Inventory)
					if (S.ItemId == Op.NameB) { S.Quantity += Op.Quantity; goto buy_done; }
			}
			for (FRogueyItem& S : Pawn->Inventory)
				if (S.IsEmpty()) { S.ItemId = Op.NameB; S.Quantity = Op.Quantity; break; }
			buy_done:;
			break;
		}

		// ── Drop ─────────────────────────────────────────────────────────────────
		case EInvOpType::DropFromInventory:
		{
			if (!Pawn->Inventory.IsValidIndex(Op.SlotA) || Pawn->Inventory[Op.SlotA].IsEmpty()) break;
			FRogueyItem ItemToDrop     = Pawn->Inventory[Op.SlotA];
			Pawn->Inventory[Op.SlotA] = FRogueyItem();
			ARogueyLootDrop* Drop = GetWorld()->SpawnActor<ARogueyLootDrop>(
				ARogueyLootDrop::StaticClass(), Pawn->GetActorLocation(), FRotator::ZeroRotator);
			if (Drop) Drop->Init(ItemToDrop, Pawn->GetTileCoord());
			break;
		}

		// ── Use item on item ─────────────────────────────────────────────────────
		case EInvOpType::UseItemOnItem:
		{
			if (Op.SlotA == Op.SlotB) break;
			if (!Pawn->Inventory.IsValidIndex(Op.SlotA) || Pawn->Inventory[Op.SlotA].IsEmpty()) break;
			if (!Pawn->Inventory.IsValidIndex(Op.SlotB) || Pawn->Inventory[Op.SlotB].IsEmpty()) break;

			// Skill recipe check first (e.g. knife + logs → fletching menu)
			{
				FName IdA = Pawn->Inventory[Op.SlotA].ItemId;
				FName IdB = Pawn->Inventory[Op.SlotB].ItemId;
				URogueySkillRecipeRegistry* RecipeReg = URogueySkillRecipeRegistry::Get(Pawn);
				if (RecipeReg)
				{
					// Try both orderings: A=tool/B=trigger and B=tool/A=trigger
					TArray<FName> Recipes = RecipeReg->GetRecipesForInventoryTool(IdA, IdB);
					if (Recipes.IsEmpty())
						Recipes = RecipeReg->GetRecipesForInventoryTool(IdB, IdA);

					if (!Recipes.IsEmpty())
					{
						if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController()))
						{
							const FRogueySkillRecipeRow* FirstRecipe = RecipeReg->FindRecipe(Recipes[0]);
							FString Header = FirstRecipe
								? FString(GatheringSkillName(FirstRecipe->Skill))
								: TEXT("Skilling");
							PC->Client_OpenSkillMenu(Recipes, Header);
						}
						break;
					}
				}
			}

			URogueyUseCombinationRegistry* CombReg = URogueyUseCombinationRegistry::Get(Pawn);
			if (!CombReg) { Pawn->PostGameMessage(TEXT("Nothing interesting happens."), RogueyChat::Game); break; }

			const FRogueyUseCombinationRow* Row = CombReg->FindItemOnItem(
				Pawn->Inventory[Op.SlotA].ItemId, Pawn->Inventory[Op.SlotB].ItemId);
			if (!Row) { Pawn->PostGameMessage(TEXT("Nothing interesting happens."), RogueyChat::Game); break; }

			if (Row->RequiredLevel > 0)
			{
				const FRogueyStat* Stat = Pawn->StatPage.Stats.Find(Row->RequiredSkill);
				if (!Stat || Stat->CurrentLevel < Row->RequiredLevel)
				{
					Pawn->ShowSpeechBubble(Row->SkillFailMessage.IsEmpty()
						? TEXT("You need a higher level.")
						: Row->SkillFailMessage);
					break;
				}
			}

			// Align canonical order: the row's ItemAId tells us which slot is "A"
			int32 CanonSlotA = (Pawn->Inventory[Op.SlotA].ItemId == Row->ItemAId) ? Op.SlotA : Op.SlotB;
			int32 CanonSlotB = (CanonSlotA == Op.SlotA)                            ? Op.SlotB : Op.SlotA;

			if (Row->bConsumeA) Pawn->Inventory[CanonSlotA] = FRogueyItem();
			if (Row->bConsumeB) Pawn->Inventory[CanonSlotB] = FRogueyItem();

			for (const FRogueyUseCombinationResult& Res : Row->ResultItems)
			{
				FRogueyItem NewItem;
				NewItem.ItemId   = Res.ResultItemId;
				NewItem.Quantity = Res.Quantity;
				Pawn->TryAddItem(NewItem);
			}

			if (Row->XpAmount > 0)
			{
				FRogueyStat& Stat = Pawn->StatPage.Get(Row->XpSkill);
				if (Stat.AddXP(Row->XpAmount))
				{
					Pawn->ShowSpeechBubble(FString::Printf(TEXT("Skill level %d!"), Stat.BaseLevel));
					URogueyPassiveRegistry::NotifyLevelUp(Pawn, Row->XpSkill, Stat.BaseLevel);
				}
				Pawn->SyncStatPage();
			}

			if (!Row->DialogueTriggerNodeId.IsNone())
			{
				if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController()))
					PC->Client_OpenDialogue(Row->DialogueTriggerNodeId, TEXT(""));
			}
			break;
		}

		// ── Cast spell on inventory item ─────────────────────────────────────────
		case EInvOpType::SpellCastOnItem:
		{
			if (!Pawn->Inventory.IsValidIndex(Op.SlotA) || Pawn->Inventory[Op.SlotA].IsEmpty()) break;

			URogueySpellCombinationRegistry* SpellCombReg = URogueySpellCombinationRegistry::Get(Pawn);
			URogueySpellRegistry*            SpellReg     = URogueySpellRegistry::Get(Pawn);
			if (!SpellCombReg || !SpellReg) break;

			const FName TargetItemId = Pawn->Inventory[Op.SlotA].ItemId;
			const FRogueySpellCombinationRow* CombRow = SpellCombReg->FindSpellOnItem(Op.NameA, TargetItemId);
			if (!CombRow) { Pawn->PostGameMessage(TEXT("Nothing interesting happens."), RogueyChat::Game); break; }

			const FRogueySpellRow* SpellRow = SpellReg->FindSpell(Op.NameA);
			if (!SpellRow) break;

			// Level gate
			if (Pawn->StatPage.Get(ERogueyStatType::Magic).BaseLevel < SpellRow->LevelRequired)
			{
				Pawn->PostGameMessage(FString::Printf(TEXT("You need level %d Magic to cast this spell."), SpellRow->LevelRequired), RogueyChat::Warning);
				break;
			}

			// Rune check
			int32 RuneSlot = -1;
			if (!SpellRow->RuneId.IsNone() && CombRow->RuneCost > 0)
			{
				for (int32 i = 0; i < Pawn->Inventory.Num(); i++)
					if (Pawn->Inventory[i].ItemId == SpellRow->RuneId && Pawn->Inventory[i].Quantity >= CombRow->RuneCost)
						{ RuneSlot = i; break; }
				if (RuneSlot < 0)
				{
					Pawn->PostGameMessage(FString::Printf(TEXT("You don't have enough %s runes."), *SpellRow->RuneId.ToString()), RogueyChat::Warning);
					break;
				}
			}

			// Consume rune
			if (RuneSlot >= 0)
			{
				Pawn->Inventory[RuneSlot].Quantity -= CombRow->RuneCost;
				if (Pawn->Inventory[RuneSlot].Quantity <= 0) Pawn->Inventory[RuneSlot] = FRogueyItem();
			}

			// Consume item
			if (CombRow->bConsumesItem) Pawn->Inventory[Op.SlotA] = FRogueyItem();

			// Spawn world object at nearest free tile
			if (!CombRow->SpawnObjectTypeId.IsNone() && GM && GM->GridManager)
			{
				const FIntVector2 Origin = Pawn->GetTileCoord();
				const int32 Offsets[][2] = {{0,0},{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1},{2,0},{-2,0},{0,2},{0,-2}};
				FIntVector2 SpawnTile(-1,-1);
				for (auto& Off : Offsets)
				{
					FIntVector2 Cand(Origin.X + Off[0], Origin.Y + Off[1]);
					if (GM->GridManager->IsInBounds(Cand) && GM->GridManager->IsWalkable(Cand)
					    && !GM->GridManager->IsOccupiedByBlocker(Cand))
						{ SpawnTile = Cand; break; }
				}
				if (SpawnTile.X >= 0)
				{
					const float TS = RogueyConstants::TileSize;
				FVector SpawnLoc(SpawnTile.X * TS + TS * 0.5f, SpawnTile.Y * TS + TS * 0.5f, Pawn->GetActorLocation().Z);
					ARogueyObject* Obj = GetWorld()->SpawnActorDeferred<ARogueyObject>(ARogueyObject::StaticClass(), FTransform(SpawnLoc));
					if (Obj) { Obj->ObjectTypeId = CombRow->SpawnObjectTypeId; Obj->FinishSpawning(FTransform(SpawnLoc)); }
				}
			}

			// Output item
			if (!CombRow->OutputItemId.IsNone()) Pawn->TryAddItem({CombRow->OutputItemId, 1});

			// XP
			if (CombRow->XpAmount > 0)
			{
				FRogueyStat& Stat = Pawn->StatPage.Get(CombRow->XpSkill);
				if (Stat.AddXP(CombRow->XpAmount))
					URogueyPassiveRegistry::NotifyLevelUp(Pawn, CombRow->XpSkill, Stat.BaseLevel);
				Pawn->SyncStatPage();
			}

			if (!CombRow->Message.IsEmpty()) Pawn->PostGameMessage(CombRow->Message, RogueyChat::Game);
			break;
		}

		// ── Trade ─────────────────────────────────────────────────────────────────
		case EInvOpType::AddTradeItem:
			if (GM && GM->TradeManager) GM->TradeManager->AddItem(Pawn, Op.SlotA, Op.Quantity);
			break;

		case EInvOpType::RemoveTradeItem:
			if (GM && GM->TradeManager) GM->TradeManager->RemoveItem(Pawn, Op.SlotA);
			break;

		case EInvOpType::AcceptTrade:
			if (GM && GM->TradeManager) GM->TradeManager->AcceptTrade(Pawn);
			break;

		case EInvOpType::CancelTrade:
			if (GM && GM->TradeManager) GM->TradeManager->CancelTrade(Pawn);
			break;

		default: break;
		}
	}
}

void URogueyActionManager::TickStatBuffs()
{
	for (TActorIterator<ARogueyPawn> It(GetWorld()); It; ++It)
	{
		ARogueyPawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn->ActiveStatBuffs.IsEmpty()) continue;

		bool bChanged = false;
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
				bChanged = true;
			}
		}
		if (bChanged) Pawn->SyncStatPage();
	}
}

// ---------------------------------------------------------------------------
// Gather
// ---------------------------------------------------------------------------

void URogueyActionManager::SetGatherAction(ARogueyPawn* Pawn, ARogueyObject* Object, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Object) || !GridManager) return;

	FIntVector2 ObjectTile = GridManager->GetActorTile(Object);
	if (ObjectTile.X == INT32_MIN) return;

	FIntPoint ObjExtent(FMath::Max(1, Object->TileExtent.X), FMath::Max(1, Object->TileExtent.Y));

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::GatherMove;
	Action.TargetActor = Object;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	// Only pathfind when not already adjacent — mirrors SetAttackAction.
	// If already in range, TickGatherMove transitions to Gather on the next tick.
	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false))
	{
		FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false);
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);
	}
}

void URogueyActionManager::TickGatherMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyObject* Object = Cast<ARogueyObject>(Action.TargetActor.Get());
	if (!IsValid(Object)) { Action.Clear(); return; }

	FIntVector2 ObjectTile = GridManager->GetActorTile(Object);
	if (ObjectTile.X == INT32_MIN) { Action.Clear(); return; }

	FIntPoint ObjExtent(FMath::Max(1, Object->TileExtent.X), FMath::Max(1, Object->TileExtent.Y));

	if (IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false))
	{
		MovementManager->CancelMove(Pawn);

		URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this);
		const FRogueyObjectRow* Row = Reg ? Reg->FindObject(Object->ObjectTypeId) : nullptr;

		// Level gate — checked first so we report the skill requirement before the tool requirement
		if (Row && Row->Skill != ERogueyStatType::Hitpoints && Row->RequiredLevel > 1)
		{
			if (Pawn->StatPage.Get(Row->Skill).BaseLevel < Row->RequiredLevel)
			{
				FString Msg = FString::Printf(TEXT("You need level %d %s to do this."),
					Row->RequiredLevel, GatheringSkillName(Row->Skill));
				Pawn->ShowSpeechBubble(Msg);
				Pawn->PostGameMessage(Msg, RogueyChat::Warning);
				Action.Clear();
				return;
			}
		}

		// Tool gate — pawn must have the required tool in inventory or equipment
		if (Row && !Row->RequiredToolItemId.IsNone() && !PawnHasTool(Pawn, Row->RequiredToolItemId))
		{
			URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);
			const FRogueyItemRow* ToolRow = ItemReg ? ItemReg->FindItem(Row->RequiredToolItemId) : nullptr;
			FString ToolName = ToolRow ? ToolRow->DisplayName : Row->RequiredToolItemId.ToString();
			FString Msg = FString::Printf(TEXT("You need a %s to do this."), *ToolName);
			Pawn->ShowSpeechBubble(Msg);
			Pawn->PostGameMessage(Msg, RogueyChat::Warning);
			Action.Clear();
			return;
		}

		Action.Type           = EActionType::Gather;
		Action.TicksRemaining = FMath::Max(1, (Row ? Row->GatherTicks : 4) - Pawn->PassiveGatherSpeedReduction);

		if (Row && Row->Skill != ERogueyStatType::Hitpoints)
			Pawn->ShowSpeechBubble(FString(GatheringSkillName(Row->Skill)) + TEXT("..."));
		return;
	}

	if (!MovementManager->HasPendingMove(Pawn))
	{
		FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false);
		if (BestGoal.X >= 0)
		{
			FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
			if (Path.IsValid())
				MovementManager->RequestMove(Pawn, MoveTemp(Path), Action.bRunning);
		}
		// No reachable adjacent tile right now — keep action alive and retry next tick
	}
}

void URogueyActionManager::TickGather(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyObject* Object = Cast<ARogueyObject>(Action.TargetActor.Get());
	if (!IsValid(Object)) { Action.Clear(); return; }

	FIntVector2 ObjectTile = GridManager->GetActorTile(Object);
	if (ObjectTile.X == INT32_MIN) { Action.Clear(); return; }

	FIntPoint ObjExtent(FMath::Max(1, Object->TileExtent.X), FMath::Max(1, Object->TileExtent.Y));

	// Stepped away — walk back
	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false))
	{
		Action.Type           = EActionType::GatherMove;
		Action.TicksRemaining = 0;
		FIntVector2 BestGoal  = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false);
		if (BestGoal.X >= 0)
		{
			FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
			if (Path.IsValid())
				MovementManager->RequestMove(Pawn, MoveTemp(Path), Action.bRunning);
		}
		return;
	}

	Action.TicksRemaining--;
	if (Action.TicksRemaining > 0) return;

	URogueyObjectRegistry* Reg = URogueyObjectRegistry::Get(this);
	const FRogueyObjectRow* Row = Reg ? Reg->FindObject(Object->ObjectTypeId) : nullptr;

	if (Row)
	{
		// Award XP
		FRogueyStat& Stat = Pawn->StatPage.Get(Row->Skill);
		if (Stat.AddXP(Row->XpPerAction))
		{
			FString LvlMsg = FString::Printf(TEXT("Congratulations, your %s level is now %d!"),
				GatheringSkillName(Row->Skill), Stat.BaseLevel);
			Pawn->ShowSpeechBubble(FString::Printf(TEXT("%s level %d!"), GatheringSkillName(Row->Skill), Stat.BaseLevel));
			Pawn->PostGameMessage(LvlMsg, RogueyChat::LevelUp);
			URogueyPassiveRegistry::NotifyLevelUp(Pawn, Row->Skill, Stat.BaseLevel);
		}
		Pawn->SyncStatPage();

		// Roll loot and give directly to inventory (OSRS-style); drop on ground only if full
		if (!Row->LootTableId.IsNone())
		{
			TArray<FRogueyLootEntry> LootTable = Reg->GetLootEntries(Row->LootTableId);
			int32 TotalWeight = 0;
			for (const FRogueyLootEntry& E : LootTable) TotalWeight += E.Weight;

			if (TotalWeight > 0)
			{
				int32 Roll = FMath::RandRange(1, TotalWeight);
				int32 Accum = 0;
				for (const FRogueyLootEntry& E : LootTable)
				{
					Accum += E.Weight;
					if (Roll <= Accum)
					{
						FRogueyItem LootItem;
						LootItem.ItemId   = E.ItemId;
						LootItem.Quantity = E.Quantity;

						URogueyItemRegistry* ItemReg = URogueyItemRegistry::Get(this);
						const FRogueyItemRow* ItemRow = ItemReg ? ItemReg->FindItem(LootItem.ItemId) : nullptr;
						FString ItemName = ItemRow ? ItemRow->DisplayName : LootItem.ItemId.ToString();

						if (Pawn->TryAddItem(LootItem))
						{
							Pawn->PostGameMessage(FString::Printf(TEXT("You get some %s."), *ItemName), RogueyChat::Game);
						}
						else
						{
							// Inventory full — drop on ground
							ARogueyLootDrop* Drop = GetWorld()->SpawnActor<ARogueyLootDrop>(
								ARogueyLootDrop::StaticClass(), Pawn->GetActorLocation(), FRotator::ZeroRotator);
							if (Drop) Drop->Init(LootItem, Pawn->GetTileCoord());
							Pawn->PostGameMessage(
								FString::Printf(TEXT("Your inventory is too full to hold any more %s."), *ItemName),
								RogueyChat::Warning);
						}
						break;
					}
				}
			}
		}
	}

	// Notify the object of a successful use — decrements UsesRemaining and destroys if exhausted (e.g. chests).
	// Objects with unlimited uses (MaxUses=0, UsesRemaining=-1) ignore this call.
	Object->NotifyUsed();

	// Restart cycle — continuous gathering like OSRS.
	// If Object was just destroyed, the next TickGatherMove will find it invalid and clear the action.
	Action.Type           = EActionType::GatherMove;
	Action.TicksRemaining = 0;
}

// ---------------------------------------------------------------------------
// Skilling — CraftMove + SkillCraft
// ---------------------------------------------------------------------------

void URogueyActionManager::SetCraftAction(ARogueyPawn* Pawn, ARogueyObject* Station, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Station)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::CraftMove;
	Action.TargetActor = Station;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	FIntVector2 StationTile = GridManager->GetActorTile(Station);
	FIntPoint   StationExt(FMath::Max(1, Station->TileExtent.X), FMath::Max(1, Station->TileExtent.Y));
	FIntVector2 Goal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, StationTile, StationExt, 1, false);
	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), Goal, Pawn->TileExtent);
	if (Path.IsValid())
		MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);
}

void URogueyActionManager::TickCraftMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyObject* Station = Cast<ARogueyObject>(Action.TargetActor.Get());
	if (!IsValid(Station)) { Action.Clear(); return; }

	FIntVector2 StationTile = GridManager->GetActorTile(Station);
	if (StationTile.X == INT32_MIN) { Action.Clear(); return; }

	FIntPoint StationExt(FMath::Max(1, Station->TileExtent.X), FMath::Max(1, Station->TileExtent.Y));

	if (IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, StationTile, StationExt, 1, false))
	{
		MovementManager->CancelMove(Pawn);

		URogueySkillRecipeRegistry* RecipeReg = URogueySkillRecipeRegistry::Get(this);
		if (!RecipeReg) { Action.Clear(); return; }

		TArray<FName> Recipes = RecipeReg->GetRecipesForStation(Station->ObjectTypeId);
		if (Recipes.IsEmpty())
		{
			Pawn->PostGameMessage(TEXT("There is nothing to craft here."), RogueyChat::Game);
			Action.Clear();
			return;
		}

		// Open skill menu on the owning client
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController()))
		{
			URogueyObjectRegistry* ObjReg = URogueyObjectRegistry::Get(this);
			const FRogueyObjectRow* Row = ObjReg ? ObjReg->FindObject(Station->ObjectTypeId) : nullptr;
			FString Header = Row ? Row->ObjectName : Station->ObjectTypeId.ToString();
			PC->Client_OpenSkillMenu(Recipes, Header);
		}
		Action.Clear();
		return;
	}

	if (!MovementManager->HasPendingMove(Pawn))
	{
		FIntVector2 Goal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, StationTile, StationExt, 1, false);
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), Goal, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), Action.bRunning);
		else
			Action.Clear();
	}
}

void URogueyActionManager::SetSkillCraftAction(ARogueyPawn* Pawn, FName RecipeId, ARogueyObject* Station)
{
	if (!IsValid(Pawn) || RecipeId.IsNone()) return;

	URogueySkillRecipeRegistry* RecipeReg = URogueySkillRecipeRegistry::Get(this);
	if (!RecipeReg) return;

	const FRogueySkillRecipeRow* Recipe = RecipeReg->FindRecipe(RecipeId);
	if (!Recipe) return;

	// If this is a station recipe and no station was supplied, find an adjacent one
	ARogueyObject* ResolvedStation = Station;
	if (!Recipe->StationTypeId.IsNone() && !IsValid(Station))
	{
		for (TActorIterator<ARogueyObject> It(Pawn->GetWorld()); It; ++It)
		{
			ARogueyObject* Obj = *It;
			if (!IsValid(Obj) || Obj->ObjectTypeId != Recipe->StationTypeId) continue;
			FIntVector2 ObjTile = GridManager->GetActorTile(Obj);
			if (ObjTile.X == INT32_MIN) continue;
			FIntPoint ObjExt(FMath::Max(1, Obj->TileExtent.X), FMath::Max(1, Obj->TileExtent.Y));
			if (IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, ObjTile, ObjExt, 1, false))
			{
				ResolvedStation = Obj;
				break;
			}
		}
		if (!IsValid(ResolvedStation))
		{
			Pawn->PostGameMessage(TEXT("You are not close enough to the crafting station."), RogueyChat::Warning);
			return;
		}
	}

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type           = EActionType::SkillCraft;
	Action.SpellId        = RecipeId; // reuse SpellId field as RecipeId
	Action.TargetActor    = ResolvedStation; // null for inventory-only skills
	Action.TicksRemaining = Recipe->ProcessingTicks;
	PendingActions.Add(Pawn, Action);
}

// Returns true if the pawn has sufficient materials to execute this recipe once.
static bool PawnHasRecipeMaterials(const ARogueyPawn* Pawn, const FRogueySkillRecipeRow& Recipe)
{
	// Tool check (not consumed, must exist)
	if (!Recipe.ToolItemId.IsNone())
	{
		bool bHasTool = false;
		for (const FRogueyItem& Slot : Pawn->Inventory)
			if (!Slot.IsEmpty() && Slot.ItemId == Recipe.ToolItemId) { bHasTool = true; break; }
		if (!bHasTool)
		{
			for (const auto& E : Pawn->Equipment)
				if (!E.Value.IsEmpty() && E.Value.ItemId == Recipe.ToolItemId) { bHasTool = true; break; }
		}
		if (!bHasTool) return false;
	}

	// Input 1
	if (!Recipe.InputItem1Id.IsNone() && Recipe.InputItem1Qty > 0)
	{
		int32 Count = 0;
		for (const FRogueyItem& Slot : Pawn->Inventory)
			if (!Slot.IsEmpty() && Slot.ItemId == Recipe.InputItem1Id) { Count += Slot.Quantity; }
		if (Count < Recipe.InputItem1Qty) return false;
	}

	// Input 2
	if (!Recipe.InputItem2Id.IsNone() && Recipe.InputItem2Qty > 0)
	{
		int32 Count = 0;
		for (const FRogueyItem& Slot : Pawn->Inventory)
			if (!Slot.IsEmpty() && Slot.ItemId == Recipe.InputItem2Id) { Count += Slot.Quantity; }
		if (Count < Recipe.InputItem2Qty) return false;
	}

	return true;
}

void URogueyActionManager::TickSkillCraft(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	URogueySkillRecipeRegistry* RecipeReg = URogueySkillRecipeRegistry::Get(this);
	if (!RecipeReg) { Action.Clear(); return; }

	const FRogueySkillRecipeRow* Recipe = RecipeReg->FindRecipe(Action.SpellId);
	if (!Recipe) { Action.Clear(); return; }

	// Station check — if a station is required, must stay adjacent
	ARogueyObject* Station = Cast<ARogueyObject>(Action.TargetActor.Get());
	if (!Recipe->StationTypeId.IsNone())
	{
		if (!IsValid(Station)) { Action.Clear(); return; }

		FIntVector2 StationTile = GridManager->GetActorTile(Station);
		FIntPoint   StationExt(FMath::Max(1, Station->TileExtent.X), FMath::Max(1, Station->TileExtent.Y));
		if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, StationTile, StationExt, 1, false))
		{
			Action.Clear();
			return;
		}
	}

	// Level gate
	if (Recipe->LevelRequired > 1)
	{
		if (Pawn->StatPage.Get(Recipe->Skill).BaseLevel < Recipe->LevelRequired)
		{
			Pawn->PostGameMessage(
				FString::Printf(TEXT("You need level %d to craft this."), Recipe->LevelRequired),
				RogueyChat::Warning);
			Action.Clear();
			return;
		}
	}

	// Material check — cancel if out of inputs
	if (!PawnHasRecipeMaterials(Pawn, *Recipe))
	{
		Pawn->PostGameMessage(TEXT("You have run out of materials."), RogueyChat::Game);
		Action.Clear();
		return;
	}

	Action.TicksRemaining--;
	if (Action.TicksRemaining > 0) return;

	// --- Execute the craft ---

	// Consume input 1
	if (!Recipe->InputItem1Id.IsNone() && Recipe->InputItem1Qty > 0)
	{
		int32 ToRemove = Recipe->InputItem1Qty;
		for (FRogueyItem& Slot : Pawn->Inventory)
		{
			if (Slot.IsEmpty() || Slot.ItemId != Recipe->InputItem1Id) continue;
			int32 Take = FMath::Min(ToRemove, Slot.Quantity);
			Slot.Quantity -= Take;
			if (Slot.Quantity <= 0) Slot = FRogueyItem();
			ToRemove -= Take;
			if (ToRemove <= 0) break;
		}
	}

	// Consume input 2
	if (!Recipe->InputItem2Id.IsNone() && Recipe->InputItem2Qty > 0)
	{
		int32 ToRemove = Recipe->InputItem2Qty;
		for (FRogueyItem& Slot : Pawn->Inventory)
		{
			if (Slot.IsEmpty() || Slot.ItemId != Recipe->InputItem2Id) continue;
			int32 Take = FMath::Min(ToRemove, Slot.Quantity);
			Slot.Quantity -= Take;
			if (Slot.Quantity <= 0) Slot = FRogueyItem();
			ToRemove -= Take;
			if (ToRemove <= 0) break;
		}
	}

	// Give output
	if (!Recipe->OutputItemId.IsNone())
	{
		FRogueyItem Out;
		Out.ItemId   = Recipe->OutputItemId;
		Out.Quantity = FMath::Max(1, Recipe->OutputQty);
		if (!Pawn->TryAddItem(Out))
		{
			ARogueyLootDrop* Drop = Pawn->GetWorld()->SpawnActor<ARogueyLootDrop>(
				ARogueyLootDrop::StaticClass(), Pawn->GetActorLocation(), FRotator::ZeroRotator);
			if (Drop) Drop->Init(Out, Pawn->GetTileCoord());
			Pawn->PostGameMessage(TEXT("Your inventory is too full — item dropped on ground."), RogueyChat::Warning);
		}
	}

	// Grant XP
	if (Recipe->XpAmount > 0)
	{
		FRogueyStat& Stat = Pawn->StatPage.Get(Recipe->Skill);
		if (Stat.AddXP(Recipe->XpAmount))
		{
			Pawn->ShowSpeechBubble(FString::Printf(TEXT("Skill level %d!"), Stat.BaseLevel));
			URogueyPassiveRegistry::NotifyLevelUp(Pawn, Recipe->Skill, Stat.BaseLevel);
		}
		Pawn->SyncStatPage();
	}

	// Try another cycle
	if (PawnHasRecipeMaterials(Pawn, *Recipe))
		Action.TicksRemaining = Recipe->ProcessingTicks;
	else
		Action.Clear();
}

// ---------------------------------------------------------------------------
// Portal enter
// ---------------------------------------------------------------------------

static FIntVector2 PortalTileFromActor(const ARogueyPortal* Portal)
{
	FVector Loc = Portal->GetActorLocation();
	return FIntVector2(
		FMath::FloorToInt(Loc.X / RogueyConstants::TileSize),
		FMath::FloorToInt(Loc.Y / RogueyConstants::TileSize));
}

void URogueyActionManager::SetEnterPortalAction(ARogueyPawn* Pawn, ARogueyPortal* Portal)
{
	if (!IsValid(Pawn) || !IsValid(Portal)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::EnterMove;
	Action.TargetActor = Portal;
	PendingActions.Add(Pawn, Action);

	FIntVector2 PortalTile = PortalTileFromActor(Portal);
	FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, PortalTile, FIntPoint(1, 1), 1, false);
	FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
	if (Path.IsValid())
		MovementManager->RequestMove(Pawn, MoveTemp(Path), true);
}

void URogueyActionManager::TickEnterMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPortal* Portal = Cast<ARogueyPortal>(Action.TargetActor.Get());
	if (!IsValid(Portal)) { Action.Clear(); return; }

	FIntVector2 PortalTile = PortalTileFromActor(Portal);

	if (IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, PortalTile, FIntPoint(1, 1), 1, false))
	{
		MovementManager->CancelMove(Pawn);
		Action.Clear();
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController()))
			PC->Client_ShowLoading();
		Portal->TryEnter(Pawn);
		return;
	}

	if (!MovementManager->HasPendingMove(Pawn))
	{
		FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, PortalTile, FIntPoint(1, 1), 1, false);
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), true);
		else
			Action.Clear();
	}
}

// ---------------------------------------------------------------------------
// Follow
// ---------------------------------------------------------------------------

void URogueyActionManager::SetFollowAction(ARogueyPawn* Pawn, ARogueyPawn* Target)
{
	if (!IsValid(Pawn) || !IsValid(Target) || Pawn == Target) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::FollowMove;
	Action.TargetActor = Target;
	PendingActions.Add(Pawn, Action);

	if (!IsAdjacent(Pawn, Target))
		MoveToAdjacentTarget(Pawn, Target, true);
}

void URogueyActionManager::TickFollowMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Cast<ARogueyPawn>(Action.TargetActor.Get());
	if (!ARogueyPawn::IsAlive(Target))
	{
		MovementManager->CancelMove(Pawn);
		Pawn->SetPawnState(EPawnState::Idle);
		Action.Clear();
		return;
	}

	// When adjacent, stop moving and wait — the action stays active so if target
	// moves again on a later tick we'll re-path below.
	if (IsAdjacent(Pawn, Target))
	{
		MovementManager->CancelMove(Pawn);
		return;
	}

	// Re-path when target moved or current path was exhausted
	FIntVector2 TargetTile = Target->GetTileCoord();
	if (Action.LastKnownTargetTile != TargetTile || !MovementManager->HasPendingMove(Pawn))
	{
		Action.LastKnownTargetTile = TargetTile;
		MoveToAdjacentTarget(Pawn, Target, true);
	}
}

// ---------------------------------------------------------------------------
// Bank
// ---------------------------------------------------------------------------

void URogueyActionManager::SetBankAction(ARogueyPawn* Pawn, ARogueyBankObject* Bank, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Bank)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::BankMove;
	Action.TargetActor = Bank;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	FIntVector2 BankTile = GridManager->WorldToTile(Bank->GetActorLocation());
	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, BankTile, FIntPoint(Bank->TileExtent.X, Bank->TileExtent.Y), 1, false))
	{
		FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, BankTile, FIntPoint(Bank->TileExtent.X, Bank->TileExtent.Y), 1, false);
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);
	}
}

void URogueyActionManager::TickBankMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyBankObject* Bank = Cast<ARogueyBankObject>(Action.TargetActor.Get());
	if (!IsValid(Bank))
	{
		MovementManager->CancelMove(Pawn);
		Action.Clear();
		return;
	}

	FIntVector2 BankTile = GridManager->WorldToTile(Bank->GetActorLocation());
	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, BankTile, FIntPoint(Bank->TileExtent.X, Bank->TileExtent.Y), 1, false))
	{
		if (!MovementManager->HasPendingMove(Pawn))
		{
			FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, BankTile, FIntPoint(Bank->TileExtent.X, Bank->TileExtent.Y), 1, false);
			FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
			if (Path.IsValid())
				MovementManager->RequestMove(Pawn, MoveTemp(Path), Action.bRunning);
		}
		return;
	}

	// Adjacent — push bank contents to client and open panel
	MovementManager->CancelMove(Pawn);
	Action.Clear();

	ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController());
	if (!PC) { UE_LOG(LogTemp, Warning, TEXT("TickBankMove: no PlayerController on pawn")); return; }

	URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>();
	if (!GI) { UE_LOG(LogTemp, Warning, TEXT("TickBankMove: GameInstance is not URogueyGameInstance")); return; }

	UE_LOG(LogTemp, Log, TEXT("TickBankMove: sending Client_OpenBank to player %s"), *URogueyGameInstance::GetPlayerKey(PC));
	FString PlayerId = URogueyGameInstance::GetPlayerKey(PC);
	TArray<FRogueyItem> BankContents = GI->GetOrCreateBank(PlayerId);
	PC->Client_OpenBank(BankContents);
}

// ---------------------------------------------------------------------------
// Bank via NPC (banker)
// ---------------------------------------------------------------------------

void URogueyActionManager::SetBankViaNpcAction(ARogueyPawn* Pawn, ARogueyNpc* Npc, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Npc)) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::BankViaNpcMove;
	Action.TargetActor = Npc;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	if (!IsAdjacent(Pawn, Npc))
		MoveToAdjacentTarget(Pawn, Npc, bRunning);
}

void URogueyActionManager::TickBankViaNpcMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyPawn* Target = Cast<ARogueyPawn>(Action.TargetActor.Get());
	if (!IsValid(Target))
	{
		MovementManager->CancelMove(Pawn);
		Action.Clear();
		return;
	}

	if (!IsAdjacent(Pawn, Target))
	{
		FIntVector2 TargetCurrentTile = Target->GetTileCoord();
		if (Action.LastKnownTargetTile != TargetCurrentTile || !MovementManager->HasPendingMove(Pawn))
		{
			Action.LastKnownTargetTile = TargetCurrentTile;
			MoveToAdjacentTarget(Pawn, Target, Action.bRunning);
		}
		return;
	}

	// Adjacent — open bank on the owning client
	MovementManager->CancelMove(Pawn);
	Action.Clear();

	ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController());
	if (!PC) { UE_LOG(LogTemp, Warning, TEXT("TickBankViaNpcMove: no PlayerController on pawn")); return; }

	URogueyGameInstance* GI = GetWorld()->GetGameInstance<URogueyGameInstance>();
	if (!GI) { UE_LOG(LogTemp, Warning, TEXT("TickBankViaNpcMove: GameInstance is not URogueyGameInstance")); return; }

	UE_LOG(LogTemp, Log, TEXT("TickBankViaNpcMove: opening bank for player %s"), *URogueyGameInstance::GetPlayerKey(PC));
	FString PlayerId = URogueyGameInstance::GetPlayerKey(PC);
	TArray<FRogueyItem> BankContents = GI->GetOrCreateBank(PlayerId);
	PC->Client_OpenBank(BankContents);
}

// ---------------------------------------------------------------------------
// Use item on actor (NPC or world object)
// ---------------------------------------------------------------------------

void URogueyActionManager::SetUseOnActorAction(ARogueyPawn* Pawn, AActor* Target, int32 ItemSlotA, bool bRunning)
{
	if (!IsValid(Pawn) || !IsValid(Target)) return;
	if (!Pawn->Inventory.IsValidIndex(ItemSlotA) || Pawn->Inventory[ItemSlotA].IsEmpty()) return;

	ClearAction(Pawn);

	FRogueyPendingAction Action;
	Action.Type        = EActionType::UseOnActorMove;
	Action.TargetActor = Target;
	Action.ItemSlotA   = ItemSlotA;
	Action.bRunning    = bRunning;
	PendingActions.Add(Pawn, Action);

	// Resolve target tile and start pathing
	FIntVector2 TargetTile(-1, -1);
	FIntPoint   TargetExtent(1, 1);

	if (const ARogueyPawn* TargetPawn = Cast<ARogueyPawn>(Target))
	{
		TargetTile   = TargetPawn->GetTileCoord();
		TargetExtent = TargetPawn->TileExtent;
	}
	else if (GridManager)
	{
		TargetTile = GridManager->GetActorTile(Target);
		if (const ARogueyObject* Obj = Cast<ARogueyObject>(Target))
			TargetExtent = FIntPoint(FMath::Max(1, Obj->TileExtent.X), FMath::Max(1, Obj->TileExtent.Y));
	}

	if (TargetTile.X == INT32_MIN) return;

	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, TargetTile, TargetExtent, 1, false))
	{
		FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, TargetTile, TargetExtent, 1, false);
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), bRunning);
	}
}

void URogueyActionManager::TickUseOnActorMove(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	AActor* Target = Action.TargetActor.Get();
	if (!IsValid(Target)) { Action.Clear(); return; }

	// Validate the item is still in the slot
	const int32 Slot = Action.ItemSlotA;
	if (!Pawn->Inventory.IsValidIndex(Slot) || Pawn->Inventory[Slot].IsEmpty()) { Action.Clear(); return; }

	// Resolve target tile and extent
	FIntVector2 TargetTile(-1, -1);
	FIntPoint   TargetExtent(1, 1);

	if (const ARogueyPawn* TargetPawn = Cast<ARogueyPawn>(Target))
	{
		TargetTile   = TargetPawn->GetTileCoord();
		TargetExtent = TargetPawn->TileExtent;
	}
	else if (GridManager)
	{
		TargetTile = GridManager->GetActorTile(Target);
		if (const ARogueyObject* Obj = Cast<ARogueyObject>(Target))
			TargetExtent = FIntPoint(FMath::Max(1, Obj->TileExtent.X), FMath::Max(1, Obj->TileExtent.Y));
	}

	if (TargetTile.X == INT32_MIN) { Action.Clear(); return; }

	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, TargetTile, TargetExtent, 1, false))
	{
		FIntVector2 TargetCurrentTile = TargetTile;
		if (Action.LastKnownTargetTile != TargetCurrentTile || !MovementManager->HasPendingMove(Pawn))
		{
			Action.LastKnownTargetTile = TargetCurrentTile;
			FIntVector2 BestGoal = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, TargetTile, TargetExtent, 1, false);
			FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
			if (Path.IsValid())
				MovementManager->RequestMove(Pawn, MoveTemp(Path), true);
			else
				Action.Clear();
		}
		return;
	}

	// Adjacent — give NPC a chance to consume the item before the combination table runs.
	// This must happen before the CombReg null check so boss offerings work even when
	// DT_UseCombinations is not configured.
	FName  ItemAId  = Pawn->Inventory[Slot].ItemId;
	int32  ItemQty  = Pawn->Inventory[Slot].Quantity;
	if (ARogueyNpc* TargetNpc = Cast<ARogueyNpc>(Target))
	{
		if (TargetNpc->OnItemOffered(ItemAId, ItemQty, Pawn))
		{
			MovementManager->CancelMove(Pawn);
			Pawn->Inventory[Slot] = FRogueyItem();
			Action.Clear();
			return;
		}
	}

	URogueyUseCombinationRegistry* CombReg = URogueyUseCombinationRegistry::Get(Pawn);
	if (!CombReg)
	{
		Action.Clear();
		Pawn->PostGameMessage(TEXT("Nothing interesting happens."), RogueyChat::Game);
		return;
	}

	const FRogueyUseCombinationRow* Row = nullptr;

	if (const ARogueyNpc* Npc = Cast<ARogueyNpc>(Target))
		Row = CombReg->FindItemOnNpc(ItemAId, Npc->NpcTypeId);
	else if (const ARogueyObject* Obj = Cast<ARogueyObject>(Target))
		Row = CombReg->FindItemOnObject(ItemAId, Obj->ObjectTypeId);

	if (!Row)
	{
		Action.Clear();
		Pawn->PostGameMessage(TEXT("Nothing interesting happens."), RogueyChat::Game);
		return;
	}

	if (Row->RequiredLevel > 0)
	{
		const FRogueyStat* Stat = Pawn->StatPage.Stats.Find(Row->RequiredSkill);
		if (!Stat || Stat->CurrentLevel < Row->RequiredLevel)
		{
			Action.Clear();
			Pawn->PostGameMessage(Row->SkillFailMessage.IsEmpty()
				? TEXT("You need a higher level.")
				: Row->SkillFailMessage, RogueyChat::Warning);
			return;
		}
	}

	// Timed interaction — transition to countdown state
	if (Row->ProcessingTicks > 0)
	{
		MovementManager->CancelMove(Pawn);
		Action.Type           = EActionType::UseOnObject;
		Action.TicksRemaining = Row->ProcessingTicks;
		Pawn->ShowSpeechBubble(TEXT("..."));
		return;
	}

	// Instant interaction
	MovementManager->CancelMove(Pawn);
	Action.Clear();

	if (Row->bConsumeA)
		Pawn->Inventory[Slot] = FRogueyItem();

	for (const FRogueyUseCombinationResult& Result : Row->ResultItems)
		Pawn->TryAddItem({Result.ResultItemId, Result.Quantity});

	if (Row->XpAmount > 0)
	{
		FRogueyStat& XpStat = Pawn->StatPage.Get(Row->XpSkill);
		if (XpStat.AddXP(Row->XpAmount))
		{
			Pawn->PostGameMessage(FString::Printf(TEXT("Congratulations, your %s level is now %d!"),
			    *UEnum::GetValueAsString(Row->XpSkill), XpStat.BaseLevel), RogueyChat::LevelUp);
			URogueyPassiveRegistry::NotifyLevelUp(Pawn, Row->XpSkill, XpStat.BaseLevel);
		}
		Pawn->SyncStatPage();
	}

	if (!Row->DialogueTriggerNodeId.IsNone())
	{
		if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(Pawn->GetController()))
			PC->Client_OpenDialogue(Row->DialogueTriggerNodeId, TEXT(""));
	}
}

void URogueyActionManager::TickUseOnObject(ARogueyPawn* Pawn, FRogueyPendingAction& Action, int32 TickIndex)
{
	ARogueyObject* Object = Cast<ARogueyObject>(Action.TargetActor.Get());
	if (!IsValid(Object)) { Action.Clear(); return; }

	const int32 Slot = Action.ItemSlotA;
	if (!Pawn->Inventory.IsValidIndex(Slot) || Pawn->Inventory[Slot].IsEmpty()) { Action.Clear(); return; }

	FIntVector2 ObjectTile = GridManager->GetActorTile(Object);
	if (ObjectTile.X == INT32_MIN) { Action.Clear(); return; }

	FIntPoint ObjExtent(FMath::Max(1, Object->TileExtent.X), FMath::Max(1, Object->TileExtent.Y));

	// Stepped away — walk back
	if (!IsInAttackRange(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false))
	{
		Action.Type           = EActionType::UseOnActorMove;
		Action.TicksRemaining = 0;
		FIntVector2 BestGoal  = FindBestAttackTile(Pawn->GetTileCoord(), Pawn->TileExtent, ObjectTile, ObjExtent, 1, false);
		FRogueyPath Path = RogueyPathfinder::FindPath(GridManager, Pawn->GetTileCoord(), BestGoal, Pawn->TileExtent);
		if (Path.IsValid())
			MovementManager->RequestMove(Pawn, MoveTemp(Path), Action.bRunning);
		return;
	}

	Action.TicksRemaining--;
	if (Action.TicksRemaining > 0) return;

	// Countdown complete — yield result
	URogueyUseCombinationRegistry* CombReg = URogueyUseCombinationRegistry::Get(Pawn);
	FName ItemAId = Pawn->Inventory[Slot].ItemId;
	const FRogueyUseCombinationRow* Row = CombReg ? CombReg->FindItemOnObject(ItemAId, Object->ObjectTypeId) : nullptr;

	if (!Row) { Action.Clear(); return; }

	if (Row->bConsumeA)
		Pawn->Inventory[Slot] = FRogueyItem();

	for (const FRogueyUseCombinationResult& Result : Row->ResultItems)
	{
		FRogueyItem Item{Result.ResultItemId, Result.Quantity};
		const FRogueyItemRow* ItemRow = URogueyItemRegistry::Get(Pawn) ? URogueyItemRegistry::Get(Pawn)->FindItem(Item.ItemId) : nullptr;
		FString Name = ItemRow ? ItemRow->DisplayName : Item.ItemId.ToString();
		if (Pawn->TryAddItem(Item))
			Pawn->PostGameMessage(FString::Printf(TEXT("You cook the %s."), *Name), RogueyChat::Game);
		else
			Pawn->PostGameMessage(TEXT("Your inventory is too full."), RogueyChat::Warning);
	}

	if (Row->XpAmount > 0)
	{
		FRogueyStat& XpStat = Pawn->StatPage.Get(Row->XpSkill);
		if (XpStat.AddXP(Row->XpAmount))
		{
			Pawn->PostGameMessage(FString::Printf(TEXT("Congratulations, your Cooking level is now %d!"), XpStat.BaseLevel),
			                      RogueyChat::LevelUp);
			URogueyPassiveRegistry::NotifyLevelUp(Pawn, Row->XpSkill, XpStat.BaseLevel);
		}
		Pawn->SyncStatPage();
	}

	// Notify the fire pit of the use (decrements use count, may destroy it)
	Object->NotifyUsed();

	Action.Clear(); // cooking is one-shot, not auto-repeating
}
