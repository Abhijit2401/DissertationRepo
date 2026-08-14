#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "Shopkeeper.generated.h"

UCLASS()
class DISSERTATION_API AShopkeeper : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AShopkeeper();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visuals")
    UStaticMeshComponent* MeshComp;
    virtual void Interact_Implementation(APawn* InstigatorPawn) override;
};