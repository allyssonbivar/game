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
};

/** Dados que o "animador" le na thread de animacao (copiados da thread do jogo a cada quadro). */
struct FEAWAnimFrame
{
	UAnimSequence* Combat = nullptr;  float CombatTime = 0.f;
	UAnimSequence* Prev = nullptr;    float PrevTime = 0.f;   float PrevBlend = 1.f;
	UAnimSequence* Loco[5] = { nullptr, nullptr, nullptr, nullptr, nullptr }; // frente, tras, esq, dir, correr
	float LocoW[4] = { 1.f, 0.f, 0.f, 0.f };
	float WalkTime = 0.f, RunTime = 0.f;
	float LocoWeight = 0.f;      // 0 parado, 1 andando: pernas seguem o movimento
	float RunWeight = 0.f;       // andar -> correr
	float UpperRunWeight = 0.f;  // correndo sem lutar: tronco tambem corre (sabre no ombro)
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

	/** Correndo sem lutar, o tronco usa a corrida (sabre no ombro). Com o mosquete, nao (a arma ficaria no chao). */
	bool bAllowUpperRun = true;

	/** Toca [Start, End] (s) em Duration segundos; transicao suave a partir do trecho anterior. */
	void SetCombatSegment(UAnimSequence* Anim, float Start, float End, float Duration, bool bLoop);

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
	float DirW[4] = { 1.f, 0.f, 0.f, 0.f };

	float SegmentTime(const FEAWSegment& S, float Elapsed) const;
};
