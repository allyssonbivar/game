#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EAWGameMode.generated.h"

class AEAWCharacter;

UCLASS()
class EMPIRESATWAR_API AEAWGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEAWGameMode();

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Cria um bot de treino na frente do personagem. */
	UFUNCTION(BlueprintCallable, Category = "Prototipo")
	AEAWCharacter* SpawnBotNear(APawn* Near);

	/** Cria um bot automaticamente quando o primeiro jogador entra. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prototipo")
	bool bSpawnBotOnStart = false;   // sem bot automatico (B cria um para treinar)

	/** Chao de teste (-EAWCampo), criado igual em cada maquina. */
	static void SpawnTestField(UWorld* World);

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

private:
	bool bSpawnedInitialBot = false;
	bool bFieldSpawned = false;
	int32 SpawnCount = 0;

	// ---------- Modo de teste ----------
	// -EAWDuelo     : sem jogador; dois bots em duelo e uma camera fixa de lado.
	// -EAWShots=N   : tira N capturas de tela (a cada EAWShotEvery s, padrao 0.2) e fecha o jogo.
	void SetupDuel(class APlayerController* Viewer);
	void TakeShot();
	// -EAWBotPassivo : bots so se defendem/ficam parados.
	// -EAWAutoAtaque : o soldado do jogador ataca sozinho (corte, corte, de cima, estocada, parry) em ciclo.
	void ConfigureBot(class AEAWCharacter* Bot);
	void AutoAttack();
	TWeakObjectPtr<class APlayerController> AutoPC;
	int32 AutoStep = 0;
	FTimerHandle AutoTimer;
	// -EAWAutoTiro : o soldado do jogador mira 1,5 s, dispara, espera e recarrega, em ciclo.
	// -EAWBotDist=N : distancia (cm) do bot de treino (padrao 300).
	void AutoFire();
	// -EAWAutoEspecial : o soldado do jogador carrega o golpe especial 3 s e solta, em ciclo.
	void AutoSpecial();
	float AutoPhaseTime = 0.f;
	int32 AutoPhase = 0;
	int32 ShotsLeft = 0;
	int32 ShotIndex = 0;
	FTimerHandle ShotTimer;
};
