#include "EAWCharacter.h"
#include "EAWBotController.h"
#include "EAWGameMode.h"
#include "EmpiresAtWar.h"
#include "Combat/MeleeCombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/EAWAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Effects/EAWBloodFX.h"
#include "Effects/EAWSmokeFX.h"
#include "Weapons/EAWFirearmComponent.h"
#include "AnimationRuntime.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"

AEAWCharacter::AEAWCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	// A capsula de movimento nao participa do combate; quem recebe golpes sao as hitboxes.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Melee, ECR_Ignore);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	// Inercia: o soldado acelera e freia (antes arrancava e parava na hora).
	GetCharacterMovement()->MaxAcceleration = 650.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 700.f;
	GetCharacterMovement()->GroundFriction = 5.f;

	// Mira: segue a rotacao de mira no dono, no servidor e nos outros clientes.
	AimRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AimRoot"));
	AimRoot->SetupAttachment(GetCapsuleComponent());
	AimRoot->SetRelativeLocation(FVector(0.f, 0.f, 64.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(AimRoot);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(90.f);

	WeaponPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponPivot"));
	WeaponPivot->SetupAttachment(AimRoot);
	WeaponPivot->SetRelativeLocation(FVector(30.f, 22.f, -24.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// Lamina provisoria: um cubo esticado de 85 cm ao longo do eixo X do pivo.
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(WeaponPivot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetRelativeLocation(FVector(12.f + 42.5f, 0.f, 0.f));
	WeaponMesh->SetRelativeScale3D(FVector(0.85f, 0.02f, 0.05f));
	if (CubeMesh.Succeeded())
	{
		WeaponMesh->SetStaticMesh(CubeMesh.Object);
	}

	// Corpo provisorio para os outros jogadores verem o soldado.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.7f));
	BodyMesh->SetOwnerNoSee(true);
	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
	}

	// Hitboxes por regiao (tags lidas pelo UBodyRegionLibrary).
	Hitboxes.Add(MakeHitbox(TEXT("HB_Head"), TEXT("Head"), 12.f, 12.f, FVector(0.f, 0.f, 68.f), FRotator::ZeroRotator));
	Hitboxes.Add(MakeHitbox(TEXT("HB_Neck"), TEXT("Neck"), 7.f, 9.f, FVector(0.f, 0.f, 51.f), FRotator::ZeroRotator));
	Hitboxes.Add(MakeHitbox(TEXT("HB_Torso"), TEXT("Torso"), 20.f, 32.f, FVector(0.f, 0.f, 16.f), FRotator::ZeroRotator));
	Hitboxes.Add(MakeHitbox(TEXT("HB_ArmL"), TEXT("Arms"), 7.f, 30.f, FVector(0.f, -28.f, 16.f), FRotator::ZeroRotator));
	Hitboxes.Add(MakeHitbox(TEXT("HB_ArmR"), TEXT("Arms"), 7.f, 30.f, FVector(0.f, 28.f, 16.f), FRotator::ZeroRotator));
	Hitboxes.Add(MakeHitbox(TEXT("HB_Legs"), TEXT("Legs"), 17.f, 44.f, FVector(0.f, 0.f, -44.f), FRotator::ZeroRotator));

	// Esqueleto e animacoes do Blender (Content/Personagem). Sem eles, cai no modo procedural.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SoldierMesh(TEXT("/Game/Personagem/SK_Soldado.SK_Soldado"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_Guarda(TEXT("/Game/Personagem/AS_Sabre_Guarda.AS_Sabre_Guarda"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_CorteEsq(TEXT("/Game/Personagem/AS_Sabre_Corte_Esq.AS_Sabre_Corte_Esq"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_CorteDir(TEXT("/Game/Personagem/AS_Sabre_Corte_Dir.AS_Sabre_Corte_Dir"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_DeCima(TEXT("/Game/Personagem/AS_Sabre_DeCima.AS_Sabre_DeCima"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_Estocada(TEXT("/Game/Personagem/AS_Sabre_Estocada.AS_Sabre_Estocada"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_Parry(TEXT("/Game/Personagem/AS_Sabre_Parry.AS_Sabre_Parry"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_Bloqueado(TEXT("/Game/Personagem/AS_Sabre_Bloqueado.AS_Sabre_Bloqueado"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> A_Finta(TEXT("/Game/Personagem/AS_Sabre_Finta.AS_Sabre_Finta"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> F_CorteEsq(TEXT("/Game/Personagem/AS_Sabre_Corte_Esq_FP.AS_Sabre_Corte_Esq_FP"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> F_CorteDir(TEXT("/Game/Personagem/AS_Sabre_Corte_Dir_FP.AS_Sabre_Corte_Dir_FP"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> F_DeCima(TEXT("/Game/Personagem/AS_Sabre_DeCima_FP.AS_Sabre_DeCima_FP"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> F_Parry(TEXT("/Game/Personagem/AS_Sabre_Parry_FP.AS_Sabre_Parry_FP"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> F_Bloqueado(TEXT("/Game/Personagem/AS_Sabre_Bloqueado_FP.AS_Sabre_Bloqueado_FP"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> F_Finta(TEXT("/Game/Personagem/AS_Sabre_Finta_FP.AS_Sabre_Finta_FP"));
	AnimCorteEsqFP = F_CorteEsq.Get(); AnimCorteDirFP = F_CorteDir.Get(); AnimDeCimaFP = F_DeCima.Get();
	AnimParryFP = F_Parry.Get(); AnimBloqueadoFP = F_Bloqueado.Get(); AnimFintaFP = F_Finta.Get();
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Frente(TEXT("/Game/Personagem/AS_Sabre_Andar_Frente.AS_Sabre_Andar_Frente"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Tras(TEXT("/Game/Personagem/AS_Sabre_Andar_Tras.AS_Sabre_Andar_Tras"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Esq(TEXT("/Game/Personagem/AS_Sabre_Andar_Esq.AS_Sabre_Andar_Esq"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Dir(TEXT("/Game/Personagem/AS_Sabre_Andar_Dir.AS_Sabre_Andar_Dir"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Correr(TEXT("/Game/Personagem/AS_Sabre_Correr.AS_Sabre_Correr"));
	AnimAndarFrente = M_Frente.Get(); AnimAndarTras = M_Tras.Get(); AnimAndarEsq = M_Esq.Get(); AnimAndarDir = M_Dir.Get(); AnimCorrer = M_Correr.Get();
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> P_Cabeca(TEXT("/Game/Personagem/Pecas/SK_Peca_Cabeca.SK_Peca_Cabeca"));
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> P_BracoE(TEXT("/Game/Personagem/Pecas/SK_Peca_BracoE.SK_Peca_BracoE"));
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> P_BracoD(TEXT("/Game/Personagem/Pecas/SK_Peca_BracoD.SK_Peca_BracoD"));
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> P_PernaE(TEXT("/Game/Personagem/Pecas/SK_Peca_PernaE.SK_Peca_PernaE"));
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> P_PernaD(TEXT("/Game/Personagem/Pecas/SK_Peca_PernaD.SK_Peca_PernaD"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> P_Cap(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> P_CapMat(TEXT("/Game/Efeitos/M_Sangue.M_Sangue"));
	PecaCabeca = P_Cabeca.Get(); PecaBracoE = P_BracoE.Get(); PecaBracoD = P_BracoD.Get();
	PecaPernaE = P_PernaE.Get(); PecaPernaD = P_PernaD.Get(); CapMesh = P_Cap.Get(); CapMaterial = P_CapMat.Get();
	AnimGuarda = A_Guarda.Object; AnimCorteEsq = A_CorteEsq.Object; AnimCorteDir = A_CorteDir.Object; AnimDeCima = A_DeCima.Object;
	AnimEstocada = A_Estocada.Object; AnimParry = A_Parry.Object; AnimBloqueado = A_Bloqueado.Object; AnimFinta = A_Finta.Object;
	if (SoldierMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(SoldierMesh.Object);
		// O FBX chega olhando para +Y e com os pes na origem.
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -88.f), FRotator(0.f, -90.f, 0.f));
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		GetMesh()->SetCollisionResponseToChannel(ECC_Melee, ECR_Ignore);
		// Animador em camadas (pernas = movimento, tronco = combate).
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(UEAWAnimInstance::StaticClass());
	}

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(AimRoot);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCastShadow(false);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetVisibility(false);
	FirstPersonMesh->SetComponentTickEnabled(false);
	if (SoldierMesh.Succeeded())
	{
		FirstPersonMesh->SetSkeletalMesh(SoldierMesh.Object);
	}

	Combat = CreateDefaultSubobject<UMeleeCombatComponent>(TEXT("Combat"));
	Wounds = CreateDefaultSubobject<UWoundComponent>(TEXT("Wounds"));
	Firearm = CreateDefaultSubobject<UEAWFirearmComponent>(TEXT("Firearm"));

	// Armas: malhas no mesmo esqueleto do soldado, seguindo a pose do corpo (3a pessoa) ou dos bracos (1a pessoa).
	WeaponTP = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponTP"));
	WeaponTP->SetupAttachment(GetMesh());
	WeaponTP->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponTP->SetOwnerNoSee(true);
	WeaponTP->bCastHiddenShadow = true;
	WeaponFP = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponFP"));
	WeaponFP->SetupAttachment(FirstPersonMesh);
	WeaponFP->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponFP->SetOnlyOwnerSee(true);
	WeaponFP->SetCastShadow(false);
	WeaponFP->SetVisibility(false);
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> W_Mosq(TEXT("/Game/Armas/SK_Arma_Mosquete.SK_Arma_Mosquete"));
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> W_Sabre(TEXT("/Game/Armas/SK_Arma_Sabre.SK_Arma_Sabre"));
	MeshMosquete = W_Mosq.Get(); MeshSabre = W_Sabre.Get();
#define EAW_ANIM(Var, Name) { static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> F(TEXT("/Game/Personagem/" Name "." Name)); Var = F.Get(); }
	EAW_ANIM(AnimMGuarda, "AS_Mosquete_Guarda"); EAW_ANIM(AnimMErguer, "AS_Mosquete_Erguer"); EAW_ANIM(AnimMMira, "AS_Mosquete_Mira");
	EAW_ANIM(AnimMTiro, "AS_Mosquete_Tiro"); EAW_ANIM(AnimMBaixar, "AS_Mosquete_Baixar"); EAW_ANIM(AnimMRecarga, "AS_Mosquete_Recarga");
	EAW_ANIM(AnimMGuardaFP, "AS_Mosquete_Guarda_FP"); EAW_ANIM(AnimMErguerFP, "AS_Mosquete_Erguer_FP"); EAW_ANIM(AnimMMiraFP, "AS_Mosquete_Mira_FP");
	EAW_ANIM(AnimMTiroFP, "AS_Mosquete_Tiro_FP"); EAW_ANIM(AnimMBaixarFP, "AS_Mosquete_Baixar_FP"); EAW_ANIM(AnimMRecargaFP, "AS_Mosquete_Recarga_FP");
#undef EAW_ANIM

	// O GameMode cria o controlador do bot explicitamente; os jogadores usam PlayerController.
	AIControllerClass = AEAWBotController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled;
}

UCapsuleComponent* AEAWCharacter::MakeHitbox(FName Name, FName RegionTag, float Radius, float HalfHeight, const FVector& Location, const FRotator& Rotation)
{
	UCapsuleComponent* Hitbox = CreateDefaultSubobject<UCapsuleComponent>(Name);
	Hitbox->SetupAttachment(GetCapsuleComponent());
	Hitbox->InitCapsuleSize(Radius, FMath::Max(Radius, HalfHeight));
	Hitbox->SetRelativeLocationAndRotation(Location, Rotation);
	Hitbox->ComponentTags.Add(RegionTag);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionObjectType(ECC_WorldDynamic);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Melee, ECR_Block);
	Hitbox->SetGenerateOverlapEvents(false);
	Hitbox->SetCanEverAffectNavigation(false);
	Hitbox->SetOwnerNoSee(true);
	Hitbox->SetHiddenInGame(false);
	Hitbox->ShapeColor = RegionTag == TEXT("Head") || RegionTag == TEXT("Neck") ? FColor::Red
		: RegionTag == TEXT("Torso") ? FColor::Orange
		: RegionTag == TEXT("Arms") ? FColor::Blue
		: FColor::Green;
	return Hitbox;
}

void AEAWCharacter::BeginPlay()
{
	Super::BeginPlay();

	BodyMeshBaseLocation = BodyMesh->GetRelativeLocation();
	Combat->SetWeaponPivot(WeaponPivot);
	Wounds->OnWounded.AddDynamic(this, &AEAWCharacter::HandleWounded);
	Combat->OnImpact.AddDynamic(this, &AEAWCharacter::HandleImpact);
	Firearm->OnStateChanged.AddDynamic(this, &AEAWCharacter::HandleFirearmState);
	WeaponTP->SetMasterPoseComponent(GetMesh());
	WeaponFP->SetMasterPoseComponent(FirstPersonMesh);

	bAnimated = GetMesh()->SkeletalMesh && AnimGuarda && AnimCorteEsq && AnimCorteDir && AnimDeCima && AnimEstocada
		&& AnimParry && AnimBloqueado && AnimFinta;
	if (UEAWAnimInstance* AnimLayers = Cast<UEAWAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimLayers->WalkForward = AnimAndarFrente; AnimLayers->WalkBack = AnimAndarTras;
		AnimLayers->WalkLeft = AnimAndarEsq; AnimLayers->WalkRight = AnimAndarDir; AnimLayers->Run = AnimCorrer;
		AnimLayers->SpeedForward = WalkSpeed; AnimLayers->SpeedBack = WalkSpeed * BackSpeedScale;
		AnimLayers->SpeedSide = WalkSpeed * SideSpeedScale; AnimLayers->SpeedRun = SprintSpeed;
	}
	if (bAnimated)
	{
		Combat->SetAnimatedWeapon(GetMesh());
		Combat->OnStateChanged.AddDynamic(this, &AEAWCharacter::HandleCombatState);
		BodyMesh->SetVisibility(false);
		WeaponMesh->SetVisibility(false);
		// Camera na altura dos olhos do modelo (158 cm do chao).
		AimRoot->SetRelativeLocation(FVector(6.f, 0.f, 70.f));
		if (IsLocallyControlled() && IsPlayerControlled())
		{
			SetupFirstPerson();
		}
		ApplyWeaponVisuals();

		// Conferencia de orientacao: o pe aponta para a frente do personagem?
		const FVector Foot = GetMesh()->GetSocketLocation(TEXT("foot_r"));
		const FVector Ball = GetMesh()->GetSocketLocation(TEXT("ball_r"));
		UE_LOG(LogEAWCombat, Log, TEXT("%s esqueleto ok; pe x frente = %.2f; altura cabeca = %.1f cm"), *GetName(),
			FVector::DotProduct((Ball - Foot).GetSafeNormal2D(), GetActorForwardVector()),
			GetMesh()->GetSocketLocation(TEXT("head")).Z - (GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	}
	else
	{
		UE_LOG(LogEAWCombat, Warning, TEXT("%s sem esqueleto/animacoes: usando arma procedural"), *GetName());
	}

	for (UCapsuleComponent* Hitbox : Hitboxes)
	{
		Hitbox->SetHiddenInGame(!bShowHitboxes);
	}
	ApplyMovementSpeed();
}

void AEAWCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEAWCharacter, bSprinting);
	DOREPLIFETIME(AEAWCharacter, CurrentWeapon);
	DOREPLIFETIME(AEAWCharacter, bBayonet);
}

void AEAWCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Mesma rotacao de mira em todas as maquinas: o servidor rastreia a lamina com ela.
	AimRoot->SetWorldRotation(GetBaseAimRotation());

	ApplyMovementSpeed();
	UpdateFeel(DeltaSeconds);
	UpdateFirearmView(DeltaSeconds);

	if (IsLocallyControlled() && IsPlayerControlled())
	{
		DrawDebugHud();
	}
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------

void AEAWCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AEAWCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AEAWCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &AEAWCharacter::TurnInput);
	PlayerInputComponent->BindAxis("LookUp", this, &AEAWCharacter::LookUpInput);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AEAWCharacter::StartSprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AEAWCharacter::StopSprint);

	PlayerInputComponent->BindAction("AttackSlash", IE_Pressed, this, &AEAWCharacter::InputSlash);
	PlayerInputComponent->BindAction("AttackOverhead", IE_Pressed, this, &AEAWCharacter::InputOverhead);
	PlayerInputComponent->BindAction("AttackStab", IE_Pressed, this, &AEAWCharacter::InputStab);
	PlayerInputComponent->BindAction("Parry", IE_Pressed, this, &AEAWCharacter::InputParry);
	PlayerInputComponent->BindAction("Parry", IE_Released, this, &AEAWCharacter::InputAimReleased);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AEAWCharacter::InputReload);
	PlayerInputComponent->BindAction("ToggleMelee", IE_Pressed, this, &AEAWCharacter::InputToggleMelee);
	PlayerInputComponent->BindAction("Weapon1", IE_Pressed, this, &AEAWCharacter::InputWeapon1);
	PlayerInputComponent->BindAction("Weapon2", IE_Pressed, this, &AEAWCharacter::InputWeapon2);
	PlayerInputComponent->BindAction("Feint", IE_Pressed, this, &AEAWCharacter::InputFeint);
	PlayerInputComponent->BindAction("ToggleHitboxes", IE_Pressed, this, &AEAWCharacter::InputToggleHitboxes);
	PlayerInputComponent->BindAction("SpawnBot", IE_Pressed, this, &AEAWCharacter::InputSpawnBot);
}

void AEAWCharacter::MoveForward(float Value)
{
	if (Value != 0.f && Wounds->CanFight())
	{
		// Para tras e mais lento (passo da animacao: 1,0 m/s contra 1,4 m/s para a frente).
		AddMovementInput(GetActorForwardVector(), Value < 0.f ? Value * BackSpeedScale : Value);
	}
}

void AEAWCharacter::MoveRight(float Value)
{
	if (Value != 0.f && Wounds->CanFight())
	{
		AddMovementInput(GetActorRightVector(), Value * SideSpeedScale);   // de lado: 0,85 m/s
	}
}

void AEAWCharacter::StartSprint()
{
	bSprinting = true;
	if (CurrentWeapon == EEAWWeapon::Mosquete && Firearm->IsBusy())
	{
		Firearm->RequestCancel();   // correr larga a mira e a recarga
	}
	if (!HasAuthority())
	{
		ServerSetSprint(true);
	}
}

void AEAWCharacter::StopSprint()
{
	bSprinting = false;
	if (!HasAuthority())
	{
		ServerSetSprint(false);
	}
}

bool AEAWCharacter::ServerSetSprint_Validate(bool bNewSprint) { return true; }
void AEAWCharacter::ServerSetSprint_Implementation(bool bNewSprint) { bSprinting = bNewSprint; }

// Com o mosquete em modo de tiro: botao esquerdo dispara, direito (segurar) mira, R recarrega.
// Tecla X: alterna entre tiro e combate com o mosquete (baioneta/coronha). 1 = mosquete, 2 = sabre.
void AEAWCharacter::InputSlash()
{
	if (UsingMusketFire()) { Firearm->RequestFire(); return; }
	if (CurrentWeapon == EEAWWeapon::Sabre) Combat->RequestSlash();
}
void AEAWCharacter::InputOverhead() { if (CurrentWeapon == EEAWWeapon::Sabre) Combat->RequestAttack(EAttackDirection::Overhead); }
void AEAWCharacter::InputStab()     { if (CurrentWeapon == EEAWWeapon::Sabre) Combat->RequestAttack(EAttackDirection::Stab); }
void AEAWCharacter::InputParry()
{
	if (UsingMusketFire()) { Firearm->RequestAim(true); return; }
	if (CurrentWeapon == EEAWWeapon::Sabre) Combat->RequestParry();
}
void AEAWCharacter::InputAimReleased() { if (UsingMusketFire()) Firearm->RequestAim(false); }
void AEAWCharacter::InputFeint()    { if (CurrentWeapon == EEAWWeapon::Sabre) Combat->RequestFeint(); }
void AEAWCharacter::InputReload()   { if (UsingMusketFire()) Firearm->RequestReload(); }
void AEAWCharacter::InputToggleMelee()
{
	if (CurrentWeapon == EEAWWeapon::Mosquete)
	{
		Firearm->RequestCancel();
		ServerSetBayonet(!bBayonet);
	}
}
void AEAWCharacter::InputWeapon1() { ServerEquip(EEAWWeapon::Mosquete); }
void AEAWCharacter::InputWeapon2() { ServerEquip(EEAWWeapon::Sabre); }
void AEAWCharacter::ServerEquip_Implementation(EEAWWeapon NewWeapon) { EquipWeapon(NewWeapon); }
void AEAWCharacter::ServerSetBayonet_Implementation(bool bNew)
{
	bBayonet = bNew;
	OnRep_Weapon();
	UE_LOG(LogEAWCombat, Log, TEXT("%s mosquete: %s"), *GetName(), bBayonet ? TEXT("combate (baioneta)") : TEXT("tiro"));
}

void AEAWCharacter::InputToggleHitboxes()
{
	// Afeta so a visualizacao local de todos os soldados.
	for (TActorIterator<AEAWCharacter> It(GetWorld()); It; ++It)
	{
		It->bShowHitboxes = !It->bShowHitboxes;
		for (UCapsuleComponent* Hitbox : It->Hitboxes)
		{
			Hitbox->SetHiddenInGame(!It->bShowHitboxes);
		}
	}
}

void AEAWCharacter::InputSpawnBot()
{
	ServerSpawnBot();
}

bool AEAWCharacter::ServerSpawnBot_Validate() { return true; }
void AEAWCharacter::ServerSpawnBot_Implementation()
{
	if (AEAWGameMode* GameMode = GetWorld()->GetAuthGameMode<AEAWGameMode>())
	{
		GameMode->SpawnBotNear(this);
	}
}

void AEAWCharacter::EAWReset()
{
	ServerResetAll();
}

void AEAWCharacter::SetShowHitboxes(bool bShow)
{
	bShowHitboxes = bShow;
	for (UCapsuleComponent* Hitbox : Hitboxes)
	{
		Hitbox->SetHiddenInGame(!bShow);
	}
}

void AEAWCharacter::EAWBot()
{
	ServerSpawnBot();
}

bool AEAWCharacter::ServerResetAll_Validate() { return true; }
void AEAWCharacter::ServerResetAll_Implementation()
{
	for (TActorIterator<AEAWCharacter> It(GetWorld()); It; ++It)
	{
		It->ResetSoldier();
	}
}

// ---------------------------------------------------------------------------
// Ferimentos e movimento
// ---------------------------------------------------------------------------

void AEAWCharacter::ApplyMovementSpeed()
{
	// Atacando nao se corre: o corpo esta comprometido com o golpe.
	float CombatMult = 1.f;
	bool bBusy = false;
	switch (Combat->GetState())
	{
	case EMeleeState::Windup:   CombatMult = 0.65f; bBusy = true; break;
	case EMeleeState::Release:  CombatMult = 0.45f; bBusy = true; break;
	case EMeleeState::Recovery: CombatMult = 0.7f;  bBusy = true; break;
	case EMeleeState::Parry:    CombatMult = 0.7f;  bBusy = true; break;
	case EMeleeState::Stunned:  CombatMult = 0.35f; bBusy = true; break;
	default: break;
	}
	// Mosquete de 4,6 kg: no ombro anda devagar; carregando, quase parado; nunca corre ocupado.
	float WeightMult = 1.f;
	if (CurrentWeapon == EEAWWeapon::Mosquete)
	{
		WeightMult = 0.95f;
		switch (Firearm->GetState())
		{
		case EFirearmState::Raising: case EFirearmState::Aiming: case EFirearmState::Firing:
			CombatMult = Firearm->Config.AimMoveSpeedMult; bBusy = true; break;
		case EFirearmState::Reloading:
			CombatMult = Firearm->Config.ReloadMoveSpeedMult; bBusy = true; break;
		case EFirearmState::Lowering:
			CombatMult = 0.7f; bBusy = true; break;
		default: break;
		}
	}
	const float Base = (bSprinting && !bBusy) ? SprintSpeed * (CurrentWeapon == EEAWWeapon::Mosquete ? 0.92f : 1.f) : WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeed = Base * Wounds->GetMoveSpeedMultiplier() * CombatMult * WeightMult;
}

// ---------------------------------------------------------------------------
// Peso e fisica
// ---------------------------------------------------------------------------

void AEAWCharacter::TurnInput(float Value)
{
	// Giro limitado durante o golpe (como no Chivalry): nao da para "arrastar" o golpe girando o mouse a vontade.
	float Cap = 0.f;
	switch (Combat->GetState())
	{
	case EMeleeState::Windup:  Cap = TurnCapWindup; break;
	case EMeleeState::Release: Cap = TurnCapRelease; break;
	default: break;
	}
	if (UsingMusketFire() && AimAlpha > 0.5f)
	{
		Cap = Firearm->Config.AimTurnCap;   // peso do mosquete no ombro
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (Cap > 0.f && PC && GetWorld())
	{
		const float Scale = FMath::Max(KINDA_SMALL_NUMBER, FMath::Abs(PC->InputYawScale));
		const float MaxDeg = Cap * GetWorld()->GetDeltaSeconds();
		Value = FMath::Clamp(Value * Scale, -MaxDeg, MaxDeg) / Scale;
	}
	AddControllerYawInput(Value);
}

void AEAWCharacter::LookUpInput(float Value)
{
	float Cap = 0.f;
	switch (Combat->GetState())
	{
	case EMeleeState::Windup:  Cap = TurnCapWindup; break;
	case EMeleeState::Release: Cap = TurnCapRelease; break;
	default: break;
	}
	if (UsingMusketFire() && AimAlpha > 0.5f)
	{
		Cap = Firearm->Config.AimTurnCap;
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (Cap > 0.f && PC && GetWorld())
	{
		const float Scale = FMath::Max(KINDA_SMALL_NUMBER, FMath::Abs(PC->InputPitchScale));
		const float MaxDeg = Cap * GetWorld()->GetDeltaSeconds();
		Value = FMath::Clamp(Value * Scale, -MaxDeg, MaxDeg) / Scale;
	}
	AddControllerPitchInput(Value);
}

void AEAWCharacter::HitStop(AActor* Actor, float Seconds)
{
	if (!Actor || !Actor->GetWorld())
	{
		return;
	}
	// "Trava" curta no impacto: a animacao quase para por alguns centesimos de segundo.
	Actor->CustomTimeDilation = 0.05f;
	TWeakObjectPtr<AActor> Weak(Actor);
	FTimerHandle Handle;
	Actor->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
	{
		if (Weak.IsValid())
		{
			Weak->CustomTimeDilation = 1.f;
		}
	}), Seconds, false);
}

void AEAWCharacter::HandleImpact(FVector Location, EBodyRegion Region, bool bParried, AActor* Victim)
{
	// Chamado em todas as maquinas (multicast) no atacante.
	AEAWCharacter* Other = Cast<AEAWCharacter>(Victim);
	const FVector Dir = Other ? (Other->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
	const float StopTime = bParried ? 0.09f : 0.07f;
	HitStop(this, StopTime);
	if (Other && !bParried)
	{
		// Borrifo na direcao do golpe (mais forte na cabeca/pescoco).
		const bool bVital = Region == EBodyRegion::Head || Region == EBodyRegion::Neck;
		AEAWBloodFX::Burst(GetWorld(), Location, Dir + FVector(0.f, 0.f, 0.25f), bVital ? 40 : 25, bVital ? 420.f : 330.f);
		Other->NoteHit(Region, Location, Dir);
	}
	if (Other && !Other->Wounds->CanFight())
	{
		// Quem cai nao recebe trava nem empurrao: o ragdoll cuida da queda (evita o corpo "voar").
		AddTrauma(0.3f);
		return;
	}
	if (Other)
	{
		HitStop(Other, StopTime);
		const float Strength = bParried ? 0.6f : (Region == EBodyRegion::Head || Region == EBodyRegion::Neck ? 1.f : 0.75f);
		Other->ReactToHit(Dir, bParried, Strength);
	}
	if (bParried)
	{
		// As laminas se repelem: o atacante tambem recua e sente o tranco.
		ReactToHit(-Dir, true, 0.5f);
	}
	else
	{
		AddTrauma(0.25f);   // quem acerta sente o impacto na mao/camera
	}
}

void AEAWCharacter::ReactToHit(const FVector& Dir, bool bParried, float Strength)
{
	LastHitDir = Dir;
	AddTrauma(bParried ? 0.35f : 0.6f * Strength);
	if (!bParried && IsLocallyControlled())
	{
		DamageFlash = FMath::Min(1.f, DamageFlash + 0.6f * Strength);
	}
	if (!Wounds->CanFight())
	{
		return;
	}
	if (HasAuthority() || IsLocallyControlled())
	{
		LaunchCharacter(Dir * (bParried ? 180.f : 260.f * Strength), true, false);
	}
	if (bParried || !bAnimated || !Wounds->CanFight() || GetMesh()->IsSimulatingPhysics())
	{
		return;
	}
	// Tronco vira fisica por um instante e e jogado na direcao do golpe; depois volta a animacao.
	GetMesh()->SetAllBodiesBelowSimulatePhysics(TEXT("spine_01"), true, true);
	GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(TEXT("spine_01"), 0.75f, false, true);
	GetMesh()->AddImpulseToAllBodiesBelow((Dir * 320.f + FVector(0.f, 0.f, 40.f)) * Strength, TEXT("spine_02"), true, true);
	HitReactAlpha = 1.f;
}

void AEAWCharacter::UpdateFeel(float Dt)
{
	// Fisica do tronco volta para a animacao em ~0,35 s.
	if (HitReactAlpha > 0.f)
	{
		HitReactAlpha = FMath::Max(0.f, HitReactAlpha - Dt / 0.35f);
		if (!GetMesh()->IsSimulatingPhysics())   // ragdoll completo nao e mexido aqui
		{
			GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(TEXT("spine_01"), 0.75f * HitReactAlpha, false, true);
			if (HitReactAlpha <= 0.f)
			{
				GetMesh()->SetAllBodiesBelowSimulatePhysics(TEXT("spine_01"), false, true);
			}
		}
	}
	// Tremor de camera (so no dono): forte e curto, cai sozinho.
	Trauma = FMath::Max(0.f, Trauma - Dt * 1.6f);
	DamageFlash = FMath::Max(0.f, DamageFlash - Dt * 1.4f);
	if (bFirstPersonActive)
	{
		ShakeTime += Dt;
		const float S = Trauma * Trauma;
		const FRotator Shake(
			S * 6.f * FMath::PerlinNoise1D(ShakeTime * 23.f),
			S * 5.f * FMath::PerlinNoise1D(ShakeTime * 23.f + 50.f),
			S * 8.f * FMath::PerlinNoise1D(ShakeTime * 23.f + 100.f));
		Camera->SetRelativeRotation(Shake);
	}
}

void AEAWCharacter::HandleWounded(EBodyRegion Region, float Damage, EWoundLife NewLife)
{
	if (!Wounds->CanFight())
	{
		if (HasAuthority())
		{
			Combat->ForceIncapacitated();
		}
		GetCharacterMovement()->DisableMovement();
	}
	else if (GetCharacterMovement()->MovementMode == MOVE_None)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	ApplyLifeVisuals();
}

void AEAWCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	// O jogador possui o soldado depois do BeginPlay; e aqui que sabemos que ele e "nosso".
	if (bAnimated && IsLocallyControlled())
	{
		SetupFirstPerson();
	}
}

void AEAWCharacter::SetupFirstPerson()
{
	if (bFirstPersonActive || !FirstPersonMesh->SkeletalMesh)
	{
		return;
	}
	bFirstPersonActive = true;

	// Ajustes de teste pela linha de comando.
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("EAWFPPitch="), FPPitch);
	FParse::Value(Cmd, TEXT("EAWFPX="), FPOffset.X);
	FParse::Value(Cmd, TEXT("EAWFPY="), FPOffset.Y);
	FParse::Value(Cmd, TEXT("EAWFPZ="), FPOffset.Z);
	FParse::Value(Cmd, TEXT("EAWFPFov="), FPFieldOfView);

	ApplyFirstPersonLayout();

	FirstPersonMesh->SetVisibility(true);
	FirstPersonMesh->SetComponentTickEnabled(true);
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	FirstPersonMesh->HideBoneByName(TEXT("neck_01"), EPhysBodyOp::PBO_None);   // cabeca e pescoco
	FirstPersonMesh->HideBoneByName(TEXT("thigh_l"), EPhysBodyOp::PBO_None);
	FirstPersonMesh->HideBoneByName(TEXT("thigh_r"), EPhysBodyOp::PBO_None);

	// O dono nao ve o proprio corpo inteiro, mas continua vendo a sombra dele.
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;
	Camera->SetFieldOfView(FPFieldOfView);

	ApplyWeaponVisuals();
	UE_LOG(LogEAWCombat, Log, TEXT("%s 1a pessoa: pitch=%.1f offset=%s fov=%.0f"), *GetName(), FPPitch, *FPOffset.ToString(), FPFieldOfView);
}

void AEAWCharacter::PlaySegment(UAnimSequence* Anim, float Start, float End, float Duration, bool bLoop, UAnimSequence* FPAnim)
{
	if (!Anim)
	{
		return;
	}
	const float Rate = Duration > KINDA_SMALL_NUMBER ? (End - Start) / Duration : 1.f;
	auto Play = [&](USkeletalMeshComponent* SkMesh, UAnimSequence* A)
	{
		SkMesh->PlayAnimation(A, bLoop);
		SkMesh->SetPosition(Start, false);
		SkMesh->SetPlayRate(Rate);
	};
	if (UEAWAnimInstance* AnimLayers = Cast<UEAWAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimLayers->SetCombatSegment(Anim, Start, End, Duration, bLoop);
	}
	else
	{
		Play(GetMesh(), Anim);
	}
	if (bFirstPersonActive)
	{
		// Os bracos de 1a pessoa usam a versao propria (mesmos quadros-chave), se existir.
		Play(FirstPersonMesh, FPAnim ? FPAnim : Anim);
	}
}

void AEAWCharacter::HandleCombatState(EMeleeState NewState, EAttackDirection Direction)
{
	if (!bAnimated)
	{
		return;
	}
	if (NewState != EMeleeState::Incapacitated && CurrentWeapon != EEAWWeapon::Sabre)
	{
		return;   // com o mosquete, quem anima e o HandleFirearmState
	}
	// Marcas de tempo das animacoes (30 fps): quadro f -> (f-1)/30 s.
	auto T = [](int32 Frame) { return (Frame - 1) / 30.f; };
	const float D = Combat->GetStateDuration();

	UAnimSequence* Attack = AnimCorteEsq;
	UAnimSequence* AttackFP = AnimCorteEsqFP;
	int32 WindupEnd = 15, ReleaseEnd = 24, Last = 41;
	switch (Direction)
	{
	case EAttackDirection::SlashRight: Attack = AnimCorteDir; AttackFP = AnimCorteDirFP; break;
	case EAttackDirection::Overhead:   Attack = AnimDeCima; AttackFP = AnimDeCimaFP; WindupEnd = 17; ReleaseEnd = 27; Last = 45; break;
	case EAttackDirection::Stab:       Attack = AnimEstocada; AttackFP = nullptr; WindupEnd = 13; ReleaseEnd = 20; Last = 35; break;
	default: break;
	}

	switch (NewState)
	{
	case EMeleeState::Idle:          PlaySegment(AnimGuarda, 0.f, AnimGuarda->SequenceLength, AnimGuarda->SequenceLength, true); break;
	case EMeleeState::Windup:        PlaySegment(Attack, T(1), T(WindupEnd), D, false, AttackFP); break;
	case EMeleeState::Release:
		PlaySegment(Attack, T(WindupEnd), T(ReleaseEnd), D, false, AttackFP);
		// O corpo avanca com o golpe (maior na estocada).
		if (HasAuthority() || IsLocallyControlled())
		{
			LaunchCharacter(GetActorForwardVector() * (Direction == EAttackDirection::Stab ? LungeStab : LungeSlash), false, false);
		}
		break;
	case EMeleeState::Recovery:      PlaySegment(Attack, T(ReleaseEnd), T(Last), D, false, AttackFP); break;
	case EMeleeState::Parry:         PlaySegment(AnimParry, T(1), T(10), D, false, AnimParryFP); break;
	case EMeleeState::ParryRecovery: PlaySegment(AnimParry, T(10), T(20), D, false, AnimParryFP); break;
	case EMeleeState::Feint:         PlaySegment(AnimFinta, T(11), T(19), D, false, AnimFintaFP); break;
	case EMeleeState::Stunned:       PlaySegment(AnimBloqueado, T(1), T(17), D, false, AnimBloqueadoFP); break;
	case EMeleeState::Incapacitated: StartRagdoll(); break;
	default: break;
	}
}

void AEAWCharacter::StartRagdoll()
{
	if (!bAnimated || GetMesh()->IsSimulatingPhysics())
	{
		return;
	}
	// Ao cair, o dono passa a ver o proprio corpo (os bracos de 1a pessoa somem).
	if (bFirstPersonActive)
	{
		FirstPersonMesh->SetVisibility(false);
		WeaponFP->SetVisibility(false);
		GetMesh()->SetOwnerNoSee(false);
		WeaponTP->SetOwnerNoSee(false);
		Camera->SetRelativeLocation(FVector::ZeroVector);
		Camera->SetFieldOfView(FPFieldOfView);
	}
	// A arma fica na mao do morto (o osso da arma nao tem corpo fisico).
	AttachWeaponToHand(GetMesh());
	// Limpa a fisica parcial do tronco e a trava de tempo antes de virar ragdoll (senao o corpo "voa").
	CustomTimeDilation = 1.f;
	if (HitReactAlpha > 0.f)
	{
		GetMesh()->SetAllBodiesBelowSimulatePhysics(TEXT("spine_01"), false, true);
		HitReactAlpha = 0.f;
	}
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	// Soldados vivos nao empurram o corpo caido (o CharacterMovement aplicava forca e o corpo "voava").
	GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	// Amortecimento: corpo pesado, sem pernas e bracos balancando no ar depois de cair.
	for (FBodyInstance* Body : GetMesh()->Bodies)
	{
		if (Body)
		{
			Body->LinearDamping = 0.6f;
			Body->AngularDamping = 4.f;
			Body->UpdateDampingProperties();
		}
	}
	GetMesh()->WakeAllRigidBodies();
	// Empurrao do golpe: o tronco e jogado para tras e o corpo cai em vez de "murchar" devagar.
	// Cai na direcao do ultimo golpe recebido (se nao houver, para tras).
	const FVector FallDir = LastHitDir.IsNearlyZero() ? -GetActorForwardVector() : LastHitDir;
	HitReactAlpha = 0.f;
	GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(TEXT("spine_01"), 1.f, false, true);
	GetMesh()->AddImpulseToAllBodiesBelow(FallDir * 280.f + FVector(0.f, 0.f, -60.f), TEXT("spine_01"), true, true);
	// A capsula nao pode segurar o proprio ragdoll (o quadril/pernas ficavam de pe).
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// O corpo caiu: as hitboxes de pe nao podem continuar recebendo golpes no ar.
	for (UCapsuleComponent* Hitbox : Hitboxes)
	{
		Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hitbox->SetHiddenInGame(true);
	}
	// Se o golpe que derrubou acabou de chegar, o membro atingido se solta agora.
	TryDismember();
}

void AEAWCharacter::NoteHit(EBodyRegion Region, const FVector& Location, const FVector& Dir, bool bAllowDismember)
{
	LastHitRegion = Region;
	bDismemberAllowed = bAllowDismember;
	// Teste: -EAWTesteCorte=Cabeca|Braco|Perna faz qualquer golpe contar como essa regiao no corte.
	FString Teste;
	if (FParse::Value(FCommandLine::Get(), TEXT("EAWTesteCorte="), Teste))
	{
		LastHitRegion = Teste == TEXT("Cabeca") ? EBodyRegion::Neck : Teste == TEXT("Braco") ? EBodyRegion::Arms
			: Teste == TEXT("Perna") ? EBodyRegion::Legs : Region;
	}
	LastHitLocation = Location;
	LastHitDir = Dir;
	LastHitTime = GetWorld()->GetTimeSeconds();
	if (!Wounds->CanFight())
	{
		StartRagdoll();   // nao faz nada se ja caiu
		TryDismember();
	}
}

void AEAWCharacter::TryDismember()
{
	// So o golpe que derruba arranca o membro; so visual (cada maquina faz o seu).
	if (CutBone != NAME_None || !bAnimated || !bDismemberAllowed || !AEAWBloodFX::IsGoreEnabled() || GetNetMode() == NM_DedicatedServer
		|| !GetMesh()->IsSimulatingPhysics() || GetWorld()->GetTimeSeconds() - LastHitTime > 0.6f)
	{
		return;
	}
	// Lado do golpe (direita/esquerda do soldado) para bracos e pernas.
	const bool bRight = FVector::DotProduct(LastHitLocation - GetActorLocation(), GetActorRightVector()) > 0.f;
	USkeletalMesh* PieceMesh = nullptr;
	FName Bone = NAME_None;
	float Diameter = 11.f;
	switch (LastHitRegion)
	{
	case EBodyRegion::Head:
	case EBodyRegion::Neck: Bone = TEXT("neck_01"); PieceMesh = PecaCabeca; Diameter = 12.f; break;
	case EBodyRegion::Arms: Bone = bRight ? TEXT("upperarm_r") : TEXT("upperarm_l"); PieceMesh = bRight ? PecaBracoD : PecaBracoE; Diameter = 10.f; break;
	case EBodyRegion::Legs: Bone = bRight ? TEXT("calf_r") : TEXT("calf_l"); PieceMesh = bRight ? PecaPernaD : PecaPernaE; Diameter = 11.f; break;
	default: return;   // tronco: sem membro (corpo partido fica para canhao/golpes muito fortes)
	}
	USkeletalMeshComponent* Body = GetMesh();
	const FName Parent = Body->GetParentBone(Bone);
	if (!PieceMesh || Parent == NAME_None)
	{
		return;
	}
	CutBone = Bone;
	const FVector CutPoint = Body->GetSocketLocation(Bone);
	const FVector OutDir = (CutPoint - Body->GetSocketLocation(Parent)).GetSafeNormal();

	// 1) Peca solta: mesma pose do corpo neste instante, so o membro simula (o resto da peca nao tem vertices).
	USkeletalMeshComponent* Piece = NewObject<USkeletalMeshComponent>(this);
	Piece->SetSkeletalMesh(PieceMesh);
	Piece->SetPhysicsAsset(Body->GetPhysicsAsset());
	Piece->SetWorldTransform(Body->GetComponentTransform());
	Piece->SetCollisionProfileName(TEXT("Ragdoll"));
	Piece->SetCollisionResponseToChannel(ECC_Melee, ECR_Ignore);
	Piece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Piece->RegisterComponent();
	GoreComponents.Add(Piece);

	auto InLimb = [&](FName B) { return B == Bone || Piece->BoneIsChildOf(B, Bone); };
	// Fora do membro: corpos e juntas fisicas removidos (antes ficavam cinematicos, prendiam a peca
	// no ar e empurravam o corpo caido - bug #40).
	for (FConstraintInstance* C : Piece->Constraints)
	{
		if (C && (!InLimb(C->ConstraintBone1) || !InLimb(C->ConstraintBone2)))
		{
			C->TermConstraint();
		}
	}
	for (FBodyInstance* B : Piece->Bodies)
	{
		if (B && B->BodySetup.IsValid() && !InLimb(B->BodySetup->BoneName))
		{
			B->TermBody();
		}
	}
	// A peca nao colide com corpos caidos (nasce encostada no proprio corpo; evita o "empurrao").
	Piece->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Piece->SetAllBodiesBelowSimulatePhysics(Bone, true, true);
	Piece->SetAllBodiesBelowPhysicsBlendWeight(Bone, 1.f, false, true);
	const FVector Fling = LastHitDir * 350.f + FVector(0.f, 0.f, 250.f);
	for (FBodyInstance* B : Piece->Bodies)
	{
		if (B && B->IsValidBodyInstance() && B->BodySetup.IsValid() && InLimb(B->BodySetup->BoneName))
		{
			const FName BName = B->BodySetup->BoneName;
			B->SetBodyTransform(Body->GetSocketTransform(BName), ETeleportType::TeleportPhysics);
			B->SetLinearVelocity(Body->GetPhysicsLinearVelocity(BName) + Fling, false);
			B->LinearDamping = 0.3f;
			B->AngularDamping = 1.5f;
			B->UpdateDampingProperties();
		}
	}
	Piece->AddAngularImpulseInDegrees(FVector::CrossProduct(FVector::UpVector, LastHitDir) * 600.f, Bone, true);

	// 2) Corpo: o membro some (vertices encolhem ate a junta) e os corpos fisicos dele saem.
	Body->HideBoneByName(Bone, EPhysBodyOp::PBO_Term);
	if (Bone == TEXT("upperarm_r"))
	{
		AttachWeaponToHand(Piece);   // a arma vai junto com o braco direito
	}

	// 3) Tampas vermelhas nos dois lados do corte e jatos de sangue pulsando.
	AddCap(Body, Parent, CutPoint, OutDir, Diameter);
	AddCap(Piece, Bone, CutPoint, -OutDir, Diameter);
	AEAWBloodFX::Jet(GetWorld(), Body, Parent, CutPoint, OutDir + FVector(0.f, 0.f, 0.3f), 1.6f, 70.f);
	AEAWBloodFX::Jet(GetWorld(), Piece, Bone, CutPoint, -OutDir, 0.7f, 35.f);
	AEAWBloodFX::Burst(GetWorld(), CutPoint, LastHitDir + FVector(0.f, 0.f, 0.5f), 45, 380.f);

	// Limite de pecas soltas no mundo (pedido: 20 por area) - as mais antigas somem.
	static TArray<TWeakObjectPtr<USkeletalMeshComponent>> AllPieces;
	AllPieces.Add(Piece);
	while (AllPieces.Num() > 20)
	{
		if (AllPieces[0].IsValid())
		{
			AllPieces[0]->DestroyComponent();
		}
		AllPieces.RemoveAt(0);
	}
	// Conferencia: quantos corpos fisicos da peca estao simulando (0 = peca presa no ar, ver bug #41).
	int32 NSim = 0;
	for (FBodyInstance* B : Piece->Bodies)
	{
		NSim += B && B->IsValidBodyInstance() && B->IsInstanceSimulatingPhysics();
	}
	UE_LOG(LogEAWCombat, Log, TEXT("%s desmembrado: %s (regiao %d), corpos da peca simulando=%d"), *GetName(), *Bone.ToString(),
		(int32)LastHitRegion, NSim);
}

void AEAWCharacter::AddCap(USceneComponent* Parent, FName Bone, const FVector& Point, const FVector& Dir, float Diameter)
{
	if (!CapMesh)
	{
		return;
	}
	UStaticMeshComponent* Cap = NewObject<UStaticMeshComponent>(this);
	Cap->SetStaticMesh(CapMesh);
	if (CapMaterial)
	{
		Cap->SetMaterial(0, CapMaterial);
	}
	Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cap->SetCastShadow(false);
	Cap->RegisterComponent();
	// Disco achatado na direcao do membro (a esfera basica tem 100 cm).
	const float S = Diameter / 100.f;
	Cap->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Dir).Rotator(), Point, FVector(S, S, S * 0.35f)));
	Cap->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform, Bone);
	GoreComponents.Add(Cap);
}

void AEAWCharacter::ClearGore()
{
	for (UPrimitiveComponent* C : GoreComponents)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	GoreComponents.Reset();
	if (CutBone != NAME_None)
	{
		GetMesh()->UnHideBoneByName(CutBone);
		CutBone = NAME_None;
	}
	LastHitTime = -100.f;
}

void AEAWCharacter::ApplyLifeVisuals()
{
	if (bAnimated)
	{
		if (!Wounds->CanFight())
		{
			StartRagdoll();
		}
		return;
	}
	switch (Wounds->GetLife())
	{
	case EWoundLife::Dead:
		BodyMesh->SetRelativeLocationAndRotation(BodyMeshBaseLocation + FVector(0.f, 0.f, -70.f), FRotator(90.f, 0.f, 0.f));
		break;
	case EWoundLife::Incapacitated:
		BodyMesh->SetRelativeLocationAndRotation(BodyMeshBaseLocation + FVector(0.f, 0.f, -40.f), FRotator(45.f, 0.f, 0.f));
		break;
	default:
		BodyMesh->SetRelativeLocationAndRotation(BodyMeshBaseLocation, FRotator::ZeroRotator);
		break;
	}
}

void AEAWCharacter::ResetSoldier()
{
	if (!HasAuthority())
	{
		return;
	}
	Wounds->ResetWounds();
	Combat->ResetCombat();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	const bool bHadCut = CutBone != NAME_None;
	ClearGore();
	WoundMarks = 0;
	Firearm->ResetFirearm();
	AttachWeaponToSkeleton();
	if (bAnimated && GetMesh()->IsSimulatingPhysics())
	{
		GetMesh()->SetSimulatePhysics(false);
		if (bHadCut)
		{
			GetMesh()->RecreatePhysicsState();   // os corpos do membro cortado foram removidos
		}
		GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
		GetMesh()->SetCollisionResponseToChannel(ECC_Melee, ECR_Ignore);
		GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -88.f), FRotator(0.f, -90.f, 0.f));
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (bFirstPersonActive)
		{
			FirstPersonMesh->SetVisibility(true);
			GetMesh()->SetOwnerNoSee(true);
		}
		for (UCapsuleComponent* Hitbox : Hitboxes)
		{
			Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Hitbox->SetHiddenInGame(!bShowHitboxes);
		}
	}
	ApplyLifeVisuals();
}

void AEAWCharacter::DrawDebugHud()
{
	if (!GEngine)
	{
		return;
	}

	static const TCHAR* StateNames[] = { TEXT("Guarda"), TEXT("Preparacao"), TEXT("GOLPE"), TEXT("Recuperacao"),
		TEXT("PARRY"), TEXT("Recup. parry"), TEXT("Finta"), TEXT("Atordoado"), TEXT("Fora de combate") };
	static const TCHAR* DirNames[] = { TEXT("corte esq."), TEXT("corte dir."), TEXT("de cima"), TEXT("estocada") };
	static const TCHAR* LifeNames[] = { TEXT("Saudavel"), TEXT("Ferido"), TEXT("Incapacitado"), TEXT("Morto") };

	const int32 StateIdx = FMath::Clamp(static_cast<int32>(Combat->GetState()), 0, 8);
	const int32 DirIdx = FMath::Clamp(static_cast<int32>(Combat->GetDirection()), 0, 3);
	const int32 LifeIdx = FMath::Clamp(static_cast<int32>(Wounds->GetLife()), 0, 3);

	GEngine->AddOnScreenDebugMessage(1001, 0.f, FColor::White, FString::Printf(
		TEXT("VOCE  %s (%s)  stamina %.0f  |  %s, ferimentos %d%s"),
		StateNames[StateIdx], DirNames[DirIdx], Combat->GetStamina(), LifeNames[LifeIdx], Wounds->GetNonVitalWounds(),
		Combat->HasRiposte() ? TEXT("  |  CONTRA-ATAQUE!") : TEXT("")));
	if (CurrentWeapon == EEAWWeapon::Mosquete)
	{
		static const TCHAR* FireNames[] = { TEXT("guarda"), TEXT("erguendo"), TEXT("MIRANDO"), TEXT("DISPARO"), TEXT("baixando"), TEXT("CARREGANDO") };
		static const TCHAR* StageNames[] = { TEXT("escorva"), TEXT("polvora e bala"), TEXT("tirar vareta"), TEXT("socar"), TEXT("guardar vareta"), TEXT("armar o cao") };
		const int32 Fs = FMath::Clamp(static_cast<int32>(Firearm->GetState()), 0, 5);
		FString Line = FString::Printf(TEXT("MOSQUETE %s  |  %s  |  %s"), bBayonet ? TEXT("[X] COMBATE (baioneta)") : TEXT("[X] TIRO"),
			Firearm->IsLoaded() ? TEXT("CARREGADO") : TEXT("descarregado (R)"), FireNames[Fs]);
		if (Firearm->GetState() == EFirearmState::Reloading)
		{
			Line += FString::Printf(TEXT(" %d/6: %s"), Firearm->GetStage() + 1, StageNames[FMath::Clamp<int32>(Firearm->GetStage(), 0, 5)]);
		}
		GEngine->AddOnScreenDebugMessage(1004, 0.f, FColor(255, 220, 120), Line);
	}

	// Adversario mais proximo
	AEAWCharacter* Nearest = nullptr;
	float NearestDist = 600.f;
	for (TActorIterator<AEAWCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this)
		{
			continue;
		}
		const float Dist = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (Dist < NearestDist)
		{
			NearestDist = Dist;
			Nearest = *It;
		}
	}
	if (Nearest)
	{
		const int32 OState = FMath::Clamp(static_cast<int32>(Nearest->Combat->GetState()), 0, 8);
		const int32 ODir = FMath::Clamp(static_cast<int32>(Nearest->Combat->GetDirection()), 0, 3);
		const int32 OLife = FMath::Clamp(static_cast<int32>(Nearest->Wounds->GetLife()), 0, 3);
		GEngine->AddOnScreenDebugMessage(1002, 0.f, FColor::Yellow, FString::Printf(
			TEXT("OPONENTE  %s (%s)  stamina %.0f  |  %s, ferimentos %d  |  %.0f cm"),
			StateNames[OState], DirNames[ODir], Nearest->Combat->GetStamina(), LifeNames[OLife],
			Nearest->Wounds->GetNonVitalWounds(), NearestDist));
	}

	GEngine->AddOnScreenDebugMessage(1003, 0.f, FColor(160, 160, 160), TEXT(
		"LMB corte | E/roda cima: de cima | C/roda baixo: estocada | RMB parry | Q finta | B novo bot | H hitboxes | console: EAWReset"));
}

// ---------------------------------------------------------------------------
// Armas: mosquete (tiro / combate com a tecla X) e sabre
// ---------------------------------------------------------------------------

void AEAWCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (Cast<APlayerController>(NewController))
	{
		FString Arma;
		FParse::Value(FCommandLine::Get(), TEXT("EAWArma="), Arma);
		EquipWeapon(Arma == TEXT("Sabre") ? EEAWWeapon::Sabre : EEAWWeapon::Mosquete);
	}
	else
	{
		EquipWeapon(EEAWWeapon::Sabre);   // bots lutam de sabre (IA de corpo a corpo)
	}
}

void AEAWCharacter::EquipWeapon(EEAWWeapon NewWeapon)
{
	if (!HasAuthority())
	{
		return;
	}
	if (CurrentWeapon == EEAWWeapon::Mosquete && NewWeapon != EEAWWeapon::Mosquete)
	{
		Firearm->RequestCancel();
	}
	CurrentWeapon = NewWeapon;
	bBayonet = false;
	OnRep_Weapon();
	UE_LOG(LogEAWCombat, Log, TEXT("%s arma: %s"), *GetName(), NewWeapon == EEAWWeapon::Mosquete ? TEXT("mosquete") : TEXT("sabre"));
}

void AEAWCharacter::OnRep_Weapon()
{
	ApplyWeaponVisuals();
}

void AEAWCharacter::ApplyWeaponVisuals()
{
	USkeletalMesh* M = CurrentWeapon == EEAWWeapon::Mosquete ? MeshMosquete : MeshSabre;
	if (M && WeaponTP->SkeletalMesh != M)
	{
		WeaponTP->SetSkeletalMesh(M);
		WeaponFP->SetSkeletalMesh(M);
		if (WeaponTP->GetAttachParent() == GetMesh())
		{
			WeaponTP->SetMasterPoseComponent(GetMesh());
		}
		WeaponFP->SetMasterPoseComponent(FirstPersonMesh);
	}
	const bool bDown = GetMesh()->IsSimulatingPhysics();
	WeaponFP->SetVisibility(bFirstPersonActive && !bDown);
	if (UEAWAnimInstance* AnimLayers = Cast<UEAWAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimLayers->bAllowUpperRun = CurrentWeapon == EEAWWeapon::Sabre;   // correr com o sabre no ombro
	}
	ApplyFirstPersonLayout();
	if (!bAnimated || bDown)
	{
		return;
	}
	if (CurrentWeapon == EEAWWeapon::Mosquete)
	{
		HandleFirearmState(Firearm->GetState(), Firearm->GetStage());
	}
	else
	{
		HandleCombatState(Combat->GetState(), Combat->GetDirection());
	}
}

void AEAWCharacter::ApplyFirstPersonLayout()
{
	if (!bFirstPersonActive)
	{
		return;
	}
	if (CurrentWeapon == EEAWWeapon::Mosquete)
	{
		// Mosquete: corpo de 1a pessoa igual ao de 3a em relacao a camera (camera 6 cm a frente do centro).
		// A mira encosta o rosto na coronha (a camera vai ate o olho, ver UpdateFirearmView).
		FPBase = FTransform(FRotator(0.f, -90.f, 0.f), FVector(-6.f, 0.f, -158.f));
	}
	else
	{
		// Sabre: bracos inclinados e deslocados para as maos aparecerem.
		const FTransform Base(FRotator(0.f, -90.f, 0.f), FVector(0.f, 0.f, -158.f));
		FPBase = Base * FTransform(FRotator(FPPitch, 0.f, 0.f)) * FTransform(FPOffset);
	}
	FirstPersonMesh->SetRelativeTransform(FPBase);
}

void AEAWCharacter::AttachWeaponToHand(USceneComponent* HandOwner)
{
	if (!WeaponTP->SkeletalMesh || !HandOwner)
	{
		return;
	}
	if (WeaponTP->MasterPoseComponent.IsValid())
	{
		// Sem a pose do corpo, a malha da arma volta a pose de referencia: posiciona o componente para que
		// o osso da arma fique exatamente onde estava.
		const FName Bone = CurrentWeapon == EEAWWeapon::Mosquete ? FName(TEXT("arma")) : FName(TEXT("weapon_r"));
		const FReferenceSkeleton& Ref = WeaponTP->SkeletalMesh->GetRefSkeleton();
		const int32 Idx = Ref.FindBoneIndex(Bone);
		if (Idx == INDEX_NONE)
		{
			return;
		}
		const FTransform BoneWorld = GetMesh()->GetSocketTransform(Bone);
		const FTransform RefCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Idx);
		WeaponTP->SetMasterPoseComponent(nullptr);
		WeaponTP->SetWorldTransform(RefCS.Inverse() * BoneWorld);
	}
	WeaponTP->AttachToComponent(HandOwner, FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
	WeaponTP->SetOwnerNoSee(false);
}

void AEAWCharacter::AttachWeaponToSkeleton()
{
	WeaponTP->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	WeaponTP->SetRelativeTransform(FTransform::Identity);
	WeaponTP->SetMasterPoseComponent(GetMesh());
	WeaponTP->SetOwnerNoSee(bFirstPersonActive);
	ApplyWeaponVisuals();
}

void AEAWCharacter::HandleFirearmState(EFirearmState NewState, uint8 Stage)
{
	if (!bAnimated || CurrentWeapon != EEAWWeapon::Mosquete || !Wounds->CanFight() || GetMesh()->IsSimulatingPhysics())
	{
		return;
	}
	auto Len = [](UAnimSequence* A) { return A ? A->SequenceLength : 0.f; };
	const FFirearmConfig& C = Firearm->Config;
	if (bBayonet)
	{
		// Combate com o mosquete (tecla X): guarda ate as animacoes de baioneta/coronha existirem.
		PlaySegment(AnimMGuarda, 0.f, Len(AnimMGuarda), Len(AnimMGuarda), true, AnimMGuardaFP);
		return;
	}
	switch (NewState)
	{
	case EFirearmState::Lowered:
		PlaySegment(AnimMGuarda, 0.f, Len(AnimMGuarda), Len(AnimMGuarda), true, AnimMGuardaFP);
		break;
	case EFirearmState::Raising:
		PlaySegment(AnimMErguer, 0.f, Len(AnimMErguer), C.AimInTime, false, AnimMErguerFP);
		break;
	case EFirearmState::Aiming:
		if (Firearm->IsLoaded()) PlaySegment(AnimMMira, 0.f, Len(AnimMMira), Len(AnimMMira), true, AnimMMiraFP);
		else PlaySegment(AnimMTiro, Len(AnimMTiro) - 0.02f, Len(AnimMTiro), 1.f, true, AnimMTiroFP);   // cao abaixado
		break;
	case EFirearmState::Firing:
		PlaySegment(AnimMTiro, 0.f, Len(AnimMTiro), C.FireRecoverTime, false, AnimMTiroFP);
		break;
	case EFirearmState::Lowering:
		if (Firearm->IsLoaded()) PlaySegment(AnimMErguer, Len(AnimMErguer), 0.f, C.AimOutTime, false, AnimMErguerFP);
		else PlaySegment(AnimMBaixar, 0.f, Len(AnimMBaixar), C.AimOutTime, false, AnimMBaixarFP);
		break;
	case EFirearmState::Reloading:
	{
		const int32 S = FMath::Clamp<int32>(Stage, 0, 5);
		const float T0 = UEAWFirearmComponent::ReloadAnimTimes[S], T1 = UEAWFirearmComponent::ReloadAnimTimes[S + 1];
		PlaySegment(AnimMRecarga, T0, T1, (T1 - T0) * Firearm->ReloadScale(), false, AnimMRecargaFP);
		break;
	}
	default:
		break;
	}
}

void AEAWCharacter::UpdateFirearmView(float Dt)
{
	if (!bFirstPersonActive || !IsLocallyControlled() || GetMesh()->IsSimulatingPhysics())
	{
		return;
	}
	const EFirearmState S = Firearm->GetState();
	const bool bUp = UsingMusketFire() && Wounds->CanFight()
		&& (S == EFirearmState::Raising || S == EFirearmState::Aiming || S == EFirearmState::Firing);
	const FFirearmConfig& C = Firearm->Config;
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bUp ? 1.f : 0.f, Dt, 1.f / (bUp ? C.AimInTime : C.AimOutTime));
	const float A = AimAlpha * AimAlpha * (3.f - 2.f * AimAlpha);

	// Rosto na coronha: a camera vai ao olho direito; o foco fecha o campo de visao.
	Camera->SetRelativeLocation(AimEyeOffset * A);
	Camera->SetFieldOfView(FMath::Lerp(FPFieldOfView, AimFieldOfView, A));

	// Balanco: respiracao + cansaco do braco (cresce segurando a mira) + andar.
	AimHoldTime = (S == EFirearmState::Aiming) ? AimHoldTime + Dt : 0.f;
	const float T = GetWorld()->GetTimeSeconds();
	const float Moving = FMath::Min(1.f, GetVelocity().Size2D() / WalkSpeed);
	const float Amp = (0.35f + FMath::Min(1.2f, 0.08f * AimHoldTime) + 0.8f * Moving) * A;
	SwayRot = FRotator(
		Amp * (0.7f * FMath::PerlinNoise1D(T * 0.55f + 20.f) + 0.3f * FMath::Sin(2.f * PI * 0.25f * T)),
		Amp * FMath::PerlinNoise1D(T * 0.45f), 0.f);
	// Os bracos e a arma giram em volta do olho (a linha do cano continua passando pelo olho).
	const FVector Eye = AimEyeOffset * A;
	FirstPersonMesh->SetRelativeTransform(FPBase * FTransform(-Eye) * FTransform(SwayRot) * FTransform(Eye));

	// Coice: a camera sobe em ~0,1 s.
	if (!RecoilPending.IsNearlyZero(0.01f) && Controller)
	{
		const FRotator Step = RecoilPending * FMath::Min(1.f, Dt * 25.f);
		Controller->SetControlRotation(Controller->GetControlRotation() + Step);
		RecoilPending -= Step;
	}
}

void AEAWCharacter::GetShotAim(FVector& Origin, FVector& Dir)
{
	Origin = Camera->GetComponentLocation();
	const FFirearmConfig& C = Firearm->Config;
	// Desvio do cano liso (normal, desvio padrao em graus); sem mirar, o tiro sai muito mais aberto.
	const float Sigma = AimAlpha > 0.9f ? C.AimSpreadDeg : 3.f;
	auto Gauss = []()
	{
		const float U1 = FMath::Max(1e-6f, FMath::FRand()), U2 = FMath::FRand();
		return FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2);
	};
	const FRotator Local = SwayRot + FRotator(Sigma * Gauss(), Sigma * Gauss(), 0.f);
	Dir = AimRoot->GetComponentRotation().RotateVector(Local.Vector());
}

void AEAWCharacter::PlayShotEffects(const FVector& Dir)
{
	USkeletalMeshComponent* Src = (bFirstPersonActive && IsLocallyControlled()) ? FirstPersonMesh : GetMesh();
	const FVector Muzzle = Src->GetSocketLocation(TEXT("arma_boca"));
	const FVector Pan = Src->GetSocketLocation(TEXT("arma_cao"));
	AEAWSmokeFX::MuzzleBlast(GetWorld(), Muzzle, Dir, Pan);
	if (IsLocallyControlled())
	{
		const FFirearmConfig& C = Firearm->Config;
		RecoilPending += FRotator(C.RecoilPitch, FMath::FRandRange(-C.RecoilYaw, C.RecoilYaw), 0.f);
		AddTrauma(0.55f);
	}
}

void AEAWCharacter::ReceiveBullet(const FVector& Location, const FVector& Dir, FName Bone, float EnergyJ, EBodyRegion Region)
{
	const FVector D = Dir.GetSafeNormal();
	if (Bone == NAME_None)
	{
		Bone = GetMesh()->FindClosestBone(Location);
	}
	// Ferida de entrada: disco vermelho-escuro de 2,6 cm preso ao osso (acompanha a animacao e o ragdoll).
	static UMaterialInterface* WoundMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Efeitos/M_Ferida.M_Ferida"));
	if (CapMesh && WoundMarks < 12 && AEAWBloodFX::IsGoreEnabled())
	{
		UStaticMeshComponent* W = NewObject<UStaticMeshComponent>(this);
		W->SetStaticMesh(CapMesh);
		if (WoundMat)
		{
			W->SetMaterial(0, WoundMat);
		}
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetCastShadow(false);
		W->RegisterComponent();
		W->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(-D).Rotator(), Location - D * 0.4f, FVector(0.026f, 0.026f, 0.008f)));
		W->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform, Bone);
		GoreComponents.Add(W);
		++WoundMarks;
	}
	// Respingo de volta na entrada; com energia alta a bala atravessa e espirra do outro lado.
	AEAWBloodFX::Burst(GetWorld(), Location, -D + FVector(0.f, 0.f, 0.3f), 10, 160.f);
	if (EnergyJ > 600.f)
	{
		AEAWBloodFX::Burst(GetWorld(), Location + D * 25.f, D + FVector(0.f, 0.f, 0.1f), 22, 380.f);
	}
	LastHitDir = D.GetSafeNormal2D();
	if (Wounds->CanFight())
	{
		ReactToHit(LastHitDir, false, FMath::Clamp(EnergyJ / 2400.f, 0.4f, 1.f));
	}
	else
	{
		// Queda: braco/perna arrancado so com tiro muito forte e de perto (mais de 1800 J).
		const bool bSever = EnergyJ > 1800.f && (Region == EBodyRegion::Arms || Region == EBodyRegion::Legs);
		NoteHit(Region, Location, D, bSever);
		if (GetMesh()->IsSimulatingPhysics())
		{
			// Quantidade de movimento da bala (exagerada 3x para se ver no corpo).
			const float SpeedMs = FMath::Sqrt(2.f * EnergyJ / FMath::Max(0.001f, Firearm->Config.BallMassKg));
			GetMesh()->AddImpulseAtLocation(D * Firearm->Config.BallMassKg * SpeedMs * 100.f * 3.f, Location, Bone);
		}
	}
}
