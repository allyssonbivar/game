#include "Animation/EAWAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "EAWCharacter.h"
#include "Combat/MeleeCombatComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** Mola critica (solucao exata): X segue Target sem passar e sem tranco no comeco nem no fim. Omega maior = mais rapida
	 *  (chega a ~95% em ~4,7/Omega s). Substitui o FInterpTo, que comeca na velocidade maxima (o "pulo" medido ao partir/parar). */
	void SpringTo(float& X, float& V, float Target, float Omega, float Dt)
	{
		const float E = FMath::Exp(-Omega * Dt);
		const float Change = X - Target;
		const float Temp = (V + Omega * Change) * Dt;
		V = (V - Omega * Temp) * E;
		X = Target + (Change + Temp) * E;
	}

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
	bool bCrouched = false;
	if (Pawn)
	{
		const FVector V = Pawn->GetVelocity();
		Local = Pawn->GetActorTransform().InverseTransformVectorNoScale(FVector(V.X, V.Y, 0.f));
		if (const AEAWCharacter* Soldier = Cast<AEAWCharacter>(Pawn))
		{
			bCombatIdle = Soldier->GetCombat() && Soldier->GetCombat()->GetState() == EMeleeState::Idle;
			bCrouched = Soldier->bIsCrouched;
		}
	}
	float Speed = Local.Size2D();

	// Torcao do tronco: a mira esta a frente do corpo (o corpo vira atras, ver AEAWCharacter::UpdateBodyTurn).
	float YawRate = 0.f;
	if (Pawn)
	{
		const float ActorYaw = Pawn->GetActorRotation().Yaw;
		const float AimWorldYaw = Pawn->Controller ? Pawn->GetControlRotation().Yaw : Pawn->GetBaseAimRotation().Yaw;
		// O tronco segue a mira com uma mola curta (~0,13 s): quando o mouse para ou comeca, a torcao nao inverte num quadro.
		const float AimTarget = FMath::Clamp(FRotator::NormalizeAxis(AimWorldYaw - ActorYaw), -90.f, 90.f);
		SpringTo(AimYawSmooth, AimYawVel, AimTarget, 35.f, Dt);   // 35: atraso pequeno (6 graus a 100 graus/s) - o golpe segue a mira
		AimYaw = FMath::Clamp(AimYawSmooth, -90.f, 90.f);
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

	// 8 direcoes por angulo (0 = frente, 90 = direita): mistura so as duas vizinhas, com troca suave.
	// Antes eram 4 direcoes somadas por componente: a diagonal misturava frente + lado e o pe escorregava ~1,5 m/s.
	const bool bStarting = LocoWeight < 0.02f && Speed > 5.f;
	if (Speed > 5.f)
	{
		float Ang = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		if (Ang < 0.f) Ang += 360.f;
		const int32 I0 = FMath::FloorToInt(Ang / 45.f) % 8;
		const int32 I1 = (I0 + 1) % 8;
		const float Frac = Ang / 45.f - FMath::FloorToFloat(Ang / 45.f);
		float W[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		W[I0] = 1.f - Frac; W[I1] += Frac;
		for (int32 i = 0; i < 8; ++i)
		{
			// Partindo do parado: ja comeca na direcao certa (nao "gira" as pernas a partir da frente).
			if (bStarting) { DirW[i] = W[i]; DirWVel[i] = 0.f; }
			else { SpringTo(DirW[i], DirWVel[i], W[i], 11.f, Dt); DirW[i] = FMath::Max(0.f, DirW[i]); }
		}
	}
	// Sem a animacao da diagonal, o peso vai metade para cada vizinha.
	float W8[8];
	for (int32 i = 0; i < 8; ++i) W8[i] = DirW[i];
	const int32 DiagIdx[4] = { 1, 7, 3, 5 };   // FD, FE, TD, TE
	for (int32 k = 0; k < 4; ++k)
	{
		if (!WalkDiag[k])
		{
			const int32 i = DiagIdx[k];
			W8[(i + 7) % 8] += W8[i] * 0.5f; W8[(i + 1) % 8] += W8[i] * 0.5f; W8[i] = 0.f;
		}
	}

	// Passo sincronizado: a fase anda na proporcao da velocidade real / velocidade da animacao.
	const float Speed8[8] = { SpeedForward, SpeedDiagFwd, SpeedSide, SpeedDiagBack, SpeedBack, SpeedDiagBack, SpeedSide, SpeedDiagFwd };
	float Natural = 0.f;
	for (int32 i = 0; i < 8; ++i) Natural += W8[i] * Speed8[i];
	Natural = FMath::Max(1.f, Natural);
	if (bStarting)
	{
		// Comeca o passo com o pe direito a frente, como na guarda (sem trocar os pes no primeiro quadro).
		WalkPhase = 0.5f;
	}
	WalkPhase = FMath::Fmod(WalkPhase + Dt * (Speed / Natural) / WalkCycle, 1.f);
	RunPhase = FMath::Fmod(RunPhase + Dt * (Speed / SpeedRun) / RunCycle, 1.f);

	SpringTo(LocoWeight, LocoWeightVel, FMath::Clamp(Speed / 40.f, 0.f, 1.f), 13.f, Dt);
	LocoWeight = FMath::Clamp(LocoWeight, 0.f, 1.f);
	// Agachado: desce/sobe em 0,25 s; o passo agachado sincroniza com a velocidade como o andar.
	CrouchWeight = FMath::FInterpConstantTo(CrouchWeight, (bCrouched && CrouchIdle) ? 1.f : 0.f, Dt, 4.f);
	// Agachado tem 4 direcoes: F, T, E, D (as diagonais se dividem entre as vizinhas).
	const float CW[4] = { DirW[0] + 0.5f * (DirW[1] + DirW[7]), DirW[4] + 0.5f * (DirW[3] + DirW[5]),
		DirW[6] + 0.5f * (DirW[5] + DirW[7]), DirW[2] + 0.5f * (DirW[1] + DirW[3]) };
	const float CrouchNatural = FMath::Max(1.f, CW[0] * CrouchSpeedForward + CW[1] * CrouchSpeedBack + (CW[2] + CW[3]) * CrouchSpeedSide);
	CrouchPhase = FMath::Fmod(CrouchPhase + Dt * (Speed / CrouchNatural) / CrouchCycle, 1.f);
	CrouchIdleTime += Dt;
	if (CrouchIdle && CrouchIdle->SequenceLength > 0.f)
	{
		CrouchIdleTime = FMath::Fmod(CrouchIdleTime, CrouchIdle->SequenceLength);
	}
	// Correr so para a frente (de lado e para tras usa o trote daquela direcao).
	RunWeight = FMath::FInterpTo(RunWeight, FMath::Clamp((Local.X - 330.f) / 120.f, 0.f, 1.f), Dt, 8.f);
	UpperRunWeight = FMath::FInterpTo(UpperRunWeight, (bCombatIdle && bAllowUpperRun && !bCrouched) ? RunWeight : 0.f, Dt, 12.f);

	Out.Combat = Seg.Anim;
	Out.CombatTime = SegmentTime(Seg, SegElapsed);
	Out.Prev = PrevBlend < 1.f ? PrevSeg.Anim : nullptr;
	Out.PrevTime = PrevSegTime;
	Out.PrevBlend = PrevBlend;
	Out.Loco[0] = WalkForward; Out.Loco[1] = WalkBack; Out.Loco[2] = WalkLeft; Out.Loco[3] = WalkRight; Out.Loco[4] = Run;
	Out.LocoW[0] = W8[0]; Out.LocoW[1] = W8[4]; Out.LocoW[2] = W8[6]; Out.LocoW[3] = W8[2];
	for (int32 k = 0; k < 4; ++k)
	{
		Out.Diag[k] = WalkDiag[k];
		Out.DiagW[k] = W8[DiagIdx[k]];
		Out.CrouchW[k] = CW[k];
	}
	Out.WalkPhase01 = WalkPhase;
	Out.WalkTime = WalkPhase * WalkCycle;
	Out.RunTime = RunPhase * RunCycle;
	Out.LocoWeight = LocoWeight;
	Out.RunWeight = RunWeight;
	Out.UpperRunWeight = UpperRunWeight;
	Out.AimYaw = AimYaw;
	Out.CrouchIdle = CrouchIdle;
	for (int32 i = 0; i < 4; ++i)
	{
		Out.CrouchWalk[i] = CrouchWalk[i];
	}
	Out.CrouchIdleTime = CrouchIdleTime;
	Out.CrouchTime = CrouchPhase * CrouchCycle;
	Out.CrouchWeight = CrouchWeight;
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
		const int32 Pist = Ref.FindBoneIndex(TEXT("pistola"));   // idem para a pistola
		UpperMask.SetNumZeroed(Out.GetNumBones());
		for (FCompactPoseBoneIndex I : Out.ForEachBoneIndex())
		{
			const int32 MeshIndex = Bones.MakeMeshPoseIndex(I).GetInt();
			const bool bUpper = Spine != INDEX_NONE && (MeshIndex == Spine || Ref.BoneIsChildOf(MeshIndex, Spine));
			const bool bArma = Arma != INDEX_NONE && (MeshIndex == Arma || Ref.BoneIsChildOf(MeshIndex, Arma));
			const bool bPist = Pist != INDEX_NONE && (MeshIndex == Pist || Ref.BoneIsChildOf(MeshIndex, Pist));
			UpperMask[I.GetInt()] = (bUpper || bArma || bPist) ? 1 : 0;
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
		for (int32 i = 0; i < 8; ++i)
		{
			UAnimSequence* A = i < 4 ? Data.Loco[i] : Data.Diag[i - 4];
			const float W = i < 4 ? Data.LocoW[i] : Data.DiagW[i - 4];
			if (W < 0.01f || !A)
			{
				continue;
			}
			const float Time = Data.WalkPhase01 * A->SequenceLength;
			if (bFirst)
			{
				SamplePose(A, Time, LocoPose);
				bFirst = false;
				Acc = W;
				continue;
			}
			FPoseContext P(this);
			SamplePose(A, Time, P);
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
	const FReferenceSkeleton& RefSk = Bones.GetReferenceSkeleton();
	const FCompactPoseBoneIndex Pelvis = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(RefSk.FindBoneIndex(TEXT("pelvis"))));

	// Agachado: pernas e altura do quadril da animacao agachada; a rotacao do quadril continua a da arma (a mira nao muda).
	if (Data.CrouchWeight > 0.01f && Data.CrouchIdle)
	{
		FPoseContext CrouchPose(this);
		SamplePose(Data.CrouchIdle, Data.CrouchIdleTime, CrouchPose);
		if (Data.LocoWeight > 0.01f)
		{
			FPoseContext Walk(this);
			float Acc = 0.f;
			for (int32 i = 0; i < 4; ++i)
			{
				const float W = Data.CrouchW[i];
				if (W < 0.01f || !Data.CrouchWalk[i]) continue;
				if (Acc <= 0.f)
				{
					SamplePose(Data.CrouchWalk[i], Data.CrouchTime, Walk);
					Acc = W;
					continue;
				}
				FPoseContext P(this);
				SamplePose(Data.CrouchWalk[i], Data.CrouchTime, P);
				Acc += W;
				LerpPose(Walk.Pose, P.Pose, W / Acc);
			}
			if (Acc > 0.f)
			{
				LerpPose(CrouchPose.Pose, Walk.Pose, Data.LocoWeight);
			}
		}
		for (FCompactPoseBoneIndex I : Out.ForEachBoneIndex())
		{
			if (UpperMask[I.GetInt()])
			{
				continue;
			}
			FTransform C = CrouchPose.Pose[I];
			if (I == Pelvis)
			{
				C.SetRotation(Out[I].GetRotation());
			}
			FTransform R;
			R.Blend(Out[I], C, FMath::Min(Data.CrouchWeight, 1.f));
			Out[I] = R;
		}
	}
	Out.NormalizeRotations();

	// Ossos das armas longas (mosquete e pistola) sao filhos da raiz, nao do corpo: quando o quadril final difere do da
	// animacao da arma (andando ou agachado), a arma acompanha o quadril - senao as maos (que seguem o corpo) saem dela.
	if (Pelvis.GetInt() != INDEX_NONE)
	{
		FCSPose<FCompactPose> CombatCS;
		CombatCS.InitPose(CombatPose.Pose);
		FCSPose<FCompactPose> FinalCS;
		FinalCS.InitPose(Out);
		const FTransform PC = CombatCS.GetComponentSpaceTransform(Pelvis);
		const FTransform PF = FinalCS.GetComponentSpaceTransform(Pelvis);
		if (!PC.Equals(PF, 0.05f))
		{
			const FTransform Delta = PC.Inverse() * PF;
			for (const TCHAR* Name : { TEXT("arma"), TEXT("pistola") })
			{
				const int32 MeshIdx = RefSk.FindBoneIndex(Name);
				if (MeshIdx == INDEX_NONE) continue;
				const FCompactPoseBoneIndex B = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIdx));
				if (B.GetInt() == INDEX_NONE) continue;
				const FTransform NewCS = CombatCS.GetComponentSpaceTransform(B) * Delta;
				Out[B] = NewCS.GetRelativeTransform(FinalCS.GetComponentSpaceTransform(Out.GetParentBoneIndex(B)));
			}
		}
	}

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
			// Ossos das armas presos a raiz (mosquete e pistola) giram junto, em volta do mesmo ponto.
			TArray<TPair<FCompactPoseBoneIndex, FTransform>> Extra;
			for (const int32 MeshIdx : { ArmaMesh, Ref.FindBoneIndex(TEXT("pistola")) })
			{
				if (MeshIdx == INDEX_NONE) continue;
				const FCompactPoseBoneIndex B = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIdx));
				if (B.GetInt() == INDEX_NONE) continue;
				const FTransform BCS = CS.GetComponentSpaceTransform(B);
				const FVector P = SpineCS.GetLocation();
				FTransform NewB = BCS;
				NewB.SetLocation(P + Qz.RotateVector(BCS.GetLocation() - P));
				NewB.SetRotation(Qz * BCS.GetRotation());
				Extra.Add(TPair<FCompactPoseBoneIndex, FTransform>(B, NewB.GetRelativeTransform(CS.GetComponentSpaceTransform(Out.GetParentBoneIndex(B)))));
			}
			Out[Spine] = NewSpine.GetRelativeTransform(ParentCS);
			for (const TPair<FCompactPoseBoneIndex, FTransform>& E : Extra)
			{
				Out[E.Key] = E.Value;
			}
		}
	}
	return true;
}
