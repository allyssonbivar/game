#include "EAWGameMode.h"
#include "EAWCharacter.h"
#include "EAWBotController.h"
#include "EAWHUD.h"
#include "Weapons/EAWFirearmComponent.h"
#include "Weapons/EAWDroppedWeapon.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/MeleeCombatComponent.h"
#include "EmpiresAtWar.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

AEAWGameMode::AEAWGameMode()
{
	DefaultPawnClass = AEAWCharacter::StaticClass();
	HUDClass = AEAWHUD::StaticClass();
}

void AEAWGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	const TCHAR* Cmd = FCommandLine::Get();
	const bool bDuel = FParse::Param(Cmd, TEXT("EAWDuelo"));

	// -EAWCampo: chao de 600 x 600 m para testar tiro a distancia (o mapa de teste termina a ~5 m).
	if (FParse::Param(Cmd, TEXT("EAWCampo")) && !bFieldSpawned)
	{
		bFieldSpawned = true;
		AActor* Start = FindPlayerStart(NewPlayer);
		const FVector From = Start ? Start->GetActorLocation() : FVector::ZeroVector;
		FHitResult Hit;
		const float FloorZ = GetWorld()->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 1000.f), ECC_Visibility) ? Hit.ImpactPoint.Z : From.Z - 100.f;
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (AStaticMeshActor* Field = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(From.X, From.Y, FloorZ - 0.5f), FRotator::ZeroRotator))
		{
			Field->SetMobility(EComponentMobility::Movable);
			Field->GetStaticMeshComponent()->SetStaticMesh(Plane);
			Field->SetActorScale3D(FVector(600.f, 600.f, 1.f));
			Field->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		}
	}

	if (bDuel)
	{
		SetupDuel(NewPlayer);   // sem pawn para o jogador
	}
	else
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		if (bSpawnBotOnStart && !bSpawnedInitialBot && NewPlayer && NewPlayer->GetPawn())
		{
			bSpawnedInitialBot = true;
			SpawnBotNear(NewPlayer->GetPawn());
		}
	}

	if (FParse::Param(Cmd, TEXT("EAWAutoAtaque")) && NewPlayer)
	{
		AutoPC = NewPlayer;
		GetWorldTimerManager().SetTimer(AutoTimer, this, &AEAWGameMode::AutoAttack, 1.8f, true, 0.6f);
	}

	// -EAWArmasNoChao: faca e machado no chao a frente do jogador; -EAWAutoPegar: segura F sozinho depois de 1,5 s.
	if (FParse::Param(Cmd, TEXT("EAWArmasNoChao")) && NewPlayer && NewPlayer->GetPawn())
	{
		const APawn* P = NewPlayer->GetPawn();
		const FVector F = P->GetActorForwardVector(), R = P->GetActorRightVector(), Base = P->GetActorLocation() - FVector(0.f, 0.f, 60.f);
		AEAWDroppedWeapon::Drop(GetWorld(), EEAWWeapon::Faca, FTransform(FRotator(0.f, 30.f, 0.f), Base + F * 120.f - R * 10.f), FVector::ZeroVector, true);
		AEAWDroppedWeapon::Drop(GetWorld(), EEAWWeapon::Machado, FTransform(FRotator(0.f, 80.f, 0.f), Base + F * 160.f + R * 120.f), FVector::ZeroVector, true);
		if (FParse::Param(Cmd, TEXT("EAWAutoPegar")))
		{
			TWeakObjectPtr<AEAWCharacter> Soldier = Cast<AEAWCharacter>(NewPlayer->GetPawn());
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Soldier]()
			{
				if (Soldier.IsValid()) Soldier->TestStartPickup();
			}), 1.5f, false);
		}
	}

	if (FParse::Param(Cmd, TEXT("EAWAutoTiro")) && NewPlayer)
	{
		AutoPC = NewPlayer;
		GetWorldTimerManager().SetTimer(AutoTimer, this, &AEAWGameMode::AutoFire, 0.1f, true, 0.5f);
	}

	int32 Shots = 0;
	if (FParse::Value(Cmd, TEXT("EAWShots="), Shots) && Shots > 0 && ShotsLeft == 0)
	{
		float Every = 0.2f, Delay = 2.0f;
		FParse::Value(Cmd, TEXT("EAWShotEvery="), Every);
		FParse::Value(Cmd, TEXT("EAWShotDelay="), Delay);
		ShotsLeft = Shots;
		GetWorldTimerManager().SetTimer(ShotTimer, this, &AEAWGameMode::TakeShot, FMath::Max(0.05f, Every), true, FMath::Max(0.05f, Delay));
	}
}

void AEAWGameMode::SetupDuel(APlayerController* Viewer)
{
	AActor* Start = FindPlayerStart(Viewer);
	const FVector Center = Start ? Start->GetActorLocation() : FVector(0.f, 0.f, 100.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (int32 i = 0; i < 2; ++i)
	{
		const float Side = i == 0 ? -1.f : 1.f;
		float Half = 110.f;
		FParse::Value(FCommandLine::Get(), TEXT("EAWDueloDist="), Half);   // teste: comecam longe e vem trotando
		Half = FMath::Max(110.f, Half * 0.5f);
		const FVector Loc = Center + FVector(0.f, Side * Half, 0.f);
		const FRotator Rot = (Center - Loc).GetSafeNormal2D().Rotation();
		if (AEAWCharacter* Bot = GetWorld()->SpawnActor<AEAWCharacter>(AEAWCharacter::StaticClass(), Loc, Rot, Params))
		{
			ConfigureBot(Bot);
			Bot->SetShowHitboxes(false);
		}
	}

	// Camera fixa de lado, um pouco acima, olhando o centro do duelo.
	const FVector CamLoc = Center + FVector(-250.f, 40.f, 30.f);
	ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(CamLoc, (Center + FVector(0.f, 0.f, 10.f) - CamLoc).Rotation());
	if (Viewer && Cam)
	{
		Viewer->SetViewTarget(Cam);
	}
	UE_LOG(LogEAWCombat, Log, TEXT("Modo duelo: dois bots em %s"), *Center.ToString());
}

void AEAWGameMode::ConfigureBot(AEAWCharacter* Bot)
{
	if (!Bot)
	{
		return;
	}
	Bot->SpawnDefaultController();
	if (FParse::Param(FCommandLine::Get(), TEXT("EAWBotPassivo")))
	{
		if (AEAWBotController* AI = Cast<AEAWBotController>(Bot->GetController()))
		{
			AI->bPassive = true;
		}
	}
}

void AEAWGameMode::AutoAttack()
{
	APawn* Pawn = AutoPC.IsValid() ? AutoPC->GetPawn() : nullptr;
	AEAWCharacter* Soldier = Cast<AEAWCharacter>(Pawn);
	if (!Soldier)
	{
		return;
	}
	UMeleeCombatComponent* C = Soldier->GetCombat();
	switch (AutoStep++ % 5)
	{
	case 0: case 1: C->RequestSlash(); break;
	case 2: C->RequestAttack(EAttackDirection::Overhead); break;
	case 3: C->RequestAttack(EAttackDirection::Stab); break;
	default: C->RequestParry(); break;
	}
}

void AEAWGameMode::AutoFire()
{
	AEAWCharacter* Soldier = AutoPC.IsValid() ? Cast<AEAWCharacter>(AutoPC->GetPawn()) : nullptr;
	if (!Soldier || !Soldier->GetFirearm())
	{
		return;
	}
	UEAWFirearmComponent* F = Soldier->GetFirearm();
	AutoPhaseTime += 0.1f;
	switch (AutoPhase)
	{
	case 0: if (AutoPhaseTime > 1.0f && F->GetState() == EFirearmState::Lowered) { F->RequestAim(true); AutoPhase = 1; AutoPhaseTime = 0.f; } break;
	case 1: if (F->GetState() == EFirearmState::Aiming && AutoPhaseTime > 1.5f) { F->RequestFire(); AutoPhase = 2; AutoPhaseTime = 0.f; } break;
	case 2: if (AutoPhaseTime > 1.5f) { F->RequestAim(false); AutoPhase = 3; AutoPhaseTime = 0.f; } break;
	case 3: if (AutoPhaseTime > 0.7f) { F->RequestReload(); AutoPhase = 4; AutoPhaseTime = 0.f; } break;
	default: if (F->IsLoaded() && F->GetState() == EFirearmState::Lowered) { AutoPhase = 0; AutoPhaseTime = 0.f; } break;
	}
}

void AEAWGameMode::TakeShot()
{
	if (ShotsLeft <= 0)
	{
		GetWorldTimerManager().ClearTimer(ShotTimer);
		FPlatformMisc::RequestExit(false);
		return;
	}
	const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Testes"), FString::Printf(TEXT("shot_%03d.png"), ShotIndex++));
	FScreenshotRequest::RequestScreenshot(File, false, false);
	--ShotsLeft;
}

AEAWCharacter* AEAWGameMode::SpawnBotNear(APawn* Near)
{
	if (!Near)
	{
		return nullptr;
	}

	const FVector Forward = Near->GetActorForwardVector().GetSafeNormal2D();
	float Dist = 300.f;
	FParse::Value(FCommandLine::Get(), TEXT("EAWBotDist="), Dist);
	const FVector Location = Near->GetActorLocation() + Forward * Dist + FVector(0.f, 0.f, 20.f);
	const FRotator Rotation = (-Forward).Rotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEAWCharacter* Bot = GetWorld()->SpawnActor<AEAWCharacter>(AEAWCharacter::StaticClass(), Location, Rotation, Params);
	ConfigureBot(Bot);
	return Bot;
}
