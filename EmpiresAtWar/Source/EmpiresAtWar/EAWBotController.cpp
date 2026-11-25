#include "EAWBotController.h"
#include "EAWCharacter.h"
#include "Combat/MeleeCombatComponent.h"
#include "Combat/WoundComponent.h"
#include "EmpiresAtWar.h"
#include "EngineUtils.h"

AEAWBotController::AEAWBotController()
{
	PrimaryActorTick.bCanEverTick = true;
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

	UMeleeCombatComponent* MyCombat = Me->GetCombat();
	UMeleeCombatComponent* TheirCombat = Target->GetCombat();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Distance = FVector::Dist2D(Me->GetActorLocation(), Target->GetActorLocation());

	// Aproxima ate a distancia de luta.
	if (Distance > EngageDistance)
	{
		Me->AddMovementInput((Target->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D(), 1.f);
	}
	else if (Distance < EngageDistance * 0.8f)   // perto demais: as laminas em guarda atravessariam o corpo
	{
		Me->AddMovementInput((Me->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D(), 0.6f);
	}

	// Defesa: decide uma vez por golpe do adversario.
	if (TheirCombat->GetState() == EMeleeState::Windup && TheirCombat->GetStateSerial() != LastJudgedSerial)
	{
		const float TimeToRelease = TheirCombat->GetTimeToRelease();
		if (TimeToRelease >= 0.f && TimeToRelease <= ReactionTime && Distance < EngageDistance * 1.6f)
		{
			LastJudgedSerial = TheirCombat->GetStateSerial();
			if (FMath::FRand() < ParryChance)
			{
				MyCombat->RequestParry();
				return;
			}
		}
	}

	// Contra-ataque depois de um parry bem-sucedido.
	if (MyCombat->HasRiposte() && MyCombat->GetState() == EMeleeState::Idle)
	{
		bFeintDecided = true; // nunca finta o contra-ataque
		MyCombat->RequestSlash();
		NextAttackTime = Now + FMath::FRandRange(AttackIntervalMin, AttackIntervalMax);
		return;
	}

	// Finta ocasional.
	if (MyCombat->GetState() == EMeleeState::Windup)
	{
		if (!bFeintDecided && MyCombat->GetStateAlpha() >= 0.3f)
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

	// Ataque.
	if (Distance <= EngageDistance * 1.1f && Now >= NextAttackTime && MyCombat->GetState() == EMeleeState::Idle)
	{
		const float Roll = FMath::FRand();
		if (Roll < 0.5f)
		{
			MyCombat->RequestSlash();
		}
		else if (Roll < 0.75f)
		{
			MyCombat->RequestAttack(EAttackDirection::Overhead);
		}
		else
		{
			MyCombat->RequestAttack(EAttackDirection::Stab);
		}
		NextAttackTime = Now + FMath::FRandRange(AttackIntervalMin, AttackIntervalMax);
		UE_LOG(LogEAWCombat, VeryVerbose, TEXT("Bot ataca: distancia=%.0f alvo=%s rel=%s yaw=%.0f"), Distance, *Target->GetName(),
			*(Target->GetActorLocation() - Me->GetActorLocation()).ToCompactString(), Me->GetActorRotation().Yaw);
	}
}
