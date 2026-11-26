#include "Weapons/EAWFirearmComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EAWCharacter.h"
#include "EmpiresAtWar.h"
#include "Combat/BodyRegionLibrary.h"
#include "Effects/EAWBallistics.h"
#include "Effects/EAWSmokeFX.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// Quadros 1, 140, 210, 296, 366, 444, 480 da AS_Mosquete_Recarga (30 qps).
const float UEAWFirearmComponent::ReloadAnimTimes[7] = { 0.f, 139.f / 30.f, 209.f / 30.f, 295.f / 30.f, 365.f / 30.f, 443.f / 30.f, 479.f / 30.f };

UEAWFirearmComponent::UEAWFirearmComponent()
{
	SetIsReplicatedByDefault(true);
}

void UEAWFirearmComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEAWFirearmComponent, Rep);
}

float UEAWFirearmComponent::GetStateElapsed() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - StateStartTime : 0.f;
}

void UEAWFirearmComponent::OnRep_Rep(FFirearmRep Old)
{
	if (Old.Serial != Rep.Serial)
	{
		StateStartTime = GetWorld()->GetTimeSeconds();
		OnStateChanged.Broadcast(Rep.State, Rep.Stage);
	}
}

// ---------------------------------------------------------------------------
// Entrada (dono)
// ---------------------------------------------------------------------------

void UEAWFirearmComponent::RequestAim(bool bWant) { ServerAim(bWant); }
void UEAWFirearmComponent::RequestReload() { ServerReload(); }
void UEAWFirearmComponent::RequestCancel() { ServerCancel(); }

void UEAWFirearmComponent::RequestFire()
{
	if (Rep.State != EFirearmState::Aiming || !Rep.bLoaded || bLockPending)
	{
		return;
	}
	// Pederneira: o cao cai, a escorva queima e so entao a carga dispara (o cano continua balancando).
	bLockPending = true;
	const float Lock = FMath::FRandRange(Config.LockTimeMin, Config.LockTimeMax);
	GetWorld()->GetTimerManager().SetTimer(LockTimer, this, &UEAWFirearmComponent::DoFireLocal, Lock, false);
}

void UEAWFirearmComponent::DoFireLocal()
{
	bLockPending = false;
	AEAWCharacter* Owner = Cast<AEAWCharacter>(GetOwner());
	if (!Owner || Rep.State != EFirearmState::Aiming || !Rep.bLoaded)
	{
		return;
	}
	FVector Origin, Dir;
	Owner->GetShotAim(Origin, Dir);
	ServerFire(Origin, Dir);
}

// ---------------------------------------------------------------------------
// Servidor
// ---------------------------------------------------------------------------

void UEAWFirearmComponent::SetState(EFirearmState NewState, uint8 Stage, float Duration)
{
	const FFirearmRep Old = Rep;
	Rep.State = NewState;
	Rep.Stage = Stage;
	Rep.Serial++;
	StateDuration = Duration;
	StateStartTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().ClearTimer(StateTimer);
	if (Duration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(StateTimer, this, &UEAWFirearmComponent::OnStateTimer, Duration, false);
	}
	OnStateChanged.Broadcast(Rep.State, Rep.Stage);
	UE_LOG(LogEAWCombat, Verbose, TEXT("%s mosquete: estado %d etapa %d (%.2f s) carregado=%d"), *GetOwner()->GetName(),
		(int32)NewState, Stage, Duration, Rep.bLoaded);
}

void UEAWFirearmComponent::OnStateTimer()
{
	switch (Rep.State)
	{
	case EFirearmState::Raising:
		if (bWantAim) SetState(EFirearmState::Aiming, 0, 0.f);
		else SetState(EFirearmState::Lowering, 0, Config.AimOutTime);
		break;
	case EFirearmState::Firing:
		if (bWantAim) SetState(EFirearmState::Aiming, 0, 0.f);
		else SetState(EFirearmState::Lowering, 0, Config.AimOutTime);
		break;
	case EFirearmState::Lowering:
		if (bReloadAfterLower && !Rep.bLoaded)
		{
			bReloadAfterLower = false;
			const uint8 S = SavedStage;
			SetState(EFirearmState::Reloading, S, (ReloadTimes[S + 1] - ReloadTimes[S]) * ReloadScale());
		}
		else SetState(EFirearmState::Lowered, 0, 0.f);
		break;
	case EFirearmState::Reloading:
	{
		const uint8 Next = Rep.Stage + 1;
		if (Next >= NumStages())
		{
			Rep.bLoaded = true;
			SavedStage = 0;
			UE_LOG(LogEAWCombat, Log, TEXT("%s mosquete carregado"), *GetOwner()->GetName());
			SetState(EFirearmState::Lowered, 0, 0.f);
		}
		else
		{
			SavedStage = Next;
			SetState(EFirearmState::Reloading, Next, (ReloadTimes[Next + 1] - ReloadTimes[Next]) * ReloadScale());
		}
		break;
	}
	default:
		break;
	}
}

void UEAWFirearmComponent::ServerAim_Implementation(bool bWant)
{
	bWantAim = bWant;
	if (bWant && Rep.State == EFirearmState::Lowered)
	{
		SetState(EFirearmState::Raising, 0, Config.AimInTime);
	}
	else if (bWant && Rep.State == EFirearmState::Reloading)
	{
		// Largar a recarga para mirar: a etapa em andamento se perde.
		SetState(EFirearmState::Lowered, 0, 0.f);
		SetState(EFirearmState::Raising, 0, Config.AimInTime);
	}
	else if (!bWant && Rep.State == EFirearmState::Aiming)
	{
		SetState(EFirearmState::Lowering, 0, Config.AimOutTime);
	}
}

void UEAWFirearmComponent::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir)
{
	AActor* Owner = GetOwner();
	if (!Owner || Rep.State != EFirearmState::Aiming || !Rep.bLoaded)
	{
		return;
	}
	// O disparo sai perto da cabeca do atirador (evita tiro "de longe" por trapaca).
	if (FVector::Dist(Origin, Owner->GetActorLocation()) > 150.f)
	{
		return;
	}
	Rep.bLoaded = false;
	SetState(EFirearmState::Firing, 0, Config.FireRecoverTime);
	AEAWBallistics::Fire(GetWorld(), Origin, Dir.GetSafeNormal() * Config.MuzzleVelocity, this, Owner, Config);
	MulticastShot(Origin, Dir);
	UE_LOG(LogEAWCombat, Log, TEXT("%s disparou: dir=%s"), *Owner->GetName(), *Dir.ToString());
}

void UEAWFirearmComponent::ServerReload_Implementation()
{
	if (Rep.bLoaded)
	{
		return;
	}
	if (Rep.State == EFirearmState::Lowered)
	{
		const uint8 S = SavedStage;
		SetState(EFirearmState::Reloading, S, (ReloadTimes[S + 1] - ReloadTimes[S]) * ReloadScale());
	}
	else if (Rep.State == EFirearmState::Aiming || Rep.State == EFirearmState::Firing)
	{
		bWantAim = false;
		bReloadAfterLower = true;
		if (Rep.State == EFirearmState::Aiming) SetState(EFirearmState::Lowering, 0, Config.AimOutTime);
	}
}

void UEAWFirearmComponent::ServerCancel_Implementation()
{
	bWantAim = false;
	bReloadAfterLower = false;
	if (Rep.State == EFirearmState::Reloading || Rep.State == EFirearmState::Raising)
	{
		SetState(EFirearmState::Lowered, 0, 0.f);
	}
	else if (Rep.State == EFirearmState::Aiming)
	{
		SetState(EFirearmState::Lowering, 0, Config.AimOutTime);
	}
}

void UEAWFirearmComponent::ResetFirearm()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bWantAim = false;
	bReloadAfterLower = false;
	SavedStage = 0;
	Rep.bLoaded = true;
	SetState(EFirearmState::Lowered, 0, 0.f);
}

// ---------------------------------------------------------------------------
// Acertos (servidor) e efeitos (todas as maquinas)
// ---------------------------------------------------------------------------

void UEAWFirearmComponent::HandleBallHit(const FHitResult& Hit, const FVector& Velocity)
{
	const float Speed = Velocity.Size() / 100.f;                       // m/s
	const float Energy = 0.5f * Config.BallMassKg * Speed * Speed;      // J
	AEAWCharacter* Victim = Cast<AEAWCharacter>(Hit.GetActor());
	const FVector Dir = Velocity.GetSafeNormal();
	if (Victim)
	{
		EBodyRegion Region = UBodyRegionLibrary::RegionFromBone(Hit.BoneName);
		// Teste: -EAWTestePescoco faz qualquer bala no soldado contar como tiro no pescoco.
		if (FParse::Param(FCommandLine::Get(), TEXT("EAWTestePescoco")))
		{
			Region = EBodyRegion::Neck;
		}
		// Bala de chumbo de 17,5 mm: cabeca/pescoco mata; tronco com energia alta e ferimento mortal;
		// bracos e pernas quebram/ferem (2o ferimento nao vital incapacita).
		float Damage = 0.f;
		switch (Region)
		{
		case EBodyRegion::Head:
		case EBodyRegion::Neck:  Damage = Energy > 60.f ? 200.f : 40.f; break;
		case EBodyRegion::Torso: Damage = Energy > 400.f ? 160.f : 45.f; break;
		default:                 Damage = Energy > 150.f ? 45.f : 15.f; break;
		}
		if (Victim->GetWounds() && Victim->GetWounds()->GetLife() != EWoundLife::Dead)
		{
			if (Region == EBodyRegion::Neck && Damage >= 200.f)
			{
				Victim->MarkDeathAnim(1);   // antes do dano: a morte vira a animacao com o jato arterial
			}
			Victim->GetWounds()->ApplyImpact(Region, Damage);
		}
		UE_LOG(LogEAWCombat, Log, TEXT("%s: bala acertou %s osso=%s regiao=%d a %.0f m/s (%.0f J), dano=%.0f, distancia=%.1f m"),
			*GetOwner()->GetName(), *Victim->GetName(), *Hit.BoneName.ToString(), (int32)Region, Speed, Energy, Damage,
			FVector::Dist(Hit.TraceStart, GetOwner()->GetActorLocation()) / 100.f);
		MulticastBallImpact(Hit.ImpactPoint, Dir, Victim, Hit.BoneName, Energy, Region);
	}
	else
	{
		UE_LOG(LogEAWCombat, Log, TEXT("bala acertou o cenario (%s, %s) a %.0f m/s em %s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Speed, *Hit.ImpactPoint.ToString());
		MulticastBallImpact(Hit.ImpactPoint, Hit.ImpactNormal, nullptr, NAME_None, Energy, EBodyRegion::None);
	}
}

void UEAWFirearmComponent::MulticastShot_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir)
{
	if (AEAWCharacter* Owner = Cast<AEAWCharacter>(GetOwner()))
	{
		Owner->PlayShotEffects(Dir);
	}
}

void UEAWFirearmComponent::MulticastBallImpact_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Dir, AActor* Victim,
	FName Bone, float EnergyJ, EBodyRegion Region)
{
	if (AEAWCharacter* V = Cast<AEAWCharacter>(Victim))
	{
		V->ReceiveBullet(Location, Dir, Bone, EnergyJ, Region);
	}
	else
	{
		AEAWSmokeFX::Dust(GetWorld(), Location, Dir);
	}
}

void UEAWFirearmComponent::SetLoaded(bool bInLoaded)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bWantAim = false;
	bReloadAfterLower = false;
	SavedStage = 0;
	Rep.bLoaded = bInLoaded;
	SetState(EFirearmState::Lowered, 0, 0.f);
}
