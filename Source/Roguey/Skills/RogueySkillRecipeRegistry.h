#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RogueySkillRecipeRow.h"
#include "RogueySkillRecipeRegistry.generated.h"

// Loaded once from DT_SkillRecipes. Provides recipe lookup for the skilling system.
UCLASS()
class ROGUEY_API URogueySkillRecipeRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static URogueySkillRecipeRegistry* Get(const UObject* WorldContext);

	// All recipe IDs for inventory-triggered crafting: tool item + trigger (material) item.
	TArray<FName> GetRecipesForInventoryTool(FName ToolItemId, FName TriggerItemId) const;

	// All recipe IDs for a crafting station (anvil / forge).
	TArray<FName> GetRecipesForStation(FName StationTypeId) const;

	const FRogueySkillRecipeRow* FindRecipe(FName RecipeId) const;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> RecipeTable;
};
