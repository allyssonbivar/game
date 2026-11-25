#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/MeleeTypes.h"
#include "Weapons/EAWFirearmTypes.h"
#include "EAWFirearmComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFirearmState, EFirearmState, NewState, uint8, Stage);

/**
 * Mosquete de pederneira: erguer/mirar, disparo com tempo de fecho, bala com balistica (gravidade + arrasto
 * que muda com a velocidade), recarga em etapas (interrompida, guarda as etapas ja feitas).
 * Servidor decide o estado e os acertos; o dono calcula a mira (balanco e desvio) e manda o disparo.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EMPIRESATWAR_API UEAWFirearmComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEAWFirearmComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mosquete")
	FFirearmConfig Config;

	UPROPERTY(BlueprintAssignable)
	FOnFirearmState OnStateChanged;

	// ---------- Entrada (chamar no dono) ----------
	void RequestAim(bool bWant);
	void RequestFire();
	void RequestReload();
	/** Corrida, troca de arma, tecla X: larga a mira/recarga (etapas feitas ficam guardadas). */
	void RequestCancel();
	/** Volta ao inicio: carregado, em guarda. */
	void ResetFirearm();
	/** Servidor: arma pega do chao vem carregada ou nao. */
	void SetLoaded(bool bInLoaded);

	EFirearmState GetState() const { return Rep.State; }
	uint8 GetStage() const { return Rep.Stage; }
	uint8 GetSerial() const { return Rep.Serial; }
	bool IsLoaded() const { return Rep.bLoaded; }
	bool IsBusy() const { return Rep.State != EFirearmState::Lowered; }
	float GetStateDuration() const { return StateDuration; }
	float GetStateElapsed() const;
	uint8 GetSavedStage() const { return SavedStage; }

	/** Tempos das etapas da recarga na animacao (s, a 30 qps): escorva, carga, vareta, socar, guardar, armar. */
	static const float ReloadAnimTimes[7];
	float ReloadScale() const { return Config.ReloadTime / ReloadAnimTimes[6]; }

	/** Chamado pelo simulador de balas (servidor). */
	void HandleBallHit(const FHitResult& Hit, const FVector& Velocity);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Rep)
	FFirearmRep Rep;

	UFUNCTION()
	void OnRep_Rep(FFirearmRep Old);

	UFUNCTION(Server, Reliable) void ServerAim(bool bWant);
	UFUNCTION(Server, Reliable) void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);
	UFUNCTION(Server, Reliable) void ServerReload();
	UFUNCTION(Server, Reliable) void ServerCancel();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShot(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBallImpact(FVector_NetQuantize Location, FVector_NetQuantizeNormal Dir, AActor* Victim, FName Bone, float EnergyJ, EBodyRegion Region);

	void SetState(EFirearmState NewState, uint8 Stage, float Duration);
	void OnStateTimer();
	void DoFireLocal();

	bool bWantAim = false;
	bool bReloadAfterLower = false;
	bool bLockPending = false;
	uint8 SavedStage = 0;
	float StateDuration = 0.f;
	float StateStartTime = 0.f;
	FTimerHandle StateTimer;
	FTimerHandle LockTimer;
};
