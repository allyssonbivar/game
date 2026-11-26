#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EAWBotController.generated.h"

class AEAWCharacter;

/** Fase do bot: de longe atira, depois carrega e luta corpo a corpo. */
UENUM()
enum class EBotPhase : uint8
{
	Approach,   // anda ate a distancia de tiro
	Shoot,      // para, mira e atira com o mosquete
	Reload,     // recarrega (o alvo ainda longe)
	Charge,     // troca para a arma branca e vem correndo
	Melee       // luta: defesa, combos, chute, golpe de salto
};

/**
 * Bot de treino (2026-11-26): nasce longe (35 m com o campo de teste), anda ate ~25 m, atira com o mosquete (o tiro tem o
 * desvio do cano liso + erro de mira de uma pessoa), recarrega se o alvo continuar longe, depois troca para a arma branca e vem
 * correndo (golpe correndo as vezes). Na luta: parry ou defesa segurada, combos de 2/3 cliques e 2 cliques + E, chute em quem
 * segura a defesa, golpe de salto de vez em quando, e anda de lado em volta do alvo.
 */
UCLASS()
class EMPIRESATWAR_API AEAWBotController : public AAIController
{
	GENERATED_BODY()

public:
	AEAWBotController();

	virtual void Tick(float DeltaSeconds) override;

	/** Chance de defender cada golpe (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float ParryChance = 0.55f;

	/** Das defesas, quantas viram defesa segurada (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float HoldBlockChance = 0.35f;

	/** Quanto antes do golpe chegar o bot tenta o parry (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float ReactionTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float AttackIntervalMin = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float AttackIntervalMax = 2.8f;

	/** Chance de fintar um golpe ja iniciado. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float FeintChance = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float EngageDistance = 150.f;

	/** Distancia de tiro (cm): para e atira daqui. Mais perto que MeleeDistance, desiste do tiro e carrega. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float ShootDistance = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	float MeleeDistance = 900.f;

	/** Quantos tiros antes de carregar (recarrega entre eles se o alvo continuar longe). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	int32 MaxShots = 2;

	/** Bot parado so para receber golpes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	bool bPassive = false;

	EBotPhase GetPhase() const { return Phase; }

private:
	AEAWCharacter* FindTarget() const;
	void SetPhase(EBotPhase NewPhase);
	void TickRanged(AEAWCharacter* Me, AEAWCharacter* Target, float Distance, float Now);
	void TickCharge(AEAWCharacter* Me, AEAWCharacter* Target, float Distance, float Now);
	void TickMelee(AEAWCharacter* Me, AEAWCharacter* Target, float Distance, float Now);

	EBotPhase Phase = EBotPhase::Approach;
	float PhaseStart = 0.f;
	int32 ShotsFired = 0;
	float AimSince = -1.f;
	float AimHold = 1.f;

	float NextAttackTime = 0.f;
	uint8 LastJudgedSerial = 255;
	bool bFeintDecided = false;

	// Combo planejado: cliques que ainda faltam e se o ultimo e o E (estocada final).
	int32 ComboClicksLeft = 0;
	bool bComboEndsWithE = false;
	uint8 LastComboSerial = 255;

	float BlockReleaseTime = -1.f;   // defesa segurada: solta neste tempo
	float NextKickTime = 0.f;
	float JumpAttackAt = -1.f;
	bool bTriedSprintAttack = false;
	float StrafeSeed = 0.f;
	int8 PreferBayonet = -1;   // -1 = ainda nao sabe; 1 = bot de baioneta (luta com o mosquete)
};
