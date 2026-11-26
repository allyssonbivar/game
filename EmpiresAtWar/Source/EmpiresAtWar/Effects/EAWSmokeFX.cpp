#include "Effects/EAWSmokeFX.h"
#include "Effects/EAWSprites.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Vento fraco e constante no mapa de teste (cm/s): a fumaca deriva devagar, como no Holdfast.
	const FVector Wind(35.f, 12.f, 0.f);
}

AEAWSmokeFX::AEAWSmokeFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // depois da camera: sprites olham para a camera deste quadro

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Smoke(TEXT("/Game/Efeitos/M_Fumaca_Sprite.M_Fumaca_Sprite"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Glow(TEXT("/Game/Efeitos/M_Clarao.M_Clarao"));

	Puffs = EAWSprites::MakeComponent(this, TEXT("Puffs"), Plane.Object, Smoke.Get(), 3);   // [0] opacidade, [1] quadro, [2] brilho
	RootComponent = Puffs;

	Flash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flash"));
	Flash->SetupAttachment(RootComponent);
	Flash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flash->SetCastShadow(false);
	Flash->SetVisibility(false);
	if (Sphere.Succeeded()) { Flash->SetStaticMesh(Sphere.Object); }
	if (Glow.Get()) { Flash->SetMaterial(0, Glow.Get()); }

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
	Light->SetCastShadows(false);
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

void AEAWSmokeFX::Add(const FVector& P, const FVector& V, float Life, float R0, float R1, float Alpha, float Drag, float Bright, float Delay)
{
	// Delay > 0: o floco so aparece depois (idade negativa = ainda nao nasceu).
	Items.Add({ P, V, -Delay, Life, R0, R1, Alpha, Drag, FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-12.f, 12.f),
		Bright * FMath::FRandRange(0.9f, 1.05f), FMath::FRand() * 10.f, (uint8)FMath::RandRange(0, 3) });
}

void AEAWSmokeFX::MuzzleBlast(UWorld* World, const FVector& Muzzle, const FVector& Dir, const FVector& Pan)
{
	AEAWSmokeFX* FX = SpawnAt(World, Muzzle);
	if (!FX)
	{
		return;
	}
	const FVector D = Dir.GetSafeNormal();
	// 1) Jato: a polvora sai a ~30 m/s e freia no ar em 0,5-3 m, formando a nuvem densa a frente do cano.
	for (int32 i = 0; i < 30; ++i)
	{
		const float k = FMath::FRand();
		const FVector V = FMath::VRandCone(D, FMath::DegreesToRadians(5.f + 16.f * k)) * FMath::Lerp(2100.f, 380.f, k);
		FX->Add(Muzzle + D * FMath::FRandRange(0.f, 20.f), V, FMath::FRandRange(9.f, 13.f), FMath::FRandRange(8.f, 14.f),
			FMath::FRandRange(60.f, 115.f), FMath::FRandRange(0.65f, 0.9f), 6.f, FMath::FRandRange(1.0f, 1.12f));
	}
	// 2) Rolos que saem de lado na boca do cano (a nuvem "abre" em volta da boca).
	for (int32 i = 0; i < 10; ++i)
	{
		const FVector Side = FVector::CrossProduct(D, FMath::VRand()).GetSafeNormal();
		FX->Add(Muzzle + D * 10.f, Side * FMath::FRandRange(100.f, 320.f) + D * FMath::FRandRange(100.f, 380.f), FMath::FRandRange(6.f, 9.f),
			6.f, FMath::FRandRange(45.f, 90.f), FMath::FRandRange(0.55f, 0.75f), 4.f, FMath::FRandRange(0.95f, 1.08f));
	}
	// 3) Corpo da nuvem: flocos grandes e lentos que nascem quando o jato chega (0,6-3 m a frente) e incham devagar
	//    ate 1,3-1,8 m de raio - e o que deixa a nuvem "parada no ar" por muitos segundos, como no video.
	for (int32 i = 0; i < 9; ++i)
	{
		const float Dist = FMath::FRandRange(60.f, 300.f);
		const FVector P = Muzzle + D * Dist + FMath::VRand() * FMath::FRandRange(0.f, 35.f);
		FX->Add(P, D * FMath::FRandRange(40.f, 120.f) + FMath::VRand() * 30.f, FMath::FRandRange(11.f, 15.f), 25.f,
			FMath::FRandRange(130.f, 180.f), FMath::FRandRange(0.5f, 0.68f), 2.5f, FMath::FRandRange(1.0f, 1.1f), Dist / 2400.f + 0.05f);
	}
	// 4) Sopro da cacoleta (escorva), do lado direito do fecho, subindo.
	for (int32 i = 0; i < 6; ++i)
	{
		FX->Add(Pan, FMath::VRand() * 50.f + FVector(0.f, 0.f, 70.f), FMath::FRandRange(2.f, 3.5f), 3.f, FMath::FRandRange(16.f, 32.f), 0.4f, 2.5f);
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
	for (int32 i = 0; i < 8; ++i)
	{
		FX->Add(Location, FMath::VRandCone(Normal, 0.7f) * FMath::FRandRange(150.f, 500.f), FMath::FRandRange(1.2f, 2.4f), 4.f,
			FMath::FRandRange(18.f, 40.f), 0.6f, 4.f, 0.75f);
	}
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
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		FPuff& F = Items[i];
		F.Age += Dt;
		if (F.Age < 0.f)
		{
			continue;   // ainda nao nasceu
		}
		if (F.Age >= F.Life)
		{
			Items.RemoveAtSwap(i);
			continue;
		}
		// Freia no ar ate andar com o vento; sobe devagar (gas quente) e ondula (turbulencia).
		const FVector Turb(FMath::Sin(F.Age * 0.9f + F.Seed) * 10.f, FMath::Cos(F.Age * 0.7f + F.Seed * 1.3f) * 10.f, 0.f);
		const FVector Target = Wind + Turb + FVector(0.f, 0.f, 14.f);
		F.V = Target + (F.V - Target) * FMath::Max(0.f, 1.f - F.Drag * Dt);
		F.P += F.V * Dt;
		F.Roll += F.Spin * Dt;
	}

	// De tras para a frente (translucido por instancia nao e ordenado pela placa de video).
	const FVector Cam = EAWSprites::ViewLocation(GetWorld());
	Order.Reset();
	for (int32 i = 0; i < Items.Num(); ++i) { if (Items[i].Age >= 0.f) Order.Add(i); }
	Order.Sort([this, &Cam](int32 A, int32 B) { return FVector::DistSquared(Items[A].P, Cam) > FVector::DistSquared(Items[B].P, Cam); });

	Puffs->ClearInstances();
	for (int32 i : Order)
	{
		const FPuff& F = Items[i];
		const float t = F.Age / F.Life;
		// Cresce rapido no comeco (expansao do gas) e depois devagar e sempre (difusao: a nuvem vai inchando no ar).
		const float Grow = 1.f - FMath::Exp(-F.Age * 3.0f);
		const float R = FMath::Lerp(F.R0, F.R1, Grow) * (1.f + 0.6f * t);
		// Densa no primeiro terco da vida, depois some devagar (no video fica ~4 s cheia e leva ~8 s para sumir).
		const float Fade = t < 0.3f ? 1.f : FMath::Pow(1.f - (t - 0.3f) / 0.7f, 1.6f);
		const float A = F.Alpha * FMath::Min(1.f, F.Age * 25.f) * Fade;
		// A nuvem ocupa ~metade do quadro da textura: o quadro tem 4x o raio.
		const int32 Idx = Puffs->AddInstanceWorldSpace(EAWSprites::Billboard(F.P, Cam, R * 4.f, R * 4.f, F.Roll));
		Puffs->SetCustomDataValue(Idx, 0, A, false);
		Puffs->SetCustomDataValue(Idx, 1, (float)F.Atlas, false);
		Puffs->SetCustomDataValue(Idx, 2, F.Bright, false);
	}
	Puffs->MarkRenderStateDirty();
	if (Items.Num() == 0 && FlashLeft <= 0.f)
	{
		Destroy();
	}
}
