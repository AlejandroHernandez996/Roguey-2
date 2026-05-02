#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "Roguey/World/RogueyPortal.h"
#include "Roguey/Npcs/RogueyNpc.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Core/RogueyPawnState.h"

#if WITH_DEV_AUTOMATION_TESTS

#define PORTAL_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

static UWorld* GetEditorWorldForPortal()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		if (Ctx.WorldType == EWorldType::Editor && Ctx.World())
			return Ctx.World();
	return nullptr;
}

// Returns true if the given actions array contains an action with the given Id.
static bool ActionsContain(const TArray<FRogueyActionDef>& Actions, FName Id)
{
	for (const FRogueyActionDef& A : Actions)
		if (A.ActionId == Id) return true;
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// bRequiresClearRoom=true, no hostile NPCs → Enter + Examine available
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortal_ClearRoom_NoNpcs_OffersEnter,
	"Roguey.Portal.ClearRoom.NoNpcs_OffersEnter", PORTAL_TEST_FLAGS)
bool FPortal_ClearRoom_NoNpcs_OffersEnter::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPortal();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyPortal* Portal = World->SpawnActor<ARogueyPortal>(ARogueyPortal::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("Portal spawned", Portal)) return false;

	Portal->bRequiresClearRoom = true;
	Portal->NextAreaId         = FName("next_area");

	TArray<FRogueyActionDef> Actions = Portal->GetActions();

	TestTrue ("Enter offered when room clear", ActionsContain(Actions, RogueyActions::Enter));
	TestTrue ("Examine always offered",        ActionsContain(Actions, RogueyActions::Examine));

	Portal->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// bRequiresClearRoom=true + live hostile NPC → Examine only
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortal_ClearRoom_HostileNpc_ExamineOnly,
	"Roguey.Portal.ClearRoom.HostileNpc_ExamineOnly", PORTAL_TEST_FLAGS)
bool FPortal_ClearRoom_HostileNpc_ExamineOnly::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPortal();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyPortal* Portal = World->SpawnActor<ARogueyPortal>(ARogueyPortal::StaticClass(), FTransform::Identity, P);
	// ARogueyNpc constructor sets TeamId=1 (hostile); BeginPlay aborts early in editor (no registry).
	ARogueyNpc*    Npc    = World->SpawnActor<ARogueyNpc>(ARogueyNpc::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("Portal spawned", Portal) || !TestNotNull("Npc spawned", Npc))
	{
		if (Portal) Portal->Destroy();
		if (Npc)    Npc->Destroy();
		return false;
	}

	Portal->bRequiresClearRoom = true;
	// Npc is alive (PawnState=Idle) and hostile (TeamId=1 from constructor)

	TArray<FRogueyActionDef> Actions = Portal->GetActions();

	TestFalse("Enter not offered while hostile NPC alive", ActionsContain(Actions, RogueyActions::Enter));
	TestTrue ("Examine still offered",                     ActionsContain(Actions, RogueyActions::Examine));

	Portal->Destroy();
	Npc->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// bRequiresClearRoom=true + dead hostile NPC → Enter available
// (IsDead() checks PawnState == Dead, not HP)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortal_ClearRoom_DeadNpc_OffersEnter,
	"Roguey.Portal.ClearRoom.DeadNpc_OffersEnter", PORTAL_TEST_FLAGS)
bool FPortal_ClearRoom_DeadNpc_OffersEnter::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPortal();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyPortal* Portal = World->SpawnActor<ARogueyPortal>(ARogueyPortal::StaticClass(), FTransform::Identity, P);
	ARogueyNpc*    Npc    = World->SpawnActor<ARogueyNpc>(ARogueyNpc::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("Portal spawned", Portal) || !TestNotNull("Npc spawned", Npc))
	{
		if (Portal) Portal->Destroy();
		if (Npc)    Npc->Destroy();
		return false;
	}

	Portal->bRequiresClearRoom = true;
	Npc->TeamId = 1;
	Npc->SetPawnState(EPawnState::Dead); // IsDead() returns true

	TArray<FRogueyActionDef> Actions = Portal->GetActions();

	TestTrue("Enter offered when only NPC is dead", ActionsContain(Actions, RogueyActions::Enter));

	Portal->Destroy();
	Npc->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// bRequiresClearRoom=false → Enter always offered regardless of NPCs
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortal_NoClearRequired_AlwaysOffersEnter,
	"Roguey.Portal.NoClearRequired.AlwaysOffersEnter", PORTAL_TEST_FLAGS)
bool FPortal_NoClearRequired_AlwaysOffersEnter::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPortal();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyPortal* Portal = World->SpawnActor<ARogueyPortal>(ARogueyPortal::StaticClass(), FTransform::Identity, P);
	ARogueyNpc*    Npc    = World->SpawnActor<ARogueyNpc>(ARogueyNpc::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("Portal spawned", Portal) || !TestNotNull("Npc spawned", Npc))
	{
		if (Portal) Portal->Destroy();
		if (Npc)    Npc->Destroy();
		return false;
	}

	Portal->bRequiresClearRoom = false; // No clear required
	// Npc is alive and hostile

	TArray<FRogueyActionDef> Actions = Portal->GetActions();

	TestTrue("Enter offered even with live hostile NPC when clear not required",
		ActionsContain(Actions, RogueyActions::Enter));

	Portal->Destroy();
	Npc->Destroy();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Friendly NPC (TeamId=0) does not block portal even with bRequiresClearRoom
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortal_ClearRoom_FriendlyNpc_OffersEnter,
	"Roguey.Portal.ClearRoom.FriendlyNpc_OffersEnter", PORTAL_TEST_FLAGS)
bool FPortal_ClearRoom_FriendlyNpc_OffersEnter::RunTest(const FString&)
{
	UWorld* World = GetEditorWorldForPortal();
	if (!World) { AddWarning(TEXT("No editor world")); return true; }

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueyPortal* Portal = World->SpawnActor<ARogueyPortal>(ARogueyPortal::StaticClass(), FTransform::Identity, P);
	ARogueyNpc*    Npc    = World->SpawnActor<ARogueyNpc>(ARogueyNpc::StaticClass(), FTransform::Identity, P);
	if (!TestNotNull("Portal spawned", Portal) || !TestNotNull("Npc spawned", Npc))
	{
		if (Portal) Portal->Destroy();
		if (Npc)    Npc->Destroy();
		return false;
	}

	Portal->bRequiresClearRoom = true;
	Npc->TeamId = 0; // Friendly — IsRoomStillHostile skips TeamId==0

	TArray<FRogueyActionDef> Actions = Portal->GetActions();

	TestTrue("Enter offered — friendly NPC (TeamId=0) does not block portal",
		ActionsContain(Actions, RogueyActions::Enter));

	Portal->Destroy();
	Npc->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
