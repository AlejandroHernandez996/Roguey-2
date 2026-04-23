#include "RogueyPawn.h"

#include "Net/UnrealNetwork.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/Items/RogueyLootDrop.h"
#include "Roguey/Terrain/RogueyTerrain.h"
#include "Roguey/UI/RogueyHUD.h"

ARogueyPawn::ARogueyPawn()
{
	GetCapsuleComponent()->InitCapsuleSize(45.f, 100.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	// CMC smooths simulated proxies toward the server's raw actor position, which conflicts with
	// our TrueTileQueue interpolation. Disable it so only our Tick-driven SetActorLocation runs.
	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;

	// We own position through TilePosition + OnRep_TilePosition — suppress UE's raw position replication
	SetReplicateMovement(false);

	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void ARogueyPawn::BeginPlay()
{
	Super::BeginPlay();

	StatPage.InitDefaults();
	CurrentHP = StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints);
	MaxHP = CurrentHP;

	Inventory.Init(FRogueyItem(), 28);

	if (HasAuthority())
	{
		if (ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (GameMode->GridManager)
			{
				FIntVector2 StartTile = GameMode->GridManager->WorldToTile(GetActorLocation());
				GameMode->GridManager->RegisterActor(this, StartTile);
				TilePosition = FIntPoint(StartTile.X, StartTile.Y);
			}
		}
	}
}

void ARogueyPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (GameMode->GridManager)
			{
				GameMode->GridManager->UnregisterActor(this);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ARogueyPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TrueTileQueue.IsEmpty()) return;

	if (!CachedTerrain)
	{
		for (TActorIterator<ARogueyTerrain> It(GetWorld()); It; ++It)
		{
			CachedTerrain = *It;
			break;
		}
	}

	// Stall until terrain height data is available so we never snap to Z=0.
	if (CachedTerrain && !CachedTerrain->IsHeightGridReady()) return;

	FIntVector2 TargetTile = TrueTileQueue[0];
	float SurfaceZ = CachedTerrain ? CachedTerrain->GetTileHeight(TargetTile) : 0.f;
	FVector Target(
		TargetTile.X * RogueyConstants::TileSize + RogueyConstants::TileSize * TileExtent.X * 0.5f,
		TargetTile.Y * RogueyConstants::TileSize + RogueyConstants::TileSize * TileExtent.Y * 0.5f,
		SurfaceZ + RogueyConstants::PawnHoverHeight
	);

	FVector Current = GetActorLocation();
	float Dist2D = FVector::Dist2D(Current, Target);

	if (Dist2D <= 1.f)
	{
		SetActorLocation(Target);
		TrueTileQueue.RemoveAt(0, 1, EAllowShrinking::No);
		return;
	}

	FVector Dir2D = (Target - Current).GetSafeNormal2D();

	// Scale speed with queue depth so the visual catches up within one tick window.
	// Cap at 8x — prevents a single hitched frame from teleporting the pawn.
	float BaseMult  = (RunStepTile != FIntPoint(-1, -1)) ? 2.f : 1.f;
	float SpeedMult = FMath::Clamp(FMath::Max(BaseMult, (float)TrueTileQueue.Num()), 1.f, 8.f);
	float StepSize  = VisualMoveSpeed * SpeedMult * DeltaSeconds;

	if (StepSize >= Dist2D)
	{
		SetActorLocation(Target);
		TrueTileQueue.RemoveAt(0, 1, EAllowShrinking::No);
	}
	else
	{
		float Alpha = StepSize / Dist2D;
		SetActorLocation(FMath::Lerp(Current, Target, Alpha));
		SetActorRotation(Dir2D.Rotation());
	}
}

void ARogueyPawn::CommitMove(FIntVector2 NewTile, FIntVector2 RunStep)
{
	RunStepTile = FIntPoint(RunStep.X, RunStep.Y);
	TilePosition = FIntPoint(NewTile.X, NewTile.Y);

	if (RunStep != FIntVector2(-1, -1))
		EnqueueVisualPosition(RunStep);
	EnqueueVisualPosition(NewTile);
}

void ARogueyPawn::OnRep_TilePosition()
{
	// RunStepTile is applied before this callback fires (same replication bundle).
	// Enqueue the intermediate step first so visual movement passes through both tiles.
	if (RunStepTile != FIntPoint(-1, -1))
		EnqueueVisualPosition(FIntVector2(RunStepTile.X, RunStepTile.Y));
	EnqueueVisualPosition(FIntVector2(TilePosition.X, TilePosition.Y));
}

void ARogueyPawn::EnqueueVisualPosition(FIntVector2 Tile)
{
	TrueTileQueue.Add(Tile);
}

void ARogueyPawn::Server_RequestMoveTo_Implementation(FIntPoint InTargetTile, bool bRunning)
{
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->ActionManager) return;
	GameMode->ActionManager->SetMoveAction(this, InTargetTile, bRunning);
}

void ARogueyPawn::Server_RequestActorAction_Implementation(AActor* Target, FName ActionId)
{
	if (!IsValid(Target)) return;
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->ActionManager) return;
	GameMode->ActionManager->SetActorAction(this, Target, ActionId);
}

void ARogueyPawn::SetPawnState(EPawnState NewState)
{
	if (PawnState == NewState) return;
	PawnState = NewState;
	OnRep_PawnState();
}

void ARogueyPawn::OnRep_PawnState()
{
	// Animation blueprint reads PawnState — Blueprint handles the actual montage switching
}

void ARogueyPawn::OnRep_HP()
{
}

void ARogueyPawn::ReceiveHit(int32 Damage, ARogueyPawn* /*Attacker*/)
{
	LastHitDamage    = Damage;
	HitSplatCounter++;
	LastHitTime      = GetWorld()->GetTimeSeconds();
	OnRep_HitSplat(); // fire locally on server/listen-server host
}

void ARogueyPawn::ShowSpeechBubble(const FString& Text)
{
	SpeechBubbleText = Text;
	SpeechBubbleCounter++;
	OnRep_SpeechBubble(); // fire locally on server / listen-server host
}

void ARogueyPawn::OnRep_SpeechBubble()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (ARogueyHUD* HUD = Cast<ARogueyHUD>(PC->GetHUD()))
			HUD->AddSpeechBubble(this, SpeechBubbleText);
}

void ARogueyPawn::OnRep_HitSplat()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (ARogueyHUD* HUD = Cast<ARogueyHUD>(PC->GetHUD()))
			HUD->AddHitSplat(GetActorLocation() + FVector(0.f, 0.f, 220.f), LastHitDamage);
}

void ARogueyPawn::Server_ConsumeFromInventory_Implementation(int32 InvSlotIndex)
{
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->ActionManager) return;
	GameMode->ActionManager->QueueConsume(this, InvSlotIndex);
}

void ARogueyPawn::Server_EquipFromInventory_Implementation(int32 InvSlotIndex)
{
	if (!Inventory.IsValidIndex(InvSlotIndex) || Inventory[InvSlotIndex].IsEmpty()) return;

	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
	if (!Registry) return;

	const FRogueyItemRow* Row = Registry->FindItem(Inventory[InvSlotIndex].ItemId);
	if (!Row || !Row->IsEquippable()) return;

	EEquipmentSlot Slot = Row->GetEquipSlot();

	// Swap: previously equipped item goes back into this inventory slot
	FRogueyItem ItemToEquip = Inventory[InvSlotIndex];
	Inventory[InvSlotIndex] = Equipment.Contains(Slot) ? Equipment[Slot] : FRogueyItem();
	Equipment.Add(Slot, ItemToEquip);

	RecalcEquipmentBonuses();
}

void ARogueyPawn::Server_UnequipToInventory_Implementation(EEquipmentSlot Slot)
{
	if (!Equipment.Contains(Slot) || Equipment[Slot].IsEmpty()) return;

	// Find first empty inventory slot
	int32 EmptySlot = Inventory.IndexOfByPredicate([](const FRogueyItem& I){ return I.IsEmpty(); });
	if (EmptySlot == INDEX_NONE) return; // inventory full

	Inventory[EmptySlot] = Equipment[Slot];
	Equipment.Remove(Slot);
	RecalcEquipmentBonuses();
}

void ARogueyPawn::Server_SwapInventorySlots_Implementation(int32 SlotA, int32 SlotB)
{
	if (!Inventory.IsValidIndex(SlotA) || !Inventory.IsValidIndex(SlotB)) return;
	Inventory.Swap(SlotA, SlotB);
}

void ARogueyPawn::Server_DropFromInventory_Implementation(int32 InvSlotIndex)
{
	if (!Inventory.IsValidIndex(InvSlotIndex) || Inventory[InvSlotIndex].IsEmpty()) return;

	FRogueyItem ItemToDrop   = Inventory[InvSlotIndex];
	Inventory[InvSlotIndex]  = FRogueyItem();

	ARogueyLootDrop* Drop = GetWorld()->SpawnActor<ARogueyLootDrop>(
		ARogueyLootDrop::StaticClass(), GetActorLocation(), FRotator::ZeroRotator);
	if (Drop)
		Drop->Init(ItemToDrop, GetTileCoord());
}

void ARogueyPawn::RecalcEquipmentBonuses()
{
	EquipmentBonuses = FRogueyEquipmentBonuses();

	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
	if (!Registry) return;

	for (const auto& Pair : Equipment)
	{
		const FRogueyItemRow* Row = Registry->FindItem(Pair.Value.ItemId);
		if (!Row) continue;

		EquipmentBonuses.MeleeAttack   += Row->MeleeAttackBonus;
		EquipmentBonuses.MeleeStrength += Row->MeleeStrengthBonus;
		EquipmentBonuses.MeleeDefence  += Row->MeleeDefenceBonus;
	}

	// Sync replicated mirror so clients see current equipment
	ReplicatedEquipment.Reset();
	for (const auto& Pair : Equipment)
	{
		if (!Pair.Value.IsEmpty())
			ReplicatedEquipment.Add({ Pair.Key, Pair.Value });
	}
	OnRep_ReplicatedEquipment(); // apply locally on server/listen-server host
}

void ARogueyPawn::OnRep_ReplicatedEquipment()
{
	Equipment.Reset();
	for (const FRogueyEquipmentEntry& Entry : ReplicatedEquipment)
		Equipment.Add(Entry.Slot, Entry.Item);
}

void ARogueyPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARogueyPawn, TilePosition);
	DOREPLIFETIME(ARogueyPawn, PawnState);
	DOREPLIFETIME(ARogueyPawn, CurrentHP);
	DOREPLIFETIME(ARogueyPawn, MaxHP);
	DOREPLIFETIME(ARogueyPawn, DestinationTile);
	DOREPLIFETIME(ARogueyPawn, RunStepTile);
	DOREPLIFETIME(ARogueyPawn, LastHitDamage);
	DOREPLIFETIME(ARogueyPawn, HitSplatCounter);
	DOREPLIFETIME(ARogueyPawn, LastHitTime);
	DOREPLIFETIME(ARogueyPawn, SpeechBubbleText);
	DOREPLIFETIME(ARogueyPawn, SpeechBubbleCounter);
	DOREPLIFETIME(ARogueyPawn, Inventory);
	DOREPLIFETIME(ARogueyPawn, ReplicatedEquipment);
}
