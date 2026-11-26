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
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerStart.h"

AEAWGameMode::AEAWGameMode()
{
	DefaultPawnClass = AEAWCharacter::StaticClass();
	HUDClass = AEAWHUD::StaticClass();
}

void AEAWGameMode::SpawnTestField(UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("EAWCampo")))
		{
			return;   // ja existe nesta maquina
		}
	}
	// Mesmo lugar em todas as maquinas: embaixo do primeiro PlayerStart do mapa.
	FVector From = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		From = It->GetActorLocation();
		break;
	}
	FHitResult Hit;
	const float FloorZ = World->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 1000.f), ECC_Visibility) ? Hit.ImpactPoint.Z : From.Z - 100.f;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AStaticMeshActor* Field = World->SpawnActor<AStaticMeshActor>(FVector(From.X, From.Y, FloorZ - 0.5f), FRotator::ZeroRotator, P))
	{
		Field->Tags.Add(TEXT("EAWCampo"));
		Field->SetMobility(EComponentMobility::Movable);
		Field->GetStaticMeshComponent()->SetStaticMesh(Plane);
		Field->SetActorScale3D(FVector(600.f, 600.f, 1.f));
		Field->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	}
}

APawn* AEAWGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Multiplayer: cada jogador nasce 2,5 m ao lado do anterior (o mapa de teste tem um so PlayerStart).
	FTransform T = SpawnTransform;
	const int32 N = SpawnCount++;
	const float Side = (N % 2 == 0 ? 1.f : -1.f) * 250.f * ((N + 1) / 2);
	T.AddToTranslation(T.GetRotation().GetRightVector() * Side);
	FActorSpawnParameters Params;
	Params.Instigator = GetInstigator();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return GetWorld()->SpawnActor<APawn>(GetDefaultPawnClassForController(NewPlayer), T, Params);
}

void AEAWGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	const TCHAR* Cmd = FCommandLine::Get();
	const bool bDuel = FParse::Param(Cmd, TEXT("EAWDuelo"));

	// -EAWCampo: chao de 600 x 600 m (o mapa de teste termina a ~5 m). Criado em cada maquina (servidor e clientes).
	if (FParse::Param(Cmd, TEXT("EAWCampo")))
	{
		SpawnTestField(GetWorld());
	}

	if (bDuel)
	{
		SetupDuel(NewPlayer);   // sem pawn para o jogador
	}
	else
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		if ((bSpawnBotOnStart || FParse::Param(Cmd, TEXT("EAWBot"))) && !bSpawnedInitialBot && NewPlayer && NewPlayer->GetPawn())
		{
			bSpawnedInitialBot = true;
			AEAWCharacter* Bot = SpawnBotNear(NewPlayer->GetPawn());
			// -EAWCamBot=N : camera fixa a N cm ao lado do bot, na altura do pescoco (ver sangue e fumaca de perto).
			float CamDist = 0.f;
			if (Bot && FParse::Value(Cmd, TEXT("EAWCamBot="), CamDist))
			{
				const FVector Neck = Bot->GetActorLocation() + FVector(0.f, 0.f, 55.f);
				const FVector Shooter = NewPlayer->GetPawn()->GetActorLocation();
				const FVector Side = FVector::CrossProduct((Neck - Shooter).GetSafeNormal2D(), FVector::UpVector);
				// -EAWCamAlto=N : sobe a camera N cm e olha para o chao do bot (ver a poca de sangue).
				float CamUp = 0.f;
				FParse::Value(Cmd, TEXT("EAWCamAlto="), CamUp);
				const FVector CamLoc = (Neck + Shooter) * 0.5f + Side * CamDist + FVector(0.f, 0.f, -10.f + CamUp);
				// -EAWCamJogador : olha para o seu soldado (ver agachar, andar agachado, recarga e fumaca em 3a pessoa).
				const FVector LookAt = FParse::Param(Cmd, TEXT("EAWCamJogador")) ? Shooter - FVector(0.f, 0.f, 25.f)
					: CamUp > 0.f ? Bot->GetActorLocation() - FVector(0.f, 0.f, 80.f) : Neck * 0.7f + Shooter * 0.3f;
				if (ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(CamLoc, (LookAt - CamLoc).Rotation()))
				{
					Cam->GetCameraComponent()->SetFieldOfView(70.f);
					NewPlayer->SetViewTarget(Cam);
				}
			}
		}
	}

	if (FParse::Param(Cmd, TEXT("EAWAutoAtaque")) && NewPlayer && !AutoPC.IsValid())
	{
		AutoPC = NewPlayer;
		GetWorldTimerManager().SetTimer(AutoTimer, this, &AEAWGameMode::AutoAttack, 1.8f, true, 0.6f);
	}
	if (FParse::Param(Cmd, TEXT("EAWAutoEspecial")) && NewPlayer && !AutoPC.IsValid())
	{
		AutoPC = NewPlayer;
		GetWorldTimerManager().SetTimer(AutoTimer, this, &AEAWGameMode::AutoSpecial, 0.1f, true, 1.0f);
	}

	// -EAWArmasNoChao: faca e machado no chao a frente do jogador; -EAWAutoPegar: segura F sozinho depois de 1,5 s.
	if (FParse::Param(Cmd, TEXT("EAWArmasNoChao")) && NewPlayer && NewPlayer->GetPawn())
	{
		const APawn* P = NewPlayer->GetPawn();
		const FVector F = P->GetActorForwardVector(), R = P->GetActorRightVector(), Base = P->GetActorLocation() - FVector(0.f, 0.f, 60.f);
		AEAWDroppedWeapon::Drop(GetWorld(), EEAWWeapon::Faca, FTransform(FRotator(0.f, 30.f, 0.f), Base + F * 120.f - R * 10.f), FVector::ZeroVector, true);
		AEAWDroppedWeapon::Drop(GetWorld(), EEAWWeapon::Machado, FTransform(FRotator(0.f, 80.f, 0.f), Base + F * 160.f + R * 120.f), FVector::ZeroVector, true);
		AEAWDroppedWeapon::Drop(GetWorld(), EEAWWeapon::Pistola, FTransform(FRotator(0.f, -40.f, 0.f), Base + F * 150.f - R * 120.f), FVector::ZeroVector, true);
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

	if (FParse::Param(Cmd, TEXT("EAWAutoTiro")) && NewPlayer && !AutoPC.IsValid())
	{
		AutoPC = NewPlayer;
		GetWorldTimerManager().SetTimer(AutoTimer, this, &AEAWGameMode::AutoFire, 0.1f, true, 0.5f);
	}

	// Testes automaticos: o mouse real (de quem esta usando o PC) nao mexe a camera.
	if (NewPlayer && (FParse::Param(Cmd, TEXT("EAWAutoTiro")) || FParse::Param(Cmd, TEXT("EAWAutoPegar")) || FParse::Param(Cmd, TEXT("EAWAutoAndar")) || FParse::Param(Cmd, TEXT("EAWAutoAtaque")) || FParse::Param(Cmd, TEXT("EAWAutoEspecial"))))
	{
		NewPlayer->SetIgnoreLookInput(true);
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

void AEAWGameMode::AutoSpecial()
{
	AEAWCharacter* Soldier = AutoPC.IsValid() ? Cast<AEAWCharacter>(AutoPC->GetPawn()) : nullptr;
	if (!Soldier || !Soldier->GetCombat())
	{
		return;
	}
	UMeleeCombatComponent* C = Soldier->GetCombat();
	AutoPhaseTime += 0.1f;
	switch (AutoPhase)
	{
	case 0:
	{
		// So carrega se ainda houver alguem vivo para acertar.
		bool bTarget = false;
		for (TActorIterator<AEAWCharacter> It(GetWorld()); It; ++It)
		{
			bTarget |= *It != Soldier && It->GetWounds() && It->GetWounds()->CanFight();
		}
		if (bTarget && C->GetState() == EMeleeState::Idle && AutoPhaseTime > 1.0f) { C->RequestChargeStart(); AutoPhase = 1; AutoPhaseTime = 0.f; }
		break;
	}
	case 1: if (C->GetChargeAlpha() >= 1.f) { C->RequestChargeRelease(true, EAttackDirection::Special); AutoPhase = 2; AutoPhaseTime = 0.f; } break;
	default: if (AutoPhaseTime > 4.0f) { AutoPhase = 0; AutoPhaseTime = 0.f; } break;
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
	// -EAWMirarPescoco: aponta para o pescoco do soldado mais proximo (teste da morte por tiro no pescoco).
	if (AutoPhase <= 1 && FParse::Param(FCommandLine::Get(), TEXT("EAWMirarPescoco")))
	{
		AEAWCharacter* Best = nullptr;
		for (TActorIterator<AEAWCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Soldier && (!Best || Soldier->GetDistanceTo(*It) < Soldier->GetDistanceTo(Best)))
			{
				Best = *It;
			}
		}
		if (Best)
		{
			FVector Origin, Dir;
			Soldier->GetShotAim(Origin, Dir);
			const FVector Neck = (Best->GetMesh()->GetSocketLocation(TEXT("neck_01")) + Best->GetMesh()->GetSocketLocation(TEXT("head"))) * 0.5f;
			AutoPC->SetControlRotation((Neck - Origin).Rotation());
		}
	}
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
