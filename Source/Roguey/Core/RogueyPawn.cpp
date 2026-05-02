#include "RogueyPawn.h"

#include "Net/UnrealNetwork.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Items/RogueyShopRegistry.h"
#include "Roguey/Items/RogueyUseCombinationRegistry.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Roguey/RogueyGameMode.h"
#include "Roguey/RogueyGameInstance.h"
#include "Roguey/RogueyPlayerController.h"
#include "Roguey/Items/RogueyLootDrop.h"
#include "Roguey/Combat/RogueySpellRegistry.h"
#include "Roguey/Passives/RogueyPassiveRegistry.h"
#include "Roguey/Terrain/RogueyTerrain.h"
#include "Roguey/UI/RogueyHUD.h"

ARogueyPawn::ARogueyPawn()
{
	GetCapsuleComponent()->InitCapsuleSize(45.f, 100.f);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
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

	Inventory.Init(FRogueyItem(), 28);

	if (HasAuthority())
	{
		// Clients receive HP and stat page via replication; server initialises them locally.
		StatPage.InitDefaults();
		CurrentHP = StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints);
		MaxHP = CurrentHP;

		if (ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (GameMode->GridManager)
			{
				FIntVector2 StartTile = GameMode->GridManager->WorldToTile(GetActorLocation());
				GameMode->GridManager->RegisterActor(this, StartTile);
				TilePosition = FIntPoint(StartTile.X, StartTile.Y);
			}
		}
		SyncStatPage();
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
	// TeleportSerial arrives in the same rep bundle (declared before TilePosition).
	// When it differs from our last seen value, this is an area transition: snap instantly
	// instead of animating, so the pawn doesn't slide from the old area position.
	if (TeleportSerial != LastTeleportSerial)
	{
		LastTeleportSerial = TeleportSerial;
		ClearVisualQueue();

		FIntVector2 NewTile(TilePosition.X, TilePosition.Y);
		FVector SnapPos(
			NewTile.X * RogueyConstants::TileSize + RogueyConstants::TileSize * TileExtent.X * 0.5f,
			NewTile.Y * RogueyConstants::TileSize + RogueyConstants::TileSize * TileExtent.Y * 0.5f,
			GetActorLocation().Z
		);
		SetActorLocation(SnapPos);
		// Enqueue so Tick corrects Z to correct terrain height once terrain is ready
		EnqueueVisualPosition(NewTile);
		return;
	}

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

void ARogueyPawn::Server_RequestActorAction_Implementation(AActor* Target, FName ActionId, bool bRunning, int32 InvSlot)
{
	if (!IsValid(Target)) return;
	ARogueyGameMode* GameMode = Cast<ARogueyGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode || !GameMode->ActionManager) return;
	GameMode->ActionManager->SetActorAction(this, Target, ActionId, bRunning, InvSlot);
}

void ARogueyPawn::Server_BoostStat_Implementation(ERogueyStatType StatType)
{
	FRogueyStat& Stat = StatPage.Get(StatType);
	Stat.BaseLevel    = FMath::Clamp(Stat.BaseLevel + 5, 1, FRogueyStat::MaxLevel);
	Stat.CurrentLevel = Stat.BaseLevel;
	Stat.CurrentXP    = Stat.XPForLevel(Stat.BaseLevel);

	if (StatType == ERogueyStatType::Hitpoints)
	{
		MaxHP    = Stat.BaseLevel;
		CurrentHP = FMath::Min(CurrentHP, MaxHP);
		OnRep_HP();
	}
	SyncStatPage();
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

void ARogueyPawn::PostGameMessage(const FString& Text, FLinearColor Color) const
{
	if (ARogueyPlayerController* PC = Cast<ARogueyPlayerController>(GetController()))
		PC->Client_PostGameMessage(Text, Color);
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
	if (!Inventory.IsValidIndex(InvSlotIndex)) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::EquipFromInventory;
		Op.SlotA  = InvSlotIndex;
		GM->ActionManager->QueueInvOp(this, Op);
	}
}

bool ARogueyPawn::TryAddItem(const FRogueyItem& Item)
{
	if (Item.IsEmpty()) return true;

	URogueyItemRegistry*  Registry = URogueyItemRegistry::Get(this);
	const FRogueyItemRow* Row      = Registry ? Registry->FindItem(Item.ItemId) : nullptr;

	if (Row && Row->bStackable)
	{
		for (FRogueyItem& Slot : Inventory)
		{
			if (Slot.ItemId == Item.ItemId)
			{
				Slot.Quantity = FMath::Min(Slot.Quantity + Item.Quantity, Row->MaxStack);
				return true;
			}
		}
	}

	for (FRogueyItem& Slot : Inventory)
	{
		if (Slot.IsEmpty())
		{
			Slot = Item;
			return true;
		}
	}

	return false;
}

void ARogueyPawn::Server_UnequipToInventory_Implementation(EEquipmentSlot Slot)
{
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType    = EInvOpType::UnequipToInventory;
		Op.EquipSlot = Slot;
		GM->ActionManager->QueueInvOp(this, Op);
		return;
	}
	// Direct path — used when ActionManager is unavailable (e.g. editor automation tests).
	if (Equipment.Contains(Slot) && !Equipment[Slot].IsEmpty())
	{
		const FRogueyItem ItemToReturn = Equipment[Slot];
		if (TryAddItem(ItemToReturn))
			Equipment.Remove(Slot);
	}
}

void ARogueyPawn::Server_SwapInventorySlots_Implementation(int32 SlotA, int32 SlotB)
{
	if (!Inventory.IsValidIndex(SlotA) || !Inventory.IsValidIndex(SlotB)) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::SwapSlots;
		Op.SlotA  = SlotA;
		Op.SlotB  = SlotB;
		GM->ActionManager->QueueInvOp(this, Op);
		return;
	}
	// Direct path — used when ActionManager is unavailable (e.g. editor automation tests).
	Inventory.Swap(SlotA, SlotB);
}

void ARogueyPawn::Server_SetSelectedSpell_Implementation(FName SpellId)
{
	if (!HasAuthority()) return;

	if (SpellId.IsNone())
	{
		SelectedSpell = NAME_None;
		return;
	}

	// Validate: must have a magic weapon equipped
	if (!bMagicWeapon) return;

	// Validate: spell must exist and level requirement must be met
	URogueySpellRegistry* SpellReg = URogueySpellRegistry::Get(this);
	const FRogueySpellRow* Def = SpellReg ? SpellReg->FindSpell(SpellId) : nullptr;
	if (!Def) return;
	if (StatPage.Get(ERogueyStatType::Magic).CurrentLevel < Def->LevelRequired) return;

	SelectedSpell = SpellId;
}

void ARogueyPawn::Server_SetDialogueFlag_Implementation(FName Flag)
{
	if (Flag.IsNone() || DialogueFlags.Contains(Flag)) return;
	DialogueFlags.Add(Flag);
}

void ARogueyPawn::Server_BuyShopItem_Implementation(FName ShopId, FName ItemId, int32 Quantity)
{
	if (Quantity <= 0) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType   = EInvOpType::BuyShopItem;
		Op.NameA    = ShopId;
		Op.NameB    = ItemId;
		Op.Quantity = Quantity;
		GM->ActionManager->QueueInvOp(this, Op);
	}
}

void ARogueyPawn::Server_DropFromInventory_Implementation(int32 InvSlotIndex)
{
	if (!Inventory.IsValidIndex(InvSlotIndex)) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::DropFromInventory;
		Op.SlotA  = InvSlotIndex;
		GM->ActionManager->QueueInvOp(this, Op);
		return;
	}
	// Direct path — used when ActionManager is unavailable (e.g. editor automation tests).
	if (Inventory[InvSlotIndex].IsEmpty()) return;
	FRogueyItem ItemToDrop    = Inventory[InvSlotIndex];
	Inventory[InvSlotIndex]   = FRogueyItem();
	if (UWorld* W = GetWorld())
	{
		ARogueyLootDrop* Drop = W->SpawnActor<ARogueyLootDrop>(
			ARogueyLootDrop::StaticClass(), GetActorLocation(), FRotator::ZeroRotator);
		if (Drop) Drop->Init(ItemToDrop, GetTileCoord());
	}
}

void ARogueyPawn::Server_BankDeposit_Implementation(int32 InvSlotIndex)
{
	if (!Inventory.IsValidIndex(InvSlotIndex)) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::BankDeposit;
		Op.SlotA  = InvSlotIndex;
		GM->ActionManager->QueueInvOp(this, Op);
	}
}

void ARogueyPawn::Server_BankWithdraw_Implementation(int32 BankSlotIndex, int32 Qty)
{
	if (Qty <= 0) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType   = EInvOpType::BankWithdraw;
		Op.SlotA    = BankSlotIndex;
		Op.Quantity = Qty;
		GM->ActionManager->QueueInvOp(this, Op);
	}
}

void ARogueyPawn::RecalcEquipmentBonuses()
{
	EquipmentBonuses           = FRogueyEquipmentBonuses();
	AttackCooldownTicks        = 4;
	AttackRange                = 1;
	bAttackCardinalOnly        = true;
	RangedProjectileSpeedTicks = 0;
	bMagicWeapon               = false;

	URogueyItemRegistry* Registry = URogueyItemRegistry::Get(this);
	if (!Registry) return;

	for (const auto& Pair : Equipment)
	{
		const FRogueyItemRow* Row = Registry->FindItem(Pair.Value.ItemId);
		if (!Row) continue;

		EquipmentBonuses.MeleeAttack    += Row->MeleeAttackBonus;
		EquipmentBonuses.MeleeStrength  += Row->MeleeStrengthBonus;
		EquipmentBonuses.MeleeDefence   += Row->MeleeDefenceBonus;
		EquipmentBonuses.RangedAttack   += Row->RangedAttackBonus;
		EquipmentBonuses.RangedStrength += Row->RangedStrengthBonus;
		EquipmentBonuses.RangedDefence  += Row->RangedDefenceBonus;
		EquipmentBonuses.MagicAttack    += Row->MagicAttackBonus;
		EquipmentBonuses.MagicStrength  += Row->MagicStrengthBonus;
		EquipmentBonuses.MagicDefence   += Row->MagicDefenceBonus;

		if (Pair.Key == EEquipmentSlot::Weapon)
		{
			if (Row->AttackSpeedTicks > 0)
				AttackCooldownTicks = Row->AttackSpeedTicks;
			if (Row->AttackRangeTiles > 1)
			{
				AttackRange                = Row->AttackRangeTiles;
				bAttackCardinalOnly        = false;
				RangedProjectileSpeedTicks = FMath::Max(1, Row->ProjectileSpeedTicks);
			}
			if (Row->bMagicWeapon)
				bMagicWeapon = true;
		}
	}

	// Sync replicated mirror so clients see current equipment
	ReplicatedEquipment.Reset();
	for (const auto& Pair : Equipment)
	{
		if (!Pair.Value.IsEmpty())
			ReplicatedEquipment.Add({ Pair.Key, Pair.Value });
	}
	OnRep_ReplicatedEquipment(); // apply locally on server/listen-server host

	// Layer passive bonuses on top of equipment bonuses (server only; no-op on clients without passives)
	ApplyPassiveBonuses();
}

void ARogueyPawn::OnRep_ReplicatedEquipment()
{
	Equipment.Reset();
	for (const FRogueyEquipmentEntry& Entry : ReplicatedEquipment)
		Equipment.Add(Entry.Slot, Entry.Item);
}

void ARogueyPawn::SyncStatPage()
{
	ReplicatedStatPage.Reset();
	for (const auto& Pair : StatPage.Stats)
		ReplicatedStatPage.Add({ Pair.Key, Pair.Value });
	OnRep_ReplicatedStatPage(); // apply locally on server / listen-server host
}

void ARogueyPawn::OnRep_ReplicatedStatPage()
{
	StatPage.Stats.Reset();
	for (const FRogueyStatEntry& Entry : ReplicatedStatPage)
		StatPage.Stats.Add(Entry.StatType, Entry.Stat);
}

void ARogueyPawn::Server_UseItemOnItem_Implementation(int32 SlotA, int32 SlotB)
{
	if (SlotA == SlotB) return;
	if (!Inventory.IsValidIndex(SlotA) || Inventory[SlotA].IsEmpty()) return;
	if (!Inventory.IsValidIndex(SlotB) || Inventory[SlotB].IsEmpty()) return;

	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::UseItemOnItem;
		Op.SlotA  = SlotA;
		Op.SlotB  = SlotB;
		GM->ActionManager->QueueInvOp(this, Op);
	}
}

void ARogueyPawn::Server_CastSpellOnItem_Implementation(FName SpellId, int32 InvSlot)
{
	if (!Inventory.IsValidIndex(InvSlot) || Inventory[InvSlot].IsEmpty()) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
	{
		FPendingInvOp Op;
		Op.OpType = EInvOpType::SpellCastOnItem;
		Op.SlotA  = InvSlot;
		Op.NameA  = SpellId;
		GM->ActionManager->QueueInvOp(this, Op);
	}
}

void ARogueyPawn::Server_StartSkillCraft_Implementation(FName RecipeId)
{
	if (RecipeId.IsNone()) return;
	ARogueyGameMode* GM = GetWorld()->GetAuthGameMode<ARogueyGameMode>();
	if (GM && GM->ActionManager)
		GM->ActionManager->SetSkillCraftAction(this, RecipeId, nullptr);
}

void ARogueyPawn::ApplyPassiveBonuses(URogueyPassiveRegistry* InRegistry)
{
	PassiveAttackCooldownReduction = 0;
	PassiveGatherSpeedReduction    = 0;
	PassiveMaxHPBonus              = 0;

	URogueyPassiveRegistry* Registry = InRegistry ? InRegistry : URogueyPassiveRegistry::Get(this);
	if (Registry)
	{
		for (const FName& Id : ActivePassiveIds)
		{
			const FRogueyPassiveRow* Row = Registry->FindPassive(Id);
			if (!Row) continue;

			switch (Row->Effect)
			{
			case ERogueyPassiveEffect::MeleeAttackBonus:    EquipmentBonuses.MeleeAttack    += Row->EffectValue; break;
			case ERogueyPassiveEffect::MeleeStrengthBonus:  EquipmentBonuses.MeleeStrength  += Row->EffectValue; break;
			case ERogueyPassiveEffect::MeleeDefenceBonus:   EquipmentBonuses.MeleeDefence   += Row->EffectValue; break;
			case ERogueyPassiveEffect::RangedAttackBonus:   EquipmentBonuses.RangedAttack   += Row->EffectValue; break;
			case ERogueyPassiveEffect::RangedStrengthBonus: EquipmentBonuses.RangedStrength += Row->EffectValue; break;
			case ERogueyPassiveEffect::MagicAttackBonus:    EquipmentBonuses.MagicAttack    += Row->EffectValue; break;
			case ERogueyPassiveEffect::MagicStrengthBonus:  EquipmentBonuses.MagicStrength  += Row->EffectValue; break;
			case ERogueyPassiveEffect::AttackSpeedReduction: PassiveAttackCooldownReduction += Row->EffectValue; break;
			case ERogueyPassiveEffect::GatherSpeedReduction: PassiveGatherSpeedReduction    += Row->EffectValue; break;
			case ERogueyPassiveEffect::MaxHPBonus:           PassiveMaxHPBonus              += Row->EffectValue; break;
			}
		}
	}

	AttackCooldownTicks = FMath::Max(1, AttackCooldownTicks - PassiveAttackCooldownReduction);

	// MaxHP is server-authoritative; don't overwrite the replicated value on clients
	if (HasAuthority())
	{
		MaxHP     = StatPage.GetCurrentLevel(ERogueyStatType::Hitpoints) + PassiveMaxHPBonus;
		CurrentHP = FMath::Min(CurrentHP, MaxHP);
	}
}

void ARogueyPawn::AddPassive(FName PassiveId)
{
	URogueyPassiveRegistry* Registry = URogueyPassiveRegistry::Get(this);
	if (!Registry) return;

	const FRogueyPassiveRow* Row = Registry->FindPassive(PassiveId);
	if (!Row) return;

	if (!Row->UpgradesFromId.IsNone())
		ActivePassiveIds.Remove(Row->UpgradesFromId);

	ActivePassiveIds.AddUnique(PassiveId);
	RecalcEquipmentBonuses();
}

void ARogueyPawn::ResetPassives()
{
	ActivePassiveIds.Empty();
	PendingPassiveOffer.Empty();
	PassiveAttackCooldownReduction = 0;
	PassiveGatherSpeedReduction    = 0;
	PassiveMaxHPBonus              = 0;
}

void ARogueyPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARogueyPawn, TeleportSerial);
	DOREPLIFETIME(ARogueyPawn, TilePosition);
	DOREPLIFETIME(ARogueyPawn, PawnState);
	DOREPLIFETIME(ARogueyPawn, CurrentHP);
	DOREPLIFETIME(ARogueyPawn, MaxHP);
	DOREPLIFETIME(ARogueyPawn, CurrentResolve);
	DOREPLIFETIME(ARogueyPawn, MaxResolve);
	DOREPLIFETIME(ARogueyPawn, DestinationTile);
	DOREPLIFETIME(ARogueyPawn, RunStepTile);
	DOREPLIFETIME(ARogueyPawn, LastHitDamage);
	DOREPLIFETIME(ARogueyPawn, HitSplatCounter);
	DOREPLIFETIME(ARogueyPawn, LastHitTime);
	DOREPLIFETIME(ARogueyPawn, DisplayName);
	DOREPLIFETIME(ARogueyPawn, SpeechBubbleText);
	DOREPLIFETIME(ARogueyPawn, SpeechBubbleCounter);
	DOREPLIFETIME(ARogueyPawn, Inventory);
	DOREPLIFETIME(ARogueyPawn, ReplicatedEquipment);
	DOREPLIFETIME(ARogueyPawn, AttackTarget);
	DOREPLIFETIME_CONDITION(ARogueyPawn, DialogueFlags,        COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARogueyPawn, ReplicatedStatPage,   COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARogueyPawn, SelectedSpell,        COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARogueyPawn, ActivePassiveIds,     COND_OwnerOnly);
}
