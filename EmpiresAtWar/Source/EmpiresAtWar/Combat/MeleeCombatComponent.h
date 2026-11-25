#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeTypes.h"
#include "MeleeCombatComponent.generated.h"

class UMeleeWeaponData;
class UWoundComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMeleeStateChanged, EMeleeState, NewState, EAttackDirection, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnMeleeImpact, FVector, Location, EBodyRegion, Region, bool, bParried, AActor*, Victim);

/**
 * Combate corpo a corpo direcional.
 *
 * Fluxo de um golpe: Idle -> Windup -> Release -> Recovery -> Idle
 *   - Windup: pode fintar (Feint) ou trocar para Parry.
 *   - Release: a lamina e rastreada a cada quadro; acerta quem ela atravessar.
 *   - Release/Recovery: apertar ataque encadeia combo (preparacao mais curta).
 *   - Parry no tempo certo: o atacante fica atordoado e o defensor ganha um contra-ataque rapido (riposte).
 *
 * O servidor e a autoridade. O dono envia pedidos por RPC; o estado volta replicado.
 * A arma e movida de forma procedural (WeaponPivot) ate existirem animacoes proprias.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EMPIRESATWAR_API UMeleeCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeCombatComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------- Configuracao ----------

	/** Arma em uso. Se vazio, usa DefaultConfig (sabre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combate")
	UMeleeWeaponData* WeaponData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combate")
	FMeleeWeaponConfig DefaultConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaRegenPerSecond = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaRegenDelay = 0.8f;

	/** Desenha a lamina e os acertos (para testar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDraw = true;

	/** Componente que a arma segue (o personagem define). */
	UFUNCTION(BlueprintCallable, Category = "Combate")
	void SetWeaponPivot(USceneComponent* InPivot);

	/** Usa as animacoes do esqueleto: a lamina e lida dos sockets GripSocket -> TipSocket. */
	UFUNCTION(BlueprintCallable, Category = "Combate")
	void SetAnimatedWeapon(class USkeletalMeshComponent* InMesh);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combate|Animacao")
	FName GripSocket = TEXT("weapon_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combate|Animacao")
	FName TipSocket = TEXT("weapon_r_tip");

	/** Distancia do centro do cabo ate o inicio da lamina (cm), no modo animado. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combate|Animacao")
	float AnimBladeStart = 7.5f;

	/** Comprimento da parte que corta (cm). 0 = ate o socket da ponta (sabre/mosquete); faca e machado usam valores curtos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combate|Animacao")
	float AnimBladeLength = 0.f;

	UFUNCTION(BlueprintPure, Category = "Combate")
	float GetStateDuration() const { return Rep.Duration; }

	const FMeleeWeaponConfig& GetConfig() const;

	// ---------- Comandos (chame no dono; vao ao servidor) ----------

	UFUNCTION(BlueprintCallable, Category = "Combate")
	void RequestAttack(EAttackDirection Direction);

	/** Corte horizontal alternando lado a cada golpe. */
	UFUNCTION(BlueprintCallable, Category = "Combate")
	void RequestSlash();

	UFUNCTION(BlueprintCallable, Category = "Combate")
	void RequestParry();

	UFUNCTION(BlueprintCallable, Category = "Combate")
	void RequestFeint();

	// ---------- Consulta ----------

	UFUNCTION(BlueprintPure, Category = "Combate")
	EMeleeState GetState() const { return Rep.State; }

	UFUNCTION(BlueprintPure, Category = "Combate")
	EAttackDirection GetDirection() const { return Rep.Direction; }

	/** Progresso do estado atual (0..1) - util para AnimBP. */
	UFUNCTION(BlueprintPure, Category = "Combate")
	float GetStateAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Combate")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Combate")
	bool HasRiposte() const;

	/** Usado pela IA para "ler" o adversario. */
	UFUNCTION(BlueprintPure, Category = "Combate")
	float GetTimeToRelease() const;

	UPROPERTY(BlueprintAssignable, Category = "Combate")
	FOnMeleeStateChanged OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combate")
	FOnMeleeImpact OnImpact;

	/** Muda a cada troca de estado (a IA usa para reagir uma vez por golpe). */
	uint8 GetStateSerial() const { return Rep.Serial; }

	/** Chamado pelo servidor quando o dono e incapacitado ou morre. */
	void ForceIncapacitated();

	/** Volta a guarda com stamina cheia (servidor; usado em teste/respawn). */
	void ResetCombat();

protected:
	// RPCs
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerAttack(EAttackDirection Direction);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerParry();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFeint();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(FVector_NetQuantize Location, EBodyRegion Region, bool bParried, AActor* Victim);

	UFUNCTION()
	void OnRep_Rep(const FMeleeStateRep& Old);

	// Logica do servidor
	void HandleAttack(EAttackDirection Direction);
	void HandleParry();
	void HandleFeint();
	void EnterState(EMeleeState NewState, EAttackDirection Direction, float Duration);
	void OnStateFinished();
	void StartWindup(EAttackDirection Direction, float WindupTime);
	void TraceBlade();
	void ResolveHit(const FHitResult& Hit);
	bool SpendStamina(float Amount);
	bool IsFacing(const AActor* Defender, const AActor* Attacker, float HalfAngleDeg) const;

	// Visual (todas as maquinas)
	void UpdateWeaponPose();
	FMeleePose ComputePose() const;
	FTransform PoseToRelative(const FMeleePose& Pose) const;
	void GetBladePoints(TArray<FVector>& OutPoints) const;

	float Now() const;
	float TimeMultiplier() const;
	UWoundComponent* GetWounds() const;

	UPROPERTY(ReplicatedUsing = OnRep_Rep)
	FMeleeStateRep Rep;

	UPROPERTY(Replicated)
	float Stamina = 100.f;

	UPROPERTY()
	USceneComponent* WeaponPivot = nullptr;

	FVector PivotBaseLocation = FVector::ZeroVector;

	UPROPERTY()
	class USkeletalMeshComponent* AnimMesh = nullptr;

	// So no servidor
	bool bComboQueued = false;
	EAttackDirection QueuedDirection = EAttackDirection::SlashLeft;
	float RiposteUntil = -1.f;
	float LastStaminaSpendTime = -100.f;
	TArray<FVector> PrevBladePoints;
	UPROPERTY()
	TArray<AActor*> HitThisSwing;

	// Dono local: alterna o lado do corte
	bool bNextSlashLeft = true;

	// Pose de onde partem finta/atordoamento (calculada localmente)
	FMeleePose TransitionFromPose;
	FMeleePose LastPose;
};
