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
#include "Weapons/EAWDroppedWeapon.h"
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

	// O corpo NAO gira grudado no mouse: a camera gira na hora e o corpo acompanha com velocidade propria
	// (parado, so vira os pes depois de 70 graus; ate la o tronco torce na cintura). Ver UpdateBodyTurn.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	// Arranque e freada rapidos (~0,15 s), como no Chivalry; o peso vem da camera, da arma e do giro do corpo.
	GetCharacterMovement()->MaxAcceleration = 1800.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.f;
	GetCharacterMovement()->GroundFriction = 8.f;
	GetCharacterMovement()->AirControl = 0.15f;
	GetCharacterMovement()->JumpZVelocity = 380.f;

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
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Frente(TEXT("/Game/Personagem/AS_Trote_Frente.AS_Trote_Frente"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Tras(TEXT("/Game/Personagem/AS_Trote_Tras.AS_Trote_Tras"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Esq(TEXT("/Game/Personagem/AS_Trote_Esq.AS_Trote_Esq"));
	static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> M_Dir(TEXT("/Game/Personagem/AS_Trote_Dir.AS_Trote_Dir"));
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
	EAW_ANIM(AnimMCoronhada, "AS_Mosquete_Coronhada"); EAW_ANIM(AnimMCoronhadaFP, "AS_Mosquete_Coronhada_FP");
	EAW_ANIM(AnimMEstocada, "AS_Mosquete_Estocada"); EAW_ANIM(AnimMEstocadaFP, "AS_Mosquete_Estocada_FP");
	EAW_ANIM(AnimMEstocadaLonga, "AS_Mosquete_EstocadaLonga"); EAW_ANIM(AnimMEstocadaLongaFP, "AS_Mosquete_EstocadaLonga_FP");
	EAW_ANIM(AnimMAparoDir, "AS_Mosquete_AparoDir"); EAW_ANIM(AnimMAparoDirFP, "AS_Mosquete_AparoDir_FP");
	EAW_ANIM(AnimMAparoEsq, "AS_Mosquete_AparoEsq"); EAW_ANIM(AnimMAparoEsqFP, "AS_Mosquete_AparoEsq_FP");
	EAW_ANIM(AnimMBloqueado, "AS_Mosquete_Bloqueado"); EAW_ANIM(AnimMBloqueadoFP, "AS_Mosquete_Bloqueado_FP");
	EAW_ANIM(AnimMFinta, "AS_Mosquete_Finta"); EAW_ANIM(AnimMFintaFP, "AS_Mosquete_Finta_FP");
#undef EAW_ANIM
	{
		static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> W_Faca(TEXT("/Game/Armas/SK_Arma_Faca.SK_Arma_Faca"));
		static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> W_Machado(TEXT("/Game/Armas/SK_Arma_Machado.SK_Arma_Machado"));
		MeshFaca = W_Faca.Get(); MeshMachado = W_Machado.Get();
	}

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
	if (FParse::Param(FCommandLine::Get(), TEXT("EAWRecargaRapida")))
	{
		Firearm->Config.ReloadTime = 3.f;   // teste: varios tiros em pouco tempo
	}
	WeaponTP->SetMasterPoseComponent(GetMesh(), true);   // forca: trocar a malha exige refazer o mapa de ossos
	WeaponFP->SetMasterPoseComponent(FirstPersonMesh, true);

	bAnimated = GetMesh()->SkeletalMesh && AnimGuarda && AnimCorteEsq && AnimCorteDir && AnimDeCima && AnimEstocada
		&& AnimParry && AnimBloqueado && AnimFinta;
	if (UEAWAnimInstance* AnimLayers = Cast<UEAWAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimLayers->WalkForward = AnimAndarFrente; AnimLayers->WalkBack = AnimAndarTras;
		AnimLayers->WalkLeft = AnimAndarEsq; AnimLayers->WalkRight = AnimAndarDir; AnimLayers->Run = AnimCorrer;
		AnimLayers->SpeedForward = WalkSpeed; AnimLayers->SpeedBack = WalkSpeed * BackSpeedScale;
		AnimLayers->SpeedSide = WalkSpeed * SideSpeedScale; AnimLayers->SpeedRun = SprintSpeed;
		AnimLayers->WalkCycle = 19.f / 30.f;   // trote: ciclo de 19 quadros nas 4 direcoes
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
	DOREPLIFETIME(AEAWCharacter, PrimaryWeapon);
	DOREPLIFETIME(AEAWCharacter, SecondaryWeapon);
}

void AEAWCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Mesma rotacao de mira em todas as maquinas: o servidor rastreia a lamina com ela.
	// Dono: rotacao do controle (a "base aim" do jogador inclui a camera - com o balanco, acumulava e a imagem tombava).
	AimRoot->SetWorldRotation((IsLocallyControlled() && Controller) ? GetControlRotation() : GetBaseAimRotation());

	// Teste -EAWAutoAndar: frente 1,5 s, direita 1 s, tras 1 s, gira 120 graus em 0,8 s, parado 1 s (em ciclo).
	static const bool bAutoWalk = FParse::Param(FCommandLine::Get(), TEXT("EAWAutoAndar"));
	if (bAutoWalk && IsLocallyControlled() && IsPlayerControlled() && Controller)
	{
		AutoWalkTime += DeltaSeconds;
		const float T = FMath::Fmod(AutoWalkTime, 5.3f);
		if (T < 1.5f) MoveForward(1.f);
		else if (T < 2.5f) MoveRight(1.f);
		else if (T < 3.5f) MoveForward(-1.f);
		else if (T < 4.3f) Controller->SetControlRotation(Controller->GetControlRotation() + FRotator(0.f, 150.f * DeltaSeconds, 0.f));
	}

	ApplyMovementSpeed();
	UpdateFeel(DeltaSeconds);
	UpdateFirearmView(DeltaSeconds);
	UpdateBodyTurn(DeltaSeconds);
	UpdatePickup(DeltaSeconds);

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
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AEAWCharacter::InputInteractPressed);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &AEAWCharacter::InputInteractReleased);
	PlayerInputComponent->BindAction("Feint", IE_Pressed, this, &AEAWCharacter::InputFeint);
	PlayerInputComponent->BindAction("ToggleHitboxes", IE_Pressed, this, &AEAWCharacter::InputToggleHitboxes);
	PlayerInputComponent->BindAction("SpawnBot", IE_Pressed, this, &AEAWCharacter::InputSpawnBot);
}

void AEAWCharacter::MoveForward(float Value)
{
	if (Value != 0.f && Wounds->CanFight())
	{
		// Frente = para onde a camera olha (o corpo vira atras). Para tras e mais lento.
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(Yaw.Vector(), Value < 0.f ? Value * BackSpeedScale : Value);
	}
}

void AEAWCharacter::MoveRight(float Value)
{
	if (Value != 0.f && Wounds->CanFight())
	{
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value * SideSpeedScale);
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
	if (IsSwitching()) return;
	if (UsingMusketFire()) { Firearm->RequestFire(); return; }
	if (UsesMeleeInput()) Combat->RequestSlash();
}
void AEAWCharacter::InputOverhead() { if (UsesMeleeInput() && !IsSwitching()) Combat->RequestAttack(EAttackDirection::Overhead); }
void AEAWCharacter::InputStab()     { if (UsesMeleeInput() && !IsSwitching()) Combat->RequestAttack(EAttackDirection::Stab); }
void AEAWCharacter::InputParry()
{
	if (UsingMusketFire()) { Firearm->RequestAim(true); return; }
	if (UsesMeleeInput()) Combat->RequestParry();
}
void AEAWCharacter::InputAimReleased() { if (UsingMusketFire()) Firearm->RequestAim(false); }
void AEAWCharacter::InputFeint()    { if (UsesMeleeInput()) Combat->RequestFeint(); }
void AEAWCharacter::InputReload()   { if (UsingMusketFire()) Firearm->RequestReload(); }
void AEAWCharacter::InputToggleMelee()
{
	if (CurrentWeapon == EEAWWeapon::Mosquete)
	{
		Firearm->RequestCancel();
		ServerSetBayonet(!bBayonet);
	}
}
// 1 = arma de fogo, 2 = arma de corpo a corpo (o soldado so carrega duas).
void AEAWCharacter::InputWeapon1() { ServerEquip(PrimaryWeapon); }
void AEAWCharacter::InputWeapon2() { ServerEquip(SecondaryWeapon); }
void AEAWCharacter::ServerEquip_Implementation(EEAWWeapon NewWeapon)
{
	// So troca entre as duas que carrega.
	if ((NewWeapon == PrimaryWeapon || NewWeapon == SecondaryWeapon) && NewWeapon != CurrentWeapon && Wounds->CanFight())
	{
		EquipWeapon(NewWeapon);
	}
}
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
	float WeightMult = CurrentWeapon == EEAWWeapon::Faca ? 1.03f : CurrentWeapon == EEAWWeapon::Machado ? 0.98f : 1.f;
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
		Camera->SetRelativeRotation(Shake + CamBob);
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
	const bool bMusket = CurrentWeapon == EEAWWeapon::Mosquete;
	if (NewState != EMeleeState::Incapacitated && bMusket && !bBayonet)
	{
		return;   // mosquete em modo de tiro: quem anima e o HandleFirearmState
	}
	// Marcas de tempo das animacoes (30 fps): quadro f -> (f-1)/30 s. As do mosquete usam as mesmas marcas.
	auto T = [](int32 Frame) { return (Frame - 1) / 30.f; };
	const float D = Combat->GetStateDuration();

	UAnimSequence* Attack = bMusket ? AnimMCoronhada : AnimCorteEsq;
	UAnimSequence* AttackFP = bMusket ? AnimMCoronhadaFP : AnimCorteEsqFP;
	int32 WindupEnd = 15, ReleaseEnd = 24, Last = 41;
	switch (Direction)
	{
	case EAttackDirection::SlashRight:
		if (!bMusket) { Attack = AnimCorteDir; AttackFP = AnimCorteDirFP; }
		break;
	case EAttackDirection::Overhead:
		Attack = bMusket ? AnimMEstocadaLonga : AnimDeCima; AttackFP = bMusket ? AnimMEstocadaLongaFP : AnimDeCimaFP;
		WindupEnd = 17; ReleaseEnd = 27; Last = 45; break;
	case EAttackDirection::Stab:
		Attack = bMusket ? AnimMEstocada : AnimEstocada; AttackFP = bMusket ? AnimMEstocadaFP : nullptr;
		WindupEnd = 13; ReleaseEnd = 20; Last = 35; break;
	default: break;
	}
	if (bMusket && NewState == EMeleeState::Windup)
	{
		// O que corta: coronhada = coronha (do punho ate a soleira); estocadas = baioneta (da boca a ponta).
		const bool bButt = Direction == EAttackDirection::SlashLeft || Direction == EAttackDirection::SlashRight;
		Combat->GripSocket = bButt ? TEXT("arma") : TEXT("arma_boca");
		Combat->TipSocket = bButt ? TEXT("arma_coronha") : TEXT("arma_ponta");
	}
	UAnimSequence* Guard = bMusket ? AnimMGuarda : AnimGuarda;
	UAnimSequence* GuardFP = bMusket ? AnimMGuardaFP : nullptr;
	UAnimSequence* Parry = AnimParry;
	UAnimSequence* ParryFP = AnimParryFP;
	if (bMusket)
	{
		// Aparo a direita ou a esquerda conforme o golpe do inimigo mais perto.
		bool bLeft = false;
		float Best = 400.f;
		for (TActorIterator<AEAWCharacter> It(GetWorld()); It; ++It)
		{
			const float Dist = It->GetDistanceTo(this);
			if (*It != this && Dist < Best && It->GetCombat())
			{
				Best = Dist;
				bLeft = It->GetCombat()->GetDirection() == EAttackDirection::SlashLeft;
			}
		}
		Parry = bLeft ? AnimMAparoEsq : AnimMAparoDir;
		ParryFP = bLeft ? AnimMAparoEsqFP : AnimMAparoDirFP;
	}
	UAnimSequence* Feint = bMusket ? AnimMFinta : AnimFinta;
	UAnimSequence* FeintFP = bMusket ? AnimMFintaFP : AnimFintaFP;
	UAnimSequence* Blocked = bMusket ? AnimMBloqueado : AnimBloqueado;
	UAnimSequence* BlockedFP = bMusket ? AnimMBloqueadoFP : AnimBloqueadoFP;
	if (!Attack || !Guard)
	{
		return;
	}

	switch (NewState)
	{
	case EMeleeState::Idle:          PlaySegment(Guard, 0.f, Guard->SequenceLength, Guard->SequenceLength, true, GuardFP); break;
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
	case EMeleeState::Parry:         PlaySegment(Parry, T(1), T(10), D, false, ParryFP); break;
	case EMeleeState::ParryRecovery: PlaySegment(Parry, T(10), T(20), D, false, ParryFP); break;
	case EMeleeState::Feint:         PlaySegment(Feint, T(11), T(19), D, false, FeintFP); break;
	case EMeleeState::Stunned:       PlaySegment(Blocked, T(1), T(17), D, false, BlockedFP); break;
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
	// O morto larga a arma da mao: ela cai com fisica e pode ser pega por outro soldado (F).
	WeaponTP->SetVisibility(false);
	if (HasAuthority())
	{
		DropWeapon(CurrentWeapon, CurrentWeapon != EEAWWeapon::Mosquete || Firearm->IsLoaded(), LastHitDir * 150.f);
	}
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
		// (a arma ja caiu da mao ao morrer)
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
		// Duas armas: mosquete + uma de corpo a corpo (padrao sabre). -EAWArma=Faca|Machado|Sabre comeca com ela na mao.
		const EEAWWeapon Melee = Arma == TEXT("Faca") ? EEAWWeapon::Faca : Arma == TEXT("Machado") ? EEAWWeapon::Machado : EEAWWeapon::Sabre;
		if (Arma.IsEmpty()) { EquipWeapon(Melee); EquipWeapon(EEAWWeapon::Mosquete); }
		else { EquipWeapon(EEAWWeapon::Mosquete); EquipWeapon(Melee); }
		if (CurrentWeapon == EEAWWeapon::Mosquete && FParse::Param(FCommandLine::Get(), TEXT("EAWBaioneta")))
		{
			bBayonet = true;   // teste: comeca no modo combate (como se tivesse apertado X)
			OnRep_Weapon();
		}
	}
	else
	{
		// Bots lutam corpo a corpo; teste: -EAWArmaBot=Mosquete (baioneta), Faca, Machado ou Misto (cada bot uma).
		FString Arma;
		FParse::Value(FCommandLine::Get(), TEXT("EAWArmaBot="), Arma);
		static int32 BotCount = 0;
		if (Arma == TEXT("Misto"))
		{
			static const TCHAR* Ordem[] = { TEXT("Machado"), TEXT("Faca"), TEXT("Mosquete"), TEXT("Sabre") };
			Arma = Ordem[BotCount % 4];
		}
		++BotCount;
		const EEAWWeapon W = Arma == TEXT("Mosquete") ? EEAWWeapon::Mosquete : Arma == TEXT("Faca") ? EEAWWeapon::Faca
			: Arma == TEXT("Machado") ? EEAWWeapon::Machado : EEAWWeapon::Sabre;
		EquipWeapon(EEAWWeapon::Mosquete);   // todo soldado tem o mosquete
		EquipWeapon(W);
		if (W == EEAWWeapon::Mosquete)
		{
			bBayonet = true;
			OnRep_Weapon();
		}
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
	// A arma em uso ocupa a vaga dela: fogo (mosquete/pistola) ou corpo a corpo (sabre/faca/machado).
	if (IsFirearmWeapon(NewWeapon)) PrimaryWeapon = NewWeapon; else SecondaryWeapon = NewWeapon;
	Combat->ResetCombat();
	bBayonet = false;
	OnRep_Weapon();
	UE_LOG(LogEAWCombat, Log, TEXT("%s arma: %s"), *GetName(), NewWeapon == EEAWWeapon::Mosquete ? TEXT("mosquete") : NewWeapon == EEAWWeapon::Faca ? TEXT("faca") : NewWeapon == EEAWWeapon::Machado ? TEXT("machado") : TEXT("sabre"));
}

void AEAWCharacter::OnRep_Weapon()
{
	if (CurrentWeapon != LastShownWeapon)
	{
		if (IsLocallyControlled()) SwitchDip = 1.f;   // a nova arma sobe de baixo da tela
		LastShownWeapon = CurrentWeapon;
	}
	ApplyWeaponVisuals();
}

void AEAWCharacter::ApplyWeaponVisuals()
{
	USkeletalMesh* M = CurrentWeapon == EEAWWeapon::Pistola ? nullptr : CurrentWeapon == EEAWWeapon::Mosquete ? MeshMosquete : CurrentWeapon == EEAWWeapon::Faca ? MeshFaca
		: CurrentWeapon == EEAWWeapon::Machado ? MeshMachado : MeshSabre;
	ApplyWeaponCombatConfig();
	if (M && WeaponTP->SkeletalMesh != M)
	{
		WeaponTP->SetSkeletalMesh(M);
		WeaponFP->SetSkeletalMesh(M);
		if (WeaponTP->GetAttachParent() == GetMesh())
		{
			WeaponTP->SetMasterPoseComponent(GetMesh(), true);   // forca: trocar a malha exige refazer o mapa de ossos
		}
		WeaponFP->SetMasterPoseComponent(FirstPersonMesh, true);
	}
	const bool bDown = GetMesh()->IsSimulatingPhysics();
	WeaponFP->SetVisibility(bFirstPersonActive && !bDown);
	if (UEAWAnimInstance* AnimLayers = Cast<UEAWAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimLayers->bAllowUpperRun = CurrentWeapon != EEAWWeapon::Mosquete;   // correr com a arma curta no ombro
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
	WeaponTP->SetMasterPoseComponent(GetMesh(), true);   // forca: trocar a malha exige refazer o mapa de ossos
	WeaponTP->SetOwnerNoSee(bFirstPersonActive);
	WeaponTP->SetVisibility(true);
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
	if (!bFirstPersonActive || !IsLocallyControlled() || GetMesh()->IsSimulatingPhysics() || Dt <= 0.f)
	{
		return;
	}
	const EFirearmState S = Firearm->GetState();
	const bool bUp = UsingMusketFire() && Wounds->CanFight()
		&& (S == EFirearmState::Raising || S == EFirearmState::Aiming || S == EFirearmState::Firing);
	const FFirearmConfig& C = Firearm->Config;
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bUp ? 1.f : 0.f, Dt, 1.f / (bUp ? C.AimInTime : C.AimOutTime));
	const float A = AimAlpha * AimAlpha * (3.f - 2.f * AimAlpha);
	const float Free = 1.f - 0.85f * A;   // mirando, quase sem balanco do passo/atraso (o balanco da mira e outro)

	// --- Arma com inercia: atrasa o giro da camera (Chivalry: ~1-2 graus a 100 graus/s, ~70 ms) ---
	const FRotator Ctrl = GetControlRotation();
	const FRotator dR = (Ctrl - PrevControlRot).GetNormalized();
	PrevControlRot = Ctrl;
	const float YawRate = dR.Yaw / Dt, PitchRate = dR.Pitch / Dt;
	const FRotator LagTarget(FMath::Clamp(-PitchRate * 0.012f, -3.f, 3.f), FMath::Clamp(-YawRate * 0.014f, -4.f, 4.f),
		FMath::Clamp(-YawRate * 0.008f, -3.f, 3.f));
	ViewLag = FMath::RInterpTo(ViewLag, LagTarget * Free, Dt, 9.f);

	// --- Passo: balanco da arma e da camera (andar ~1,5 Hz, trote ~2,2 Hz, correr ~3 Hz), inclina de lado ---
	const FVector LocalVel = GetActorTransform().InverseTransformVectorNoScale(GetVelocity());
	const float Speed = GetCharacterMovement()->IsMovingOnGround() ? GetVelocity().Size2D() : 0.f;
	const float Amp = FMath::Clamp(Speed / 300.f, 0.f, 1.4f);
	BobPhase = FMath::Fmod(BobPhase + Dt * 2.f * PI * (1.f + Speed / 250.f), 2.f * PI);
	ViewBob = FVector(0.f, 0.9f * Amp * FMath::Sin(BobPhase), -0.6f * Amp * FMath::Abs(FMath::Sin(BobPhase))) * Free;
	const FVector CamRight = FRotationMatrix(FRotator(0.f, Ctrl.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	StrafeRoll = FMath::FInterpTo(StrafeRoll, FMath::Clamp(FVector::DotProduct(GetVelocity(), CamRight) / 300.f, -1.f, 1.f) * 1.5f, Dt, 6.f);
	CamBob = FRotator(0.5f * Amp * FMath::Sin(2.f * BobPhase), 0.f, 0.35f * Amp * FMath::Sin(BobPhase) + StrafeRoll) * (1.f - 0.6f * A);

	// --- Aterrissagem: a camera afunda e volta (mola) ---
	LandDipVel += (-LandDip * 120.f - LandDipVel * 18.f) * Dt;
	LandDip += LandDipVel * Dt;

	// Rosto na coronha: a camera vai ao olho direito; o foco fecha o campo de visao.
	Camera->SetRelativeLocation(AimEyeOffset * A + FVector(0.f, 0.f, LandDip));
	const float BaseFov = CurrentWeapon == EEAWWeapon::Mosquete ? FPFieldOfView : FPFieldOfView;
	Camera->SetFieldOfView(FMath::Lerp(BaseFov, AimFieldOfView, A));

	// Balanco: respiracao + cansaco do braco (cresce segurando a mira) + andar.
	AimHoldTime = (S == EFirearmState::Aiming) ? AimHoldTime + Dt : 0.f;
	const float T = GetWorld()->GetTimeSeconds();
	const float Moving = FMath::Min(1.f, GetVelocity().Size2D() / WalkSpeed);
	const float SwayAmp = (0.35f + FMath::Min(1.2f, 0.08f * AimHoldTime) + 0.8f * Moving) * A;
	SwayRot = FRotator(
		SwayAmp * (0.7f * FMath::PerlinNoise1D(T * 0.55f + 20.f) + 0.3f * FMath::Sin(2.f * PI * 0.25f * T)),
		SwayAmp * FMath::PerlinNoise1D(T * 0.45f), 0.f);
	// Os bracos e a arma giram em volta do olho (a linha do cano continua passando pelo olho);
	// por cima, o atraso do giro e o balanco do passo.
	const FVector Eye = AimEyeOffset * A;
	SwitchDip = FMath::Max(0.f, SwitchDip - Dt / 0.4f);
	const float Dip = FMath::Sin(SwitchDip * PI * 0.5f);   // cai rapido, sobe suave
	FirstPersonMesh->SetRelativeTransform(FPBase * FTransform(ViewLag + FRotator(-25.f * Dip, 0.f, 0.f), ViewBob + FVector(0.f, 0.f, -35.f * Dip))
		* FTransform(-Eye) * FTransform(SwayRot) * FTransform(Eye));

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

// ---------------------------------------------------------------------------
// Giro do corpo: a camera gira na hora; o corpo acompanha com velocidade propria.
// Parado e sem lutar, os pes so viram quando a mira passa de TurnInPlaceAngle (o tronco torce ate la,
// ver EAWAnimInstance); andando, lutando ou mirando, o corpo segue a mira a 540 graus/s.
// ---------------------------------------------------------------------------

void AEAWCharacter::UpdateBodyTurn(float Dt)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move || !Controller || !Wounds->CanFight())
	{
		if (Move) Move->bUseControllerDesiredRotation = false;
		return;
	}
	const float Delta = FRotator::NormalizeAxis(GetControlRotation().Yaw - GetActorRotation().Yaw);
	const bool bMoving = GetVelocity().Size2D() > 20.f;
	const bool bBusy = Combat->GetState() != EMeleeState::Idle
		|| (CurrentWeapon == EEAWWeapon::Mosquete && Firearm->GetState() != EFirearmState::Lowered && Firearm->GetState() != EFirearmState::Reloading);
	if (!bMoving && !bBusy)
	{
		if (FMath::Abs(Delta) > TurnInPlaceAngle) bTurnInPlace = true;
		else if (FMath::Abs(Delta) < 4.f) bTurnInPlace = false;
	}
	else
	{
		bTurnInPlace = false;
	}
	const bool bFollow = bMoving || bBusy || bTurnInPlace;
	Move->bUseControllerDesiredRotation = bFollow;
	// Virar no lugar e mais calmo (passinhos); andando/lutando, rapido.
	Move->RotationRate = FRotator(0.f, bTurnInPlace ? 320.f : 540.f, 0.f);
}

void AEAWCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	// Aterrissagem pesada: camera afunda proporcional a velocidade de queda (mola em UpdateFirearmView).
	const float Fall = -GetCharacterMovement()->Velocity.Z;
	LandDipVel -= FMath::Clamp(Fall * 0.12f, 0.f, 90.f);
	AddTrauma(FMath::Clamp((Fall - 300.f) / 900.f, 0.f, 0.5f));
}

// ---------------------------------------------------------------------------
// Configuracao de combate de cada arma (valores iniciais de teste; ajustar jogando)
// ---------------------------------------------------------------------------

void AEAWCharacter::ApplyWeaponCombatConfig()
{
	FMeleeWeaponConfig C;   // comeca com os valores do sabre
	auto Scale = [&C](EAttackDirection Dir, float Time, float Damage, float Stamina)
	{
		if (FMeleeAttackSpec* S = C.Attacks.Find(Dir))
		{
			S->Windup *= Time; S->ComboWindup *= Time; S->RiposteWindup *= Time; S->Release *= Time; S->Recovery *= Time;
			S->BaseDamage = Damage; S->StaminaCost = Stamina;
		}
	};
	Combat->GripSocket = TEXT("weapon_r");
	Combat->TipSocket = TEXT("weapon_r_tip");
	Combat->AnimBladeStart = 7.5f;
	Combat->AnimBladeLength = 0.f;
	switch (CurrentWeapon)
	{
	case EEAWWeapon::Faca:
		// Faca: rapida, curta (20 cm), fere menos no corte e muito na estocada.
		C.WeaponName = TEXT("Faca");
		Scale(EAttackDirection::SlashLeft, 0.7f, 28.f, 7.f);
		Scale(EAttackDirection::SlashRight, 0.7f, 28.f, 7.f);
		Scale(EAttackDirection::Overhead, 0.75f, 32.f, 8.f);
		Scale(EAttackDirection::Stab, 0.7f, 48.f, 7.f);
		C.ParryWindow = 0.26f;
		Combat->AnimBladeStart = 2.f;
		Combat->AnimBladeLength = 20.f;
		break;
	case EEAWWeapon::Machado:
		// Machado de sapador: lento e pesado; o que corta e a cabeca (ultimos 16 cm do cabo de 70 cm).
		C.WeaponName = TEXT("Machado");
		Scale(EAttackDirection::SlashLeft, 1.2f, 55.f, 14.f);
		Scale(EAttackDirection::SlashRight, 1.2f, 55.f, 14.f);
		Scale(EAttackDirection::Overhead, 1.2f, 72.f, 16.f);
		Scale(EAttackDirection::Stab, 1.1f, 30.f, 10.f);
		C.StunWhenParried = 0.7f;
		Combat->AnimBladeStart = 46.f;
		Combat->AnimBladeLength = 18.f;
		break;
	case EEAWWeapon::Mosquete:
		// Mosquete com baioneta (tecla X): coronhada atordoa, estocadas tem alcance de ~1,9 e ~2,1 m.
		C.WeaponName = TEXT("Mosquete");
		Scale(EAttackDirection::SlashLeft, 1.25f, 38.f, 14.f);    // coronhada
		Scale(EAttackDirection::SlashRight, 1.25f, 38.f, 14.f);
		Scale(EAttackDirection::Overhead, 1.25f, 62.f, 18.f);     // estocada longa
		Scale(EAttackDirection::Stab, 1.15f, 55.f, 12.f);         // estocada
		C.StunWhenParried = 0.65f;
		C.ParryWindow = 0.32f;
		Combat->GripSocket = TEXT("arma_boca");
		Combat->TipSocket = TEXT("arma_ponta");
		Combat->AnimBladeStart = 0.f;
		break;
	default:
		break;
	}
	Combat->DefaultConfig = C;
}

// ---------------------------------------------------------------------------
// Pegar arma do chao: olhar para ela perto (ate 2,2 m), segurar F por 3 s. A nova ocupa a vaga
// do mesmo tipo (fogo ou corpo a corpo) e a antiga cai no chao.
// ---------------------------------------------------------------------------

static const float PickupRange = 220.f;
static const float PickupTime = 3.f;

void AEAWCharacter::UpdatePickup(float Dt)
{
	if (!IsLocallyControlled() || !IsPlayerControlled() || !Wounds->CanFight())
	{
		PickupTarget = nullptr;
		bPickupHeld = false;
		return;
	}
	// A arma no chao mais perto do centro da visao.
	const FVector Eye = Camera->GetComponentLocation();
	const FVector View = Camera->GetForwardVector();
	AEAWDroppedWeapon* Best = nullptr;
	float BestScore = -1.f;
	for (TActorIterator<AEAWDroppedWeapon> It(GetWorld()); It; ++It)
	{
		const FVector To = It->GetActorLocation() - Eye;
		const float Dist = To.Size();
		if (Dist > PickupRange)
		{
			continue;
		}
		const float Dot = FVector::DotProduct(To / FMath::Max(1.f, Dist), View);
		if (Dot < 0.55f && Dist > 110.f)
		{
			continue;
		}
		const float Score = Dot - Dist / 1000.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = *It;
		}
	}
	PickupTarget = Best;
	if (bPickupHeld)
	{
		if (!PickupHeldTarget.IsValid() || PickupHeldTarget.Get() != Best)
		{
			InputInteractReleased();   // saiu de perto ou olhou para outra: cancela
			return;
		}
		PickupProgress = FMath::Min(1.f, PickupProgress + Dt / PickupTime);
	}
}

void AEAWCharacter::InputInteractPressed()
{
	if (!PickupTarget.IsValid() || !Wounds->CanFight())
	{
		return;
	}
	bPickupHeld = true;
	PickupProgress = 0.f;
	PickupHeldTarget = PickupTarget;
	ServerStartPickup(PickupTarget.Get());
}

void AEAWCharacter::InputInteractReleased()
{
	if (bPickupHeld)
	{
		bPickupHeld = false;
		PickupProgress = 0.f;
		PickupHeldTarget = nullptr;
		ServerCancelPickup();
	}
}

void AEAWCharacter::ServerStartPickup_Implementation(AEAWDroppedWeapon* Target)
{
	if (!Target || FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > PickupRange + 120.f)
	{
		return;
	}
	ServerPickupTarget = Target;
	GetWorldTimerManager().SetTimer(PickupTimer, this, &AEAWCharacter::CompletePickup, PickupTime, false);
}

void AEAWCharacter::ServerCancelPickup_Implementation()
{
	ServerPickupTarget = nullptr;
	GetWorldTimerManager().ClearTimer(PickupTimer);
}

void AEAWCharacter::CompletePickup()
{
	AEAWDroppedWeapon* Target = ServerPickupTarget.Get();
	ServerPickupTarget = nullptr;
	if (!Target || !Wounds->CanFight() || FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > PickupRange + 120.f)
	{
		return;
	}
	const EEAWWeapon New = Target->GetWeapon();
	const bool bNewLoaded = Target->IsLoaded();
	const EEAWWeapon Old = IsFirearmWeapon(New) ? PrimaryWeapon : SecondaryWeapon;
	// A antiga cai no chao (o mosquete leva junto se estava carregado).
	const bool bOldLoaded = Old != EEAWWeapon::Mosquete || Firearm->IsLoaded();
	Target->Destroy();
	DropWeapon(Old, bOldLoaded, GetActorForwardVector() * 80.f);
	EquipWeapon(New);
	if (New == EEAWWeapon::Mosquete)
	{
		Firearm->SetLoaded(bNewLoaded);
	}
	UE_LOG(LogEAWCombat, Log, TEXT("%s pegou %s e largou %s"), *GetName(), *AEAWDroppedWeapon::WeaponName(New).ToString(),
		*AEAWDroppedWeapon::WeaponName(Old).ToString());
}

void AEAWCharacter::DropWeapon(EEAWWeapon W, bool bLoaded, const FVector& ExtraVelocity)
{
	if (!HasAuthority())
	{
		return;
	}
	// Sai da mao, na posicao do osso da arma (o mosquete pelo osso proprio; as outras pela mao).
	const FName Bone = W == EEAWWeapon::Mosquete && W == CurrentWeapon ? FName(TEXT("arma")) : FName(TEXT("weapon_r"));
	FTransform At = GetMesh()->GetSocketTransform(Bone);
	At.SetScale3D(FVector::OneVector);
	AEAWDroppedWeapon::Drop(GetWorld(), W, At, GetVelocity() + ExtraVelocity + FVector(0.f, 0.f, 60.f), bLoaded);
}
