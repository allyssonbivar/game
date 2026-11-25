#include "Effects/EAWBallistics.h"
#include "EAWCharacter.h"
#include "EmpiresAtWar.h"
#include "Weapons/EAWFirearmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AEAWBallistics::AEAWBallistics()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // depois das animacoes: os ossos ja estao no lugar
	SetActorHiddenInGame(true);
}

float AEAWBallistics::SphereDrag(float SpeedMs)
{
	const float Mach = SpeedMs / 343.f;
	if (Mach < 0.6f) return 0.47f;
	if (Mach < 1.0f) return FMath::Lerp(0.47f, 0.80f, (Mach - 0.6f) / 0.4f);
	if (Mach < 1.2f) return FMath::Lerp(0.80f, 0.95f, (Mach - 1.0f) / 0.2f);
	return 0.95f;
}

void AEAWBallistics::Fire(UWorld* World, const FVector& Origin, const FVector& Velocity, UEAWFirearmComponent* Firearm, AActor* Shooter,
	const FFirearmConfig& Config)
{
	if (!World)
	{
		return;
	}
	AEAWBallistics* Sim = nullptr;
	for (TActorIterator<AEAWBallistics> It(World); It; ++It)
	{
		Sim = *It;
		break;
	}
	if (!Sim)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Sim = World->SpawnActor<AEAWBallistics>(AEAWBallistics::StaticClass(), FTransform::Identity, P);
	}
	const float R = Config.BallDiameterCm * 0.005f;   // raio em m
	FBall B;
	B.Pos = Origin;
	B.Vel = Velocity;
	B.Age = 0.f;
	B.K = 0.5f * 1.225f * PI * R * R / Config.BallMassKg;
	B.Life = Config.BallLifetime;
	B.Firearm = Firearm;
	B.Shooter = Shooter;
	Sim->Balls.Add(B);
}

bool AEAWBallistics::Step(FBall& B, float Dt)
{
	// a = g - K*Cd*|v|*v   (em m/s^2; posicoes em cm)
	const FVector Vm = B.Vel / 100.f;
	const float Speed = Vm.Size();
	const FVector Acc = FVector(0.f, 0.f, -9.81f) - Vm * (B.K * SphereDrag(Speed) * Speed);
	const FVector NewVel = B.Vel + Acc * 100.f * Dt;
	const FVector NewPos = B.Pos + (B.Vel + NewVel) * 0.5f * Dt;

	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	Obj.AddObjectTypesToQuery(ECC_Pawn);
	Obj.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(EAWBall), false);
	if (B.Shooter.IsValid())
	{
		Q.AddIgnoredActor(B.Shooter.Get());
	}
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, B.Pos, NewPos, Obj, Q);
	Hits.Sort([](const FHitResult& A, const FHitResult& C) { return A.Distance < C.Distance; });
	for (const FHitResult& H : Hits)
	{
		UPrimitiveComponent* C = H.GetComponent();
		if (!C)
		{
			continue;
		}
		if (AEAWCharacter* Soldier = Cast<AEAWCharacter>(H.GetActor()))
		{
			// So os corpos fisicos do esqueleto contam (a capsula de movimento e as hitboxes do corpo a corpo nao).
			if (C != Soldier->GetMesh() || H.BoneName == NAME_None)
			{
				continue;
			}
		}
		else if (C->IsA<USkeletalMeshComponent>() && C->GetCollisionObjectType() == ECC_PhysicsBody)
		{
			continue;   // pecas soltas do desmembramento: deixa passar
		}
		// Velocidade no ponto do impacto (interpolada no passo).
		const float Alpha = (H.Location - B.Pos).Size() / FMath::Max(1.f, (NewPos - B.Pos).Size());
		if (B.Firearm.IsValid())
		{
			B.Firearm->HandleBallHit(H, FMath::Lerp(B.Vel, NewVel, Alpha));
		}
		return false;
	}
	B.Pos = NewPos;
	B.Vel = NewVel;
	B.Age += Dt;
	return B.Age < B.Life && B.Pos.Z > -100000.f;
}

void AEAWBallistics::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float H = 1.f / 120.f;
	for (int32 i = Balls.Num() - 1; i >= 0; --i)
	{
		float Left = DeltaSeconds;
		bool bAlive = true;
		while (bAlive && Left > 0.f)
		{
			const float Dt = FMath::Min(H, Left);
			bAlive = Step(Balls[i], Dt);
			Left -= Dt;
		}
		if (!bAlive)
		{
			Balls.RemoveAtSwap(i);
		}
	}
}
