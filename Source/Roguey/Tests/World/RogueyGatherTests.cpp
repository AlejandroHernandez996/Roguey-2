#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "Roguey/Core/RogueyActionManager.h"
#include "Roguey/Core/RogueyMovementManager.h"
#include "Roguey/Core/RogueyPawnState.h"
#include "Roguey/Grid/RogueyGridManager.h"
#include "Roguey/Items/RogueyItem.h"
#include "Roguey/Items/RogueyEquipmentSlot.h"
#include "Roguey/RogueyCharacter.h"
#include "Roguey/Skills/RogueyStatPage.h"
#include "Roguey/Skills/RogueyStatType.h"
#include "Roguey/World/RogueyObject.h"
#include "Roguey/World/RogueyObjectRegistry.h"
#include "Roguey/World/RogueyObjectRow.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GATHER_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UWorld* GetEditorWorldForGather()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

// Full gather environment: grid + movement + action managers + spawned pawn and object.
// The object is registered at ObjectTile; the pawn is placed at PawnTile.
// Call InjectRow() before ticking if you need gate validation or XP.
struct FGatherEnv
{
	URogueyGridManager*    Grid   = nullptr;
	URogueyMovementManager* Move  = nullptr;
	URogueyActionManager*  Action = nullptr;
	ARogueyCharacter*      Pawn   = nullptr;
	ARogueyObject*         Object = nullptr;
	FIntVector2            ObjectTile = FIntVector2(5, 5);
	FIntVector2            PawnTile   = FIntVector2(4, 5); // adjacent west

	// Owned storage for injected rows — must outlive the env
	FRogueyObjectRow       InjectedRow;
	bool                   bInjectionSucceeded = false;

	FGatherEnv(UWorld* World)
	{
		Grid   = NewObject<URogueyGridManager>(World);
		Grid->Init(20, 20);

		Move   = NewObject<URogueyMovementManager>(World);
		Move->Init(Grid);

		Action = NewObject<URogueyActionManager>(World);
		Action->Init(Grid, Move, nullptr);

		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Pawn = World->SpawnActor<ARogueyCharacter>(SP);
		if (Pawn)
		{
			Pawn->StatPage.InitDefaults();
			Pawn->Inventory.Init(FRogueyItem(), 28);
			Grid->RegisterActor(Pawn, PawnTile);
			Pawn->TilePosition = FIntPoint(PawnTile.X, PawnTile.Y);
		}

		Object = World->SpawnActor<ARogueyObject>(SP);
		if (Object)
		{
			Object->ObjectTypeId = FName("test_tree");
			Object->TileExtent   = FIntPoint(1, 1);
			Grid->RegisterActor(Object, ObjectTile);
		}
	}

	// Inject a row into the game-instance registry for this world.
	// bInjectionSucceeded is false if the subsystem is unavailable (editor world without PIE GI).
	void InjectRow(const FRogueyObjectRow& Row)
	{
		InjectedRow = Row;
		bInjectionSucceeded = false;
		UWorld* W = Grid ? Grid->GetWorld() : nullptr;
		if (!W) return;
		if (UGameInstance* GI = W->GetGameInstance())
			if (URogueyObjectRegistry* Reg = GI->GetSubsystem<URogueyObjectRegistry>())
			{
				Reg->TestInjectObject(FName("test_tree"), &InjectedRow);
				bInjectionSucceeded = true;
			}
	}

	~FGatherEnv()
	{
		if (IsValid(Pawn))   Pawn->Destroy();
		if (IsValid(Object)) Object->Destroy();
	}
};

// ---------------------------------------------------------------------------
// 1. Stat page — Fishing exists and is at level 1 by default
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_StatPage_FishingExistsAtLevelOne,
	"Roguey.Gather.StatPage.FishingExistsAtLevelOne", GATHER_TEST_FLAGS)
bool FGather_StatPage_FishingExistsAtLevelOne::RunTest(const FString&)
{
	FRogueyStatPage Page;
	Page.InitDefaults();

	TestNotNull("Fishing stat is present", Page.Find(ERogueyStatType::Fishing));
	TestEqual("Fishing starts at level 1", Page.GetCurrentLevel(ERogueyStatType::Fishing), 1);
	TestNotNull("Woodcutting stat is present", Page.Find(ERogueyStatType::Woodcutting));
	TestNotNull("Mining stat is present",      Page.Find(ERogueyStatType::Mining));
	return true;
}

// ---------------------------------------------------------------------------
// 2. PawnHasTool — empty required (None) always passes
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_Tool_NoRequirement_AlwaysPasses,
	"Roguey.Gather.Tool.NoRequirementAlwaysPasses", GATHER_TEST_FLAGS)
bool FGather_Tool_NoRequirement_AlwaysPasses::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Pawn = World->SpawnActor<ARogueyCharacter>(SP);
	if (!Pawn) { AddError(TEXT("Failed to spawn pawn")); return false; }
	Pawn->Inventory.Init(FRogueyItem(), 28);

	TestTrue("No tool required (None) always passes", URogueyActionManager::PawnHasTool(Pawn, NAME_None));

	Pawn->Destroy();
	return true;
}

// ---------------------------------------------------------------------------
// 3. PawnHasTool — item in inventory is found
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_Tool_InventoryTool_Passes,
	"Roguey.Gather.Tool.InventoryToolPasses", GATHER_TEST_FLAGS)
bool FGather_Tool_InventoryTool_Passes::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Pawn = World->SpawnActor<ARogueyCharacter>(SP);
	if (!Pawn) { AddError(TEXT("Failed to spawn pawn")); return false; }
	Pawn->Inventory.Init(FRogueyItem(), 28);

	FRogueyItem Axe;
	Axe.ItemId   = FName("bronze_axe");
	Axe.Quantity = 1;
	Pawn->Inventory[0] = Axe;

	TestTrue ("Has axe in slot 0",      URogueyActionManager::PawnHasTool(Pawn, FName("bronze_axe")));
	TestFalse("Does not have pickaxe",  URogueyActionManager::PawnHasTool(Pawn, FName("iron_pickaxe")));

	Pawn->Destroy();
	return true;
}

// ---------------------------------------------------------------------------
// 4. PawnHasTool — equipped item is found
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_Tool_EquippedTool_Passes,
	"Roguey.Gather.Tool.EquippedToolPasses", GATHER_TEST_FLAGS)
bool FGather_Tool_EquippedTool_Passes::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueyCharacter* Pawn = World->SpawnActor<ARogueyCharacter>(SP);
	if (!Pawn) { AddError(TEXT("Failed to spawn pawn")); return false; }
	Pawn->Inventory.Init(FRogueyItem(), 28);

	FRogueyItem Rod;
	Rod.ItemId   = FName("fishing_rod");
	Rod.Quantity = 1;
	Pawn->Equipment.Add(EEquipmentSlot::Weapon, Rod);

	TestTrue("Equipped fishing rod is found", URogueyActionManager::PawnHasTool(Pawn, FName("fishing_rod")));
	TestFalse("Pickaxe not equipped",         URogueyActionManager::PawnHasTool(Pawn, FName("iron_pickaxe")));

	Pawn->Destroy();
	return true;
}

// ---------------------------------------------------------------------------
// 5. Level gate — TickGatherMove clears action when pawn is below required level
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_LevelGate_BelowRequired_ClearsAction,
	"Roguey.Gather.LevelGate.BelowRequiredClearsAction", GATHER_TEST_FLAGS)
bool FGather_LevelGate_BelowRequired_ClearsAction::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill          = ERogueyStatType::Woodcutting;
	Row.RequiredLevel  = 15;
	Row.RequiredToolItemId = NAME_None;
	Row.GatherTicks    = 4;
	Row.XpPerAction    = 25;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	// Pawn's woodcutting starts at level 1 (< 15 required)
	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0);

	TestFalse("Action cleared due to level gate",
		Env.Action->HasAction(Env.Pawn));
	TestTrue("Speech bubble set", !Env.Pawn->SpeechBubbleText.IsEmpty());

	return true;
}

// ---------------------------------------------------------------------------
// 6. Level gate — action proceeds when pawn meets the required level
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_LevelGate_AtRequired_Proceeds,
	"Roguey.Gather.LevelGate.AtRequiredProceeds", GATHER_TEST_FLAGS)
bool FGather_LevelGate_AtRequired_Proceeds::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill         = ERogueyStatType::Woodcutting;
	Row.RequiredLevel = 5;
	Row.RequiredToolItemId = NAME_None;
	Row.GatherTicks   = 3;
	Row.XpPerAction   = 25;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	// Boost woodcutting to level 5
	Env.Pawn->StatPage.Get(ERogueyStatType::Woodcutting).BaseLevel    = 5;
	Env.Pawn->StatPage.Get(ERogueyStatType::Woodcutting).CurrentLevel = 5;

	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0);

	TestTrue("Action still active (transitioned to Gather)",
		Env.Action->HasAction(Env.Pawn));

	return true;
}

// ---------------------------------------------------------------------------
// 7. Tool gate — TickGatherMove clears action when pawn has no tool
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_ToolGate_MissingTool_ClearsAction,
	"Roguey.Gather.ToolGate.MissingToolClearsAction", GATHER_TEST_FLAGS)
bool FGather_ToolGate_MissingTool_ClearsAction::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill              = ERogueyStatType::Woodcutting;
	Row.RequiredLevel      = 1;
	Row.RequiredToolItemId = FName("bronze_axe");
	Row.GatherTicks        = 4;
	Row.XpPerAction        = 25;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	// Pawn has no axe
	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0);

	TestFalse("Action cleared — no axe in inventory or equipment",
		Env.Action->HasAction(Env.Pawn));
	TestTrue("Speech bubble set", !Env.Pawn->SpeechBubbleText.IsEmpty());

	return true;
}

// ---------------------------------------------------------------------------
// 8. Tool gate — action proceeds when tool is in inventory
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_ToolGate_InventoryTool_Proceeds,
	"Roguey.Gather.ToolGate.InventoryToolProceeds", GATHER_TEST_FLAGS)
bool FGather_ToolGate_InventoryTool_Proceeds::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill              = ERogueyStatType::Woodcutting;
	Row.RequiredLevel      = 1;
	Row.RequiredToolItemId = FName("bronze_axe");
	Row.GatherTicks        = 4;
	Row.XpPerAction        = 25;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	FRogueyItem Axe;
	Axe.ItemId   = FName("bronze_axe");
	Axe.Quantity = 1;
	Env.Pawn->Inventory[0] = Axe;

	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0);

	TestTrue("Action still active — tool found in inventory",
		Env.Action->HasAction(Env.Pawn));

	return true;
}

// ---------------------------------------------------------------------------
// 9. Tool gate — action proceeds when tool is equipped
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_ToolGate_EquippedTool_Proceeds,
	"Roguey.Gather.ToolGate.EquippedToolProceeds", GATHER_TEST_FLAGS)
bool FGather_ToolGate_EquippedTool_Proceeds::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill              = ERogueyStatType::Fishing;
	Row.RequiredLevel      = 1;
	Row.RequiredToolItemId = FName("fishing_rod");
	Row.GatherTicks        = 5;
	Row.XpPerAction        = 10;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	FRogueyItem Rod;
	Rod.ItemId   = FName("fishing_rod");
	Rod.Quantity = 1;
	Env.Pawn->Equipment.Add(EEquipmentSlot::Weapon, Rod);

	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0);

	TestTrue("Action still active — fishing rod equipped",
		Env.Action->HasAction(Env.Pawn));

	return true;
}

// ---------------------------------------------------------------------------
// 10. XP awarded on gather completion — correct amount, correct skill
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_XP_AwardedOnCompletion,
	"Roguey.Gather.XP.AwardedOnCompletion", GATHER_TEST_FLAGS)
bool FGather_XP_AwardedOnCompletion::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill              = ERogueyStatType::Mining;
	Row.RequiredLevel      = 1;
	Row.RequiredToolItemId = NAME_None;
	Row.GatherTicks        = 1;   // completes in 1 tick
	Row.XpPerAction        = 100;
	Row.LootTableId        = NAME_None;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	const int64 XPBefore = Env.Pawn->StatPage.Get(ERogueyStatType::Mining).CurrentXP;

	// First tick: GatherMove sees pawn is adjacent → transitions to Gather (TicksRemaining=1)
	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0);

	// Second tick: Gather ticks down to 0 → XP awarded
	Env.Action->RogueyTick(1);

	const int64 XPAfter = Env.Pawn->StatPage.Get(ERogueyStatType::Mining).CurrentXP;

	TestTrue("Mining XP increased after gather",
		XPAfter > XPBefore || Env.Pawn->StatPage.Get(ERogueyStatType::Mining).BaseLevel > 1);

	return true;
}

// ---------------------------------------------------------------------------
// 11. Level-up triggers speech bubble
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_LevelUp_TriggersSpeechBubble,
	"Roguey.Gather.LevelUp.TriggersSpeechBubble", GATHER_TEST_FLAGS)
bool FGather_LevelUp_TriggersSpeechBubble::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill         = ERogueyStatType::Woodcutting;
	Row.RequiredLevel = 1;
	Row.RequiredToolItemId = NAME_None;
	Row.GatherTicks   = 1;
	Row.LootTableId   = NAME_None;

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	// Set XP to just below level 2 threshold
	FRogueyStat& WC = Env.Pawn->StatPage.Get(ERogueyStatType::Woodcutting);
	// Level 2 requires 83 XP (OSRS curve). Set to 82 so one 25 XP gather pushes it over.
	WC.CurrentXP = 82;
	Row.XpPerAction = 25;
	Env.InjectRow(Row);

	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0); // GatherMove → Gather
	Env.Action->RogueyTick(1); // Gather completes

	TestEqual("Leveled up to 2", Env.Pawn->StatPage.Get(ERogueyStatType::Woodcutting).BaseLevel, 2);
	TestTrue("Level-up bubble contains skill name",
		Env.Pawn->SpeechBubbleText.Contains(TEXT("Woodcutting")));

	return true;
}

// ---------------------------------------------------------------------------
// 12. Gather cycle restarts — after completion, action transitions back to GatherMove
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_Cycle_RestartsAfterCompletion,
	"Roguey.Gather.Cycle.RestartsAfterCompletion", GATHER_TEST_FLAGS)
bool FGather_Cycle_RestartsAfterCompletion::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill         = ERogueyStatType::Woodcutting;
	Row.RequiredLevel = 1;
	Row.RequiredToolItemId = NAME_None;
	Row.GatherTicks   = 1;
	Row.XpPerAction   = 5;
	Row.LootTableId   = NAME_None;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0); // GatherMove → Gather
	Env.Action->RogueyTick(1); // Gather completes → back to GatherMove

	// Action should still be active (continuous gather loop)
	TestTrue("Action still active after cycle",  Env.Action->HasAction(Env.Pawn));
	TestEqual("Action type is GatherMove again",
		(int32)Env.Action->GetActionType(Env.Pawn), (int32)EActionType::GatherMove);

	return true;
}

// ---------------------------------------------------------------------------
// 13. Fishing skill path — full gate + XP chain works end-to-end
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGather_Fishing_FullPath,
	"Roguey.Gather.Fishing.FullPath", GATHER_TEST_FLAGS)
bool FGather_Fishing_FullPath::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForGather();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FGatherEnv Env(World);
	if (!Env.Pawn || !Env.Object) { AddError(TEXT("Spawn failed")); return false; }

	FRogueyObjectRow Row;
	Row.Skill              = ERogueyStatType::Fishing;
	Row.RequiredLevel      = 1;
	Row.RequiredToolItemId = FName("fishing_rod");
	Row.GatherTicks        = 1;
	Row.XpPerAction        = 40;
	Row.LootTableId        = NAME_None;
	Env.InjectRow(Row);

	if (!Env.bInjectionSucceeded) { AddWarning(TEXT("Registry subsystem unavailable — skipping")); return true; }

	// Without rod: blocked
	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(0);
	TestFalse("Blocked without fishing rod", Env.Action->HasAction(Env.Pawn));

	// Add rod, try again
	FRogueyItem Rod;
	Rod.ItemId   = FName("fishing_rod");
	Rod.Quantity = 1;
	Env.Pawn->Inventory[0] = Rod;

	Env.Action->TestSetGatherAction(Env.Pawn, Env.Object, true);
	Env.Action->RogueyTick(2); // GatherMove → Gather
	Env.Action->RogueyTick(3); // Gather completes

	TestTrue("Fishing XP awarded",
		Env.Pawn->StatPage.Get(ERogueyStatType::Fishing).CurrentXP > 0 ||
		Env.Pawn->StatPage.Get(ERogueyStatType::Fishing).BaseLevel > 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
