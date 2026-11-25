#pragma once

#include "CoreMinimal.h"
#include "EAWFirearmTypes.generated.h"

/** Arma que o soldado tem na mao. */
UENUM(BlueprintType)
enum class EEAWWeapon : uint8
{
	Mosquete,
	Sabre,
	Faca,
	Machado,
	Pistola,   // arma de fogo (modelo, animacoes e tiro: proxima etapa)
};

/** Estado do mosquete (tiro). */
UENUM(BlueprintType)
enum class EFirearmState : uint8
{
	Lowered,    // guarda/pronto
	Raising,    // levando ao ombro
	Aiming,     // no ombro, mirando
	Firing,     // coice do disparo
	Lowering,   // tirando do ombro
	Reloading,  // carregando (por etapas)
};

/** Mosquete Charleville 1777 (valores historicos; ajustaveis). */
USTRUCT(BlueprintType)
struct FFirearmConfig
{
	GENERATED_BODY()

	/** Velocidade na boca (cm/s). Carga de servico: ~420 m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MuzzleVelocity = 42000.f;
	/** Bala esferica de chumbo: 27,5 g, 17,5 mm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BallMassKg = 0.0275f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BallDiameterCm = 1.75f;
	/** Desvio do cano liso (desvio padrao, graus). ~56 cm a 90 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimSpreadDeg = 0.35f;
	/** Pederneira: tempo entre o gatilho e o tiro (cao cai, escorva, carga). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LockTimeMin = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LockTimeMax = 0.10f;
	/** Tempos das animacoes (iguais aos do Blender). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimInTime = 0.53f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimOutTime = 0.47f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FireRecoverTime = 0.77f;
	/** Recarga completa (s). Soldado treinado: 3 tiros/min (~20 s); bem treinado ~15 s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReloadTime = 15.f;
	/** Vida maxima da bala (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BallLifetime = 4.f;
	/** Coice na camera (graus). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RecoilPitch = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RecoilYaw = 1.2f;
	/** Peso: 4,6 kg no ombro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimMoveSpeedMult = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReloadMoveSpeedMult = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimTurnCap = 110.f;
};

/** Estado replicado do mosquete. */
USTRUCT(BlueprintType)
struct FFirearmRep
{
	GENERATED_BODY()

	UPROPERTY() EFirearmState State = EFirearmState::Lowered;
	UPROPERTY() uint8 Stage = 0;     // etapa da recarga (0..5)
	UPROPERTY() uint8 Serial = 0;    // muda a cada troca de estado (reinicia a animacao)
	UPROPERTY() bool bLoaded = true;
};
