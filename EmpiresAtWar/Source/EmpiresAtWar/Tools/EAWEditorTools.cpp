#include "Tools/EAWEditorTools.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

#if WITH_EDITOR
#include "PhysicsAssetUtils.h"
#endif

namespace
{
	/** Osso que recebe um corpo fisico: capsula do osso ate Toward (ou Extra cm na mesma direcao, se nao houver). */
	struct FEAWBodyDef
	{
		const TCHAR* Bone;
		const TCHAR* Toward;
		float Radius;
		float Extra;
	};

	// Medidas para o Y Bot (cm). O gerador automatico da Unreal so criava 4 corpos neste modelo (bug #41).
	const FEAWBodyDef GBodies[] = {
		{ TEXT("pelvis"),     TEXT("spine_01"),   14.f, 0.f },
		{ TEXT("spine_01"),   TEXT("spine_02"),   13.f, 0.f },
		{ TEXT("spine_02"),   TEXT("spine_03"),   14.f, 0.f },
		{ TEXT("spine_03"),   TEXT("neck_01"),    15.f, 0.f },
		{ TEXT("neck_01"),    TEXT("head"),        6.f, 0.f },
		{ TEXT("head"),       nullptr,             10.f, 18.f },
		{ TEXT("upperarm_l"), TEXT("lowerarm_l"),  6.f, 0.f },
		{ TEXT("lowerarm_l"), TEXT("hand_l"),      5.f, 0.f },
		{ TEXT("hand_l"),     nullptr,             4.f, 12.f },
		{ TEXT("upperarm_r"), TEXT("lowerarm_r"),  6.f, 0.f },
		{ TEXT("lowerarm_r"), TEXT("hand_r"),      5.f, 0.f },
		{ TEXT("hand_r"),     nullptr,             4.f, 12.f },
		{ TEXT("thigh_l"),    TEXT("calf_l"),      8.f, 0.f },
		{ TEXT("calf_l"),     TEXT("foot_l"),      6.f, 0.f },
		{ TEXT("foot_l"),     TEXT("ball_l"),      5.f, 0.f },
		{ TEXT("thigh_r"),    TEXT("calf_r"),      8.f, 0.f },
		{ TEXT("calf_r"),     TEXT("foot_r"),      6.f, 0.f },
		{ TEXT("foot_r"),     TEXT("ball_r"),      5.f, 0.f },
	};

	FTransform ComponentRefPose(const FReferenceSkeleton& Ref, int32 Index)
	{
		FTransform T = FTransform::Identity;
		while (Index != INDEX_NONE)
		{
			T = T * Ref.GetRefBonePose()[Index];
			Index = Ref.GetParentIndex(Index);
		}
		return T;
	}
}

FString UEAWEditorTools::RebuildPhysicsAsset(USkeletalMesh* Mesh, float MinBoneSize)
{
#if WITH_EDITOR
	if (!Mesh || !Mesh->GetPhysicsAsset())
	{
		return TEXT("ERRO: malha sem PhysicsAsset");
	}
	UPhysicsAsset* PA = Mesh->GetPhysicsAsset();
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();

	// Limpa o que a importacao criou.
	while (PA->ConstraintSetup.Num() > 0)
	{
		FPhysicsAssetUtils::DestroyConstraint(PA, PA->ConstraintSetup.Num() - 1);
	}
	while (PA->SkeletalBodySetups.Num() > 0)
	{
		FPhysicsAssetUtils::DestroyBody(PA, PA->SkeletalBodySetups.Num() - 1);
	}
	PA->CollisionDisableTable.Empty();

	FPhysAssetCreateParams Params;
	FString Out;
	// 1) Corpos: uma capsula por osso, alinhada do osso ate o filho.
	for (const FEAWBodyDef& Def : GBodies)
	{
		const int32 Bi = Ref.FindBoneIndex(Def.Bone);
		if (Bi == INDEX_NONE)
		{
			Out += FString::Printf(TEXT(" [sem osso %s]"), Def.Bone);
			continue;
		}
		const FTransform BoneTM = ComponentRefPose(Ref, Bi);
		// A pose de referencia pode ter escala (FBX em metros com raiz x100): medidas em cm viram unidades do osso.
		const float S = FMath::Max(KINDA_SMALL_NUMBER, BoneTM.GetMaximumAxisScale());
		FVector LocalDir;
		float Len;
		const int32 Ti = Def.Toward ? Ref.FindBoneIndex(Def.Toward) : INDEX_NONE;
		if (Ti != INDEX_NONE)
		{
			const FVector D = BoneTM.InverseTransformPosition(ComponentRefPose(Ref, Ti).GetLocation());
			Len = D.Size();
			LocalDir = D.GetSafeNormal();
		}
		else
		{
			// Sem filho de referencia: segue a direcao do pai ate este osso.
			const int32 Pi = Ref.GetParentIndex(Bi);
			const FVector D = BoneTM.InverseTransformVectorNoScale(BoneTM.GetLocation() - ComponentRefPose(Ref, Pi).GetLocation());
			Len = Def.Extra / S;
			LocalDir = D.GetSafeNormal();
		}
		const int32 BodyIndex = FPhysicsAssetUtils::CreateNewBody(PA, Def.Bone, Params);
		UBodySetup* BS = PA->SkeletalBodySetups[BodyIndex];
		BS->AggGeom.EmptyElements();
		FKSphylElem Capsule;
		Capsule.Radius = Def.Radius / S;
		Capsule.Length = FMath::Max(1.f / S, Len - Def.Radius / S);
		Capsule.Center = LocalDir * Len * 0.5f;
		Capsule.Rotation = FRotationMatrix::MakeFromZ(LocalDir).Rotator();
		BS->AggGeom.SphylElems.Add(Capsule);
		BS->InvalidatePhysicsData();
		BS->CreatePhysicsMeshes();
		Out += FString::Printf(TEXT(" %s(%.1fcm s=%.2f)"), Def.Bone, Len * S, S);
	}

	// 2) Juntas: cada corpo preso ao corpo-pai mais proximo (igual ao gerador da Unreal), com limites.
	for (int32 BodyIndex = 0; BodyIndex < PA->SkeletalBodySetups.Num(); ++BodyIndex)
	{
		const FName BoneName = PA->SkeletalBodySetups[BodyIndex]->BoneName;
		int32 Index = Ref.FindBoneIndex(BoneName);
		FTransform RelTM = FTransform::Identity;
		int32 ParentBody = INDEX_NONE;
		FName ParentName;
		while (true)
		{
			RelTM = RelTM * Ref.GetRefBonePose()[Index];
			Index = Ref.GetParentIndex(Index);
			if (Index == INDEX_NONE)
			{
				break;
			}
			ParentName = Ref.GetBoneName(Index);
			ParentBody = PA->FindBodyIndex(ParentName);
			if (ParentBody != INDEX_NONE)
			{
				break;
			}
		}
		if (ParentBody == INDEX_NONE)
		{
			continue;   // raiz (pelve)
		}
		const int32 Ci = FPhysicsAssetUtils::CreateNewConstraint(PA, BoneName);
		UPhysicsConstraintTemplate* CS = PA->ConstraintSetup[Ci];
		FConstraintInstance& CI = CS->DefaultInstance;
		CI.ConstraintBone1 = BoneName;
		CI.Pos1 = FVector::ZeroVector;
		CI.PriAxis1 = FVector(1, 0, 0);
		CI.SecAxis1 = FVector(0, 1, 0);
		CI.ConstraintBone2 = ParentName;
		CI.Pos2 = RelTM.GetLocation();
		CI.PriAxis2 = RelTM.GetUnitAxis(EAxis::X);
		CI.SecAxis2 = RelTM.GetUnitAxis(EAxis::Y);

		const FString Child = BoneName.ToString();
		float S1 = 40.f, S2 = 40.f, Tw = 30.f;           // ombro/quadril
		if (Child.StartsWith(TEXT("lowerarm")) || Child.StartsWith(TEXT("calf"))) { S1 = 70.f; S2 = 5.f; Tw = 10.f; }   // cotovelo/joelho
		else if (Child.StartsWith(TEXT("spine")) || Child.StartsWith(TEXT("neck")) || Child.StartsWith(TEXT("head"))) { S1 = 25.f; S2 = 25.f; Tw = 20.f; }
		else if (Child.StartsWith(TEXT("hand")) || Child.StartsWith(TEXT("foot"))) { S1 = 35.f; S2 = 25.f; Tw = 15.f; }
		CI.SetAngularSwing1Limit(ACM_Limited, S1);
		CI.SetAngularSwing2Limit(ACM_Limited, S2);
		CI.SetAngularTwistLimit(ACM_Limited, Tw);
		CS->SetDefaultProfile(CI);
		PA->DisableCollision(BodyIndex, ParentBody);
	}
	// Bracos/coxas nao colidem com o tronco (capsulas encostadas tremiam).
	const TCHAR* Pairs[][2] = { { TEXT("upperarm_l"), TEXT("spine_02") }, { TEXT("upperarm_r"), TEXT("spine_02") },
		{ TEXT("thigh_l"), TEXT("spine_01") }, { TEXT("thigh_r"), TEXT("spine_01") }, { TEXT("thigh_l"), TEXT("thigh_r") } };
	for (const auto& P : Pairs)
	{
		const int32 A = PA->FindBodyIndex(P[0]);
		const int32 B = PA->FindBodyIndex(P[1]);
		if (A != INDEX_NONE && B != INDEX_NONE)
		{
			PA->DisableCollision(A, B);
		}
	}
	PA->UpdateBodySetupIndexMap();
	PA->UpdateBoundsBodiesArray();
	PA->MarkPackageDirty();
	return FString::Printf(TEXT("corpos=%d juntas=%d:"), PA->SkeletalBodySetups.Num(), PA->ConstraintSetup.Num()) + Out;
#else
	return TEXT("so no editor");
#endif
}
