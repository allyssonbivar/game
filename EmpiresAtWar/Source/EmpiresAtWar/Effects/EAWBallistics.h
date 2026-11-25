#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/EAWFirearmTypes.h"
#include "EAWBallistics.generated.h"

class UEAWFirearmComponent;

/**
 * Simulador de balas (so no servidor). Cada bala: gravidade + arrasto de esfera (o coeficiente sobe perto
 * da velocidade do som), passos de 1/120 s, rastreio entre os passos. Acerta os corpos fisicos do soldado
 * (18 capsulas do esqueleto, por osso) e o cenario.
 */
UCLASS()
class EMPIRESATWAR_API AEAWBallistics : public AActor
{
	GENERATED_BODY()

public:
	AEAWBallistics();
	virtual void Tick(float DeltaSeconds) override;

	static void Fire(UWorld* World, const FVector& Origin, const FVector& Velocity, UEAWFirearmComponent* Firearm, AActor* Shooter,
		const FFirearmConfig& Config);

	/** Coeficiente de arrasto de uma esfera pela velocidade (Mach). */
	static float SphereDrag(float SpeedMs);

private:
	struct FBall
	{
		FVector Pos;
		FVector Vel;        // cm/s
		float Age;
		float K;            // 0,5 * rho * A / m  (1/m), multiplicado pelo Cd a cada passo
		float Life;
		TWeakObjectPtr<UEAWFirearmComponent> Firearm;
		TWeakObjectPtr<AActor> Shooter;
	};
	TArray<FBall> Balls;

	bool Step(FBall& B, float Dt);
};
