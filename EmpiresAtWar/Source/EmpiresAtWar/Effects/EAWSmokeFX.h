#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EAWSmokeFX.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Fumaca de polvora negra (bolhas translucidas que crescem, sobem e somem), clarao do disparo com luz,
 * e poeira no impacto. C++ puro (sem Niagara), so visual.
 */
UCLASS()
class EMPIRESATWAR_API AEAWSmokeFX : public AActor
{
	GENERATED_BODY()

public:
	AEAWSmokeFX();
	virtual void Tick(float DeltaSeconds) override;

	/** Disparo: clarao + nuvem grande na boca do cano e um sopro na cacoleta. */
	static void MuzzleBlast(UWorld* World, const FVector& Muzzle, const FVector& Dir, const FVector& Pan);
	/** Poeira/estilhaco onde a bala bate no cenario. */
	static void Dust(UWorld* World, const FVector& Location, const FVector& Normal);

private:
	struct FPuff
	{
		FVector P;
		FVector V;
		float Age;
		float Life;
		float R0;
		float R1;
		float Alpha;
	};

	UPROPERTY() UInstancedStaticMeshComponent* Puffs;
	UPROPERTY() UStaticMeshComponent* Flash;
	UPROPERTY() UPointLightComponent* Light;

	TArray<FPuff> Items;
	float FlashLeft = 0.f;
	float Drag = 1.2f;

	void Add(const FVector& P, const FVector& V, float Life, float R0, float R1, float Alpha);
	static AEAWSmokeFX* SpawnAt(UWorld* World, const FVector& Location);
};
