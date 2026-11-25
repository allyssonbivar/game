#include "Combat/BodyRegionLibrary.h"
#include "Components/PrimitiveComponent.h"

EBodyRegion UBodyRegionLibrary::RegionFromBone(FName BoneName)
{
	if (BoneName.IsNone())
	{
		return EBodyRegion::None;
	}

	const FString Bone = BoneName.ToString().ToLower();

	if (Bone.Contains(TEXT("neck")))
	{
		return EBodyRegion::Neck;
	}
	if (Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("jaw")) || Bone.Contains(TEXT("eye")))
	{
		return EBodyRegion::Head;
	}
	if (Bone.Contains(TEXT("clavicle")) || Bone.Contains(TEXT("shoulder")) || Bone.Contains(TEXT("arm"))
		|| Bone.Contains(TEXT("hand")) || Bone.Contains(TEXT("elbow")) || Bone.Contains(TEXT("finger"))
		|| Bone.Contains(TEXT("thumb")) || Bone.Contains(TEXT("wrist")))
	{
		return EBodyRegion::Arms;
	}
	if (Bone.Contains(TEXT("thigh")) || Bone.Contains(TEXT("calf")) || Bone.Contains(TEXT("leg"))
		|| Bone.Contains(TEXT("knee")) || Bone.Contains(TEXT("foot")) || Bone.Contains(TEXT("toe"))
		|| Bone.Contains(TEXT("ankle")))
	{
		return EBodyRegion::Legs;
	}
	if (Bone.Contains(TEXT("spine")) || Bone.Contains(TEXT("pelvis")) || Bone.Contains(TEXT("chest"))
		|| Bone.Contains(TEXT("hip")) || Bone.Contains(TEXT("torso")))
	{
		return EBodyRegion::Torso;
	}
	return EBodyRegion::None;
}

EBodyRegion UBodyRegionLibrary::RegionFromComponent(const UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return EBodyRegion::None;
	}
	if (Component->ComponentHasTag(TEXT("Head")))  return EBodyRegion::Head;
	if (Component->ComponentHasTag(TEXT("Neck")))  return EBodyRegion::Neck;
	if (Component->ComponentHasTag(TEXT("Torso"))) return EBodyRegion::Torso;
	if (Component->ComponentHasTag(TEXT("Arms")))  return EBodyRegion::Arms;
	if (Component->ComponentHasTag(TEXT("Legs")))  return EBodyRegion::Legs;
	return EBodyRegion::None;
}

EBodyRegion UBodyRegionLibrary::RegionFromHit(const FHitResult& Hit)
{
	const EBodyRegion FromBone = RegionFromBone(Hit.BoneName);
	if (FromBone != EBodyRegion::None)
	{
		return FromBone;
	}
	return RegionFromComponent(Hit.GetComponent());
}
