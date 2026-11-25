#include "Animation/EAWAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "EAWCharacter.h"
#include "Combat/MeleeCombatComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	void SamplePose(UAnimSequence* Anim, float Time, FPoseContext& Ctx)
	{
		if (Anim)
		{
			// UE 4.26+: pose, curvas e atributos vao juntos.
			FAnimationPoseData PoseData(Ctx);
			Anim->GetAnimationPose(PoseData, FAnimExtractContext(Time, false));
		}
		else
		{
			Ctx.Pose.ResetToRefPose();
		}
	}

	/** Acc = lerp(Acc, Other, Alpha), osso a osso. */
	void LerpPose(FCompactPose& Acc, const FCompactPose& Other, float Alpha)
	{
		if (Alpha <= 0.f)
		{
			return;
		}
		for (FCompactPoseBoneIndex I : Acc.ForEachBoneIndex())
		{
			FTransform R;
			R.Blend(Acc[I], Other[I], FMath::Min(Alpha, 1.f));
			Acc[I] = R;
		}
	}
}

// ---------------------------------------------------------------------------
// Thread do jogo
// ---------------------------------------------------------------------------

float UEAWAnimInstance::SegmentTime(const FEAWSegment& S, float Elapsed) const
{
	if (!S.Anim)
	{
		return 0.f;
	}
	const float Len = FMath::Max(S.End - S.Start, KINDA_SMALL_NUMBER);
	float T = Elapsed * S.Rate;
	T = S.bLoop ? FMath::Fmod(T, Len) : FMath::Min(T, Len);
	return S.Start + T;
}

void UEAWAnimInstance::SetCombatSegment(UAnimSequence* Anim, float Start, float End, float Duration, bool bLoop)
{
	if (Seg.Anim)
	{
		// O trecho atual congela onde esta e some em 0,1 s (sem "pulo" de pose).
		PrevSeg = Seg;
		PrevSegTime = SegmentTime(Seg, SegElapsed);
		PrevBlend = 0.f;
	}
	Seg.Anim = Anim;
	Seg.Start = Start;
	Seg.End = End;
	Seg.bLoop = bLoop;
	Seg.Rate = Duration > KINDA_SMALL_NUMBER ? (End - Start) / Duration : 1.f;
	SegElapsed = 0.f;
}

void UEAWAnimInstance::TickValues(float Dt, FEAWAnimFrame& Out)
{
	SegElapsed += Dt;
	PrevBlend = FMath::Min(1.f, PrevBlend + Dt / 0.1f);

	// Velocidade no referencial do personagem.
	const APawn* Pawn = TryGetPawnOwner();
	FVector Local = FVector::ZeroVector;
	bool bCombatIdle = true;
	if (Pawn)
	{
		const FVector V = Pawn->GetVelocity();
		Local = Pawn->GetActorTransform().InverseTransformVectorNoScale(FVector(V.X, V.Y, 0.f));
		if (const AEAWCharacter* Soldier = Cast<AEAWCharacter>(Pawn))
		{
			bCombatIdle = Soldier->GetCombat() && Soldier->GetCombat()->GetState() == EMeleeState::Idle;
		}
	}
	float Speed = Local.Size2D();

	// Torcao do tronco: a mira esta a frente do corpo (o corpo vira atras, ver AEAWCharacter::UpdateBodyTurn).
	float YawRate = 0.f;
	if (Pawn)
	{
		const float ActorYaw = Pawn->GetActorRotation().Yaw;
		const float AimWorldYaw = Pawn->Controller ? Pawn->GetControlRotation().Yaw : Pawn->GetBaseAimRotation().Yaw;
		AimYaw = FMath::Clamp(FRotator::NormalizeAxis(AimWorldYaw - ActorYaw), -90.f, 90.f);
		if (bHavePrevYaw && Dt > 0.f)
		{
			YawRate = FRotator::NormalizeAxis(ActorYaw - PrevActorYaw) / Dt;
		}
		PrevActorYaw = ActorYaw;
		bHavePrevYaw = true;
	}
	// Virar no lugar: passinhos de lado (a velocidade do giro vira um passo virtual), em vez de patinar.
	if (Speed < 20.f && FMath::Abs(YawRate) > 40.f)
	{
		const float Step = FMath::Min(110.f, FMath::Abs(YawRate) * 0.45f);
		Local = FVector(0.f, YawRate > 0.f ? Step : -Step, 0.f);
		Speed = Step;
	}

	// Pesos das 4 direcoes (frente, tras, esquerda, direita).
	if (Speed > 5.f)
	{
		const FVector D = Local / Speed;
		float W[4] = { FMath::Max(0.f, D.X), FMath::Max(0.f, -D.X), FMath::Max(0.f, -D.Y), FMath::Max(0.f, D.Y) };
		const float Sum = W[0] + W[1] + W[2] + W[3];
		for (int32 i = 0; i < 4; ++i)
		{
			DirW[i] = FMath::FInterpTo(DirW[i], W[i] / Sum, Dt, 12.f);
		}
	}

	// Passo sincronizado: a fase anda na proporcao da velocidade real / velocidade da animacao.
	const float Natural = FMath::Max(1.f, DirW[0] * SpeedForward + DirW[1] * SpeedBack + (DirW[2] + DirW[3]) * SpeedSide);
	WalkPhase = FMath::Fmod(WalkPhase + Dt * (Speed / Natural) / WalkCycle, 1.f);
	RunPhase = FMath::Fmod(RunPhase + Dt * (Speed / SpeedRun) / RunCycle, 1.f);

	LocoWeight = FMath::FInterpTo(LocoWeight, FMath::Clamp(Speed / 40.f, 0.f, 1.f), Dt, 10.f);
	// Correr so para a frente (de lado e para tras usa o trote daquela direcao).
	RunWeight = FMath::FInterpTo(RunWeight, FMath::Clamp((Local.X - 330.f) / 120.f, 0.f, 1.f), Dt, 8.f);
	UpperRunWeight = FMath::FInterpTo(UpperRunWeight, (bCombatIdle && bAllowUpperRun) ? RunWeight : 0.f, Dt, 12.f);

	Out.Combat = Seg.Anim;
	Out.CombatTime = SegmentTime(Seg, SegElapsed);
	Out.Prev = PrevBlend < 1.f ? PrevSeg.Anim : nullptr;
	Out.PrevTime = PrevSegTime;
	Out.PrevBlend = PrevBlend;
	Out.Loco[0] = WalkForward; Out.Loco[1] = WalkBack; Out.Loco[2] = WalkLeft; Out.Loco[3] = WalkRight; Out.Loco[4] = Run;
	for (int32 i = 0; i < 4; ++i)
	{
		Out.LocoW[i] = DirW[i];
	}
	Out.WalkTime = WalkPhase * WalkCycle;
	Out.RunTime = RunPhase * RunCycle;
	Out.LocoWeight = LocoWeight;
	Out.RunWeight = RunWeight;
	Out.UpperRunWeight = UpperRunWeight;
	Out.AimYaw = AimYaw;
}

// ---------------------------------------------------------------------------
// Thread de animacao
// ---------------------------------------------------------------------------

void FEAWAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (UEAWAnimInstance* Inst = Cast<UEAWAnimInstance>(InAnimInstance))
	{
		Inst->TickValues(DeltaSeconds, Data);
	}
}

bool FEAWAnimProxy::Evaluate(FPoseContext& Output)
{
	FCompactPose& Out = Output.Pose;
	if (!Data.Combat)
	{
		Out.ResetToRefPose();
		return true;
	}

	// Mascara: spine_01 e todos os filhos = tronco/bracos.
	const FBoneContainer& Bones = Out.GetBoneContainer();
	if (UpperMask.Num() != Out.GetNumBones())
	{
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		const int32 Spine = Ref.FindBoneIndex(TEXT("spine_01"));
		const int32 Arma = Ref.FindBoneIndex(TEXT("arma"));   // ossos da arma longa: sempre da camada de combate
		UpperMask.SetNumZeroed(Out.GetNumBones());
		for (FCompactPoseBoneIndex I : Out.ForEachBoneIndex())
		{
			const int32 MeshIndex = Bones.MakeMeshPoseIndex(I).GetInt();
			const bool bUpper = Spine != INDEX_NONE && (MeshIndex == Spine || Ref.BoneIsChildOf(MeshIndex, Spine));
			const bool bArma = Arma != INDEX_NONE && (MeshIndex == Arma || Ref.BoneIsChildOf(MeshIndex, Arma));
			UpperMask[I.GetInt()] = (bUpper || bArma) ? 1 : 0;
		}
	}

	// Combate (com transicao a partir do trecho anterior).
	FPoseContext CombatPose(this);
	SamplePose(Data.Combat, Data.CombatTime, CombatPose);
	if (Data.Prev && Data.PrevBlend < 1.f)
	{
		FPoseContext PrevPose(this);
		SamplePose(Data.Prev, Data.PrevTime, PrevPose);
		LerpPose(PrevPose.Pose, CombatPose.Pose, Data.PrevBlend);
		CombatPose.Pose = PrevPose.Pose;
	}

	// Movimento: mistura das 4 direcoes + corrida.
	const bool bLoco = Data.LocoWeight > 0.01f && Data.Loco[0];
	FPoseContext LocoPose(this);
	if (bLoco)
	{
		float Acc = 0.f;
		bool bFirst = true;
		for (int32 i = 0; i < 4; ++i)
		{
			const float W = Data.LocoW[i];
			if (W < 0.01f || !Data.Loco[i])
			{
				continue;
			}
			if (bFirst)
			{
				SamplePose(Data.Loco[i], Data.WalkTime, LocoPose);
				bFirst = false;
				Acc = W;
				continue;
			}
			FPoseContext P(this);
			SamplePose(Data.Loco[i], Data.WalkTime, P);
			Acc += W;
			LerpPose(LocoPose.Pose, P.Pose, W / Acc);
		}
		if (Data.RunWeight > 0.01f && Data.Loco[4])
		{
			FPoseContext P(this);
			SamplePose(Data.Loco[4], Data.RunTime, P);
			LerpPose(LocoPose.Pose, P.Pose, Data.RunWeight);
		}
	}

	// Monta: pernas seguem o movimento; tronco segue o combate (ou a corrida, se correndo sem lutar).
	for (FCompactPoseBoneIndex I : Out.ForEachBoneIndex())
	{
		FTransform T = CombatPose.Pose[I];
		if (bLoco)
		{
			const float W = UpperMask[I.GetInt()] ? Data.UpperRunWeight : Data.LocoWeight;
			if (W > 0.f)
			{
				FTransform R;
				R.Blend(T, LocoPose.Pose[I], FMath::Min(W, 1.f));
				T = R;
			}
		}
		Out[I] = T;
	}
	Out.NormalizeRotations();

	// Torcao na cintura: tronco, bracos e a arma longa giram em volta do spine_01 (as pernas ficam).
	if (FMath::Abs(Data.AimYaw) > 0.5f)
	{
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		const FCompactPoseBoneIndex Spine = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Ref.FindBoneIndex(TEXT("spine_01"))));
		const int32 ArmaMesh = Ref.FindBoneIndex(TEXT("arma"));
		if (Spine.GetInt() != INDEX_NONE)
		{
			FCSPose<FCompactPose> CS;
			CS.InitPose(Out);
			const FQuat Qz(FVector::UpVector, FMath::DegreesToRadians(Data.AimYaw));
			const FTransform SpineCS = CS.GetComponentSpaceTransform(Spine);
			const FCompactPoseBoneIndex SpineParent = Out.GetParentBoneIndex(Spine);
			const FTransform ParentCS = CS.GetComponentSpaceTransform(SpineParent);
			FTransform NewSpine = SpineCS;
			NewSpine.SetRotation(Qz * SpineCS.GetRotation());
			FTransform ArmaLocal;
			FCompactPoseBoneIndex Arma(INDEX_NONE);
			if (ArmaMesh != INDEX_NONE)
			{
				Arma = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ArmaMesh));
			}
			if (Arma.GetInt() != INDEX_NONE)
			{
				const FTransform ArmaCS = CS.GetComponentSpaceTransform(Arma);
				const FVector P = SpineCS.GetLocation();
				FTransform NewArma = ArmaCS;
				NewArma.SetLocation(P + Qz.RotateVector(ArmaCS.GetLocation() - P));
				NewArma.SetRotation(Qz * ArmaCS.GetRotation());
				ArmaLocal = NewArma.GetRelativeTransform(CS.GetComponentSpaceTransform(Out.GetParentBoneIndex(Arma)));
			}
			Out[Spine] = NewSpine.GetRelativeTransform(ParentCS);
			if (Arma.GetInt() != INDEX_NONE)
			{
				Out[Arma] = ArmaLocal;
			}
		}
	}
	return true;
}
