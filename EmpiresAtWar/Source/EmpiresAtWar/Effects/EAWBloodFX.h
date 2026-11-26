#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EAWBloodFX.generated.h"

class UInstancedStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInterface;

/**
 * Sangue feito em C++ (sem Niagara), desenhado com sprites que olham para a camera:
 *  - filetes esticados na direcao do voo (textura T_Sangue_Jato: jato com contas e fiapos),
 *  - gotas soltas, nevoa vermelha no impacto,
 *  - jato arterial pulsando no ritmo do coracao preso a um osso (pescoco, coto de membro),
 *  - manchas no chao (decals com 4 formatos de respingo, com limite).
 * So visual: nada disso e replicado; cada maquina desenha o seu.
 */
UCLASS()
class EMPIRESATWAR_API AEAWBloodFX : public AActor
{
	GENERATED_BODY()

public:
	AEAWBloodFX();
	virtual void Tick(float DeltaSeconds) override;

	/** Borrifo a partir de Location, na direcao Dir (filetes + gotas + nevoa). */
	static void Burst(UWorld* World, const FVector& Location, const FVector& Dir, int32 Count, float Speed, AActor* StainOwner = nullptr);

	/** Poca de sangue embaixo do osso: aparece depois de Delay s (corpo no chao) e cresce ate MaxSize cm em GrowTime s. */
	static void Pool(UWorld* World, USkeletalMeshComponent* Mesh, FName Bone, float MaxSize, float GrowTime, float Delay);

	/** Apaga o sangue no chao deixado por este soldado (ao renascer / sair do jogo). */
	static void ClearStainsOf(AActor* Owner);

	/** Jato pulsante preso ao osso Bone (o ponto e a direcao acompanham o osso, mesmo em ragdoll). */
	static void Jet(UWorld* World, USkeletalMeshComponent* Mesh, FName Bone, const FVector& WorldPoint, const FVector& WorldDir, float Duration, float Rate,
		float JetSpeed = 320.f, bool bHeartbeat = true);

	/** Mancha no chao. Fica ate o dono renascer (com dono) ou ate passar de 300 no mundo (sem dono). Variant 0..2 = respingo, 3 = poca. */
	static class UDecalComponent* SpawnStain(UWorld* World, const FVector& Location, float Size, AActor* Owner = nullptr, int32 Variant = -1, int32 SortOrder = 0);

	/** Desligado com -EAWSemSangue (opcao do jogador, como pede o documento). */
	static bool IsGoreEnabled();

private:
	enum class EKind : uint8 { Streak, Drop, Mist };
	struct FDrop
	{
		FVector P;
		FVector V;
		float Age;
		float Life;
		float Size;
		float Roll;
		uint8 Atlas;
		EKind Kind;
	};

	UPROPERTY() UInstancedStaticMeshComponent* Sprites;      // filetes e gotas (M_Sangue_Sprite, mascarado e brilhante)
	UPROPERTY() UInstancedStaticMeshComponent* MistSprites;  // nevoa (M_Sangue_Nevoa, translucido)

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
	float JetSpeed = 320.f;
	float JetPhase = 0.f;
	bool bJetHeartbeat = true;   // false = jorro continuo que enfraquece (ferida que atravessa)
	TWeakObjectPtr<AActor> StainOwner;   // dono das manchas que este efeito deixa no chao

	// Poca
	TWeakObjectPtr<USkeletalMeshComponent> PoolMesh;
	FName PoolBone;
	TWeakObjectPtr<class UDecalComponent> PoolDecal;
	float PoolMax = 0.f;
	float PoolGrow = 1.f;
	float PoolDelay = 0.f;
	float PoolTime = -1.f;   // < 0 = sem poca
	void UpdatePool(float Dt);
	float EmitAccumulator = 0.f;

	void AddStreaks(const FVector& Location, const FVector& Dir, int32 Count, float Speed, float SpreadDeg, float SizeScale);
	void AddDrops(const FVector& Location, const FVector& Dir, int32 Count, float Speed, float SpreadDeg);
	void AddMist(const FVector& Location, const FVector& Dir, int32 Count, float Size);
	void FindGround(const FVector& From);

	static AEAWBloodFX* SpawnFX(UWorld* World, const FVector& Location);
	static UMaterialInterface* StainMaterial;
	static TArray<TWeakObjectPtr<class UDecalComponent>> Stains;
	static TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<class UDecalComponent>>> OwnedStains;
};
