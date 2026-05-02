#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "Roguey/Core/RogueyDeathManager.h"
#include "Roguey/Core/RogueyActionManager.h"
#include "Roguey/Core/RogueyPawnState.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/RogueyCharacter.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEATH_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForDeath()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

// Creates a DeathManager whose GetWorld() resolves to the editor world.
// The outer must be a UObject that reports the editor world — using the world itself.
struct FDeathTestEnv
{
	URogueyGridManager*   Grid    = nullptr;
	URogueyActionManager* Action  = nullptr;
	URogueyDeathManager*  Death   = nullptr;

	FDeathTestEnv(UWorld* World)
	{
		Grid   = NewObject<URogueyGridManager>(World);
		Grid->Init(10, 10);

		Action = NewObject<URogueyActionManager>(World);
		Action->Init(Grid, nullptr, nullptr);

		Death  = NewObject<URogueyDeathManager>(World);
		Death->Init(Grid, Action);
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Dead NPC is destroyed on the next tick
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeath_DeadNpc_DestroyedOnTick,
	"Roguey.Death.Npc.DeadNpcDestroyedOnTick", DEATH_TEST_FLAGS)
bool FDeath_DeadNpc_DestroyedOnTick::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForDeath();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FDeathTestEnv Env(World);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyNpc* Npc = World->SpawnActor<ARogueyNpc>(ARogueyNpc::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("NPC spawned", Npc)) return false;

	Npc->SetPawnState(EPawnState::Dead);
	TestTrue("NPC reports dead before tick", Npc->IsDead());

	// After ticking DeathManager the NPC should be queued for destruction.
	// In editor world IsValid() returns false once Destroy() is called.
	Env.Death->RogueyTick(1);

	TestFalse("NPC invalid after death tick", IsValid(Npc));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Living NPC is NOT destroyed on tick
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeath_AliveNpc_NotDestroyedOnTick,
	"Roguey.Death.Npc.AliveNpcNotDestroyedOnTick", DEATH_TEST_FLAGS)
bool FDeath_AliveNpc_NotDestroyedOnTick::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForDeath();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FDeathTestEnv Env(World);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyNpc* Npc = World->SpawnActor<ARogueyNpc>(ARogueyNpc::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("NPC spawned", Npc)) return false;

	// NPC is alive (default PawnState=Idle)
	Env.Death->RogueyTick(1);

	TestTrue("Living NPC still valid after death tick", IsValid(Npc));

	Npc->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Dead player loses inventory and equipment; stats are preserved
// Note: TriggerGameOver is a no-op in editor (no AuthGameMode) — this is fine.
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeath_DeadPlayer_ClearsInventoryAndEquipment,
	"Roguey.Death.Player.ClearsInventoryAndEquipment", DEATH_TEST_FLAGS)
bool FDeath_DeadPlayer_ClearsInventoryAndEquipment::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForDeath();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FDeathTestEnv Env(World);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Player = World->SpawnActor<ARogueyCharacter>(
		ARogueyCharacter::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("Player spawned", Player)) return false;

	// Give the player some inventory and equipment
	if (Player->Inventory.Num() == 0)
		Player->Inventory.Init(FRogueyItem(), 28);

	FRogueyItem Sword;
	Sword.ItemId   = FName("iron_sword");
	Sword.Quantity = 1;
	Player->Inventory[0] = Sword;
	Player->Equipment.Add(EEquipmentSlot::Weapon, Sword);

	// Set stat so we can verify it is NOT cleared
	Player->StatPage.Get(ERogueyStatType::Strength).BaseLevel = 5;

	Player->SetPawnState(EPawnState::Dead);
	Player->CurrentHP = 0;

	Env.Death->RogueyTick(1);

	TestTrue ("Inventory slot 0 cleared",    Player->Inventory[0].IsEmpty());
	TestTrue ("Equipment map emptied",        Player->Equipment.IsEmpty());
	TestEqual("Strength BaseLevel preserved",  Player->StatPage.Get(ERogueyStatType::Strength).BaseLevel, 5);

	Player->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Single dead player with one living player does NOT trigger game-over path
// (verified by player actor remaining valid — game-over only destroys world
//  in a real map transition, not in-place)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeath_OneSurvivor_NoGameOver,
	"Roguey.Death.Player.OneSurvivor_NoGameOver", DEATH_TEST_FLAGS)
bool FDeath_OneSurvivor_NoGameOver::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForDeath();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FDeathTestEnv Env(World);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyCharacter* Dead  = World->SpawnActor<ARogueyCharacter>(ARogueyCharacter::StaticClass(), FTransform::Identity, P);
	ARogueyCharacter* Alive = World->SpawnActor<ARogueyCharacter>(ARogueyCharacter::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("Dead player spawned", Dead) || !TestNotNull("Alive player spawned", Alive))
	{
		if (Dead)  Dead->Destroy();
		if (Alive) Alive->Destroy();
		return false;
	}

	if (Dead->Inventory.Num() == 0)  Dead->Inventory.Init(FRogueyItem(), 28);
	if (Alive->Inventory.Num() == 0) Alive->Inventory.Init(FRogueyItem(), 28);

	Dead->SetPawnState(EPawnState::Dead);
	Dead->CurrentHP = 0;
	// Alive stays Idle

	Env.Death->RogueyTick(1);

	// Alive player must be unaffected
	TestTrue ("Alive player still valid after tick", IsValid(Alive));
	TestFalse("Alive player is not in Dead state",   Alive->IsDead());

	if (IsValid(Alive)) Alive->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
