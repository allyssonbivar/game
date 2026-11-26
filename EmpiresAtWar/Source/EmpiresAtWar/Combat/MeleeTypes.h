#pragma once

#include "CoreMinimal.h"
#include "MeleeTypes.generated.h"

/** Estado do combate corpo a corpo. O servidor decide; os clientes so mostram. */
UENUM(BlueprintType)
enum class EMeleeState : uint8
{
	Idle,           // em guarda
	Windup,         // preparacao do golpe (pode fintar ou trocar para parry)
	Release,        // golpe ativo: a lamina causa dano ao atravessar o alvo
	Recovery,       // volta a guarda (pode encadear combo)
	Parry,          // janela de defesa ativa
	ParryRecovery,  // parry terminou sem defender nada
	Feint,          // finta: cancelou o golpe na preparacao
	Stunned,        // golpe defendido / guarda quebrada
	Incapacitated,  // ferido demais para lutar, ou morto
	Block           // defesa segurada (botao direito apertado depois da janela do parry): anda devagar, bloqueia sem contra-ataque
};

UENUM(BlueprintType)
enum class EAttackDirection : uint8
{
	SlashLeft,   // corte horizontal vindo da direita para a esquerda
	SlashRight,  // corte horizontal vindo da esquerda para a direita
	Overhead,    // golpe de cima
	Stab,        // estocada
	Special,     // golpe especial carregado: segurar o ataque 3 s + W (puxa o braco para tras e estoca com o corpo)
	Sprint,      // golpe correndo (Shift + botao esquerdo): corte diagonal de cima sem perder a corrida
	Jump,        // golpe de salto (botao esquerdo no ar): de cima, aterrissando
	Kick         // chute (V): nao fere; atravessa a defesa, gasta stamina, atordoa e empurra
};

UENUM(BlueprintType)
enum class EBodyRegion : uint8
{
	None,
	Head,
	Neck,
	Torso,
	Arms,
	Legs
};

/** Pose procedural da arma, relativa ao ponto de pivo na mao/camera. */
USTRUCT(BlueprintType)
struct FMeleePose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	FRotator Rotation = FRotator::ZeroRotator;

	FMeleePose() {}
	FMeleePose(const FVector& InOffset, const FRotator& InRotation) : Offset(InOffset), Rotation(InRotation) {}
};

/** Tempos e valores de um tipo de ataque de uma arma. */
USTRUCT(BlueprintType)
struct FMeleeAttackSpec
{
	GENERATED_BODY()

	/** Preparacao normal (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo", meta = (ClampMin = "0.05"))
	float Windup = 0.45f;

	/** Preparacao quando encadeado em combo (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo", meta = (ClampMin = "0.05"))
	float ComboWindup = 0.32f;

	/** Preparacao do contra-ataque depois de um parry bem-sucedido (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo", meta = (ClampMin = "0.05"))
	float RiposteWindup = 0.28f;

	/** Duracao do golpe ativo (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo", meta = (ClampMin = "0.05"))
	float Release = 0.30f;

	/** Volta a guarda (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo", meta = (ClampMin = "0.05"))
	float Recovery = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valores")
	float StaminaCost = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valores")
	float BaseDamage = 40.f;

	/** Pose no fim da preparacao. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	FMeleePose WindupPose;

	/** Pose no fim do golpe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	FMeleePose EndPose;
};

/** Configuracao completa de uma arma branca. */
USTRUCT(BlueprintType)
struct FMeleeWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arma")
	FName WeaponName = TEXT("Sabre");

	/** Distancia do pivo ate o inicio da lamina (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamina")
	float BladeStart = 12.f;

	/** Comprimento da lamina (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamina")
	float BladeLength = 85.f;

	/** Raio da varredura de colisao (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamina")
	float BladeRadius = 3.f;

	/** Quantos pontos da lamina sao rastreados por quadro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamina", meta = (ClampMin = "2", ClampMax = "12"))
	int32 BladeSamples = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ataques")
	TMap<EAttackDirection, FMeleeAttackSpec> Attacks;

	/** Golpe especial: tempo segurando o ataque (com o braco puxado para tras) ate poder soltar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ataques")
	float ChargeTime = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float ParryWindow = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float ParryRecovery = 0.35f;

	/** Stamina gasta ao defender um golpe com parry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float ParryBlockStaminaCost = 12.f;

	/** Defesa segurada: cada golpe bloqueado custa mais stamina que o parry na hora certa e nao da contra-ataque. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float HeldBlockStaminaCost = 20.f;

	/** Angulo maximo (graus) entre a frente do defensor e o atacante para o parry valer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float ParryHalfAngle = 70.f;

	/** Tempo atordoado quando o golpe e defendido. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float StunWhenParried = 0.55f;

	/** Tempo atordoado quando a guarda quebra por falta de stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float StunWhenGuardBroken = 0.9f;

	/** Janela para usar o contra-ataque rapido depois de um parry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defesa")
	float RiposteWindow = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finta")
	float FeintStaminaCost = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finta")
	float FeintRecovery = 0.25f;

	/** So da para fintar ate esta fracao da preparacao (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finta")
	float FeintLatestAlpha = 0.8f;

	/** A partir desta fracao do golpe, apertar ataque guarda um combo (0,35: o clique "pega" mais cedo = combo mais fluido). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	float ComboQueueFromReleaseAlpha = 0.35f;

	/** Golpe final do combo (3o clique, ou 2 cliques + E): mais pesado - dano x1,35, preparacao x1,1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	float ComboFinisherDamageMult = 1.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	float ComboFinisherWindupMult = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	FMeleePose GuardPose = FMeleePose(FVector(0.f, 0.f, 0.f), FRotator(55.f, -10.f, 0.f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	FMeleePose ParryPose = FMeleePose(FVector(5.f, -18.f, 18.f), FRotator(5.f, -5.f, 90.f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ferimento")
	TMap<EBodyRegion, float> RegionDamageMultiplier;

	FMeleeWeaponConfig()
	{
		// Valores iniciais de um sabre. Sao um ponto de partida para testar, nao numeros finais.
		FMeleeAttackSpec SlashL;
		// O braco avanca durante o golpe (offset X maior no fim) para dar alcance de ~1,4 m.
		SlashL.WindupPose = FMeleePose(FVector(10.f, 20.f, 5.f), FRotator(10.f, 75.f, -80.f));
		SlashL.EndPose = FMeleePose(FVector(35.f, -40.f, 0.f), FRotator(0.f, -80.f, -80.f));
		Attacks.Add(EAttackDirection::SlashLeft, SlashL);

		FMeleeAttackSpec SlashR = SlashL;
		SlashR.WindupPose = FMeleePose(FVector(10.f, -40.f, 5.f), FRotator(10.f, -75.f, 80.f));
		SlashR.EndPose = FMeleePose(FVector(35.f, 20.f, 0.f), FRotator(0.f, 80.f, 80.f));
		Attacks.Add(EAttackDirection::SlashRight, SlashR);

		FMeleeAttackSpec Over;
		Over.Windup = 0.55f;
		Over.ComboWindup = 0.40f;
		Over.Release = 0.32f;
		Over.Recovery = 0.60f;
		Over.StaminaCost = 12.f;
		Over.BaseDamage = 50.f;
		// Pivo fica 22 cm a direita do olho; Y negativo traz o golpe para o centro.
		Over.WindupPose = FMeleePose(FVector(-5.f, -15.f, 25.f), FRotator(100.f, 0.f, 0.f));
		Over.EndPose = FMeleePose(FVector(35.f, -18.f, -15.f), FRotator(-45.f, 0.f, 0.f));
		Attacks.Add(EAttackDirection::Overhead, Over);

		FMeleeAttackSpec Stab;
		Stab.Windup = 0.40f;
		Stab.ComboWindup = 0.30f;
		Stab.Release = 0.25f;
		Stab.Recovery = 0.50f;
		Stab.StaminaCost = 8.f;
		Stab.BaseDamage = 35.f;
		Stab.WindupPose = FMeleePose(FVector(-35.f, -16.f, 8.f), FRotator(0.f, 0.f, 0.f));
		Stab.EndPose = FMeleePose(FVector(45.f, -20.f, 8.f), FRotator(0.f, 0.f, 0.f));
		Attacks.Add(EAttackDirection::Stab, Stab);

		// Especial: a preparacao e a carga (ChargeTime); o golpe e rapido, longo e pesado; recuperacao lenta.
		FMeleeAttackSpec Special = Stab;
		Special.Windup = 0.55f;   // so o tempo de puxar o braco (a carga segura a pose)
		Special.Release = 0.30f;
		Special.Recovery = 0.95f;
		Special.StaminaCost = 25.f;
		Special.BaseDamage = 150.f;   // atravessa o tronco: fatal (LethalEnergyThreshold = 150)
		Attacks.Add(EAttackDirection::Special, Special);

		// Fase 2 do combate (2026-11-26).
		FMeleeAttackSpec Sprint = Over;
		Sprint.Windup = 0.30f; Sprint.ComboWindup = 0.30f; Sprint.RiposteWindup = 0.30f;
		Sprint.Release = 0.30f; Sprint.Recovery = 0.70f;
		Sprint.StaminaCost = 16.f; Sprint.BaseDamage = 55.f;
		Attacks.Add(EAttackDirection::Sprint, Sprint);

		FMeleeAttackSpec Jump = Over;
		Jump.Windup = 0.26f; Jump.ComboWindup = 0.26f; Jump.RiposteWindup = 0.26f;
		Jump.Release = 0.26f; Jump.Recovery = 0.70f;
		Jump.StaminaCost = 16.f; Jump.BaseDamage = 60.f;
		Attacks.Add(EAttackDirection::Jump, Jump);

		FMeleeAttackSpec Kick = Stab;
		Kick.Windup = 0.24f; Kick.ComboWindup = 0.24f; Kick.RiposteWindup = 0.24f;
		Kick.Release = 0.14f; Kick.Recovery = 0.42f;
		Kick.StaminaCost = 12.f; Kick.BaseDamage = 0.f;
		Attacks.Add(EAttackDirection::Kick, Kick);

		RegionDamageMultiplier.Add(EBodyRegion::Head, 2.0f);
		RegionDamageMultiplier.Add(EBodyRegion::Neck, 2.0f);
		RegionDamageMultiplier.Add(EBodyRegion::Torso, 1.0f);
		RegionDamageMultiplier.Add(EBodyRegion::Arms, 0.7f);
		RegionDamageMultiplier.Add(EBodyRegion::Legs, 0.8f);
	}
};

/** Estado replicado como um bloco so, para chegar inteiro nos clientes. */
USTRUCT(BlueprintType)
struct FMeleeStateRep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combate")
	EMeleeState State = EMeleeState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Combate")
	EAttackDirection Direction = EAttackDirection::SlashLeft;

	/** Tempo do servidor em que o estado comecou. */
	UPROPERTY(BlueprintReadOnly, Category = "Combate")
	float StartTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combate")
	float Duration = 0.f;

	/** Muda a cada transicao, para os clientes notarem estados repetidos (combo de mesmo tipo). */
	UPROPERTY()
	uint8 Serial = 0;

	/** Passo do combo deste golpe: 0 = 1o golpe, 1 = 2o, 2 = golpe final. */
	UPROPERTY()
	uint8 ComboStep = 0;
};
