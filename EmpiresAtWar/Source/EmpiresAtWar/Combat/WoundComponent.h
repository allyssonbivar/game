#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeTypes.h"
#include "WoundComponent.generated.h"

UENUM(BlueprintType)
enum class EWoundLife : uint8
{
	Healthy,
	Wounded,
	Incapacitated,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWounded, EBodyRegion, Region, float, Damage, EWoundLife, NewLife);

/**
 * Modelo de ferimentos do documento (secao 9):
 * - impacto vital (cabeca/pescoco forte, ou impacto de energia muito alta) mata;
 * - ate 2 ferimentos nao vitais; o segundo incapacita;
 * - cada regiao ferida aplica uma penalidade (braco: golpes mais lentos; perna: anda mais devagar).
 * O servidor decide tudo; os clientes recebem o estado replicado.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EMPIRESATWAR_API UWoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWoundComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Dano minimo em cabeca/pescoco para ser fatal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ferimento")
	float VitalDamageThreshold = 60.f;

	/** Qualquer impacto acima disto e fatal, em qualquer regiao (ex.: bala de canhao). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ferimento")
	float LethalEnergyThreshold = 150.f;

	/** Impactos abaixo disto nao contam como ferimento. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ferimento")
	float MinWoundDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ferimento")
	int32 MaxNonVitalWounds = 2;

	/** Multiplicador da duracao dos golpes com braco ferido. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Penalidade")
	float ArmWoundTimeMultiplier = 1.2f;

	/** Multiplicador da velocidade com perna ferida. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Penalidade")
	float LegWoundSpeedMultiplier = 0.6f;

	/** Multiplicador da regeneracao de stamina com tronco ferido. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Penalidade")
	float TorsoWoundStaminaRegenMultiplier = 0.6f;

	UPROPERTY(BlueprintAssignable, Category = "Ferimento")
	FOnWounded OnWounded;

	/** Ponto de entrada do dano (servidor). Retorna o novo estado. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ferimento")
	EWoundLife ApplyImpact(EBodyRegion Region, float Damage);

	UFUNCTION(BlueprintPure, Category = "Ferimento")
	EWoundLife GetLife() const { return Life; }

	UFUNCTION(BlueprintPure, Category = "Ferimento")
	bool CanFight() const { return Life == EWoundLife::Healthy || Life == EWoundLife::Wounded; }

	UFUNCTION(BlueprintPure, Category = "Ferimento")
	bool HasWound(EBodyRegion Region) const;

	UFUNCTION(BlueprintPure, Category = "Ferimento")
	int32 GetNonVitalWounds() const { return NonVitalWounds; }

	UFUNCTION(BlueprintPure, Category = "Penalidade")
	float GetAttackTimeMultiplier() const { return HasWound(EBodyRegion::Arms) ? ArmWoundTimeMultiplier : 1.f; }

	UFUNCTION(BlueprintPure, Category = "Penalidade")
	float GetMoveSpeedMultiplier() const { return HasWound(EBodyRegion::Legs) ? LegWoundSpeedMultiplier : 1.f; }

	UFUNCTION(BlueprintPure, Category = "Penalidade")
	float GetStaminaRegenMultiplier() const { return HasWound(EBodyRegion::Torso) ? TorsoWoundStaminaRegenMultiplier : 1.f; }

	/** Volta ao estado saudavel (teste / respawn). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ferimento")
	void ResetWounds();

protected:
	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation,
		class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection,
		const class UDamageType* DamageType, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_Life(EWoundLife OldLife);

	UPROPERTY(ReplicatedUsing = OnRep_Life)
	EWoundLife Life = EWoundLife::Healthy;

	UPROPERTY(Replicated)
	int32 NonVitalWounds = 0;

	/** Bits por regiao ferida (1 << EBodyRegion). */
	UPROPERTY(Replicated)
	uint8 WoundedRegionBits = 0;

	UPROPERTY(Replicated)
	EBodyRegion LastRegion = EBodyRegion::None;
};
