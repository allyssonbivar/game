#include "Effects/EAWSmokeFX.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AEAWSmokeFX::AEAWSmokeFX()
{
	PrimaryActorTick.bCanEverTick = true;
	Puffs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Puffs"));
	RootComponent = Puffs;
	Puffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puffs->SetCastShadow(false);
	Puffs->NumCustomDataFloats = 1;   // opacidade por bolha (material M_Fumaca)

	Flash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flash"));
	Flash->SetupAttachment(RootComponent);
	Flash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flash->SetCastShadow(false);
	Flash->SetVisibility(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
	Light->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Smoke(TEXT("/Game/Efeitos/M_Fumaca.M_Fumaca"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Efeitos/M_Clarao.M_Clarao"));
	if (Sphere.Succeeded()) { Puffs->SetStaticMesh(Sphere.Object); Flash->SetStaticMesh(Sphere.Object); }
	if (Smoke.Succeeded()) { Puffs->SetMaterial(0, Smoke.Object); }
	if (Glow.Succeeded()) { Flash->SetMaterial(0, Glow.Object); }
}

AEAWSmokeFX* AEAWSmokeFX::SpawnAt(UWorld* World, const FVector& Location)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AEAWSmokeFX>(AEAWSmokeFX::StaticClass(), FTransform(Location), P);
}

void AEAWSmokeFX::Add(const FVector& P, const FVector& V, float Life, float R0, float R1, float Alpha)
{
	Items.Add({ P, V, 0.f, Life, R0, R1, Alpha });
}

void AEAWSmokeFX::MuzzleBlast(UWorld* World, const FVector& Muzzle, const FVector& Dir, const FVector& Pan)
{
	AEAWSmokeFX* FX = SpawnAt(World, Muzzle);
	if (!FX)
	{
		return;
	}
	const FVector D = Dir.GetSafeNormal();
	// Nuvem de polvora negra: jato forte para a frente que freia no ar, cresce ate ~1,2 m e fica no lugar.
	for (int32 i = 0; i < 22; ++i)
	{
		const float k = FMath::FRand();
		const FVector V = FMath::VRandCone(D, FMath::DegreesToRadians(14.f + 20.f * k)) * FMath::Lerp(2600.f, 500.f, k)
			+ FVector(0.f, 0.f, FMath::FRandRange(5.f, 25.f));
		FX->Add(Muzzle + D * FMath::FRandRange(0.f, 15.f), V, FMath::FRandRange(4.5f, 7.f), FMath::FRandRange(6.f, 12.f),
			FMath::FRandRange(55.f, 120.f), FMath::FRandRange(0.35f, 0.6f));
	}
	// Sopro da cacoleta (escorva) do lado direito do fecho.
	for (int32 i = 0; i < 5; ++i)
	{
		FX->Add(Pan, FMath::VRand() * 60.f + FVector(0.f, 0.f, 50.f), FMath::FRandRange(2.f, 3.5f), 3.f, FMath::FRandRange(18.f, 30.f), 0.4f);
	}
	FX->Flash->SetWorldLocation(Muzzle + D * 12.f);
	FX->Flash->SetWorldScale3D(FVector(0.35f, 0.22f, 0.22f));
	FX->Flash->SetWorldRotation(D.Rotation());
	FX->Flash->SetVisibility(true);
	FX->Light->SetWorldLocation(Muzzle + D * 20.f);
	FX->Light->SetIntensity(60000.f);
	FX->FlashLeft = 0.06f;
}

void AEAWSmokeFX::Dust(UWorld* World, const FVector& Location, const FVector& Normal)
{
	AEAWSmokeFX* FX = SpawnAt(World, Location);
	if (!FX)
	{
		return;
	}
	for (int32 i = 0; i < 7; ++i)
	{
		FX->Add(Location, FMath::VRandCone(Normal, 0.7f) * FMath::FRandRange(150.f, 500.f), FMath::FRandRange(1.2f, 2.2f), 2.f,
			FMath::FRandRange(12.f, 30.f), 0.5f);
	}
	FX->Drag = 3.f;
}

void AEAWSmokeFX::Tick(float Dt)
{
	Super::Tick(Dt);
	if (FlashLeft > 0.f)
	{
		FlashLeft -= Dt;
		if (FlashLeft <= 0.f)
		{
			Flash->SetVisibility(false);
			Light->SetIntensity(0.f);
		}
	}
	Puffs->ClearInstances();
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		FPuff& F = Items[i];
		F.Age += Dt;
		if (F.Age >= F.Life)
		{
			Items.RemoveAtSwap(i);
			continue;
		}
		F.V *= FMath::Max(0.f, 1.f - Drag * Dt);
		F.V.Z += 12.f * Dt;   // fumaca quente sobe devagar
		F.P += F.V * Dt;
	}
	for (const FPuff& F : Items)
	{
		const float t = F.Age / F.Life;
		const float R = FMath::Lerp(F.R0, F.R1, 1.f - FMath::Square(1.f - FMath::Min(1.f, t * 2.5f)));
		const int32 Idx = Puffs->AddInstanceWorldSpace(FTransform(FRotator::ZeroRotator, F.P, FVector(R / 50.f)));
		const float A = F.Alpha * FMath::Min(1.f, t * 20.f) * (1.f - t) * (1.f - t);
		Puffs->SetCustomDataValue(Idx, 0, A, false);
	}
	Puffs->MarkRenderStateDirty();
	if (Items.Num() == 0 && FlashLeft <= 0.f)
	{
		Destroy();
	}
}
