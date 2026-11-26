#include "EAWBotController.h"
#include "EAWCharacter.h"
#include "Combat/MeleeCombatComponent.h"
#include "Combat/WoundComponent.h"
#include "Weapons/EAWFirearmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EmpiresAtWar.h"
#include "EngineUtils.h"

AEAWBotController::AEAWBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	StrafeSeed = FMath::FRand() * 10.f;
}

AEAWCharacter* AEAWBotController::FindTarget() const
{
	const APawn* Self = GetPawn();
	AEAWCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AEAWCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Self || !It->GetWounds()->CanFight())
		{
			continue;
		}
		const float Dist = FVector::DistSquared(It->GetActorLocation(), Self->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

void AEAWBotController::SetPhase(EBotPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}
	static const TCHAR* Names[] = { TEXT("aproximando"), TEXT("atirando"), TEXT("recarregando"), TEXT("carregando (corpo a corpo)"), TEXT("lutando") };
	UE_LOG(LogEAWCombat, Log, TEXT("%s bot: %s"), GetPawn() ? *GetPawn()->GetName() : TEXT("?"), Names[(int32)NewPhase]);
	Phase = NewPhase;
	PhaseStart = GetWorld()->GetTimeSeconds();
	AimSince = -1.f;
}

void AEAWBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AEAWCharacter* Me = Cast<AEAWCharacter>(GetPawn());
	if (!Me || !Me->GetWounds()->CanFight())
	{
		return;
	}

	AEAWCharacter* Target = FindTarget();
	if (!Target)
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	SetFocus(Target);
	if (bPassive)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float Distance = FVector::Dist2D(Me->GetActorLocation(), Target->GetActorLocation());
	if (PreferBayonet < 0)
	{
		PreferBayonet = (Me->CurrentWeapon == EEAWWeapon::Mosquete && Me->bBayonet) ? 1 : 0;   // bot de baioneta (-EAWArmaBot=Mosquete)
	}

	// Perto demais para atirar: vai para a luta (de qualquer fase de tiro).
	if ((Phase == EBotPhase::Approach || Phase == EBotPhase::Shoot || Phase == EBotPhase::Reload) && Distance < MeleeDistance)
	{
		Me->Firearm->RequestCancel();
		SetPhase(EBotPhase::Charge);
	}

	switch (Phase)
	{
	case EBotPhase::Approach:
	case EBotPhase::Shoot:
	case EBotPhase::Reload:
		TickRanged(Me, Target, Distance, Now);
		break;
	case EBotPhase::Charge:
		TickCharge(Me, Target, Distance, Now);
		break;
	default:
		TickMelee(Me, Target, Distance, Now);
		break;
	}
}

// ---------------------------------------------------------------------------
// De longe: anda, para, mira e atira; recarrega se o alvo continuar longe.
// ---------------------------------------------------------------------------
void AEAWBotController::TickRanged(AEAWCharacter* Me, AEAWCharacter* Target, float Distance, float Now)
{
	UEAWFirearmComponent* Gun = Me->Firearm;
	const bool bHasMusket = Me->PrimaryWeapon == EEAWWeapon::Mosquete || Me->PrimaryWeapon == EEAWWeapon::Pistola;
	if (!bHasMusket || (ShotsFired >= MaxShots && !Gun->IsLoaded()))
	{
		SetPhase(EBotPhase::Charge);
		return;
	}
	// Arma de fogo na mao, em modo de tiro.
	if (Me->CurrentWeapon != Me->PrimaryWeapon)
	{
		Me->ServerEquip(Me->PrimaryWeapon);
		return;
	}
	if (Me->CurrentWeapon == EEAWWeapon::Mosquete && Me->bBayonet)
	{
		Me->ServerSetBayonet(false);
		return;
	}
	if (Me->IsSwitching())
	{
		return;
	}

	if (Phase == EBotPhase::Approach)
	{
		if (Distance > ShootDistance)
		{
			// Anda (sem correr) - da para ver a caminhada ate o alvo.
			Me->AddMovementInput((Target->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D(), 1.f);
			return;
		}
		SetPhase(Gun->IsLoaded() ? EBotPhase::Shoot : EBotPhase::Reload);
		return;
	}

	if (Phase == EBotPhase::Reload)
	{
		if (Gun->IsLoaded() && Gun->GetState() == EFirearmState::Lowered)
		{
			SetPhase(EBotPhase::Shoot);
		}
		else if (Gun->GetState() == EFirearmState::Lowered)
		{
			Gun->RequestReload();
		}
		return;
	}

	// Shoot: mira, segura a mira um pouco (respira) e atira. O desvio sai em AEAWCharacter::GetShotAim.
	if (!Gun->IsLoaded())
	{
		if (Gun->GetState() == EFirearmState::Aiming)
		{
			Gun->RequestAim(false);
		}
		if (Gun->GetState() == EFirearmState::Lowered)
		{
			SetPhase(ShotsFired < MaxShots && Distance > MeleeDistance * 1.6f ? EBotPhase::Reload : EBotPhase::Charge);
		}
		return;
	}
	if (Gun->GetState() == EFirearmState::Lowered)
	{
		Gun->RequestAim(true);
		AimHold = FMath::FRandRange(0.9f, 1.8f);
		AimSince = -1.f;
	}
	else if (Gun->GetState() == EFirearmState::Aiming)
	{
		if (AimSince < 0.f)
		{
			AimSince = Now;
		}
		else if (Now - AimSince >= AimHold)
		{
			Gun->RequestFire();
			++ShotsFired;
			AimSince = Now + 100.f;   // nao pede de novo enquanto o cao cai
			UE_LOG(LogEAWCombat, Log, TEXT("%s bot atira de %.1f m (tiro %d)"), *Me->GetName(), Distance / 100.f, ShotsFired);
		}
	}
}

// ---------------------------------------------------------------------------
// Carga: arma branca (ou baioneta) e vem correndo; as vezes golpe correndo.
// ---------------------------------------------------------------------------
void AEAWBotController::TickCharge(AEAWCharacter* Me, AEAWCharacter* Target, float Distance, float Now)
{
	const EEAWWeapon Melee = PreferBayonet == 1 ? EEAWWeapon::Mosquete : Me->SecondaryWeapon;
	if (Me->CurrentWeapon != Melee && Melee != EEAWWeapon::Mosquete)
	{
		Me->ServerEquip(Melee);
		return;
	}
	if (Melee == EEAWWeapon::Mosquete && Me->CurrentWeapon == EEAWWeapon::Mosquete && !Me->bBayonet)
	{
		Me->ServerSetBayonet(true);
		return;
	}
	if (Me->IsSwitching())
	{
		return;
	}
	const FVector ToTarget = (Target->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
	if (Distance > 420.f || (Me->bSprinting && Distance > 250.f))
	{
		if (Distance > 600.f && !Me->bSprinting) { Me->StartSprint(); bTriedSprintAttack = false; }
		Me->AddMovementInput(ToTarget, 1.f);
		// Golpe correndo: ja em velocidade de corrida, a ~3,3 m (prepara 0,3 s correndo ~1,4 m; o corte pega a ~1,2 m).
		if (Me->bSprinting && !bTriedSprintAttack && Distance < 335.f && Me->GetVelocity().Size2D() > 380.f)
		{
			bTriedSprintAttack = true;
			if (FMath::FRand() < 0.5f)
			{
				Me->GetCombat()->RequestAttack(EAttackDirection::Sprint);
				UE_LOG(LogEAWCombat, Log, TEXT("%s bot: golpe correndo"), *Me->GetName());
			}
		}
		return;
	}
	if (Me->bSprinting)
	{
		Me->StopSprint();
	}
	SetPhase(EBotPhase::Melee);
}

// ---------------------------------------------------------------------------
// Luta corpo a corpo.
// ---------------------------------------------------------------------------
void AEAWBotController::TickMelee(AEAWCharacter* Me, AEAWCharacter* Target, float Distance, float Now)
{
	UMeleeCombatComponent* MyCombat = Me->GetCombat();
	UMeleeCombatComponent* TheirCombat = Target->GetCombat();
	if (Distance > MeleeDistance * 1.5f)
	{
		SetPhase(EBotPhase::Charge);   // o alvo fugiu: corre atras
		return;
	}
	if (Me->bSprinting)
	{
		Me->StopSprint();
	}

	// Distancia de luta + anda de lado em volta do alvo (nao fica parado como um poste).
	const FVector ToTarget = (Target->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, ToTarget);
	if (Distance > EngageDistance)
	{
		Me->AddMovementInput(ToTarget, 1.f);
	}
	else if (Distance < EngageDistance * 0.8f)   // perto demais: as laminas em guarda atravessariam o corpo
	{
		Me->AddMovementInput(-ToTarget, 0.6f);
	}
	else if (MyCombat->GetState() == EMeleeState::Idle || MyCombat->IsBlocking())
	{
		Me->AddMovementInput(Side, 0.45f * FMath::Sin(Now * 0.8f + StrafeSeed));
	}

	// Defesa segurada: solta na hora marcada (ou se o alvo parou de atacar ha tempo).
	if (MyCombat->IsBlocking() && Now >= BlockReleaseTime)
	{
		MyCombat->RequestBlockHold(false);
	}

	// Defesa: decide uma vez por golpe do adversario.
	if (TheirCombat->GetState() == EMeleeState::Windup && TheirCombat->GetStateSerial() != LastJudgedSerial)
	{
		const float TimeToRelease = TheirCombat->GetTimeToRelease();
		if (TimeToRelease >= 0.f && TimeToRelease <= ReactionTime && Distance < EngageDistance * 1.6f)
		{
			LastJudgedSerial = TheirCombat->GetStateSerial();
			if (TheirCombat->GetDirection() != EAttackDirection::Kick && FMath::FRand() < ParryChance)
			{
				MyCombat->RequestParry();
				if (FMath::FRand() < HoldBlockChance)
				{
					MyCombat->RequestBlockHold(true);
					BlockReleaseTime = Now + FMath::FRandRange(0.8f, 1.8f);
				}
				ComboClicksLeft = 0;
				return;
			}
		}
	}

	// Contra-ataque depois de um parry bem-sucedido.
	if (MyCombat->HasRiposte() && MyCombat->GetState() == EMeleeState::Idle)
	{
		bFeintDecided = true; // nunca finta o contra-ataque
		MyCombat->RequestSlash();
		ComboClicksLeft = 0;
		NextAttackTime = Now + FMath::FRandRange(AttackIntervalMin, AttackIntervalMax);
		return;
	}

	// Combo em andamento: clica de novo no meio do golpe (a partir de 45% dele).
	if (ComboClicksLeft > 0 && MyCombat->GetState() == EMeleeState::Release && MyCombat->GetStateAlpha() >= 0.45f
		&& MyCombat->GetStateSerial() != LastComboSerial)
	{
		LastComboSerial = MyCombat->GetStateSerial();
		--ComboClicksLeft;
		if (ComboClicksLeft == 0 && bComboEndsWithE)
		{
			MyCombat->RequestAttack(EAttackDirection::Overhead);   // 2 cliques + E: estocada final
		}
		else
		{
			MyCombat->RequestSlash();
		}
		return;
	}

	// Golpe de salto: pulou um pouco antes, ataca no ar.
	if (JumpAttackAt > 0.f && Now >= JumpAttackAt)
	{
		JumpAttackAt = -1.f;
		if (Me->GetCharacterMovement()->IsFalling())
		{
			MyCombat->RequestAttack(EAttackDirection::Jump);
		}
		return;
	}

	// Finta ocasional (nao no meio de um combo).
	if (MyCombat->GetState() == EMeleeState::Windup)
	{
		if (!bFeintDecided && ComboClicksLeft == 0 && MyCombat->GetStateAlpha() >= 0.3f)
		{
			bFeintDecided = true;
			if (FMath::FRand() < FeintChance)
			{
				MyCombat->RequestFeint();
			}
		}
		return;
	}
	if (MyCombat->GetState() != EMeleeState::Release)
	{
		bFeintDecided = false;
	}

	// Chute em quem segura a defesa.
	if (TheirCombat->IsBlocking() && Distance < 160.f && Now >= NextKickTime && MyCombat->GetState() == EMeleeState::Idle)
	{
		NextKickTime = Now + 2.f;
		if (FMath::FRand() < 0.6f)
		{
			MyCombat->RequestAttack(EAttackDirection::Kick);
			NextAttackTime = Now + FMath::FRandRange(AttackIntervalMin, AttackIntervalMax);
			return;
		}
	}

	// Ataque: escolhe um golpe ou um combo.
	if (Now >= NextAttackTime && MyCombat->GetState() == EMeleeState::Idle)
	{
		if (Distance > EngageDistance * 1.2f && Distance < 270.f && FMath::FRand() < 0.12f)
		{
			Me->Jump();   // golpe de salto: entra pulando
			JumpAttackAt = Now + 0.18f;
			NextAttackTime = Now + FMath::FRandRange(AttackIntervalMin, AttackIntervalMax) + 0.8f;
			UE_LOG(LogEAWCombat, Log, TEXT("%s bot: golpe de salto"), *Me->GetName());
			return;
		}
		if (Distance > EngageDistance * 1.1f)
		{
			return;
		}
		const float Roll = FMath::FRand();
		ComboClicksLeft = 0;
		bComboEndsWithE = false;
		if (Roll < 0.30f) { MyCombat->RequestSlash(); }
		else if (Roll < 0.45f) { MyCombat->RequestAttack(EAttackDirection::Overhead); }
		else if (Roll < 0.55f) { MyCombat->RequestAttack(EAttackDirection::Stab); }
		else if (Roll < 0.72f) { MyCombat->RequestSlash(); ComboClicksLeft = 1; }                          // 2 cliques
		else if (Roll < 0.87f) { MyCombat->RequestSlash(); ComboClicksLeft = 2; }                          // 3 cliques: golpe final
		else if (Roll < 0.95f) { MyCombat->RequestSlash(); ComboClicksLeft = 2; bComboEndsWithE = true; }  // 2 cliques + E
		else { MyCombat->RequestAttack(EAttackDirection::Kick); }
		LastComboSerial = 255;
		NextAttackTime = Now + FMath::FRandRange(AttackIntervalMin, AttackIntervalMax) + 0.6f * ComboClicksLeft;
		UE_LOG(LogEAWCombat, VeryVerbose, TEXT("Bot ataca: distancia=%.0f combo=%d"), Distance, ComboClicksLeft);
	}
}
