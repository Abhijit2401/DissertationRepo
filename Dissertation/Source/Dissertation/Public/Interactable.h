#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI)
class UInteractable : public UInterface
{
    GENERATED_BODY()
};

class DISSERTATION_API IInteractable
{
    GENERATED_BODY()

public:
    /** * @brief Called when the player looks at this and presses the interact key.
     * Meant to be used in Blueprints so we can trigger UI stuff like opening the shop.
     * @param InstigatorPawn The player who pressed interact.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void Interact(APawn* InstigatorPawn);
};