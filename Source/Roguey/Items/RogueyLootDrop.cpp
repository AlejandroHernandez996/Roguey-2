#include "RogueyLootDrop.h"

#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "Roguey/Core/RogueyAction.h"
#include "Roguey/Core/RogueyActionNames.h"
#include "Roguey/Core/RogueyPawn.h"
#include "Roguey/Items/RogueyItemRegistry.h"
#include "Roguey/Core/RogueyConstants.h"

ARogueyLootDrop::ARogueyLootDrop()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetSphereRadius(32.f);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Sphere;
}

void ARogueyLootDrop::Init(FRogueyItem InItem, FIntVector2 InTile)
{
	Item     = InItem;
	LootTile = InTile;
}

void ARogueyLootDrop::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			DespawnTimer,
			[this]() { Destroy(); },
			60.f,
			false
		);
	}
}

void ARogueyLootDrop::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueyLootDrop, Item);
}

TArray<FRogueyActionDef> ARogueyLootDrop::GetActions() const
{
	FRogueyActionDef Take;
	Take.ActionId    = RogueyActions::Take;
	Take.DisplayName = FText::FromName(RogueyActions::Take);
	return { Take };
}

FText ARogueyLootDrop::GetTargetName() const
{
	if (const URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this))
		if (const FRogueyItemRow* Row = Reg->FindItem(Item.ItemId))
			return FText::FromString(Row->DisplayName);
	return FText::FromName(Item.ItemId);
}

FString ARogueyLootDrop::GetExamineText() const
{
	if (const URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this))
		if (const FRogueyItemRow* Row = Reg->FindItem(Item.ItemId))
			return Row->ExamineText;
	return TEXT("");
}

void ARogueyLootDrop::TakeItem(ARogueyPawn* Taker)
{
	if (!IsValid(Taker) || Item.IsEmpty()) return;

	if (Taker->TryAddItem(Item))
	{
		FString ItemName = Item.ItemId.ToString();
		if (const URogueyItemRegistry* Reg = URogueyItemRegistry::Get(this))
			if (const FRogueyItemRow* Row = Reg->FindItem(Item.ItemId))
				ItemName = Row->DisplayName;
		Taker->PostGameMessage(FString::Printf(TEXT("You pick up the %s."), *ItemName), RogueyChat::Game);
		Destroy();
	}
	else
		Taker->PostGameMessage(TEXT("Your inventory is too full to pick that up."), RogueyChat::Warning);
}
