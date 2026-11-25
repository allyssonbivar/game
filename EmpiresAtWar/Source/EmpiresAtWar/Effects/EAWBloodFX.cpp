#include "Effects/EAWBloodFX.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

UMaterialInterface* AEAWBloodFX::StainMaterial = nullptr;
TArray<TWeakObjectPtr<UDecalComponent>> AEAWBloodFX::Stains;

AEAWBloodFX::AEAWBloodFX()
{
	PrimaryActorTick.bCanEverTick = true;
	Drops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	RootComponent = Drops;
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Blood(TEXT("/Game/Efeitos/M_Sangue.M_Sangue"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Stain(TEXT("/Game/Efeitos/M_Sangue_Mancha.M_Sangue_Mancha"));
	if (Sphere.Succeeded()) { Drops->SetStaticMesh(Sphere.Object); }
	if (Blood.Succeeded()) { Drops->SetMaterial(0, Blood.Object); }
	if (Stain.Succeeded()) { StainMaterial = Stain.Object; }
}

bool AEAWBloodFX::IsGoreEnabled()
{
	return !FParse::Param(FCommandLine::Get(), TEXT("EAWSemSangue"));
}

AEAWBloodFX* AEAWBloodFX::SpawnFX(UWorld* World, const FVector& Location)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer || !IsGoreEnabled())
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEAWBloodFX* FX = World->SpawnActor<AEAWBloodFX>(AEAWBloodFX::StaticClass(), FTransform(Location), P);
	if (FX)
	{
		FX->FindGround(Location);
	}
	return FX;
}

void AEAWBloodFX::FindGround(const FVector& From)
{
	FHitResult Hit;
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, From - FVector(0.f, 0.f, 600.f), Objects))
	{
		GroundZ = Hit.ImpactPoint.Z;
	}
}

void AEAWBloodFX::Burst(UWorld* World, const FVector& Location, const FVector& Dir, int32 Count, float Speed)
{
	if (AEAWBloodFX* FX = SpawnFX(World, Location))
	{
		FX->AddDrops(Location, Dir, Count, Speed, 35.f);
	}
}

void AEAWBloodFX::Jet(UWorld* World, USkeletalMeshComponent* Mesh, FName Bone, const FVector& WorldPoint, const FVector& WorldDir, float Duration, float Rate)
{
	if (!Mesh)
	{
		return;
	}
	if (AEAWBloodFX* FX = SpawnFX(World, WorldPoint))
	{
		const FTransform BoneTM = Mesh->GetSocketTransform(Bone);
		FX->JetMesh = Mesh;
		FX->JetBone = Bone;
		FX->JetLocalPoint = BoneTM.InverseTransformPosition(WorldPoint);
		FX->JetLocalDir = BoneTM.InverseTransformVectorNoScale(WorldDir.GetSafeNormal());
		FX->JetDuration = Duration;
		FX->JetRate = Rate;
	}
}

void AEAWBloodFX::SpawnStain(UWorld* World, const FVector& Location, float Size)
{
	if (!World || !StainMaterial)
	{
		return;
	}
	const FRotator Rot(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
	UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(World, StainMaterial, FVector(8.f, Size, Size * FMath::FRandRange(0.7f, 1.3f)), Location, Rot, 40.f);
	if (D)
	{
		Stains.Add(D);
		while (Stains.Num() > 60)
		{
			if (Stains[0].IsValid())
			{
				Stains[0]->DestroyComponent();
			}
			Stains.RemoveAt(0);
		}
	}
}

void AEAWBloodFX::AddDrops(const FVector& Location, const FVector& Dir, int32 Count, float Speed, float SpreadDeg)
{
	const FVector D = Dir.GetSafeNormal();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector V = FMath::VRandCone(D, FMath::DegreesToRadians(SpreadDeg)) * Speed * FMath::FRandRange(0.4f, 1.1f);
		Items.Add({ Location, V + FVector(0.f, 0.f, FMath::FRandRange(20.f, 120.f)), FMath::FRandRange(0.8f, 2.0f), FMath::FRandRange(0.6f, 1.8f) });
	}
}

void AEAWBloodFX::Tick(float Dt)
{
	Super::Tick(Dt);

	// Jato pulsante: acompanha o osso (coto), diminui ate parar.
	if (JetDuration > 0.f)
	{
		JetTime += Dt;
		if (JetMesh.IsValid() && JetTime < JetDuration)
		{
			const FTransform BoneTM = JetMesh->GetSocketTransform(JetBone);
			const float Fade = 1.f - JetTime / JetDuration;
			const float Pulse = 0.35f + 0.65f * FMath::Max(0.f, FMath::Sin(JetTime * 11.f));
			EmitAccumulator += JetRate * Fade * Pulse * Dt;
			const int32 N = FMath::FloorToInt(EmitAccumulator);
			if (N > 0)
			{
				EmitAccumulator -= N;
				AddDrops(BoneTM.TransformPosition(JetLocalPoint), BoneTM.TransformVectorNoScale(JetLocalDir), N, 320.f * (0.5f + 0.5f * Fade), 14.f);
			}
		}
		else
		{
			JetDuration = 0.f;
		}
	}

	// Gotas: gravidade + arrasto; ao tocar o chao, algumas viram mancha.
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		FDrop& Dr = Items[i];
		Dr.V.Z -= 980.f * Dt;
		Dr.V *= FMath::Max(0.f, 1.f - 0.7f * Dt);
		Dr.P += Dr.V * Dt;
		Dr.Life -= Dt;
		if (Dr.P.Z <= GroundZ + 0.5f)
		{
			if (FMath::FRand() < 0.35f)
			{
				SpawnStain(GetWorld(), FVector(Dr.P.X, Dr.P.Y, GroundZ), 6.f + 10.f * Dr.Size);
			}
			Items.RemoveAtSwap(i);
		}
		else if (Dr.Life <= 0.f)
		{
			Items.RemoveAtSwap(i);
		}
	}

	Drops->ClearInstances();
	for (const FDrop& Dr : Items)
	{
		const float S = 0.016f * Dr.Size;
		// Gota alongada na direcao do movimento.
		const FRotator R = Dr.V.Rotation();
		Drops->AddInstanceWorldSpace(FTransform(R, Dr.P, FVector(S * 2.2f, S, S)));
	}

	if (Items.Num() == 0 && JetDuration <= 0.f)
	{
		Destroy();
	}
}
