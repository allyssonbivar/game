#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

/**
 * Sprites feitos com instancias do plano basico (100 x 100 cm, normal +Z): cada instancia e girada
 * para olhar a camera local. Usado pela fumaca e pelo sangue (efeitos so visuais, cada maquina desenha o seu).
 */
namespace EAWSprites
{
	/** Componente de instancias sem colisao nem sombra, com NumCustom dados por instancia para o material. */
	inline UInstancedStaticMeshComponent* MakeComponent(AActor* Owner, FName Name, UStaticMesh* Mesh, UMaterialInterface* Material, int32 NumCustom)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetMobility(EComponentMobility::Movable);
		C->NumCustomDataFloats = NumCustom;
		C->bUseAsOccluder = false;
		C->SetGenerateOverlapEvents(false);
		if (Mesh) { C->SetStaticMesh(Mesh); }
		if (Material) { C->SetMaterial(0, Material); }
		return C;
	}

	/** Onde esta a camera que vai ver o efeito (jogador local). */
	inline FVector ViewLocation(UWorld* World)
	{
		if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, 0))
		{
			return Cam->GetCameraLocation();
		}
		return FVector::ZeroVector;
	}

	/** Quadro virado para a camera, com giro proprio (graus) sobre o eixo de visao. */
	inline FTransform Billboard(const FVector& P, const FVector& Cam, float Width, float Height, float RollDeg)
	{
		FVector Z = (Cam - P).GetSafeNormal();
		if (Z.IsNearlyZero()) { Z = FVector::UpVector; }
		FVector X = FVector::CrossProduct(FVector::UpVector, Z);
		if (X.SizeSquared() < 1e-4f) { X = FVector::ForwardVector; }
		const FQuat Base = FRotationMatrix::MakeFromZX(Z, X).ToQuat();
		const FQuat Q = FQuat(Z, FMath::DegreesToRadians(RollDeg)) * Base;
		return FTransform(Q, P, FVector(Width / 100.f, Height / 100.f, 1.f));
	}

	/** Quadro esticado na direcao Vel (risco/filete), girado em torno dela para encarar a camera. */
	inline FTransform Stretched(const FVector& P, const FVector& Vel, const FVector& Cam, float Length, float Width)
	{
		FVector X = Vel.GetSafeNormal();
		if (X.IsNearlyZero()) { X = FVector::DownVector; }
		FVector ToCam = (Cam - P).GetSafeNormal();
		if (FMath::Abs(FVector::DotProduct(ToCam, X)) > 0.999f) { ToCam = FVector::UpVector; }
		return FTransform(FRotationMatrix::MakeFromXZ(X, ToCam).ToQuat(), P, FVector(Length / 100.f, Width / 100.f, 1.f));
	}
}
