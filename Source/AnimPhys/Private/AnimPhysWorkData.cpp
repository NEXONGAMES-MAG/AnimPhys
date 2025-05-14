// Copyright NEXON Games Co., MIT License

// This code is primarily based on the code from KawaiiPhysics.
// https://github.com/pafuhana1213/KawaiiPhysics
// The license for KawaiiPhysics is reproduced below:

/*
MIT License

Copyright (c) 2019-2024 pafuhana1213

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "AnimPhysWorkData.h"

void FAnimPhys_WorkData::BuildSimulatedBones(const FCompactPose& InPose, const TArray<FBoneReference>& InBonesToSimulate, const TArray<FBoneReference>& InBonesToExculude, const FAnimPhysSetupSettings& InSetupSettings)
{
	if (InBonesToSimulate.IsEmpty())
	{
		return;
	}

	if (Cached.ComponentSpaceTMs.IsEmpty())
	{
		return;
	}

	TArray<FAnimPhys_SimulatedBone_WorkData> OldSimulateBones = MoveTemp(Simulated.SimulatedBones);

	Simulated.CapturedPoseBonesNum = InPose.GetNumBones();

	Simulated.SimulatedBones.Empty();

	const FBoneContainer& RequiredBones = InPose.GetBoneContainer();

	TMap<FCompactPoseBoneIndex, int32> SimulatedBoneMap;
	TArray<FCompactPoseBoneIndex> ParentBones;
	for (const auto& EachRootBone : InBonesToSimulate)
	{
		if (EachRootBone.IsValidToEvaluate() == false)
		{
			continue;
		}

		FCompactPoseBoneIndex CompactPoseIndex = EachRootBone.GetCompactPoseIndex(RequiredBones);
		ParentBones.Add(CompactPoseIndex);

		FAnimPhys_SimulatedBone_WorkData NewRootBone;
		NewRootBone.CompactPoseBoneIndex = EachRootBone.GetCompactPoseIndex(RequiredBones);
		NewRootBone.MeshPoseBoneIndex = EachRootBone.GetMeshPoseIndex(RequiredBones);

		const int32 Index = Simulated.SimulatedBones.Add(NewRootBone);

		SimulatedBoneMap.Add(CompactPoseIndex, Index);
	}

	TArray<FCompactPoseBoneIndex> ExcludedParentBones;
	for (const auto& EachRootBone : InBonesToExculude)
	{
		if (EachRootBone.IsValidToEvaluate() == false)
		{
			continue;
		}

		FCompactPoseBoneIndex CompactPoseIndex = EachRootBone.GetCompactPoseIndex(RequiredBones);
		ExcludedParentBones.Add(CompactPoseIndex);
	}

	FCompactPoseBoneIndex BoneIndex(0);
	while (BoneIndex.GetInt() != RequiredBones.GetCompactPoseNumBones())
	{
		bool ShouldExclude = false;
		for (const auto& ExcludedParentBone : ExcludedParentBones)
		{
			if (RequiredBones.BoneIsChildOf(BoneIndex, ExcludedParentBone))
			{
				ShouldExclude = true;
				break;
			}
		}
		if (ShouldExclude)
		{
			++BoneIndex;
			continue;
		}

		bool FoundChild = false;
		for (const auto& ParentBone : ParentBones)
		{
			if (RequiredBones.BoneIsChildOf(BoneIndex, ParentBone))
			{
				FoundChild = true;
				break;
			}
		}
		if (FoundChild == false)
		{
			++BoneIndex;
			continue;
		}

		const int32* ParentIndex = SimulatedBoneMap.Find(RequiredBones.GetParentBoneIndex(BoneIndex));
		if (ParentIndex && Simulated.SimulatedBones.IsValidIndex(*ParentIndex))
		{
			FAnimPhys_SimulatedBone_WorkData NewChildBone;
			NewChildBone.ParentIndex = (*ParentIndex);
			NewChildBone.CompactPoseBoneIndex = BoneIndex;
			NewChildBone.MeshPoseBoneIndex = RequiredBones.MakeMeshPoseIndex(BoneIndex);

			const int32 SimulatedBoneIndex = Simulated.SimulatedBones.Add(NewChildBone);
			SimulatedBoneMap.Add(BoneIndex, SimulatedBoneIndex);

			Simulated.SimulatedBones[NewChildBone.ParentIndex].NumChildren += 1;
			Simulated.SimulatedBones[NewChildBone.ParentIndex].LastChildIndex = SimulatedBoneIndex;
		}

		++BoneIndex;
	}

	const bool ShouldBuildEndBone = (InSetupSettings.EndBoneLength > 0.0f && ExcludedParentBones.IsEmpty());
	TArray<int32> EndBoneParentIndexes;

	for (int32 SimulatedBoneIndex = 0; SimulatedBoneIndex < Simulated.SimulatedBones.Num(); ++SimulatedBoneIndex)
	{
		auto& SimulatedBone = Simulated.SimulatedBones[SimulatedBoneIndex];
		if (InPose.IsValidIndex(SimulatedBone.CompactPoseBoneIndex) == false)
		{
			continue;
		}

		SimulatedBone.CompactPoseBoneIndex = RequiredBones.MakeCompactPoseIndex(SimulatedBone.MeshPoseBoneIndex);

		CalculatePoseComponentSpace(InPose, InSetupSettings, SimulatedBone);

		SimulatedBone.ComponentSpaceTM = SimulatedBone.PoseComponentSpaceTM;
		SimulatedBone.PrevLocation = SimulatedBone.PoseComponentSpaceTM.GetLocation();

		SimulatedBone.BoneLengthToParent = InPose[SimulatedBone.CompactPoseBoneIndex].GetLocation().Size();

		SimulatedBone.bValid = true;

		if (ShouldBuildEndBone && SimulatedBone.NumChildren == 0)
		{
			EndBoneParentIndexes.Add(SimulatedBoneIndex);
		}
	}

	if (ShouldBuildEndBone)
	{
		int32 NewCapacity = Simulated.SimulatedBones.Num() + EndBoneParentIndexes.Num();
		Simulated.SimulatedBones.Reserve(NewCapacity);
		for (int32& EndBoneParentIndex : EndBoneParentIndexes)
		{
			if (Simulated.SimulatedBones.IsValidIndex(EndBoneParentIndex))
			{
				const auto& ParentBone = Simulated.SimulatedBones[EndBoneParentIndex];

				FAnimPhys_SimulatedBone_WorkData NewEndBone;
				NewEndBone.ParentIndex = EndBoneParentIndex;

				const FVector EndBoneLocation = ParentBone.PoseComponentSpaceTM.GetLocation() + ParentBone.PoseComponentSpaceTM.GetRotation().GetForwardVector() * InSetupSettings.EndBoneLength;
				NewEndBone.PoseComponentSpaceTM = ParentBone.PoseComponentSpaceTM;
				NewEndBone.PoseComponentSpaceTM.SetLocation(EndBoneLocation);

				NewEndBone.ComponentSpaceTM = NewEndBone.PoseComponentSpaceTM;
				NewEndBone.PrevLocation = NewEndBone.PoseComponentSpaceTM.GetLocation();

				NewEndBone.BoneLengthToParent = InSetupSettings.EndBoneLength;

				NewEndBone.bValid = true;

				const int32 SimulatedBoneIndex = Simulated.SimulatedBones.Add(NewEndBone);

				Simulated.SimulatedBones[NewEndBone.ParentIndex].NumChildren += 1;
				Simulated.SimulatedBones[NewEndBone.ParentIndex].LastChildIndex = SimulatedBoneIndex;
			}
		}
	}

	CopyFromOldSimulateBones(OldSimulateBones);
}

void FAnimPhys_WorkData::CopyFromOldSimulateBones(const TArray<FAnimPhys_SimulatedBone_WorkData>& OldSimulateBones)
{
	if (OldSimulateBones.Num() != Simulated.SimulatedBones.Num())
	{
		return;
	}

	for (int32 SimulatedBoneIndex = 0; SimulatedBoneIndex < Simulated.SimulatedBones.Num(); ++SimulatedBoneIndex)
	{
		auto& SimulatedBone = Simulated.SimulatedBones[SimulatedBoneIndex];
		SimulatedBone.ComponentSpaceTM = OldSimulateBones[SimulatedBoneIndex].ComponentSpaceTM;
		SimulatedBone.PrevLocation = OldSimulateBones[SimulatedBoneIndex].PrevLocation;
	}
}


bool FAnimPhys_WorkData::TryGetPoseComponentSpaceTransform(const FMeshPoseBoneIndex& InMeshPoseBoneIndex, FTransform& OutPoseComponentSpaceTM) const
{
	if (InMeshPoseBoneIndex.IsValid() == false)
	{
		return false;
	}

	const FTransform* Found = Cached.ComponentSpaceTMs.Find(InMeshPoseBoneIndex);
	if (Found == nullptr)
	{
		return false;
	}

	OutPoseComponentSpaceTM = (*Found);
	return true;
}

void FAnimPhys_WorkData::CalculatePoseComponentSpace(const FCompactPose& InPose, const FAnimPhysSetupSettings& InSetupSettings, FAnimPhys_SimulatedBone_WorkData& OutBone) const
{
	const FBoneContainer& RequiredBones = InPose.GetBoneContainer();

	if (OutBone.CompactPoseBoneIndex.IsValid() && OutBone.ParentIndex == INDEX_NONE)
	{
		FTransform ParentPoseComponentSpcaeTM;
		const FMeshPoseBoneIndex ParentMeshPoseBoneIndex = RequiredBones.MakeMeshPoseIndex(RequiredBones.GetParentBoneIndex(OutBone.CompactPoseBoneIndex));
		if (TryGetPoseComponentSpaceTransform(ParentMeshPoseBoneIndex, ParentPoseComponentSpcaeTM))
		{
			check(InPose.IsValidIndex(OutBone.CompactPoseBoneIndex));
			OutBone.PoseComponentSpaceTM = InPose[OutBone.CompactPoseBoneIndex] * ParentPoseComponentSpcaeTM;
			OutBone.BoneLengthToParent = (InPose[OutBone.CompactPoseBoneIndex].GetLocation() * ParentPoseComponentSpcaeTM.GetScale3D()).Size() ;
			OutBone.bValid = true;
		}
	}
	else if (Simulated.SimulatedBones.IsValidIndex(OutBone.ParentIndex))
	{
		const auto& ParentBone = Simulated.SimulatedBones[OutBone.ParentIndex];
		if (ParentBone.bValid)
		{
			if (OutBone.CompactPoseBoneIndex.IsValid())
			{
				check(InPose.IsValidIndex(OutBone.CompactPoseBoneIndex));
				OutBone.PoseComponentSpaceTM = InPose[OutBone.CompactPoseBoneIndex] * ParentBone.PoseComponentSpaceTM;
				OutBone.BoneLengthToParent = (InPose[OutBone.CompactPoseBoneIndex].GetLocation() * OutBone.PoseComponentSpaceTM.GetScale3D()).Size();
				OutBone.bValid = true;
			}
			else
			{
				OutBone.PoseComponentSpaceTM = ParentBone.PoseComponentSpaceTM;

				const FVector EndBoneLocation = ParentBone.PoseComponentSpaceTM.GetLocation() + ParentBone.PoseComponentSpaceTM.GetRotation().GetForwardVector() * InSetupSettings.EndBoneLength;
				OutBone.PoseComponentSpaceTM.SetLocation(EndBoneLocation);

				OutBone.bValid = true;
			}
		}
	}
}

void FAnimPhys_WorkData::SimulateBones(const FCompactPose& InPose, const float& InDeltaTime, const float InLastDeltaTime, const float& InTargetFramerate, const FAnimPhysSetupSettings& InSetupSettings, const FAnimPhysExternalForceSettings& InExternalForceSettings, const FAnimPhysSmoothingSettings& InSmoothingSettings)
{
	const bool bValidDeltaTime = (InDeltaTime > 0.0f && InLastDeltaTime > 0.0f);
	if (bValidDeltaTime == false)
	{
		return;
	}

	// for Gravity
	FVector GravityFactor = FVector::ZeroVector;
	if (Simulated.bGravityEnabled)
	{
		const float GravityTargetFramerate = 60.0f;
		const float GravityCoefficient = 0.5f * InDeltaTime / (GravityTargetFramerate);

		GravityFactor = InExternalForceSettings.Gravity * GravityCoefficient;
		GravityFactor.Z += (Forced.GravityZ * GravityCoefficient);

		GravityFactor = Moved.WorldToComponent.TransformVector(GravityFactor);
	}

	FVector WindFactor = FVector::ZeroVector;
	if (Simulated.bWindEnabled)
	{
		WindFactor = Moved.WorldToComponent.TransformVector(Forced.WindVelocity);
	}

	FVector WorldLocationVelocity = FVector::ZeroVector;
	float WorldLocationSpeed = 0.0f;
	if (Simulated.bWorldDampingEnabled)
	{
		const float LocationVelocityCoefficient = (1.0f - InSetupSettings.WorldDampingLocation);
		WorldLocationVelocity = Moved.WorldLocationDelta / InLastDeltaTime * LocationVelocityCoefficient;

		if (InExternalForceSettings.WorldMaxSpeed > 0.0f)
		{
			WorldLocationVelocity = WorldLocationVelocity.GetClampedToMaxSize2D(InExternalForceSettings.WorldMaxSpeed);
		}

		WorldLocationSpeed = WorldLocationVelocity.Size();
	}

	const bool bWorldLocationMoved = (WorldLocationSpeed > 0.0f);
	const float DampingCoefficient = Simulated.bDampingEnabled ? (1.0f - InSetupSettings.Damping) * InDeltaTime : 0.0f;
	const float StiffnessCoefficient = Simulated.bStiffnessEnabled ? FMath::Clamp((1.0f - FMath::Pow(1.0f - InSetupSettings.Stiffness, InTargetFramerate * InDeltaTime)), 0.0f, 1.0f) : 0.0f;

	for (auto& Bone : Simulated.SimulatedBones)
	{
		if (Bone.ParentIndex == INDEX_NONE)
		{
			Bone.PrevLocation = Bone.ComponentSpaceTM.GetLocation();
			Bone.ComponentSpaceTM = Bone.PoseComponentSpaceTM;
			continue;
		}

		auto& ParentBone = Simulated.SimulatedBones[Bone.ParentIndex];

		const FVector ParentBonePoseLocation = ParentBone.PoseComponentSpaceTM.GetLocation();
		const FVector ParentBoneLocation = ParentBone.ComponentSpaceTM.GetLocation();

		const FVector BonePoseLocation = Bone.PoseComponentSpaceTM.GetLocation();
		FVector BoneLocation = Bone.ComponentSpaceTM.GetLocation();

		const FVector PoseDelta = BonePoseLocation - ParentBonePoseLocation;

		if (Simulated.bDampingEnabled)
		{
			FVector Velocity = (BoneLocation - Bone.PrevLocation) / InLastDeltaTime;

			if (InSmoothingSettings.bScaleDampingWithExternalSpeed)
			{
				const float Speed = Velocity.Size();
				if (Simulated.bWorldDampingEnabled && WorldLocationSpeed > 0.0f && Speed > 0.0f)
				{
					Velocity *= FMath::Max(1.0f, (WorldLocationSpeed / Speed)) * InSmoothingSettings.ScaleDampingMultiplier;
				}

				Bone.Velocity = FMath::Lerp(Bone.Velocity, Velocity, FMath::Min(1.0f, InDeltaTime * InSmoothingSettings.ScaleDampingLerpSpeed));
			}
			else
			{
				Bone.Velocity = Velocity;
			}

			Bone.PrevLocation = BoneLocation;
			BoneLocation += Bone.Velocity * DampingCoefficient;
		}
		else
		{
			Bone.PrevLocation = BoneLocation;
		}

		FVector AccumulatedExternalDelta = FVector::ZeroVector;

		AccumulatedExternalDelta += Forced.Impulse;

		// Wind
		if (Simulated.bWindEnabled)
		{
			const float WindCoefficient = FMath::FRandRange(0.0f, 2.0f) * InTargetFramerate * InDeltaTime;
			AccumulatedExternalDelta += WindFactor * WindCoefficient;
		}

		// Follow Translation & Rotation
		if (Simulated.bWorldDampingEnabled)
		{
			AccumulatedExternalDelta += (WorldLocationVelocity * InDeltaTime);

			const FVector WorldRotationVelocity = (Moved.WorldRotationDelta.RotateVector(Bone.PrevLocation) - Bone.PrevLocation) / InLastDeltaTime * (1.0f - InSetupSettings.WorldDampingRotation);
			AccumulatedExternalDelta += (WorldRotationVelocity * InDeltaTime);
		}

		// Gravity
		if (Simulated.bGravityEnabled)
		{
			AccumulatedExternalDelta += GravityFactor;
		}

		// Prevent overextension
		if (bWorldLocationMoved)
		{
			const FVector PrevDelta = (Bone.PrevLocation - ParentBone.PrevLocation);
			const FVector AdjustDelta = (PrevDelta + AccumulatedExternalDelta).GetSafeNormal() * Bone.BoneLengthToParent - PrevDelta;
			BoneLocation += AdjustDelta;
		}
		else
		{
			BoneLocation += AccumulatedExternalDelta;
			AdjustBoneLength(Bone, BoneLocation);
		}

		// Pull to Pose Location
		if (Simulated.bStiffnessEnabled)
		{
			const FVector BaseLocation = ParentBoneLocation + PoseDelta;
			FVector PoseMoveDelta = (BaseLocation - BoneLocation);

			PoseMoveDelta *= StiffnessCoefficient;

			BoneLocation += PoseMoveDelta;

			static float StiffnessTranslationThreshold = 0.01f;
			if (bWorldLocationMoved == false && BoneLocation.Equals(BaseLocation, StiffnessTranslationThreshold))
			{
				BoneLocation = BaseLocation;
			}
		}

		AdjustBoneLocation(BoneLocation);
		AdjustBoneLength(Bone, BoneLocation);
		AdjustBoneDirection(ParentBoneLocation, Bone.PoseComponentSpaceTM, ParentBone.PoseComponentSpaceTM, InSetupSettings, BoneLocation);

		Bone.ComponentSpaceTM.SetLocation(BoneLocation);
		Bone.ComponentSpaceTM.CopyRotationPart(Bone.PoseComponentSpaceTM);

		if (ParentBone.NumChildren <= 1)
		{
			const FVector InitialDir = PoseDelta.GetSafeNormal();
			const FVector TargetDir = (BoneLocation - ParentBoneLocation).GetSafeNormal();

			const FQuat DeltaRotation = FQuat::FindBetweenNormals(InitialDir, TargetDir);
			ParentBone.ComponentSpaceTM.SetRotation(DeltaRotation * ParentBone.PoseComponentSpaceTM.GetRotation());
		}
	}
}

void FAnimPhys_WorkData::ApplySimulateBones(FCompactPose& OutPose)
{
	const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();

	for (const auto& Bone : Simulated.SimulatedBones)
	{
		if (Bone.bValid == false)
		{
			continue;
		}

		if (OutPose.IsValidIndex(Bone.CompactPoseBoneIndex) == false)
		{
			continue;
		}

		FTransform TargetAtom = Bone.ComponentSpaceTM;

		if (Bone.ParentIndex == INDEX_NONE)
		{
			FTransform ParentPoseComponentSpcaeTM;
			const FMeshPoseBoneIndex ParentMeshPoseBoneIndex = RequiredBones.MakeMeshPoseIndex(RequiredBones.GetParentBoneIndex(Bone.CompactPoseBoneIndex));
			if (TryGetPoseComponentSpaceTransform(ParentMeshPoseBoneIndex, ParentPoseComponentSpcaeTM))
			{
				TargetAtom.SetToRelativeTransform(ParentPoseComponentSpcaeTM);
			}
		}
		else
		{
			const auto& ParentBone = Simulated.SimulatedBones[Bone.ParentIndex];
			TargetAtom.SetToRelativeTransform(ParentBone.ComponentSpaceTM);
		}

		OutPose[Bone.CompactPoseBoneIndex] = TargetAtom;
	}
}

void FAnimPhys_WorkData::AdjustBoneLocation(FVector& OutBoneLocation) const
{
	// AdjustBySphereCollision
	for (const auto& CollidedSphere : Collided.Spheres)
	{
		if (CollidedSphere.bValid == false)
		{
			continue;
		}

		if ((OutBoneLocation - CollidedSphere.Center).SizeSquared() > CollidedSphere.LimitDistanceSquared)
		{
			continue;
		}

		OutBoneLocation = CollidedSphere.Center + (OutBoneLocation - CollidedSphere.Center).GetSafeNormal() * CollidedSphere.LimitDistance;
	}

	// AdjustByCapsuleCollision
	for (const auto& CollidedCapsule : Collided.Capsules)
	{
		if (CollidedCapsule.bValid == false)
		{
			continue;
		}

		const FVector ClosestPoint = FMath::ClosestPointOnSegment(OutBoneLocation, CollidedCapsule.SegmentStart, CollidedCapsule.SegmentEnd);

		if ((OutBoneLocation - ClosestPoint).SizeSquared() > CollidedCapsule.LimitDistanceSquared)
		{
			continue;
		}

		OutBoneLocation = ClosestPoint + (OutBoneLocation - ClosestPoint).GetSafeNormal() * CollidedCapsule.LimitDistance;
	}

	// AdjustByPlanerCollision
	for (const auto& CollidedPlanar : Collided.Planars)
	{
		if (CollidedPlanar.bValid == false)
		{
			continue;
		}

		const FVector PointOnPlane = FVector::PointPlaneProject(OutBoneLocation, CollidedPlanar.Plane);
		const FVector Direction = (OutBoneLocation - PointOnPlane);
		const float DistSquared = Direction.SizeSquared();

		bool bIntersects = (DistSquared < CollidedPlanar.LimitDistanceSquared);
		if (bIntersects == false)
		{
			bIntersects = (FVector::DotProduct(Direction.GetSafeNormal(), CollidedPlanar.Plane.GetNormal()) < 0.0f);
		}

		if (bIntersects == false)
		{
			continue;
		}

		OutBoneLocation = PointOnPlane + CollidedPlanar.Plane.GetNormal() * CollidedPlanar.LimitDistance;
	}

	// AdjustByFloorCollision
	if (Collided.Floor.bValid)
	{
		const FVector PointOnPlane = FVector::PointPlaneProject(OutBoneLocation, Collided.Floor.Plane);
		const FVector Direction = (OutBoneLocation - PointOnPlane);
		const float DistSquared = Direction.SizeSquared();

		bool bIntersects = (DistSquared < Collided.Floor.LimitDistanceSquared);
		if (bIntersects == false)
		{
			bIntersects = (FVector::DotProduct(Direction.GetSafeNormal(), Collided.Floor.Plane.GetNormal()) < 0.0f);
		}

		if (bIntersects)
		{
			OutBoneLocation = PointOnPlane + Collided.Floor.Plane.GetNormal() * Collided.Floor.LimitDistance;
		}
	}

}

void FAnimPhys_WorkData::AdjustBoneLength(const FAnimPhys_SimulatedBone_WorkData& InBone, FVector& OutBoneLocation) const
{
	const FAnimPhys_SimulatedBone_WorkData& ParentBone = Simulated.SimulatedBones[InBone.ParentIndex];
	const FVector ParentBoneLocation = ParentBone.ComponentSpaceTM.GetLocation();

	OutBoneLocation = (OutBoneLocation - ParentBoneLocation).GetSafeNormal() * InBone.BoneLengthToParent + ParentBoneLocation;
}

void FAnimPhys_WorkData::AdjustBoneDirection(const FVector& InParentBoneLocation, const FTransform& InPoseComponentSpaceTM, const FTransform& InParentPoseComponentSpaceTM, const FAnimPhysSetupSettings& InSetupSettings, FVector& OutBoneLocation) const
{
	bool bAdjusted = false;
	FVector BoneDir = FVector::ZeroVector;

	if (InSetupSettings.LimitAngle > 0.0f)
	{
		BoneDir = (OutBoneLocation - InParentBoneLocation).GetSafeNormal();
		const FVector PoseDir = (InPoseComponentSpaceTM.GetLocation() - InParentPoseComponentSpaceTM.GetLocation()).GetSafeNormal();
		const FVector Axis = FVector::CrossProduct(PoseDir, BoneDir);
		float Angle = FMath::RadiansToDegrees(FMath::Atan2(Axis.Size(), FVector::DotProduct(PoseDir, BoneDir)));
		const float AngleOverLimit = Angle - InSetupSettings.LimitAngle;
		if (AngleOverLimit > 0.0f)
		{
			BoneDir = BoneDir.RotateAngleAxis(-AngleOverLimit, Axis);
			bAdjusted = true;
		}
	}
	else if (InSetupSettings.LimitAngleX.IsZero() == false || InSetupSettings.LimitAngleY.IsZero() == false || InSetupSettings.LimitAngleZ.IsZero() == false)
	{
		BoneDir = (OutBoneLocation - InParentBoneLocation).GetSafeNormal();
		const FVector PoseDir = (InPoseComponentSpaceTM.GetLocation() - InParentPoseComponentSpaceTM.GetLocation()).GetSafeNormal();

		FMatrix RotMatrix = FRotationMatrix(InPoseComponentSpaceTM.GetRotation().Rotator());

		FVector AxisX, AxisY, AxisZ;
		RotMatrix.GetUnitAxes(AxisX, AxisY, AxisZ);

		if (TryAdjustBoneDirectionByAngleLimitAxis(AxisX, PoseDir, InSetupSettings.LimitAngleX, BoneDir))
		{
			bAdjusted = true;
		}

		if (TryAdjustBoneDirectionByAngleLimitAxis(AxisY, PoseDir, InSetupSettings.LimitAngleY, BoneDir))
		{
			bAdjusted = true;
		}

		if (TryAdjustBoneDirectionByAngleLimitAxis(AxisZ, PoseDir, InSetupSettings.LimitAngleZ, BoneDir))
		{
			bAdjusted = true;
		}
	}

	if (bAdjusted)
	{
		OutBoneLocation = BoneDir * (OutBoneLocation - InParentBoneLocation).Size() + InParentBoneLocation;
	}
}

bool FAnimPhys_WorkData::TryAdjustBoneDirectionByAngleLimitAxis(const FVector& InAxis, const FVector& InPoseDir, const FVector2D& InLimitAngleAxis, FVector& OutBoneDir) const
{
	if (InLimitAngleAxis.IsZero())
	{
		return false;
	}

	const float AngleX_Pose = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(InAxis, InPoseDir)));
	const float AngleX_Bone = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(InAxis, OutBoneDir)));
	const float AngleX_Diff = (AngleX_Pose - AngleX_Bone);

	const float LimitX_Min = InLimitAngleAxis.GetMin();
	const float LimitX_Max = InLimitAngleAxis.GetMax();
	if (AngleX_Diff < LimitX_Min)
	{
		const float AngleX_OverLimit = AngleX_Diff - LimitX_Min;
		OutBoneDir = OutBoneDir.RotateAngleAxis(AngleX_OverLimit, FVector::CrossProduct(InAxis, OutBoneDir).GetSafeNormal());
		return true;
	}
	else if (AngleX_Diff > LimitX_Max)
	{
		const float AngleX_OverLimit = AngleX_Diff - LimitX_Max;
		OutBoneDir = OutBoneDir.RotateAngleAxis(AngleX_OverLimit, FVector::CrossProduct(InAxis, OutBoneDir).GetSafeNormal());
		return true;
	}

	return false;
}

bool FAnimPhys_WorkData::IsInvalidSimulatedBones(const FCompactPose& InPose) const
{
	return (Simulated.SimulatedBones.IsEmpty() || Simulated.CapturedPoseBonesNum != InPose.GetNumBones());
}