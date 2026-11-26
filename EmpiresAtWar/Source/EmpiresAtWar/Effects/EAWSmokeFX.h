#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EAWSmokeFX.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Fumaca de polvora negra com sprites (textura T_Fumaca: flocos com sombra propria) que olham para a camera,
 * clarao do disparo com luz, e poeira no impacto. C++ puro (sem Niagara), so visual.
 * Referencia de comportamento: video do Holdfast (nuvem densa ~1,5 m a frente do cano, rastro na linha do tiro,
 * fica no ar 5-8 s e deriva com o vento).
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
		float Drag;
		float Roll;
		float Spin;
		float Bright;
		float Seed;
		uint8 Atlas;
	};

	UPROPERTY() UInstancedStaticMeshComponent* Puffs;
	UPROPERTY() UStaticMeshComponent* Flash;
	UPROPERTY() UPointLightComponent* Light;

	TArray<FPuff> Items;
	TArray<int32> Order;
	float FlashLeft = 0.f;

	void Add(const FVector& P, const FVector& V, float Life, float R0, float R1, float Alpha, float Drag, float Bright = 1.f);
	static AEAWSmokeFX* SpawnAt(UWorld* World, const FVector& Location);
};
