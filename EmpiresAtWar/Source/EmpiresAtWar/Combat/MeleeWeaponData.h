#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MeleeTypes.h"
#include "MeleeWeaponData.generated.h"

/**
 * Asset de dados de uma arma branca (sabre, espada, lanca, baioneta...).
 * Crie no editor: Content Browser > botao direito > Miscellaneous > Data Asset > MeleeWeaponData.
 */
UCLASS(BlueprintType)
class EMPIRESATWAR_API UMeleeWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arma")
	FMeleeWeaponConfig Config;

	/** Malha da arma (opcional). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arma")
	class UStaticMesh* Mesh = nullptr;
};
