#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MeleeTypes.h"
#include "BodyRegionLibrary.generated.h"

class UPrimitiveComponent;

UCLASS()
class EMPIRESATWAR_API UBodyRegionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Converte o nome de um osso em regiao do corpo (funciona com o manequim padrao e nomes comuns). */
	UFUNCTION(BlueprintPure, Category = "Ferimento")
	static EBodyRegion RegionFromBone(FName BoneName);

	/** Usa a tag do componente atingido (Head, Neck, Torso, Arms, Legs) - usado pelas hitboxes do prototipo. */
	UFUNCTION(BlueprintPure, Category = "Ferimento")
	static EBodyRegion RegionFromComponent(const UPrimitiveComponent* Component);

	/** Tenta o osso primeiro e depois a tag do componente. */
	UFUNCTION(BlueprintPure, Category = "Ferimento")
	static EBodyRegion RegionFromHit(const FHitResult& Hit);

	UFUNCTION(BlueprintPure, Category = "Ferimento")
	static bool IsVitalRegion(EBodyRegion Region) { return Region == EBodyRegion::Head || Region == EBodyRegion::Neck; }
};
