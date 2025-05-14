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

#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "AnimPhysCollisionData.h"

#include "AnimPhysWorkData.generated.h"

UENUM()
enum class FAnimPhysRule : uint8
{
	AlwaysEnabled,
	EnabledWhenPhysBodyWasSimulated,
};

UENUM()
enum class EAnimPhysDisabledState : uint8
{
	None = 0,
	DisableWind = 1 << 0,
	DisableGravity = 1 << 1,
	DisableSimulation = 1 << 2,
	DisableAll = (DisableWind | DisableGravity | DisableSimulation),
};
ENUM_CLASS_FLAGS(EAnimPhysDisabledState);

USTRUCT()
struct ANIMPHYS_API FAnimPhysSetupSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float WorldDampingLocation = 0.8f;
	UPROPERTY(EditAnywhere)
	float WorldDampingRotation = 0.8f;
	UPROPERTY(EditAnywhere)
	float Damping = 0.1f;
	UPROPERTY(EditAnywhere)
	float Stiffness = 0.05f;
	UPROPERTY(EditAnywhere)
	float Radius = 3.0f;
	UPROPERTY(EditAnywhere)
	float EndBoneLength = 0.0f;

	UPROPERTY(EditAnywhere)
	float LimitAngle = 0.0f;
	UPROPERTY(EditAnywhere)
	FVector2D LimitAngleX = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere)
	FVector2D LimitAngleY = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere)
	FVector2D LimitAngleZ = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere)
	FAnimPhysRule Rule = FAnimPhysRule::AlwaysEnabled;
};

USTRUCT()
struct ANIMPHYS_API FAnimPhysCollisionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float PhysBodyScale = 0.7f;

	UPROPERTY(EditAnywhere)
	bool bCollidedWithAttachedMesh = false;

	UPROPERTY(EditAnywhere)
	bool bCollidedWithSimulatedPhysBody = false;

	UPROPERTY(EditAnywhere)
	bool bCollidedWithFloor = false;

	UPROPERTY(EditAnywhere)
	TArray<FAnimPhysSphereCollider> SphereColliders;

	UPROPERTY(EditAnywhere)
	TArray<FAnimPhysCapsuleCollider> CapsuleColliders;

	UPROPERTY(EditAnywhere)
	TArray<FAnimPhysPlanarCollider> PlanarColliders;
};

USTRUCT(BlueprintType)
struct ANIMPHYS_API FWindChannels
{
	GENERATED_BODY()

	FWindChannels() :
		bWorld(true),
		bChannel1(false),
		bChannel2(false)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Channels)
	uint8 bWorld : 1;

	/** First custom channel only for owner skeletalmesh's AnimPhys*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Channels)
	uint8 bChannel1 : 1;

	/** Second custom channel only for owner skeletalmesh's AnimPhys*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Channels)
	uint8 bChannel2 : 1;
};

USTRUCT()
struct ANIMPHYS_API FAnimPhysExternalForceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float WorldMaxSpeed = 0.0f;

	UPROPERTY(EditAnywhere)
	FVector Gravity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere)
	bool bEnableWind = true;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEnableWind"))
	FWindChannels WindChannels;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bEnableWind"))
	float WindScale = 1.0f;
};

USTRUCT()
struct ANIMPHYS_API FAnimPhysSmoothingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	bool bScaleDampingWithExternalSpeed = false;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bScaleDampingWithExternalSpeed"))
	float ScaleDampingLerpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bScaleDampingWithExternalSpeed"))
	FVector ScaleDampingMultiplier = FVector::OneVector;
};

struct ANIMPHYS_API FAnimPhys_SimulatedBone_WorkData
{
	FAnimPhys_SimulatedBone_WorkData()
		: MeshPoseBoneIndex(INDEX_NONE)
		, CompactPoseBoneIndex(INDEX_NONE)
	{}

	int32 ParentIndex = INDEX_NONE;
	int32 LastChildIndex = INDEX_NONE;

	int32 NumChildren = 0;
	float BoneLengthToParent = 0.0f;

	FMeshPoseBoneIndex MeshPoseBoneIndex;
	FCompactPoseBoneIndex CompactPoseBoneIndex;

	FTransform ComponentSpaceTM;
	FTransform PoseComponentSpaceTM;

	FVector PrevLocation = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FVector Velocity = FVector::ZeroVector;

	bool bValid = false;
};

struct ANIMPHYS_API FAnimPhys_Simulated_WorkData
{
	TArray<FAnimPhys_SimulatedBone_WorkData> SimulatedBones;
	int32 CapturedPoseBonesNum = 0;

	bool bDampingEnabled = false;
	bool bStiffnessEnabled = false;
	bool bGravityEnabled = false;
	bool bWindEnabled = false;
	bool bWorldDampingEnabled = false;
};

struct ANIMPHYS_API FAnimPhys_Cached_WorkData
{
	TMap<FMeshPoseBoneIndex, FTransform> ComponentSpaceTMs;
	TMap<FMeshPoseBoneIndex, FTransform> AttachedComponentSpaceTMs;

	int32 NumComponentSpaceTransforms = 0;
	int32 NumAttachedComponentSpaceTransforms = 0;

	FName AttachedMesh = NAME_None;

	bool bHasCachedTransforms = false;

	bool bInterpolated = false;
};

struct ANIMPHYS_API FAnimPhys_Forced_WorkData
{
	FVector WindVelocity = FVector::ZeroVector;
	float GravityZ = 0.0f;
	FVector Impulse = FVector::ZeroVector;
};

struct ANIMPHYS_API FAnimPhys_CollidedBase_WorkData
{
	FAnimPhys_CollidedBase_WorkData()
		: MeshPoseBoneIndex(INDEX_NONE)
	{}

	float LimitDistanceSquared = 0.0f;
	float LimitDistance = 0.0f;

	FMeshPoseBoneIndex MeshPoseBoneIndex;
	FTransform OffsetTransform = FTransform::Identity;
	bool bHasOffset = false;
	bool bFromAttachedMesh = false;

	bool bValid = false;

#if WITH_EDITORONLY_DATA
	FTransform DebugTransform = FTransform::Identity;
#endif
};

struct ANIMPHYS_API FAnimPhys_CollidedSphere_WorkData : public FAnimPhys_CollidedBase_WorkData
{
	FVector Center = FVector::ZeroVector;

#if WITH_EDITORONLY_DATA
	float DebugRadius = 0.0f;
#endif
};

struct ANIMPHYS_API FAnimPhys_CollidedCapsule_WorkData : public FAnimPhys_CollidedBase_WorkData
{
	FVector SegmentStart = FVector::ZeroVector;
	FVector SegmentEnd = FVector::ZeroVector;
	float HalfHeight = 0.0f;

#if WITH_EDITORONLY_DATA
	float DebugRadius = 0.0f;
#endif

};
struct ANIMPHYS_API FAnimPhys_CollidedPlanar_WorkData : public FAnimPhys_CollidedBase_WorkData
{
	FPlane Plane = FPlane(FVector::ZeroVector);
};

struct ANIMPHYS_API FAnimPhys_CollidedFloor_WorkData
{
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::ZeroVector;

	FPlane Plane = FPlane(FVector::ZeroVector);
	float LimitDistanceSquared = 0.0f;
	float LimitDistance = 0.0f;

	bool bValid = false;
};

struct ANIMPHYS_API FAnimPhys_Collided_WorkData
{
	TArray<FAnimPhys_CollidedSphere_WorkData> Spheres;
	TArray<FAnimPhys_CollidedCapsule_WorkData> Capsules;
	TArray<FAnimPhys_CollidedPlanar_WorkData> Planars;
	FAnimPhys_CollidedFloor_WorkData Floor;

	TArray<FAnimPhys_CollidedSphere_WorkData> PhysBodySpheres;
	TArray<FAnimPhys_CollidedCapsule_WorkData> PhysBodyCapsules;

	bool bValidColliders = false;
	bool bValidPhysBodyColliders = false;
};

struct ANIMPHYS_API FAnimPhys_Moved_WorkData
{
	FTransform WorldToComponent = FTransform::Identity;
	FTransform LastComponentTransform = FTransform::Identity;
	FVector WorldLocationDelta = FVector::ZeroVector;
	FQuat WorldRotationDelta = FQuat::Identity;

	bool OnGround = false;
};

struct ANIMPHYS_API FAnimPhys_WorkData
{
	FAnimPhys_Simulated_WorkData Simulated;
	FAnimPhys_Cached_WorkData Cached;
	FAnimPhys_Forced_WorkData Forced;
	FAnimPhys_Collided_WorkData Collided;
	FAnimPhys_Moved_WorkData Moved;

public:
	void BuildSimulatedBones(const FCompactPose& InPose, const TArray<FBoneReference>& InBonesToSimulate, const TArray<FBoneReference>& InBonesToExculude, const FAnimPhysSetupSettings& InSetupSettings);
	void SimulateBones(const FCompactPose& InPose, const float& InDeltaTime, const float InLastDeltaTime, const float& InTargetFramerate, const FAnimPhysSetupSettings& InSetupSettings, const FAnimPhysExternalForceSettings& InExternalForceSettings, const FAnimPhysSmoothingSettings& InSmoothingSettings);	
	void ApplySimulateBones(FCompactPose& OutPose);

	void CopyFromOldSimulateBones(const TArray<FAnimPhys_SimulatedBone_WorkData>& OldSimulateBones);

	bool TryGetPoseComponentSpaceTransform(const FMeshPoseBoneIndex& InMeshPoseBoneIndex, FTransform& OutPoseComponentSpaceTM) const;
	void CalculatePoseComponentSpace(const FCompactPose& InPose, const FAnimPhysSetupSettings& InSetupSettings, FAnimPhys_SimulatedBone_WorkData& OutBone) const;
	
	void AdjustBoneLocation(FVector& OutBoneLocation) const;
	void AdjustBoneLength(const FAnimPhys_SimulatedBone_WorkData& InBone, FVector& OutBoneLocation) const;
	void AdjustBoneDirection(const FVector& InParentBoneLocation, const FTransform& InPoseComponentSpaceTM, const FTransform& InParentPoseComponentSpaceTM, const FAnimPhysSetupSettings& InSetupSettings, FVector& OutBoneLocation) const;
	bool TryAdjustBoneDirectionByAngleLimitAxis(const FVector& InAxis, const FVector& InPoseDir, const FVector2D& InLimitAngleAxis, FVector& OutBoneDir) const;

	bool IsInvalidSimulatedBones(const FCompactPose& InPose) const;
};
