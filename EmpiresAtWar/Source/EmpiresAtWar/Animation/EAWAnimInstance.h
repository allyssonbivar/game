#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "EAWAnimInstance.generated.h"

class UAnimSequence;

/** Um trecho de animacao de combate tocando no tempo do codigo. */
struct FEAWSegment
{
	UAnimSequence* Anim = nullptr;
	float Start = 0.f;
	float End = 0.f;
	float Rate = 1.f;
	bool bLoop = false;
	bool bFullBody = false;   // pernas tambem seguem o trecho (chute, golpe de salto)
};

/** Dados que o "animador" le na thread de animacao (copiados da thread do jogo a cada quadro). */
struct FEAWAnimFrame
{
	UAnimSequence* Combat = nullptr;  float CombatTime = 0.f;
	UAnimSequence* Prev = nullptr;    float PrevTime = 0.f;   float PrevBlend = 1.f;
	UAnimSequence* Loco[5] = { nullptr, nullptr, nullptr, nullptr, nullptr }; // frente, tras, esq, dir, correr
	float LocoW[4] = { 1.f, 0.f, 0.f, 0.f };
	UAnimSequence* Diag[4] = { nullptr, nullptr, nullptr, nullptr };   // diagonais: frente-dir, frente-esq, tras-dir, tras-esq
	float DiagW[4] = { 0.f, 0.f, 0.f, 0.f };
	float CrouchW[4] = { 1.f, 0.f, 0.f, 0.f };   // agachado so tem 4 direcoes: diagonais divididas entre as vizinhas
	float WalkPhase01 = 0.f;   // fase do passo 0..1 (cada animacao e tocada na sua propria duracao)
	float WalkTime = 0.f, RunTime = 0.f;
	float LocoWeight = 0.f;      // 0 parado, 1 andando: pernas seguem o movimento
	float RunWeight = 0.f;       // andar -> correr
	float UpperRunWeight = 0.f;  // correndo sem lutar: tronco tambem corre (sabre no ombro)
	float AimYaw = 0.f;          // mira - corpo (graus): o tronco e a arma torcem na cintura
	float FullBodyWeight = 0.f;  // 1 = pernas do trecho de combate (chute, salto); 0 = pernas do movimento
	// Agachado: pernas e altura do quadril vem destas animacoes; o tronco continua no combate/arma.
	UAnimSequence* CrouchIdle = nullptr;
	UAnimSequence* CrouchWalk[4] = { nullptr, nullptr, nullptr, nullptr };
	float CrouchIdleTime = 0.f, CrouchTime = 0.f;
	float CrouchWeight = 0.f;     // 0 em pe, 1 agachado (transicao de 0,25 s)
};

struct FEAWAnimProxy : public FAnimInstanceProxy
{
	FEAWAnimProxy() {}
	FEAWAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	FEAWAnimFrame Data;
	TArray<uint8> UpperMask;   // 1 = osso do tronco/bracos (spine_01 e filhos)
};

/**
 * Animador em camadas, sem Animation Blueprint:
 *  - pernas: mistura de andar (4 direcoes) e correr, sincronizada com a velocidade real;
 *  - tronco/bracos: trecho de combate no tempo do codigo, com transicao suave de 0,1 s;
 *  - parado: corpo inteiro segue o combate.
 */
UCLASS(Transient)
class EMPIRESATWAR_API UEAWAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Animacoes de movimento (frente, tras, esquerda, direita, correr). */
	UPROPERTY(EditAnywhere, Category = "Movimento") UAnimSequence* WalkForward = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movimento") UAnimSequence* WalkBack = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movimento") UAnimSequence* WalkLeft = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movimento") UAnimSequence* WalkRight = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movimento") UAnimSequence* Run = nullptr;

	/** Velocidades (cm/s) em que cada animacao foi feita - o passo e sincronizado com elas. */
	float SpeedForward = 140.f, SpeedBack = 100.f, SpeedSide = 85.f, SpeedRun = 400.f;
	float WalkCycle = 0.8f, RunCycle = 0.6f;
	/** Trote nas diagonais (frente-dir, frente-esq, tras-dir, tras-esq) e as velocidades em que foram feitas (cm/s). */
	UAnimSequence* WalkDiag[4] = { nullptr, nullptr, nullptr, nullptr };
	float SpeedDiagFwd = 255.f, SpeedDiagBack = 208.f;

	/** Agachado (so armas de fogo): parado + andar nas 4 direcoes; velocidades (cm/s) em que foram feitas. */
	UPROPERTY(EditAnywhere, Category = "Movimento") UAnimSequence* CrouchIdle = nullptr;
	UAnimSequence* CrouchWalk[4] = { nullptr, nullptr, nullptr, nullptr };   // frente, tras, esq, dir
	float CrouchSpeedForward = 100.f, CrouchSpeedBack = 80.f, CrouchSpeedSide = 70.f, CrouchCycle = 0.9f;

	/** Correndo sem lutar, o tronco usa a corrida (sabre no ombro). Com o mosquete, nao (a arma ficaria no chao). */
	bool bAllowUpperRun = true;

	/** Toca [Start, End] (s) em Duration segundos; transicao suave a partir do trecho anterior. */
	void SetCombatSegment(UAnimSequence* Anim, float Start, float End, float Duration, bool bLoop, bool bFullBody = false);

	/** Chamado na thread do jogo antes de cada atualizacao. */
	void TickValues(float DeltaSeconds, FEAWAnimFrame& Out);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FEAWAnimProxy(this); }

private:
	FEAWSegment Seg;
	float SegElapsed = 0.f;
	FEAWSegment PrevSeg;
	float PrevSegTime = 0.f;
	float PrevBlend = 1.f;

	float WalkPhase = 0.f, RunPhase = 0.f;
	float LocoWeight = 0.f, RunWeight = 0.f, UpperRunWeight = 0.f;
	// Molas criticas (comecam e terminam com velocidade zero): velocidades de cada valor suavizado.
	float LocoWeightVel = 0.f, AimYawVel = 0.f, AimYawSmooth = 0.f;
	float FullBody = 0.f, FullBodyVel = 0.f;
	float DirWVel[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float DirW[8] = { 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };   // 8 direcoes por angulo: F, FD, D, TD, T, TE, E, FE
	float AimYaw = 0.f;
	float CrouchWeight = 0.f, CrouchPhase = 0.f, CrouchIdleTime = 0.f;
	float PrevActorYaw = 0.f;
	bool bHavePrevYaw = false;

	float SegmentTime(const FEAWSegment& S, float Elapsed) const;
};
