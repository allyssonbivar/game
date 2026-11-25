#include "Combat/WoundComponent.h"
#include "Combat/BodyRegionLibrary.h"
#include "EmpiresAtWar.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UWoundComponent::UWoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWoundComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->OnTakePointDamage.AddDynamic(this, &UWoundComponent::HandlePointDamage);
	}
}

void UWoundComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWoundComponent, Life);
	DOREPLIFETIME(UWoundComponent, NonVitalWounds);
	DOREPLIFETIME(UWoundComponent, WoundedRegionBits);
	DOREPLIFETIME(UWoundComponent, LastRegion);
}

void UWoundComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	EBodyRegion Region = UBodyRegionLibrary::RegionFromBone(BoneName);
	if (Region == EBodyRegion::None)
	{
		Region = UBodyRegionLibrary::RegionFromComponent(FHitComponent);
	}
	if (Region == EBodyRegion::None)
	{
		Region = EBodyRegion::Torso;
	}
	ApplyImpact(Region, Damage);
}

EWoundLife UWoundComponent::ApplyImpact(EBodyRegion Region, float Damage)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return Life;
	}
	if (Life == EWoundLife::Dead || Damage < MinWoundDamage)
	{
		return Life;
	}

	const EWoundLife OldLife = Life;
	LastRegion = Region;

	const bool bVitalHit = UBodyRegionLibrary::IsVitalRegion(Region) && Damage >= VitalDamageThreshold;
	const bool bLethalEnergy = Damage >= LethalEnergyThreshold;

	if (bVitalHit || bLethalEnergy)
	{
		Life = EWoundLife::Dead;
	}
	else
	{
		NonVitalWounds++;
		WoundedRegionBits |= static_cast<uint8>(1u << static_cast<uint8>(Region));
		Life = (NonVitalWounds >= MaxNonVitalWounds) ? EWoundLife::Incapacitated : EWoundLife::Wounded;
	}

	UE_LOG(LogEAWCombat, Log, TEXT("%s ferido: regiao=%d dano=%.1f ferimentos=%d estado=%d"),
		*GetOwner()->GetName(), static_cast<int32>(Region), Damage, NonVitalWounds, static_cast<int32>(Life));

	OnWounded.Broadcast(Region, Damage, Life);
	if (Life != OldLife)
	{
		OnRep_Life(OldLife);
	}
	return Life;
}

bool UWoundComponent::HasWound(EBodyRegion Region) const
{
	return (WoundedRegionBits & static_cast<uint8>(1u << static_cast<uint8>(Region))) != 0;
}

void UWoundComponent::ResetWounds()
{
	const EWoundLife OldLife = Life;
	Life = EWoundLife::Healthy;
	NonVitalWounds = 0;
	WoundedRegionBits = 0;
	LastRegion = EBodyRegion::None;
	OnRep_Life(OldLife);
}

void UWoundComponent::OnRep_Life(EWoundLife OldLife)
{
	// Nos clientes, avisa quem estiver ouvindo (personagem, HUD) que o estado mudou.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OnWounded.Broadcast(LastRegion, 0.f, Life);
	}
}
