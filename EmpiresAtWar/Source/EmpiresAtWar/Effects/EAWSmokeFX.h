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
 * Referencia de comportamento: videos do Holdfast (x1, 2026-11-26): nuvem branca densa que abre ~3 m a frente do cano,
 * cresce ate ~4-5 m, fica densa ~4 s, some devagar em ~12 s e deriva com o vento (envolve quem atirou).
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

	void Add(const FVector& P, const FVector& V, float Life, float R0, float R1, float Alpha, float Drag, float Bright = 1.f, float Delay = 0.f);
	static AEAWSmokeFX* SpawnAt(UWorld* World, const FVector& Location);
};
