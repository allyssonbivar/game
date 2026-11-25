#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EAWEditorTools.generated.h"

class USkeletalMesh;

/** Ferramentas do editor chamadas pelos scripts Python (Scripts/*.py). Nao fazem nada no jogo final. */
UCLASS()
class EMPIRESATWAR_API UEAWEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Refaz o PhysicsAsset do esqueleto com um corpo por osso principal (pelve, coluna, pescoco, cabeca,
	 * bracos, antebracos, maos, coxas, canelas, pes). O gerado na importacao tinha so 4 corpos (bug #41).
	 * Devolve a lista de corpos criados.
	 */
	UFUNCTION(BlueprintCallable, Category = "EAW|Editor")
	static FString RebuildPhysicsAsset(USkeletalMesh* Mesh, float MinBoneSize = 6.f);
};
