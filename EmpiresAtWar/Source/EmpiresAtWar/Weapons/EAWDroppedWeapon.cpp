#include "Weapons/EAWDroppedWeapon.h"
#include "AnimationRuntime.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AEAWDroppedWeapon::AEAWDroppedWeapon()
{
	bReplicates = true;
	SetReplicateMovement(true);
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetCollisionProfileName(TEXT("PhysicsActor"));
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);     // soldados passam por cima
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Box->SetSimulatePhysics(true);
	Box->SetBoxExtent(FVector(40.f, 5.f, 5.f));
	Box->BodyInstance.LinearDamping = 0.2f;
	Box->BodyInstance.AngularDamping = 1.2f;
	Visual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Box);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEAWDroppedWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEAWDroppedWeapon, Weapon);
	DOREPLIFETIME(AEAWDroppedWeapon, bLoaded);
}

FText AEAWDroppedWeapon::WeaponName(EEAWWeapon W)
{
	switch (W)
	{
	case EEAWWeapon::Mosquete: return FText::FromString(TEXT("Mosquete"));
	case EEAWWeapon::Pistola:  return FText::FromString(TEXT("Pistola"));
	case EEAWWeapon::Faca:     return FText::FromString(TEXT("Faca"));
	case EEAWWeapon::Machado:  return FText::FromString(TEXT("Machado de sapador"));
	default:                   return FText::FromString(TEXT("Sabre"));
	}
}

void AEAWDroppedWeapon::OnRep_Weapon()
{
	ApplyVisual();
}

void AEAWDroppedWeapon::ApplyVisual()
{
	const TCHAR* Path = Weapon == EEAWWeapon::Mosquete ? TEXT("/Game/Armas/SK_Arma_Mosquete.SK_Arma_Mosquete")
		: Weapon == EEAWWeapon::Faca ? TEXT("/Game/Armas/SK_Arma_Faca.SK_Arma_Faca")
		: Weapon == EEAWWeapon::Machado ? TEXT("/Game/Armas/SK_Arma_Machado.SK_Arma_Machado")
		: TEXT("/Game/Armas/SK_Arma_Sabre.SK_Arma_Sabre");
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Path);
	if (!Mesh)
	{
		return;
	}
	Visual->SetSkeletalMesh(Mesh);
	// A malha da arma esta na pose de referencia do soldado: traz o osso da arma para a origem do ator
	// (sem a escala 100 da raiz, ver bug #41) e centraliza a caixa de fisica na arma.
	const FName Bone = Weapon == EEAWWeapon::Mosquete ? FName(TEXT("arma")) : FName(TEXT("weapon_r"));
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 Idx = Ref.FindBoneIndex(Bone);
	FTransform BoneCS = Idx != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Idx) : FTransform::Identity;
	BoneCS.SetScale3D(FVector::OneVector);
	const FTransform ToBone = BoneCS.Inverse();
	const FBoxSphereBounds B = Mesh->GetBounds().TransformBy(ToBone);
	VisualCenter = B.Origin;
	Visual->SetRelativeTransform(ToBone * FTransform(-B.Origin));
	Box->SetBoxExtent(B.BoxExtent.ComponentMax(FVector(2.f)));
	const float Kg = Weapon == EEAWWeapon::Mosquete ? 4.6f : Weapon == EEAWWeapon::Machado ? 1.6f : Weapon == EEAWWeapon::Faca ? 0.3f : 1.1f;
	Box->SetMassOverrideInKg(NAME_None, Kg, true);
}

AEAWDroppedWeapon* AEAWDroppedWeapon::Drop(UWorld* World, EEAWWeapon InWeapon, const FTransform& At, const FVector& Velocity, bool bInLoaded)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEAWDroppedWeapon* D = World->SpawnActorDeferred<AEAWDroppedWeapon>(AEAWDroppedWeapon::StaticClass(), At, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!D)
	{
		return nullptr;
	}
	D->Weapon = InWeapon;
	D->bLoaded = bInLoaded;
	D->FinishSpawning(At);
	D->ApplyVisual();
	D->SetActorLocation(At.TransformPosition(D->VisualCenter), false, nullptr, ETeleportType::TeleportPhysics);   // a arma aparece exatamente onde estava na mao
	D->Box->SetPhysicsLinearVelocity(Velocity);
	D->Box->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 200.f);
	return D;
}
