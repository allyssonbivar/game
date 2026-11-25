#include "Combat/MeleeCombatComponent.h"
#include "Combat/MeleeWeaponData.h"
#include "Combat/WoundComponent.h"
#include "Combat/BodyRegionLibrary.h"
#include "EmpiresAtWar.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FMeleePose LerpPose(const FMeleePose& A, const FMeleePose& B, float T)
	{
		FMeleePose Out;
		Out.Offset = FMath::Lerp(A.Offset, B.Offset, T);
		Out.Rotation = FQuat::Slerp(A.Rotation.Quaternion(), B.Rotation.Quaternion(), T).Rotator();
		return Out;
	}

	const FMeleePose IncapacitatedPose(FVector(-5.f, 5.f, -35.f), FRotator(-70.f, 10.f, 0.f));
}

UMeleeCombatComponent::UMeleeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UMeleeCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	Stamina = MaxStamina;
	LastPose = GetConfig().GuardPose;
	TransitionFromPose = LastPose;

	// O personagem atualiza a mira antes de movermos a arma.
	if (GetOwner())
	{
		AddTickPrerequisiteActor(GetOwner());
	}
}

void UMeleeCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMeleeCombatComponent, Rep);
	DOREPLIFETIME(UMeleeCombatComponent, Stamina);
}

const FMeleeWeaponConfig& UMeleeCombatComponent::GetConfig() const
{
	return WeaponData ? WeaponData->Config : DefaultConfig;
}

void UMeleeCombatComponent::SetWeaponPivot(USceneComponent* InPivot)
{
	WeaponPivot = InPivot;
	PivotBaseLocation = InPivot ? InPivot->GetRelativeLocation() : FVector::ZeroVector;
}

void UMeleeCombatComponent::SetAnimatedWeapon(USkeletalMeshComponent* InMesh)
{
	AnimMesh = InMesh;
}

float UMeleeCombatComponent::Now() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

UWoundComponent* UMeleeCombatComponent::GetWounds() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UWoundComponent>() : nullptr;
}

float UMeleeCombatComponent::TimeMultiplier() const
{
	const UWoundComponent* Wounds = GetWounds();
	return Wounds ? Wounds->GetAttackTimeMultiplier() : 1.f;
}

float UMeleeCombatComponent::GetStateAlpha() const
{
	if (Rep.Duration <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	return FMath::Clamp((Now() - Rep.StartTime) / Rep.Duration, 0.f, 1.f);
}

bool UMeleeCombatComponent::HasRiposte() const
{
	return Now() <= RiposteUntil;
}

float UMeleeCombatComponent::GetTimeToRelease() const
{
	if (Rep.State != EMeleeState::Windup)
	{
		return -1.f;
	}
	return FMath::Max(0.f, Rep.StartTime + Rep.Duration - Now());
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UMeleeCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const bool bAuthority = Owner->HasAuthority();

	if (bAuthority)
	{
		// Stamina volta depois de uma pausa sem gastar.
		if (Stamina < MaxStamina && Now() - LastStaminaSpendTime >= StaminaRegenDelay)
		{
			const UWoundComponent* Wounds = GetWounds();
			const float RegenMult = Wounds ? Wounds->GetStaminaRegenMultiplier() : 1.f;
			Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenPerSecond * RegenMult * DeltaTime);
		}

		if (Rep.Duration > 0.f && Now() >= Rep.StartTime + Rep.Duration)
		{
			OnStateFinished();
		}
	}

	UpdateWeaponPose();

	if (bAuthority && Rep.State == EMeleeState::Release)
	{
		TraceBlade();
	}
}

// ---------------------------------------------------------------------------
// Comandos do dono
// ---------------------------------------------------------------------------

void UMeleeCombatComponent::RequestAttack(EAttackDirection Direction)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		HandleAttack(Direction);
	}
	else
	{
		ServerAttack(Direction);
	}
}

void UMeleeCombatComponent::RequestSlash()
{
	const EAttackDirection Direction = bNextSlashLeft ? EAttackDirection::SlashLeft : EAttackDirection::SlashRight;
	bNextSlashLeft = !bNextSlashLeft;
	RequestAttack(Direction);
}

void UMeleeCombatComponent::RequestParry()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		HandleParry();
	}
	else
	{
		ServerParry();
	}
}

void UMeleeCombatComponent::RequestFeint()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		HandleFeint();
	}
	else
	{
		ServerFeint();
	}
}

bool UMeleeCombatComponent::ServerAttack_Validate(EAttackDirection Direction) { return true; }
void UMeleeCombatComponent::ServerAttack_Implementation(EAttackDirection Direction) { HandleAttack(Direction); }

bool UMeleeCombatComponent::ServerParry_Validate() { return true; }
void UMeleeCombatComponent::ServerParry_Implementation() { HandleParry(); }

bool UMeleeCombatComponent::ServerFeint_Validate() { return true; }
void UMeleeCombatComponent::ServerFeint_Implementation() { HandleFeint(); }

// ---------------------------------------------------------------------------
// Logica do servidor
// ---------------------------------------------------------------------------

bool UMeleeCombatComponent::SpendStamina(float Amount)
{
	if (Stamina < Amount)
	{
		return false;
	}
	Stamina -= Amount;
	LastStaminaSpendTime = Now();
	return true;
}

void UMeleeCombatComponent::HandleAttack(EAttackDirection Direction)
{
	const UWoundComponent* Wounds = GetWounds();
	if (Wounds && !Wounds->CanFight())
	{
		return;
	}

	const FMeleeWeaponConfig& Config = GetConfig();
	const FMeleeAttackSpec* Spec = Config.Attacks.Find(Direction);
	if (!Spec)
	{
		return;
	}

	switch (Rep.State)
	{
	case EMeleeState::Idle:
		if (HasRiposte())
		{
			RiposteUntil = -1.f;
			StartWindup(Direction, Spec->RiposteWindup);
		}
		else
		{
			StartWindup(Direction, Spec->Windup);
		}
		break;

	case EMeleeState::Windup:
		// "Morph": trocar de golpe no meio da preparacao custa como uma finta.
		if (Direction != Rep.Direction && GetStateAlpha() <= Config.FeintLatestAlpha && SpendStamina(Config.FeintStaminaCost))
		{
			StartWindup(Direction, Spec->Windup);
		}
		break;

	case EMeleeState::Release:
		if (GetStateAlpha() >= Config.ComboQueueFromReleaseAlpha)
		{
			bComboQueued = true;
			QueuedDirection = Direction;
		}
		break;

	case EMeleeState::Recovery:
		StartWindup(Direction, Spec->ComboWindup);
		break;

	default:
		break;
	}
}

void UMeleeCombatComponent::HandleParry()
{
	const UWoundComponent* Wounds = GetWounds();
	if (Wounds && !Wounds->CanFight())
	{
		return;
	}

	const FMeleeWeaponConfig& Config = GetConfig();
	// Braco ferido encurta a janela de defesa.
	const float Window = Config.ParryWindow / TimeMultiplier();

	switch (Rep.State)
	{
	case EMeleeState::Idle:
	case EMeleeState::Recovery:
		bComboQueued = false;
		EnterState(EMeleeState::Parry, Rep.Direction, Window);
		break;

	case EMeleeState::Windup:
		// Finta para parry.
		if (GetStateAlpha() <= Config.FeintLatestAlpha && SpendStamina(Config.FeintStaminaCost))
		{
			EnterState(EMeleeState::Parry, Rep.Direction, Window);
		}
		break;

	default:
		break;
	}
}

void UMeleeCombatComponent::HandleFeint()
{
	const FMeleeWeaponConfig& Config = GetConfig();
	if (Rep.State == EMeleeState::Windup && GetStateAlpha() <= Config.FeintLatestAlpha && SpendStamina(Config.FeintStaminaCost))
	{
		EnterState(EMeleeState::Feint, Rep.Direction, Config.FeintRecovery);
	}
}

void UMeleeCombatComponent::StartWindup(EAttackDirection Direction, float WindupTime)
{
	const FMeleeAttackSpec* Spec = GetConfig().Attacks.Find(Direction);
	if (!Spec || !SpendStamina(Spec->StaminaCost))
	{
		EnterState(EMeleeState::Idle, Direction, 0.f);
		return;
	}
	bComboQueued = false;
	EnterState(EMeleeState::Windup, Direction, WindupTime * TimeMultiplier());
}

void UMeleeCombatComponent::EnterState(EMeleeState NewState, EAttackDirection Direction, float Duration)
{
	Rep.State = NewState;
	Rep.Direction = Direction;
	Rep.StartTime = Now();
	Rep.Duration = Duration;
	Rep.Serial++;

	TransitionFromPose = LastPose;

	if (NewState == EMeleeState::Release)
	{
		HitThisSwing.Reset();
		PrevBladePoints.Reset();
	}

	OnStateChanged.Broadcast(NewState, Direction);

	if (GetOwner())
	{
		UE_LOG(LogEAWCombat, Verbose, TEXT("%s estado=%d direcao=%d duracao=%.2f stamina=%.0f"), *GetOwner()->GetName(),
			static_cast<int32>(NewState), static_cast<int32>(Direction), Duration, Stamina);
		GetOwner()->ForceNetUpdate();
	}
}

void UMeleeCombatComponent::OnStateFinished()
{
	const FMeleeWeaponConfig& Config = GetConfig();
	const FMeleeAttackSpec* Spec = Config.Attacks.Find(Rep.Direction);
	const float Mult = TimeMultiplier();

	switch (Rep.State)
	{
	case EMeleeState::Windup:
		EnterState(EMeleeState::Release, Rep.Direction, Spec ? Spec->Release * Mult : 0.3f);
		break;

	case EMeleeState::Release:
		if (bComboQueued)
		{
			const FMeleeAttackSpec* Next = Config.Attacks.Find(QueuedDirection);
			StartWindup(QueuedDirection, Next ? Next->ComboWindup : 0.3f);
		}
		else
		{
			EnterState(EMeleeState::Recovery, Rep.Direction, Spec ? Spec->Recovery * Mult : 0.5f);
		}
		break;

	case EMeleeState::Parry:
		EnterState(EMeleeState::ParryRecovery, Rep.Direction, Config.ParryRecovery);
		break;

	case EMeleeState::Recovery:
	case EMeleeState::ParryRecovery:
	case EMeleeState::Feint:
	case EMeleeState::Stunned:
		EnterState(EMeleeState::Idle, Rep.Direction, 0.f);
		break;

	default:
		break;
	}
}

void UMeleeCombatComponent::ForceIncapacitated()
{
	bComboQueued = false;
	EnterState(EMeleeState::Incapacitated, Rep.Direction, 0.f);
}

void UMeleeCombatComponent::ResetCombat()
{
	bComboQueued = false;
	RiposteUntil = -1.f;
	Stamina = MaxStamina;
	EnterState(EMeleeState::Idle, Rep.Direction, 0.f);
}

bool UMeleeCombatComponent::IsFacing(const AActor* Defender, const AActor* Attacker, float HalfAngleDeg) const
{
	if (!Defender || !Attacker)
	{
		return false;
	}
	const FVector ToAttacker = (Attacker->GetActorLocation() - Defender->GetActorLocation()).GetSafeNormal2D();
	const FVector Forward = Defender->GetActorForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(Forward, ToAttacker) >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
}

void UMeleeCombatComponent::GetBladePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	const FMeleeWeaponConfig& Config = GetConfig();
	const int32 Samples = FMath::Clamp(Config.BladeSamples, 2, 12);

	// Modo animado: a lamina vai do inicio da lamina (perto da guarda) ate o osso da ponta.
	if (AnimMesh && AnimMesh->DoesSocketExist(GripSocket) && AnimMesh->DoesSocketExist(TipSocket))
	{
		const FVector Grip = AnimMesh->GetSocketLocation(GripSocket);
		const FVector Tip = AnimMesh->GetSocketLocation(TipSocket);
		const FVector Start = Grip + (Tip - Grip).GetSafeNormal() * AnimBladeStart;
		for (int32 i = 0; i < Samples; ++i)
		{
			OutPoints.Add(FMath::Lerp(Start, Tip, static_cast<float>(i) / static_cast<float>(Samples - 1)));
		}
		return;
	}

	if (!WeaponPivot)
	{
		return;
	}
	const FTransform& PivotTransform = WeaponPivot->GetComponentTransform();
	for (int32 i = 0; i < Samples; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(Samples - 1);
		const FVector Local(Config.BladeStart + T * Config.BladeLength, 0.f, 0.f);
		OutPoints.Add(PivotTransform.TransformPosition(Local));
	}
}

void UMeleeCombatComponent::TraceBlade()
{
	TArray<FVector> Points;
	GetBladePoints(Points);
	if (Points.Num() == 0)
	{
		return;
	}

	if (PrevBladePoints.Num() == Points.Num())
	{
		const FMeleeWeaponConfig& Config = GetConfig();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeBladeTrace), false, GetOwner());
		const FCollisionShape Shape = FCollisionShape::MakeSphere(Config.BladeRadius);

		for (int32 i = 0; i < Points.Num(); ++i)
		{
			TArray<FHitResult> Hits;
			GetWorld()->SweepMultiByChannel(Hits, PrevBladePoints[i], Points[i], FQuat::Identity, ECC_Melee, Shape, Params);

			if (bDebugDraw)
			{
				DrawDebugLine(GetWorld(), PrevBladePoints[i], Points[i], FColor::Orange, false, 1.0f, 0, 0.5f);
			}

			for (const FHitResult& Hit : Hits)
			{
				ResolveHit(Hit);
				if (Rep.State != EMeleeState::Release)
				{
					return; // golpe defendido: para de rastrear
				}
			}
		}
	}

	if (PrevBladePoints.Num() == 0 && UE_LOG_ACTIVE(LogEAWCombat, VeryVerbose))
	{
		const FVector OwnerLoc = GetOwner()->GetActorLocation();
		UE_LOG(LogEAWCombat, VeryVerbose, TEXT("%s lamina: base=%s ponta=%s (relativo ao dono)"), *GetOwner()->GetName(),
			*(Points[0] - OwnerLoc).ToCompactString(), *(Points.Last() - OwnerLoc).ToCompactString());
	}

	PrevBladePoints = Points;
}

void UMeleeCombatComponent::ResolveHit(const FHitResult& Hit)
{
	AActor* Owner = GetOwner();
	AActor* Victim = Hit.GetActor();
	if (!Owner || !Victim || Victim == Owner || HitThisSwing.Contains(Victim))
	{
		return;
	}
	HitThisSwing.Add(Victim);

	const FMeleeWeaponConfig& Config = GetConfig();
	const FMeleeAttackSpec* Spec = Config.Attacks.Find(Rep.Direction);
	UMeleeCombatComponent* VictimCombat = Victim->FindComponentByClass<UMeleeCombatComponent>();

	// Parry: defensor na janela de defesa e virado para o atacante.
	if (VictimCombat && VictimCombat->Rep.State == EMeleeState::Parry)
	{
		const FMeleeWeaponConfig& VictimConfig = VictimCombat->GetConfig();
		if (IsFacing(Victim, Owner, VictimConfig.ParryHalfAngle))
		{
			if (VictimCombat->SpendStamina(VictimConfig.ParryBlockStaminaCost))
			{
				VictimCombat->RiposteUntil = Now() + VictimConfig.RiposteWindow;
				VictimCombat->EnterState(EMeleeState::Idle, VictimCombat->Rep.Direction, 0.f);
				EnterState(EMeleeState::Stunned, Rep.Direction, Config.StunWhenParried);
			}
			else
			{
				// Sem stamina: a guarda quebra.
				VictimCombat->Stamina = 0.f;
				VictimCombat->LastStaminaSpendTime = Now();
				VictimCombat->EnterState(EMeleeState::Stunned, VictimCombat->Rep.Direction, VictimConfig.StunWhenGuardBroken);
				EnterState(EMeleeState::Recovery, Rep.Direction, Spec ? Spec->Recovery * TimeMultiplier() : 0.5f);
			}
			MulticastImpact(Hit.ImpactPoint, EBodyRegion::None, true, Victim);
			return;
		}
	}

	EBodyRegion Region = UBodyRegionLibrary::RegionFromHit(Hit);
	if (Region == EBodyRegion::None)
	{
		Region = EBodyRegion::Torso;
	}
	const float* RegionMult = Config.RegionDamageMultiplier.Find(Region);
	const float Damage = (Spec ? Spec->BaseDamage : 30.f) * (RegionMult ? *RegionMult : 1.f);

	const FVector HitDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
	AController* InstigatorController = Owner->GetInstigatorController();
	UGameplayStatics::ApplyPointDamage(Victim, Damage, HitDirection, Hit, InstigatorController, Owner, UDamageType::StaticClass());

	// Levar um golpe interrompe quem esta preparando ou se recuperando (troca de golpes no Release continua).
	if (VictimCombat)
	{
		const EMeleeState VictimState = VictimCombat->Rep.State;
		if (VictimState == EMeleeState::Windup || VictimState == EMeleeState::Idle || VictimState == EMeleeState::Recovery
			|| VictimState == EMeleeState::Feint || VictimState == EMeleeState::ParryRecovery || VictimState == EMeleeState::Parry)
		{
			VictimCombat->bComboQueued = false;
			VictimCombat->EnterState(EMeleeState::Stunned, VictimCombat->Rep.Direction, 0.35f);
		}
	}

	MulticastImpact(Hit.ImpactPoint, Region, false, Victim);

	UE_LOG(LogEAWCombat, Log, TEXT("%s acertou %s: regiao=%d dano=%.1f"), *Owner->GetName(), *Victim->GetName(),
		static_cast<int32>(Region), Damage);
}

void UMeleeCombatComponent::MulticastImpact_Implementation(FVector_NetQuantize Location, EBodyRegion Region, bool bParried, AActor* Victim)
{
	if (bDebugDraw && GetWorld())
	{
		DrawDebugSphere(GetWorld(), Location, bParried ? 10.f : 7.f, 10, bParried ? FColor::Yellow : FColor::Red, false, 1.5f);
	}
	OnImpact.Broadcast(Location, Region, bParried, Victim);
}

void UMeleeCombatComponent::OnRep_Rep(const FMeleeStateRep& Old)
{
	TransitionFromPose = LastPose;
	OnStateChanged.Broadcast(Rep.State, Rep.Direction);
}

// ---------------------------------------------------------------------------
// Pose procedural da arma
// ---------------------------------------------------------------------------

FTransform UMeleeCombatComponent::PoseToRelative(const FMeleePose& Pose) const
{
	return FTransform(Pose.Rotation.Quaternion(), PivotBaseLocation + Pose.Offset);
}

FMeleePose UMeleeCombatComponent::ComputePose() const
{
	const FMeleeWeaponConfig& Config = GetConfig();
	const FMeleeAttackSpec* Spec = Config.Attacks.Find(Rep.Direction);
	const float A = GetStateAlpha();

	switch (Rep.State)
	{
	case EMeleeState::Windup:
		return Spec ? LerpPose(TransitionFromPose, Spec->WindupPose, FMath::InterpEaseInOut(0.f, 1.f, A, 2.f)) : Config.GuardPose;

	case EMeleeState::Release:
		// Acelera durante o golpe.
		return Spec ? LerpPose(TransitionFromPose, Spec->EndPose, FMath::InterpEaseIn(0.f, 1.f, A, 1.6f)) : Config.GuardPose;

	case EMeleeState::Recovery:
	case EMeleeState::ParryRecovery:
		return LerpPose(TransitionFromPose, Config.GuardPose, FMath::InterpEaseInOut(0.f, 1.f, A, 2.f));

	case EMeleeState::Parry:
		// Levanta a guarda rapido, no primeiro quarto da janela.
		return LerpPose(TransitionFromPose, Config.ParryPose, FMath::InterpEaseOut(0.f, 1.f, FMath::Min(1.f, A * 4.f), 2.f));

	case EMeleeState::Feint:
	case EMeleeState::Stunned:
		return LerpPose(TransitionFromPose, Config.GuardPose, FMath::InterpEaseOut(0.f, 1.f, A, 2.f));

	case EMeleeState::Incapacitated:
		return IncapacitatedPose;

	case EMeleeState::Idle:
	default:
		return Config.GuardPose;
	}
}

void UMeleeCombatComponent::UpdateWeaponPose()
{
	if (AnimMesh)
	{
		// A animacao move a arma; aqui so desenhamos a lamina durante o golpe.
		if (bDebugDraw && Rep.State == EMeleeState::Release && GetWorld())
		{
			TArray<FVector> Points;
			GetBladePoints(Points);
			if (Points.Num() >= 2)
			{
				DrawDebugLine(GetWorld(), Points[0], Points.Last(), FColor::Red, false, 0.f, 0, 1.5f);
			}
		}
		return;
	}
	if (!WeaponPivot)
	{
		return;
	}
	const FMeleePose Pose = ComputePose();
	LastPose = Pose;
	WeaponPivot->SetRelativeTransform(PoseToRelative(Pose));

	if (bDebugDraw && Rep.State == EMeleeState::Release && GetWorld())
	{
		TArray<FVector> Points;
		GetBladePoints(Points);
		if (Points.Num() >= 2)
		{
			DrawDebugLine(GetWorld(), Points[0], Points.Last(), FColor::Red, false, 0.f, 0, 1.5f);
		}
	}
}
