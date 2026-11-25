#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/EAWFirearmTypes.h"
#include "EAWDroppedWeapon.generated.h"

class UBoxComponent;
class USkeletalMeshComponent;

/**
 * Arma caida no chao (de um morto ou largada ao pegar outra). Tem fisica (caixa do tamanho da arma)
 * e pode ser pega segurando F por 3 s. Replicada: todos veem a mesma arma no mesmo lugar.
 */
UCLASS()
class EMPIRESATWAR_API AEAWDroppedWeapon : public AActor
{
	GENERATED_BODY()

public:
	AEAWDroppedWeapon();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Servidor: cria a arma no mundo com a velocidade de quem largou. */
	static AEAWDroppedWeapon* Drop(UWorld* World, EEAWWeapon Weapon, const FTransform& At, const FVector& Velocity, bool bLoaded);

	EEAWWeapon GetWeapon() const { return Weapon; }
	bool IsLoaded() const { return bLoaded; }
	/** Nome para o aviso na tela. */
	static FText WeaponName(EEAWWeapon W);

protected:
	UPROPERTY(VisibleAnywhere) UBoxComponent* Box;
	UPROPERTY(VisibleAnywhere) USkeletalMeshComponent* Visual;

	UPROPERTY(ReplicatedUsing = OnRep_Weapon) EEAWWeapon Weapon = EEAWWeapon::Sabre;
	UPROPERTY(Replicated) bool bLoaded = true;   // mosquete largado carregado continua carregado

	UFUNCTION() void OnRep_Weapon();
	FVector VisualCenter = FVector::ZeroVector;   // centro da arma no espaco do osso da arma
	void ApplyVisual();
};
