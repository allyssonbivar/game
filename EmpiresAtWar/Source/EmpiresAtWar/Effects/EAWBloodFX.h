#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EAWBloodFX.generated.h"

class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInterface;

/**
 * Sangue feito em C++ (sem Niagara): gotas com gravidade/arrasto desenhadas por instancias,
 * manchas no chao (decals, com limite) e jato pulsante preso a um osso (coto de membro cortado).
 * So visual: nada disso e replicado; cada maquina desenha o seu.
 */
UCLASS()
class EMPIRESATWAR_API AEAWBloodFX : public AActor
{
	GENERATED_BODY()

public:
	AEAWBloodFX();
	virtual void Tick(float DeltaSeconds) override;

	/** Borrifo de gotas a partir de Location, na direcao Dir. */
	static void Burst(UWorld* World, const FVector& Location, const FVector& Dir, int32 Count, float Speed);

	/** Jato pulsante preso ao osso Bone (o ponto e a direcao acompanham o osso, mesmo em ragdoll). */
	static void Jet(UWorld* World, USkeletalMeshComponent* Mesh, FName Bone, const FVector& WorldPoint, const FVector& WorldDir, float Duration, float Rate);

	/** Mancha no chao (maximo de 60 no mundo; as mais antigas somem). */
	static void SpawnStain(UWorld* World, const FVector& Location, float Size);

	/** Desligado com -EAWSemSangue (opcao do jogador, como pede o documento). */
	static bool IsGoreEnabled();

private:
	struct FDrop
	{
		FVector P;
		FVector V;
		float Life;
		float Size;
	};

	UPROPERTY()
	UInstancedStaticMeshComponent* Drops;

	TArray<FDrop> Items;
	float GroundZ = -100000.f;

	// Jato
	TWeakObjectPtr<USkeletalMeshComponent> JetMesh;
	FName JetBone;
	FVector JetLocalPoint = FVector::ZeroVector;
	FVector JetLocalDir = FVector::UpVector;
	float JetTime = 0.f;
	float JetDuration = 0.f;
	float JetRate = 0.f;
	float EmitAccumulator = 0.f;

	void AddDrops(const FVector& Location, const FVector& Dir, int32 Count, float Speed, float SpreadDeg);
	void FindGround(const FVector& From);

	static AEAWBloodFX* SpawnFX(UWorld* World, const FVector& Location);
	static UMaterialInterface* StainMaterial;
	static TArray<TWeakObjectPtr<class UDecalComponent>> Stains;
};
