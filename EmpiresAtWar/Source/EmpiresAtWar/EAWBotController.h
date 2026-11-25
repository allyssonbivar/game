#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EAWBotController.generated.h"

class AEAWCharacter;

/**
 * Bot de treino para testar o combate sozinho: anda ate o jogador,
 * ataca em intervalos aleatorios, tenta parry quando ve um golpe chegando
 * e usa o contra-ataque quando o parry da certo.
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

	/** Bot parado so para receber golpes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bot")
	bool bPassive = false;

private:
	AEAWCharacter* FindTarget() const;

	float NextAttackTime = 0.f;
	uint8 LastJudgedSerial = 255;
	bool bFeintDecided = false;
};
