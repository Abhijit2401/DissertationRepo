#include "Shopkeeper.h"

AShopkeeper::AShopkeeper()
{
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
}

void AShopkeeper::BeginPlay()
{
    Super::BeginPlay();
}

void AShopkeeper::Interact_Implementation(APawn* InstigatorPawn)
{
    // Left blank on purpose. 
    // All the UI shop widget creation is done in Blueprints so I don't have to hardcode UI in C++ and therefore allows me to edit the appearance nad functionltiy easier

}