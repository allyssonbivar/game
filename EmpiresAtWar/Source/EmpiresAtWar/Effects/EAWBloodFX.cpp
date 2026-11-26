#include "Effects/EAWBloodFX.h"
#include "Effects/EAWSprites.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

UMaterialInterface* AEAWBloodFX::StainMaterial = nullptr;
TArray<TWeakObjectPtr<UDecalComponent>> AEAWBloodFX::Stains;
TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UDecalComponent>>> AEAWBloodFX::OwnedStains;

AEAWBloodFX::AEAWBloodFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // depois da camera: sprites olham para a camera deste quadro

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Blood(TEXT("/Game/Efeitos/M_Sangue_Sprite.M_Sangue_Sprite"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Mist(TEXT("/Game/Efeitos/M_Sangue_Nevoa.M_Sangue_Nevoa"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Stain(TEXT("/Game/Efeitos/M_Sangue_Mancha.M_Sangue_Mancha"));

	Sprites = EAWSprites::MakeComponent(this, TEXT("Sprites"), Plane.Object, Blood.Get(), 2);   // [0] quadro do atlas, [1] desaparecer
	RootComponent = Sprites;
	MistSprites = EAWSprites::MakeComponent(this, TEXT("Mist"), Plane.Object, Mist.Get(), 3);   // [0] opacidade, [1] quadro, [2] brilho
	MistSprites->SetupAttachment(RootComponent);
	if (Stain.Get()) { StainMaterial = Stain.Get(); }
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

void AEAWBloodFX::Burst(UWorld* World, const FVector& Location, const FVector& Dir, int32 Count, float Speed, AActor* InStainOwner)
{
	if (AEAWBloodFX* FX = SpawnFX(World, Location))
	{
		FX->StainOwner = InStainOwner;
		// Como nas referencias: leque de filetes rapidos e finos, gotas e uma nevoa curta no ponto.
		FX->AddStreaks(Location, Dir, Count * 3, Speed * 1.7f, 32.f, 1.3f);
		FX->AddDrops(Location, Dir, Count * 2, Speed, 45.f);
		FX->AddMist(Location, Dir, FMath::Clamp(Count / 5, 2, 6), FMath::Clamp(Speed / 12.f, 14.f, 40.f));
	}
}

void AEAWBloodFX::Jet(UWorld* World, USkeletalMeshComponent* Mesh, FName Bone, const FVector& WorldPoint, const FVector& WorldDir, float Duration, float Rate,
	float InJetSpeed, bool bHeartbeat)
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
		FX->JetSpeed = InJetSpeed;
		FX->JetPhase = FMath::FRand();
		FX->bJetHeartbeat = bHeartbeat;
		FX->StainOwner = Mesh->GetOwner();
	}
}

UDecalComponent* AEAWBloodFX::SpawnStain(UWorld* World, const FVector& Location, float Size, AActor* Owner, int32 Variant, int32 SortOrder)
{
	if (!World || !StainMaterial)
	{
		return nullptr;
	}
	const FRotator Rot(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
	// Sem tempo de vida: o sangue fica no chao (profundidade de 8 cm: so pinta o chao e a parte do corpo encostada nele).
	UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(World, StainMaterial, FVector(8.f, Size, Size * FMath::FRandRange(0.8f, 1.2f)), Location, Rot, 0.f);
	if (!D)
	{
		return nullptr;
	}
	D->SortOrder = SortOrder;
	if (UMaterialInstanceDynamic* MID = D->CreateDynamicMaterialInstance())
	{
		MID->SetScalarParameterValue(TEXT("Variante"), (float)(Variant >= 0 ? Variant : FMath::RandRange(0, 2)));
	}
	TArray<TWeakObjectPtr<UDecalComponent>>& List = Owner ? OwnedStains.FindOrAdd(Owner) : Stains;
	List.Add(D);
	const int32 Max = Owner ? 200 : 300;
	while (List.Num() > Max)
	{
		if (List[0].IsValid())
		{
			List[0]->DestroyComponent();
		}
		List.RemoveAt(0);
	}
	return D;
}

void AEAWBloodFX::ClearStainsOf(AActor* Owner)
{
	if (TArray<TWeakObjectPtr<UDecalComponent>>* List = OwnedStains.Find(Owner))
	{
		for (TWeakObjectPtr<UDecalComponent>& D : *List)
		{
			if (D.IsValid())
			{
				D->DestroyComponent();
			}
		}
		OwnedStains.Remove(Owner);
	}
}

void AEAWBloodFX::Pool(UWorld* World, USkeletalMeshComponent* Mesh, FName Bone, float MaxSize, float GrowTime, float Delay)
{
	if (!Mesh)
	{
		return;
	}
	if (AEAWBloodFX* FX = SpawnFX(World, Mesh->GetSocketLocation(Bone)))
	{
		FX->PoolMesh = Mesh;
		FX->PoolBone = Bone;
		FX->PoolMax = MaxSize;
		FX->PoolGrow = FMath::Max(0.5f, GrowTime);
		FX->PoolDelay = Delay;
		FX->PoolTime = 0.f;
		FX->StainOwner = Mesh->GetOwner();
	}
}

void AEAWBloodFX::UpdatePool(float Dt)
{
	if (PoolTime < 0.f)
	{
		return;
	}
	PoolTime += Dt;
	if (PoolTime < PoolDelay)
	{
		return;
	}
	if (!PoolDecal.IsValid())
	{
		if (!PoolMesh.IsValid())
		{
			PoolTime = -1.f;
			return;
		}
		// Corpo no chao: a poca nasce no chao embaixo do osso (rastreio so no cenario).
		const FVector From = PoolMesh->GetSocketLocation(PoolBone) + FVector(0.f, 0.f, 30.f);
		FHitResult Hit;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		const float Z = GetWorld()->LineTraceSingleByObjectType(Hit, From, From - FVector(0.f, 0.f, 300.f), Objects) ? Hit.ImpactPoint.Z : GroundZ;
		PoolDecal = SpawnStain(GetWorld(), FVector(From.X, From.Y, Z), 10.f, StainOwner.Get(), 3, 5);
		if (!PoolDecal.IsValid())
		{
			PoolTime = -1.f;
			return;
		}
	}
	// Cresce rapido no comeco e devagar no fim (o sangue escorre e se espalha).
	const float T = FMath::Clamp((PoolTime - PoolDelay) / PoolGrow, 0.f, 1.f);
	const float Size = FMath::Lerp(10.f, PoolMax, 1.f - FMath::Square(1.f - T));
	const FVector Cur = PoolDecal->DecalSize;
	PoolDecal->DecalSize = FVector(8.f, Size, Size * (Cur.Z / FMath::Max(1.f, Cur.Y)));
	PoolDecal->MarkRenderStateDirty();
	if (T >= 1.f)
	{
		PoolTime = -1.f;   // terminou de crescer; a mancha fica
	}
}

void AEAWBloodFX::AddStreaks(const FVector& Location, const FVector& Dir, int32 Count, float Speed, float SpreadDeg, float SizeScale)
{
	const FVector D = Dir.GetSafeNormal();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector V = FMath::VRandCone(D, FMath::DegreesToRadians(SpreadDeg)) * Speed * FMath::FRandRange(0.35f, 1.15f);
		Items.Add({ Location + FMath::VRand() * 1.5f, V, 0.f, FMath::FRandRange(0.5f, 1.4f), SizeScale * FMath::FRandRange(0.6f, 1.4f), 0.f,
			(uint8)FMath::RandRange(0, 2), EKind::Streak });
	}
}

void AEAWBloodFX::AddDrops(const FVector& Location, const FVector& Dir, int32 Count, float Speed, float SpreadDeg)
{
	const FVector D = Dir.GetSafeNormal();
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector V = FMath::VRandCone(D, FMath::DegreesToRadians(SpreadDeg)) * Speed * FMath::FRandRange(0.2f, 1.f)
			+ FVector(0.f, 0.f, FMath::FRandRange(0.f, 80.f));
		Items.Add({ Location, V, 0.f, FMath::FRandRange(0.8f, 2.2f), FMath::FRandRange(0.5f, 1.6f), FMath::FRandRange(0.f, 360.f), 3, EKind::Drop });
	}
}

void AEAWBloodFX::AddMist(const FVector& Location, const FVector& Dir, int32 Count, float Size)
{
	const FVector D = Dir.GetSafeNormal();
	for (int32 i = 0; i < Count; ++i)
	{
		Items.Add({ Location + D * FMath::FRandRange(0.f, 10.f), FMath::VRandCone(D, 0.8f) * FMath::FRandRange(60.f, 220.f), 0.f,
			FMath::FRandRange(0.5f, 1.1f), Size * FMath::FRandRange(0.7f, 1.3f), FMath::FRandRange(0.f, 360.f), (uint8)FMath::RandRange(0, 3), EKind::Mist });
	}
}

void AEAWBloodFX::Tick(float Dt)
{
	Super::Tick(Dt);
	UpdatePool(Dt);

	// Jato arterial: pulsa no ritmo do coracao (~85 por minuto, caindo com a perda de sangue) e enfraquece ate parar.
	if (JetDuration > 0.f)
	{
		JetTime += Dt;
		if (JetMesh.IsValid() && JetTime < JetDuration)
		{
			const FTransform BoneTM = JetMesh->GetSocketTransform(JetBone);
			const float Fade = 1.f - JetTime / JetDuration;
			const float Bpm = FMath::Lerp(60.f, 95.f, Fade);
			JetPhase += Dt * Bpm / 60.f;
			const float Beat = FMath::Frac(JetPhase);
			// Sistole curta e forte (jato alto), diastole com fio fraco.
			const float Pulse = !bJetHeartbeat ? FMath::Lerp(0.55f, 1.f, Fade) : Beat < 0.22f ? FMath::Sin(Beat / 0.22f * PI) : 0.12f;
			const FVector P = BoneTM.TransformPosition(JetLocalPoint);
			const FVector Dir = BoneTM.TransformVectorNoScale(JetLocalDir);
			EmitAccumulator += JetRate * (0.25f + 0.75f * Fade) * Pulse * Dt;
			const int32 N = FMath::FloorToInt(EmitAccumulator);
			if (N > 0)
			{
				EmitAccumulator -= N;
				const float Spd = JetSpeed * (0.35f + 0.65f * Fade) * (0.4f + 0.6f * Pulse) * FMath::FRandRange(0.9f, 1.1f);
				AddStreaks(P, Dir, N, Spd, 5.f, 0.9f + 0.6f * Pulse);
				AddDrops(P, Dir, FMath::Max(1, N / 3), Spd * 0.6f, 20.f);
				if (Pulse > 0.9f && FMath::FRand() < 0.3f)
				{
					AddMist(P, Dir, 1, 12.f);
				}
			}
		}
		else
		{
			JetDuration = 0.f;
		}
	}

	// Gravidade + arrasto; ao tocar o chao, filetes e gotas viram mancha.
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		FDrop& Dr = Items[i];
		Dr.Age += Dt;
		if (Dr.Kind == EKind::Mist)
		{
			Dr.V *= FMath::Max(0.f, 1.f - 4.f * Dt);
			Dr.V.Z -= 30.f * Dt;
		}
		else
		{
			Dr.V.Z -= 980.f * Dt;
			Dr.V *= FMath::Max(0.f, 1.f - 0.5f * Dt);
		}
		Dr.P += Dr.V * Dt;
		if (Dr.Kind != EKind::Mist && Dr.P.Z <= GroundZ + 0.5f)
		{
			if (FMath::FRand() < (Dr.Kind == EKind::Streak ? 0.45f : 0.2f))
			{
				SpawnStain(GetWorld(), FVector(Dr.P.X, Dr.P.Y, GroundZ), 5.f + 10.f * Dr.Size, StainOwner.Get());
			}
			Items.RemoveAtSwap(i);
		}
		else if (Dr.Age >= Dr.Life)
		{
			Items.RemoveAtSwap(i);
		}
	}

	const FVector Cam = EAWSprites::ViewLocation(GetWorld());
	Sprites->ClearInstances();
	MistSprites->ClearInstances();
	for (const FDrop& Dr : Items)
	{
		const float t = Dr.Age / Dr.Life;
		if (Dr.Kind == EKind::Mist)
		{
			const float R = Dr.Size * (0.5f + 1.2f * FMath::Sqrt(t));
			const int32 Idx = MistSprites->AddInstanceWorldSpace(EAWSprites::Billboard(Dr.P, Cam, R * 4.f, R * 4.f, Dr.Roll));
			MistSprites->SetCustomDataValue(Idx, 0, 0.55f * (1.f - t) * (1.f - t), false);
			MistSprites->SetCustomDataValue(Idx, 1, (float)Dr.Atlas, false);
			MistSprites->SetCustomDataValue(Idx, 2, 1.f, false);
			continue;
		}
		const float Fade = FMath::Clamp((1.f - t) * 4.f, 0.f, 1.f);
		int32 Idx;
		if (Dr.Kind == EKind::Streak)
		{
			// Filete: comprimento pelo rastro de ~35 ms de voo (o olho ve um risco, nao uma gota).
			const float Len = FMath::Clamp(Dr.V.Size() * 0.06f, 8.f, 70.f) * Dr.Size;
			Idx = Sprites->AddInstanceWorldSpace(EAWSprites::Stretched(Dr.P - Dr.V.GetSafeNormal() * Len * 0.3f, Dr.V, Cam, Len, Len * 0.6f));
		}
		else
		{
			const float S = 2.2f * Dr.Size;
			Idx = Sprites->AddInstanceWorldSpace(EAWSprites::Billboard(Dr.P, Cam, S, S, Dr.Roll));
		}
		Sprites->SetCustomDataValue(Idx, 0, (float)Dr.Atlas, false);
		Sprites->SetCustomDataValue(Idx, 1, Fade, false);
	}
	Sprites->MarkRenderStateDirty();
	MistSprites->MarkRenderStateDirty();

	if (Items.Num() == 0 && JetDuration <= 0.f && PoolTime < 0.f)
	{
		Destroy();
	}
}
