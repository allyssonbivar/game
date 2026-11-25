#include "Effects/EAWBallistics.h"
#include "EAWCharacter.h"
#include "EmpiresAtWar.h"
#include "Weapons/EAWFirearmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
	// Diagnostico (-EAWDiagBala): rastreio de teste ate o peito de cada soldado.
	if (FParse::Param(FCommandLine::Get(), TEXT("EAWDiagBala")))
	{
		for (TActorIterator<AEAWCharacter> It(World); It; ++It)
		{
			if (*It == Shooter) continue;
			USkeletalMeshComponent* M = It->GetMesh();
			int32 Valid = 0;
			for (FBodyInstance* Bi : M->Bodies) Valid += Bi && Bi->IsValidBodyInstance();
			const FVector Target = M->GetSocketLocation(TEXT("spine_02"));
			FHitResult H;
			FCollisionQueryParams Q2; Q2.AddIgnoredActor(Shooter); Q2.AddIgnoredComponent(It->GetCapsuleComponent());
			FCollisionObjectQueryParams O2; O2.AddObjectTypesToQuery(ECC_Pawn); O2.AddObjectTypesToQuery(ECC_PhysicsBody); O2.AddObjectTypesToQuery(ECC_WorldDynamic);
			const bool bHit = World->LineTraceSingleByObjectType(H, Origin, Target + (Target - Origin).GetSafeNormal() * 50.f, O2, Q2);
			FHitResult H2;
			const bool bComp = M->LineTraceComponent(H2, Origin, Target + (Target - Origin).GetSafeNormal() * 50.f, FCollisionQueryParams());
			UE_LOG(LogEAWCombat, Log, TEXT("diag bala: %s malha colisao=%d tipo=%d corpos=%d/%d | rastreio: %d %s osso=%s | so a malha: %d osso=%s"),
				*It->GetName(), (int32)M->GetCollisionEnabled(), (int32)M->GetCollisionObjectType(), Valid, M->Bodies.Num(),
				bHit, *GetNameSafe(H.GetComponent()), *H.BoneName.ToString(), bComp, *H2.BoneName.ToString());
		}
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
	// Rastreio por tipo de objeto para no primeiro que encontra: a capsula de movimento do soldado esconde
	// os corpos do esqueleto que estao dentro dela (bug #48). Ignora o que nao conta e rastreia de novo.
	for (int32 Try = 0; Try < 8; ++Try)
	{
		FHitResult H;
		if (!GetWorld()->LineTraceSingleByObjectType(H, B.Pos, NewPos, Obj, Q))
		{
			break;
		}
		UPrimitiveComponent* C = H.GetComponent();
		if (!C)
		{
			break;
		}
		bool bSkip = false;
		if (AEAWCharacter* Soldier = Cast<AEAWCharacter>(H.GetActor()))
		{
			// So os corpos fisicos do esqueleto contam (a capsula de movimento e as hitboxes do corpo a corpo nao).
			bSkip = C != Soldier->GetMesh() || H.BoneName == NAME_None;
		}
		else if (C->IsA<USkeletalMeshComponent>() && C->GetCollisionObjectType() == ECC_PhysicsBody)
		{
			bSkip = true;   // pecas soltas do desmembramento: deixa passar
		}
		if (bSkip)
		{
			Q.AddIgnoredComponent(C);
			continue;
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
