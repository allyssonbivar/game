#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/MeleeTypes.h"
#include "Combat/WoundComponent.h"
#include "Weapons/EAWFirearmTypes.h"
#include "EAWCharacter.generated.h"

class UEAWFirearmComponent;

class UCameraComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UMeleeCombatComponent;

/**
 * Soldado do prototipo: camera em primeira pessoa, arma procedural,
 * hitboxes por regiao do corpo (ate existir um esqueleto proprio) e ferimentos.
 */
UCLASS()
class EMPIRESATWAR_API AEAWCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEAWCharacter();

	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;

	// ---------- Bracos de 1a pessoa ----------
	/** Inclinacao para cima dos bracos de 1a pessoa em relacao a camera (graus). Teste: -EAWFPPitch= */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1a pessoa")
	float FPPitch = 14.f;

	/** Deslocamento dos bracos em relacao a camera (cm; X frente, Z cima). Teste: -EAWFPX= -EAWFPZ= */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1a pessoa")
	FVector FPOffset = FVector(-14.f, 10.f, 14.f);   // testado 2026-11-24: ombro atras da camera, sabre a direita

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "1a pessoa")
	float FPFieldOfView = 95.f;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UMeleeCombatComponent* GetCombat() const { return Combat; }
	UWoundComponent* GetWounds() const { return Wounds; }
	/** Servidor, antes do dano que pode matar: 1 = tiro no pescoco, 2 = golpe especial. */
	void MarkDeathAnim(uint8 Kind);
	/** Servidor, depois do dano: o soldado sobreviveu, desmarca. */
	void ClearDeathAnimMark();

	/** Trote normal (3,0 m/s; antes 1,4 - lento demais). As animacoes de trote foram feitas nessas velocidades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movimento")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movimento")
	float SprintSpeed = 500.f;
	/** Andando agachado (cm/s) - a animacao foi feita para 100 cm/s para a frente. */
	UPROPERTY(EditAnywhere, Category = "Movimento") float CrouchSpeed = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movimento")
	float BackSpeedScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movimento")
	float SideSpeedScale = 0.75f;

	/** Parado, o corpo so vira os pes quando a mira passa deste angulo (ate la o tronco torce). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movimento")
	float TurnInPlaceAngle = 70.f;
	/** Giro do corpo com aceleracao (graus/s e graus/s^2): comeca e para suave, sem o tronco inverter de repente. */
	float TurnRateNow = 0.f;
	float TurnAccel = 1800.f;
	/** Teclas de andar deste quadro: juntas viram uma direcao so (diagonal na velocidade certa). */
	float AxisForward = 0.f, AxisRight = 0.f;

	virtual void Landed(const FHitResult& Hit) override;

	/** Mostra as hitboxes (cabeca, pescoco, tronco, bracos, pernas). Tecla H. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowHitboxes = true;

	/** Volta todos os soldados ao estado inicial (console: EAWReset). */
	UFUNCTION(Exec)
	void EAWReset();

	/** Cria um bot de treino na sua frente (console: EAWBot). */
	UFUNCTION(Exec)
	void EAWBot();

	/** Mostra/esconde as hitboxes deste soldado. */
	void SetShowHitboxes(bool bShow);

protected:
	// Componentes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	USceneComponent* AimRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	USceneComponent* WeaponPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	UStaticMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	UStaticMeshComponent* BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	UMeleeCombatComponent* Combat;

	/** Copia do soldado presa a camera, vista so pelo dono (bracos + sabre). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	class USkeletalMeshComponent* FirstPersonMesh;

	void SetupFirstPerson();
	bool bFirstPersonActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes")
	UWoundComponent* Wounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hitboxes")
	TArray<UCapsuleComponent*> Hitboxes;

	// Entrada
	void MoveForward(float Value);
	void MoveRight(float Value);
	void StartSprint();
	/** C ou Ctrl: agacha/levanta. So com mosquete (modo tiro) ou pistola; correr, pular ou trocar para arma branca levanta. */
	void InputCrouch();
	/** V: chute (so no corpo a corpo, inclusive baioneta). */
	void InputKick();
	void ComposeMoveInput();
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	void InputJump();
	bool CanCrouchWithWeapon() const;
	void StopSprint();
	void InputSlash();
	void InputSlashReleased();
	/** Golpe especial: segurando o ataque com W, carrega 3 s (braco para tras) e solta sozinho com W apertado. */
	void UpdateCharge();
	/** Fim do trecho tocado nos bracos de 1a pessoa: sem laco, a pose para e fica ali (como no corpo de 3a pessoa). */
	float FPSegEnd = 0.f;
	bool bFPSegLoop = true;
	bool bAttackHeld = false;
	/** Efeitos do golpe especial que atravessa o corpo (todas as maquinas). */
	void PlaySpecialHitEffects(class AEAWCharacter* Victim, const FVector& Location, const FVector& Dir);
	UPROPERTY() class UAnimSequence* AnimSabreEspecial = nullptr;   UPROPERTY() class UAnimSequence* AnimSabreEspecialFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMEspecial = nullptr;       UPROPERTY() class UAnimSequence* AnimMEspecialFP = nullptr;
	void InputOverhead();
	void InputStab();
	void InputParry();
	void InputFeint();
	void InputToggleHitboxes();
	void InputSpawnBot();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetSprint(bool bNewSprint);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSpawnBot();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResetAll();

	UFUNCTION()
	void HandleWounded(EBodyRegion Region, float Damage, EWoundLife NewLife);

	void ApplyMovementSpeed();
	void ApplyLifeVisuals();
	void ResetSoldier();
	void DrawDebugHud();

	UCapsuleComponent* MakeHitbox(FName Name, FName RegionTag, float Radius, float HalfHeight, const FVector& Location, const FRotator& Rotation);

	// ---------- Animacoes do sabre (importadas do Blender) ----------
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimGuarda = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimCorteEsq = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimCorteDir = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimDeCima = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimEstocada = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimParry = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimBloqueado = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao") class UAnimSequence* AnimFinta = nullptr;

	// Movimento (pernas)
	UPROPERTY(EditAnywhere, Category = "Animacao|Movimento") class UAnimSequence* AnimAndarFrente = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Movimento") class UAnimSequence* AnimAndarTras = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Movimento") class UAnimSequence* AnimAndarEsq = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Movimento") class UAnimSequence* AnimAndarDir = nullptr;
	// Trote nas diagonais (2026-11-26): frente-dir, frente-esq, tras-dir, tras-esq.
	UPROPERTY() class UAnimSequence* AnimTroteDiag[4] = { nullptr, nullptr, nullptr, nullptr };
	UPROPERTY(EditAnywhere, Category = "Animacao|Movimento") class UAnimSequence* AnimCorrer = nullptr;

	// Versoes de 1a pessoa (bracos presos a camera). Se faltar, os bracos usam a de corpo inteiro.
	UPROPERTY(EditAnywhere, Category = "Animacao|1a pessoa") class UAnimSequence* AnimCorteEsqFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|1a pessoa") class UAnimSequence* AnimCorteDirFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|1a pessoa") class UAnimSequence* AnimDeCimaFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|1a pessoa") class UAnimSequence* AnimParryFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|1a pessoa") class UAnimSequence* AnimBloqueadoFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|1a pessoa") class UAnimSequence* AnimFintaFP = nullptr;

	/** true quando o esqueleto e as animacoes foram encontrados. */
	bool bAnimated = false;

	UFUNCTION()
	void HandleCombatState(EMeleeState NewState, EAttackDirection Direction);

	/** Toca o trecho [Start, End] (s) da animacao em exatamente Duration segundos. */
	void PlaySegment(class UAnimSequence* Anim, float Start, float End, float Duration, bool bLoop, class UAnimSequence* FPAnim = nullptr);

	void StartRagdoll();

	// ---------- Mortes com animacao (em vez do ragdoll) ----------
	// 1 = tiro no pescoco: AS_Morte_Pescoco (Mixamo "Dying") + arteria esguichando no ritmo do coracao.
	// 2 = golpe especial (faca, espada, baioneta): AS_Morte_Especial (Mixamo, "dycomboFacaEspadaBaioneta") + ferida sangrando.
	// Nas duas: poca de sangue crescendo embaixo do corpo; o sangue no chao fica ate o soldado renascer.
	// O servidor marca DeathAnimKind antes do dano; o valor chega aos clientes no mesmo pacote do estado
	// "fora de combate", entao todas as maquinas tocam a mesma morte.
	UPROPERTY(ReplicatedUsing = OnRep_DeathAnim) uint8 DeathAnimKind = 0;
	UFUNCTION() void OnRep_DeathAnim();
	bool bAnimDying = false;
	bool bDeathBegun = false;
	void StartAnimDeath();
	void EndAnimDeath();
	/** Comum as duas mortes: a camera do dono, larga a arma, desliga capsula e hitboxes. */
	void BeginDeathCommon();
	UPROPERTY() class UAnimSequence* AnimMortePescoco = nullptr;
	UPROPERTY() class UAnimSequence* AnimMorteEspecial = nullptr;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---------- Peso e fisica ----------
	/** Giro maximo da camera (graus/s) durante preparacao e golpe - o golpe "compromete" o corpo. */
	UPROPERTY(EditAnywhere, Category = "Peso") float TurnCapWindup = 220.f;
	UPROPERTY(EditAnywhere, Category = "Peso") float TurnCapRelease = 110.f;
	/** Passo a frente no inicio do golpe (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Peso") float LungeSlash = 160.f;
	UPROPERTY(EditAnywhere, Category = "Peso") float LungeStab = 240.f;
	UPROPERTY(EditAnywhere, Category = "Peso") float LungeSpecial = 900.f;

	void TurnInput(float Value);
	void LookUpInput(float Value);

	UFUNCTION()
	void HandleImpact(FVector Location, EBodyRegion Region, bool bParried, AActor* Victim);

	/** Reacao a um golpe recebido: empurrao, tronco com fisica por um instante, tremor de camera. */
	void ReactToHit(const FVector& Dir, bool bParried, float Strength);
	void AddTrauma(float Amount) { Trauma = FMath::Min(1.f, Trauma + Amount); }
	static void HitStop(AActor* Actor, float Seconds);
	void UpdateFeel(float DeltaSeconds);

	float Trauma = 0.f;             // tremor de camera (0..1), cai com o tempo
	float HitReactAlpha = 0.f;      // fisica do tronco (0..1), cai com o tempo
	FVector LastHitDir = FVector::ZeroVector;
	float ShakeTime = 0.f;

	// ---------- Sangue e desmembramento ----------
public:
	/** Vermelho na tela do dono quando e atingido (0..1). Lido pelo AEAWHUD. */
	float GetDamageFlash() const { return DamageFlash; }

	/** Registra o golpe recebido (todas as maquinas); se o soldado caiu, tenta desmembrar. */
	void NoteHit(EBodyRegion Region, const FVector& Location, const FVector& Dir, bool bAllowDismember = true);

	// ---------- Armas ----------
	virtual void PossessedBy(AController* NewController) override;
	UEAWFirearmComponent* GetFirearm() const { return Firearm; }
	EEAWWeapon GetWeapon() const { return CurrentWeapon; }
	bool IsBayonetMode() const { return bBayonet; }
	/** Servidor: troca a arma na mao (sabre/mosquete). */
	void EquipWeapon(EEAWWeapon NewWeapon);
	/** Dono: origem (olho) e direcao do tiro, com o balanco da arma e o desvio do cano liso. */
	void GetShotAim(FVector& Origin, FVector& Dir);
	/** Todas as maquinas: clarao, fumaca, coice. */
	void PlayShotEffects(const FVector& Dir);
	/** Todas as maquinas: ferida de bala no corpo, sangue, reacao ou queda. */
	void ReceiveBullet(const FVector& Location, const FVector& Dir, FName Bone, float EnergyJ, EBodyRegion Region);

protected:
	/** Pecas soltas (mesmo esqueleto do soldado, so os vertices do membro). */
	UPROPERTY(EditAnywhere, Category = "Sangue") class USkeletalMesh* PecaCabeca = nullptr;
	UPROPERTY(EditAnywhere, Category = "Sangue") class USkeletalMesh* PecaBracoE = nullptr;
	UPROPERTY(EditAnywhere, Category = "Sangue") class USkeletalMesh* PecaBracoD = nullptr;
	UPROPERTY(EditAnywhere, Category = "Sangue") class USkeletalMesh* PecaPernaE = nullptr;
	UPROPERTY(EditAnywhere, Category = "Sangue") class USkeletalMesh* PecaPernaD = nullptr;
	UPROPERTY() class UStaticMesh* CapMesh = nullptr;
	UPROPERTY() class UMaterialInterface* CapMaterial = nullptr;

	/** Componentes criados pelo desmembramento (pecas e tampas de sangue), apagados no reset. */
	UPROPERTY() TArray<UPrimitiveComponent*> GoreComponents;

	void TryDismember();
	void ClearGore();
	void AddCap(USceneComponent* Parent, FName Bone, const FVector& Point, const FVector& Dir, float Diameter);

	bool bDismemberAllowed = true;
	int32 WoundMarks = 0;

	// ---------- Mosquete ----------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes") UEAWFirearmComponent* Firearm;
	/** Arma vista pelos outros (segue o corpo) e pelo dono (segue os bracos de 1a pessoa). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes") USkeletalMeshComponent* WeaponTP;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Componentes") USkeletalMeshComponent* WeaponFP;
	UPROPERTY(EditAnywhere, Category = "Armas") class USkeletalMesh* MeshMosquete = nullptr;
	UPROPERTY(EditAnywhere, Category = "Armas") class USkeletalMesh* MeshSabre = nullptr;
	UPROPERTY(EditAnywhere, Category = "Armas") class USkeletalMesh* MeshFaca = nullptr;
	UPROPERTY(EditAnywhere, Category = "Armas") class USkeletalMesh* MeshMachado = nullptr;
	UPROPERTY(EditAnywhere, Category = "Armas") class USkeletalMesh* MeshPistola = nullptr;
	// Pistola An IX (uma mao): guarda, erguer, mira, tiro, baixar, recarga (+ 1a pessoa).
	UPROPERTY() class UAnimSequence* AnimPGuarda = nullptr;  UPROPERTY() class UAnimSequence* AnimPGuardaFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimPErguer = nullptr;  UPROPERTY() class UAnimSequence* AnimPErguerFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimPMira = nullptr;    UPROPERTY() class UAnimSequence* AnimPMiraFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimPTiro = nullptr;    UPROPERTY() class UAnimSequence* AnimPTiroFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimPBaixar = nullptr;  UPROPERTY() class UAnimSequence* AnimPBaixarFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimPRecarga = nullptr; UPROPERTY() class UAnimSequence* AnimPRecargaFP = nullptr;
	/** Mosquete/pistola: valores de tiro e etapas da recarga de cada arma. */
	void ApplyFirearmConfig();

	UPROPERTY(ReplicatedUsing = OnRep_Weapon) EEAWWeapon CurrentWeapon = EEAWWeapon::Sabre;
	UPROPERTY(ReplicatedUsing = OnRep_Weapon) bool bBayonet = false;
	UFUNCTION() void OnRep_Weapon();
	void ApplyWeaponVisuals();
	void ApplyFirstPersonLayout();
	/** Arma presa a mao (ragdoll) ou de volta ao esqueleto. */
	void AttachWeaponToHand(USceneComponent* HandOwner);
	void AttachWeaponToSkeleton();

	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMGuarda = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMErguer = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMMira = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMTiro = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMBaixar = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMRecarga = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMGuardaFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMErguerFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMMiraFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMTiroFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMBaixarFP = nullptr;
	UPROPERTY(EditAnywhere, Category = "Animacao|Mosquete") class UAnimSequence* AnimMRecargaFP = nullptr;
	// Combate com o mosquete (tecla X): coronhada, estocada, estocada longa, aparos, bloqueado, finta (+ 1a pessoa).
	UPROPERTY() class UAnimSequence* AnimMCoronhada = nullptr;      UPROPERTY() class UAnimSequence* AnimMCoronhadaFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMEstocada = nullptr;       UPROPERTY() class UAnimSequence* AnimMEstocadaFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMEstocadaLonga = nullptr;  UPROPERTY() class UAnimSequence* AnimMEstocadaLongaFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMAparoDir = nullptr;       UPROPERTY() class UAnimSequence* AnimMAparoDirFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMAparoEsq = nullptr;       UPROPERTY() class UAnimSequence* AnimMAparoEsqFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMBloqueado = nullptr;      UPROPERTY() class UAnimSequence* AnimMBloqueadoFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMFinta = nullptr;          UPROPERTY() class UAnimSequence* AnimMFintaFP = nullptr;
	// Baioneta como no video de referencia x1 (2026-11-26): guarda com o mosquete na diagonal do peito e defesa alta (horizontal sobre a cabeca).
	UPROPERTY() class UAnimSequence* AnimMGuardaBaioneta = nullptr; UPROPERTY() class UAnimSequence* AnimMGuardaBaionetaFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimMAparoAlto = nullptr;      UPROPERTY() class UAnimSequence* AnimMAparoAltoFP = nullptr;
	// Agachado (so com arma de fogo): pernas e altura do quadril; o tronco continua tocando a arma (mirar, atirar, recarregar).
	UPROPERTY() class UAnimSequence* AnimAgachadoParado = nullptr;
	// Fase 2 do combate: golpe correndo, golpe de salto, chute (+ chute com o mosquete na guarda da baioneta).
	UPROPERTY() class UAnimSequence* AnimSabreCorrendo = nullptr;
	UPROPERTY() class UAnimSequence* AnimSabreSalto = nullptr;
	UPROPERTY() class UAnimSequence* AnimSabreCorrendoFP = nullptr;   UPROPERTY() class UAnimSequence* AnimSabreSaltoFP = nullptr;
	UPROPERTY() class UAnimSequence* AnimChute = nullptr;
	UPROPERTY() class UAnimSequence* AnimMChute = nullptr;   UPROPERTY() class UAnimSequence* AnimMChuteFP = nullptr;
	/** O proximo trecho de combate vale para o corpo inteiro (pernas tambem): chute e golpe de salto. */
	bool bSegmentFullBody = false;
	/** Ultima defesa tocada (a defesa segurada e a volta a guarda usam a mesma, sem trocar de pose no meio). */
	UPROPERTY() class UAnimSequence* LastParryAnim = nullptr;
	UPROPERTY() class UAnimSequence* LastParryAnimFP = nullptr;
	/** Baioneta: escolhe a defesa (alta/esq./dir.) pelo golpe do inimigo mais perto. bOnlyIfAttacking: so se houver golpe vindo. */
	bool ChooseMusketParry(class UAnimSequence*& OutAnim, class UAnimSequence*& OutFP, bool bOnlyIfAttacking) const;
	/** Lamina da arma atual (o chute troca para a perna e depois volta). */
	FName SavedGrip, SavedTip;
	float SavedBladeStart = 0.f, SavedBladeLength = 0.f;
	UPROPERTY() class UAnimSequence* AnimAgachadoFrente = nullptr;  UPROPERTY() class UAnimSequence* AnimAgachadoTras = nullptr;
	UPROPERTY() class UAnimSequence* AnimAgachadoEsq = nullptr;     UPROPERTY() class UAnimSequence* AnimAgachadoDir = nullptr;

	UFUNCTION() void HandleFirearmState(EFirearmState NewState, uint8 Stage);

	/** Ao mirar, a camera vai ao olho direito sobre a coronha (medido no Blender: 9,4 cm a frente, 5,6 a direita, 2,8 acima). */
	UPROPERTY(EditAnywhere, Category = "Mosquete") FVector AimEyeOffset = FVector(9.4f, 5.6f, 4.5f);   // +1,7 cm acima do olho medido: o alvo fica livre por cima do cano
	/** Campo de visao com o olho na mira (foco). */
	UPROPERTY(EditAnywhere, Category = "Mosquete") float AimFieldOfView = 65.f;
	FTransform FPBase = FTransform::Identity;   // encaixe dos bracos de 1a pessoa (sem o balanco da mira)
	float AimAlpha = 0.f;
	float AimHoldTime = 0.f;
	FRotator SwayRot = FRotator::ZeroRotator;
	FRotator RecoilPending = FRotator::ZeroRotator;
	void UpdateFirearmView(float DeltaSeconds);

	// ---------- Giro do corpo e camera viva (medidos no video do Chivalry) ----------
	void UpdateBodyTurn(float DeltaSeconds);
	bool bTurnInPlace = false;
	FRotator PrevControlRot = FRotator::ZeroRotator;
	FRotator ViewLag = FRotator::ZeroRotator;   // arma atrasa o giro da camera (~1-2 graus a 100 graus/s)
	FRotator CamBob = FRotator::ZeroRotator;    // balanco do passo e inclinacao de lado
	FVector ViewBob = FVector::ZeroVector;
	float BobPhase = 0.f;
	float StrafeRoll = 0.f;
	float LandDip = 0.f;
	float LandDipVel = 0.f;
	float AutoWalkTime = 0.f;

	void InputAimReleased();
	void InputReload();
	void InputToggleMelee();
	void InputWeapon1();
	void InputWeapon2();
	UFUNCTION(Server, Reliable) void ServerEquip(EEAWWeapon NewWeapon);

	// ---------- Duas armas: 1 de fogo (mosquete/pistola) + 1 de corpo a corpo (sabre/faca/machado) ----------
	UPROPERTY(Replicated) EEAWWeapon PrimaryWeapon = EEAWWeapon::Mosquete;
	UPROPERTY(Replicated) EEAWWeapon SecondaryWeapon = EEAWWeapon::Sabre;
	/** Troca "desce/sobe" em 1a pessoa (~0,4 s, como no Chivalry); bloqueia golpes ate a arma subir. */
	float SwitchDip = 0.f;
	EEAWWeapon LastShownWeapon = EEAWWeapon::Sabre;
	bool IsSwitching() const { return SwitchDip > 0.25f; }

	// ---------- Pegar arma do chao: segurar F por 3 s ----------
	void InputInteractPressed();
	void InputInteractReleased();
	void UpdatePickup(float DeltaSeconds);
	UFUNCTION(Server, Reliable) void ServerStartPickup(class AEAWDroppedWeapon* Target);
	UFUNCTION(Server, Reliable) void ServerCancelPickup();
	void CompletePickup();
	/** Servidor: larga uma arma no chao (da mao, com a velocidade do corpo). */
	void DropWeapon(EEAWWeapon W, bool bLoaded, const FVector& ExtraVelocity);
	TWeakObjectPtr<class AEAWDroppedWeapon> PickupTarget;       // a mais perto/na mira (dono)
	TWeakObjectPtr<class AEAWDroppedWeapon> PickupHeldTarget;   // a que esta sendo pega (dono)
	TWeakObjectPtr<class AEAWDroppedWeapon> ServerPickupTarget; // servidor
	float PickupProgress = 0.f;
	bool bPickupHeld = false;
	FTimerHandle PickupTimer;
public:
	static bool IsFirearmWeapon(EEAWWeapon W) { return W == EEAWWeapon::Mosquete || W == EEAWWeapon::Pistola; }
	class AEAWDroppedWeapon* GetPickupTarget() const { return PickupTarget.Get(); }
	float GetPickupProgress() const { return bPickupHeld ? PickupProgress : 0.f; }
	EEAWWeapon GetPrimaryWeapon() const { return PrimaryWeapon; }
	EEAWWeapon GetSecondaryWeapon() const { return SecondaryWeapon; }
	/** Teste (-EAWAutoPegar): como se apertasse F. */
	void TestStartPickup() { InputInteractPressed(); }
protected:
	UFUNCTION(Server, Reliable) void ServerSetBayonet(bool bNew);
	/** Arma de fogo em modo de tiro (mosquete sem X, ou pistola). */
	bool UsingMusketFire() const { return (CurrentWeapon == EEAWWeapon::Mosquete && !bBayonet) || CurrentWeapon == EEAWWeapon::Pistola; }
	/** Botoes de corpo a corpo valem para sabre, faca, machado e mosquete no modo combate (X). */
	bool UsesMeleeInput() const { return (CurrentWeapon != EEAWWeapon::Mosquete && CurrentWeapon != EEAWWeapon::Pistola) || (CurrentWeapon == EEAWWeapon::Mosquete && bBayonet); }
	/** Tempos, dano e lamina de cada arma (sabre, faca, machado, mosquete com baioneta/coronha). */
	void ApplyWeaponCombatConfig();

	EBodyRegion LastHitRegion = EBodyRegion::None;
	FVector LastHitLocation = FVector::ZeroVector;
	float LastHitTime = -100.f;
	FName CutBone = NAME_None;
	float DamageFlash = 0.f;

	UPROPERTY(Replicated)
	bool bSprinting = false;

	FVector BodyMeshBaseLocation = FVector::ZeroVector;
};
