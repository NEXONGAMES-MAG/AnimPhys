// Copyright NEXON Games Co., MIT License
#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "AnimPhysWorkData.h"
#include "AnimNode_AnimPhys.generated.h"

class USkeletalMeshComponent;

struct FAnimPhys_EditData
{
	bool bInEditor = false;
	bool bPlayInEditor = false;
	FDelegateHandle ObjectPropertyChangedHandle;
	FCSPose<FCompactHeapPose> ForwardedPose;
};

struct FAnimPhys_NodeData
{
	bool bIsSequencerBound = false;
	bool bHasEvaluated = false;

	bool bWasRootBoneIdentity = false;
	FTransform LastActorTransform = FTransform::Identity;
	FTransform LastRootComponentTransform = FTransform::Identity;

	ETeleportType CurrentTeleportType = ETeleportType::None;
	ETeleportType PendingDynamicResetTeleportType = ETeleportType::None;

	float DeltaTime = 0.0f;
	float LastDeltaTime = 0.0f;
	float AccumulatedDeltaTime = 0.0f;
	float WorldTimeSeconds = 0.0f;
	float LastEvalTimeSeconds = 0.0f;
	float SuspendedSimTimeSeconds = 0.0f;
	float StartedSmoothingTimeSeconds = 0.0f;

	bool bPhysBodyWasSimulated = false;
	bool bWindWasEnabled = false;

	EAnimPhysDisabledState AnimPhysDisabledStateByOwner = EAnimPhysDisabledState::None;
};

USTRUCT(BlueprintType)
struct ANIMPHYS_API FAnimNode_AnimPhys : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	FAnimNode_AnimPhys();
	virtual ~FAnimNode_AnimPhys();

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& RESTRICT Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& RESTRICT Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& RESTRICT Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& RESTRICT Output) override;

	virtual bool HasPreUpdate() const override;
	virtual void PreUpdate(const UAnimInstance* RESTRICT InAnimInstance) override;
	virtual bool NeedsDynamicReset() const override { return true; }
	virtual void ResetDynamics(ETeleportType InTeleportType) override;
	
#if WITH_EDITOR
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEventEvent);
	void ResetSimulatedBones();
	void ResetColliders();
	FCSPose<FCompactHeapPose>& GetForwardedPose() { return EditData.ForwardedPose; }
	const TArray<FAnimPhys_SimulatedBone_WorkData>& GetSimulatedBones() const { return WorkData.Simulated.SimulatedBones; }
	const FAnimPhys_Collided_WorkData& GetColliders() const { return WorkData.Collided; }
#endif

private:
	USkeletalMeshComponent* GetMeshComponent(USkeletalMeshComponent* RESTRICT MeshComponent) const;

	bool CanCacheBoneTransformsFrom(USkeletalMeshComponent* RESTRICT MeshComponent) const;
	void CopyBoneTransformsFromComponent(USkeletalMeshComponent* RESTRICT MeshComponent);
	void CopyBoneTransformsFromAttachedComponent(USkeletalMeshComponent* RESTRICT MeshComponent);
	void BuildCachedTransformIndexes(USkeletalMeshComponent* RESTRICT MeshComponent);
	void BuildCachedAttachedTransformIndexes(USkeletalMeshComponent* RESTRICT MeshComponent);

	bool IsWindEnabled(const USkeletalMeshComponent* RESTRICT MeshComponent) const;
	void ComputeWind(const USkeletalMeshComponent* RESTRICT MeshComponent);

	bool IsAllPhysBodiesSimulated(const USkeletalMeshComponent* RESTRICT MeshComponent) const;
	void ComputePhysBody(const USkeletalMeshComponent* RESTRICT MeshComponent);
	void ComputeFloor(const USkeletalMeshComponent* RESTRICT MeshComponent);

	void BuildColliders(const USkeletalMeshComponent* RESTRICT MeshComponent);
	void BuildSphereColliders(const FReferenceSkeleton& RefSkeleton, const bool bFromAttachedMesh, const TArray<FAnimPhysSphereCollider>& SphereColliders);
	void BuildCapsuleColliders(const FReferenceSkeleton& RefSkeleton, const bool bFromAttachedMesh, const TArray<FAnimPhysCapsuleCollider>& CapsuleColliders);
	void BuildPlanarColliders(const FReferenceSkeleton& RefSkeleton, const bool bFromAttachedMesh, const TArray<FAnimPhysPlanarCollider>& PlanarColliders);
	void BuildPhysBodyColliders(const USkeletalMeshComponent* RESTRICT MeshComponent);
	void BuildPhysBodyCollidersFromPhysicsAsset(const FReferenceSkeleton& RefSkeleton, const UPhysicsAsset* PhysicsAsset, const bool bFromAttachedMesh);

	bool IsAnimPhysValid(const FPoseContext& RESTRICT Context) const;
	bool IsAnimPhysEnabled() const;
	void EvaluateAnimPhys(FPoseContext& RESTRICT Output);

	void ComputeComponentMovement(FPoseContext& RESTRICT Output);
	void ComputePoseTransform(FPoseContext& RESTRICT Output);
	void ComputeColliderTransform(FPoseContext& RESTRICT Output);
	void ComputeSphereColliderTransform(TArray<FAnimPhys_CollidedSphere_WorkData>& RESTRICT CollidedSpheres);
	void ComputeCapsuleColliderTransform(TArray<FAnimPhys_CollidedCapsule_WorkData>& RESTRICT CollidedCapsules);
	void SimulateBones(FPoseContext& RESTRICT Output);
	
	void CopyBoneTransformsFromPose(const FCompactPose& RESTRICT Pose);

	bool TryGetCollisionComponentSpaceTransform(FTransform& RESTRICT PoseComponentSpaceTM, const FAnimPhys_CollidedBase_WorkData& RESTRICT Collider) const;

	void CheckTeleport(FPoseContext& RESTRICT Output);
	void ConditionalSetTeleportType(ETeleportType InTeleportType, ETeleportType& OutTeleportType);

	const TArray<FTransform>& GetAppropriateComponentSpaceTransforms(USkeletalMeshComponent* RESTRICT ComponentToCopyFrom) const;

	const bool IsDisabledState(EAnimPhysDisabledState InState) const;
	const bool IsEnableDamping() const;
	const bool IsEnableStiffness() const;
	const bool IsEnableGravity() const;
	const bool IsEnableWind() const;
	const bool IsEnableWorldDamping() const;

#if !UE_BUILD_SHIPPING
	void DebugDraw(const USkeletalMeshComponent* RESTRICT MeshComponent);
#endif

public:
	UPROPERTY(EditAnywhere, Category=Links)
	FPoseLink Base;

	UPROPERTY(EditAnywhere, Category = Alpha, meta = (PinShownByDefault))
	float Alpha = 1.0f;

	UPROPERTY(EditAnywhere, Category = ModifyTarget)
	TArray<FBoneReference> BonesToSimulate;

	UPROPERTY(EditAnywhere, Category = ModifyTarget)
	TArray<FBoneReference> BonesToExculude;

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimPhysSetupSettings SetupSettings;
	
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimPhysCollisionSettings CollisionSettings;

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimPhysExternalForceSettings ExternalForceSettings;

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimPhysSmoothingSettings SmoothingSettings;

private:
	FAnimPhys_WorkData WorkData;
	FAnimPhys_NodeData NodeData;

#if WITH_EDITORONLY_DATA
	FAnimPhys_EditData EditData;
#endif

private:
	static const float TargetFramerate;
	static const float MaxPhysicsDeltaTime;
	static const float SmoothingPhysicsDeltaTime;
	static const float TeleportDistanceThreshold;
	static const float TeleportRotationThreshold;
	static const float EvaluationResetTime;
	static const float EvaluationSuspendSimTimeAfterReset;
	static const float EvaluationWarmUpTime;
};