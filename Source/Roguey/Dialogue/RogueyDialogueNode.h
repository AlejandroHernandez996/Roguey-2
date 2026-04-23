#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RogueyDialogueNode.generated.h"

// One player-selectable option inside a branch node.
USTRUCT(BlueprintType)
struct FRogueyDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText ChoiceText;

	// Row name of the node to transition to. NAME_None = close dialogue.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NextNodeId;

	// Player must have this flag to see this choice. NAME_None = always shown.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RequiredFlag;

	// Server sets this flag when the player selects this choice. NAME_None = no flag.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SetsFlag;
};

// One row in DT_Dialogue. Row name = NodeId (e.g. "guide_intro_1").
USTRUCT(BlueprintType)
struct FRogueyDialogueNode : public FTableRowBase
{
	GENERATED_BODY()

	// Name shown above the text box (e.g. "Guide"). Leave blank for player lines.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DialogueText;

	// Empty = linear node, advance via Space/click using NextNodeId.
	// Non-empty = show numbered choices.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FRogueyDialogueChoice> Choices;

	// Used only when Choices is empty. NAME_None = closes dialogue.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NextNodeId;

	// Server sets this flag when this node is first reached. NAME_None = no flag.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SetsFlag;
};
