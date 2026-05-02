#include "RogueySkillRecipeRegistry.h"
#include "Roguey/Items/RogueyItemSettings.h"
#include "Engine/DataTable.h"

void URogueySkillRecipeRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const URogueyItemSettings* Settings = GetDefault<URogueyItemSettings>();
	RecipeTable = Settings->SkillRecipeTable.LoadSynchronous();
}

URogueySkillRecipeRegistry* URogueySkillRecipeRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = WorldContext->GetWorld();
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<URogueySkillRecipeRegistry>() : nullptr;
}

TArray<FName> URogueySkillRecipeRegistry::GetRecipesForInventoryTool(FName ToolItemId, FName TriggerItemId) const
{
	TArray<FName> Result;
	if (!RecipeTable) return Result;

	RecipeTable->ForeachRow<FRogueySkillRecipeRow>(TEXT("GetRecipesForInventoryTool"),
		[&](const FName& RowName, const FRogueySkillRecipeRow& Row)
		{
			if (Row.StationTypeId.IsNone()
				&& Row.ToolItemId == ToolItemId
				&& Row.TriggerItemId == TriggerItemId)
			{
				Result.Add(RowName);
			}
		});
	return Result;
}

TArray<FName> URogueySkillRecipeRegistry::GetRecipesForStation(FName StationTypeId) const
{
	TArray<FName> Result;
	if (!RecipeTable || StationTypeId.IsNone()) return Result;

	RecipeTable->ForeachRow<FRogueySkillRecipeRow>(TEXT("GetRecipesForStation"),
		[&](const FName& RowName, const FRogueySkillRecipeRow& Row)
		{
			if (Row.StationTypeId == StationTypeId)
				Result.Add(RowName);
		});
	return Result;
}

const FRogueySkillRecipeRow* URogueySkillRecipeRegistry::FindRecipe(FName RecipeId) const
{
	if (!RecipeTable || RecipeId.IsNone()) return nullptr;
	return RecipeTable->FindRow<FRogueySkillRecipeRow>(RecipeId, TEXT("FindRecipe"), false);
}
