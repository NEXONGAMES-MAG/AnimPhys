// Copyright NEXON Games Co., MIT License
#include "AnimNode_AnimPhys.h"
#include "AnimPhysInterface.h"
#include "Animation/AnimInstanceProxy.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

static TAutoConsoleVariable<int32> CVarEnableAnimPhys(TEXT("EnableAnimPhys"), 1, TEXT("Enable Anim Phys"));

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarDrawDebugAnimPhys(TEXT("DrawDebugAnimPhys"), 0, TEXT("Draw Debug Anim Phys"));
#endif

const float FAnimNode_AnimPhys::TargetFramerate = 30.0f;
const float FAnimNode_AnimPhys::MaxPhysicsDeltaTime = 1.0f / 30.0f;
const float FAnimNode_AnimPhys::SmoothingPhysicsDeltaTime = 0.5f / 30.0f;
const float FAnimNode_AnimPhys::TeleportDistanceThreshold = 300.0f;
const float FAnimNode_AnimPhys::TeleportRotationThreshold = 10.0f;
const float FAnimNode_AnimPhys::EvaluationResetTime = 5.0f;
const float FAnimNode_AnimPhys::EvaluationSuspendSimTimeAfterReset = 2.0f;
const float FAnimNode_AnimPhys::EvaluationWarmUpTime = 2.0f;

FAnimNode_AnimPhys::FAnimNode_AnimPhys()
{
}

FAnimNode_AnimPhys::~FAnimNode_AnimPhys()
{
#if WITH_EDITORONLY_DATA
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(EditData.ObjectPropertyChangedHandle);
	EditData.ObjectPropertyChangedHandle.Reset();
#endif
}

void FAnimNode_AnimPhys::Initialize_AnyThread(const FAnimationInitializeContext& RESTRICT Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_Base::Initialize_AnyThread(Context);

	Base.Initialize(Context);

	WorkData.Simulated.SimulatedBones.Empty();

	WorkData.Cached.ComponentSpaceTMs.Empty();
	WorkData.Cached.AttachedComponentSpaceTMs.Empty();
	WorkData.Cached.NumComponentSpaceTransforms = 0;
	WorkData.Cached.NumAttachedComponentSpaceTransforms = 0;
	WorkData.Cached.AttachedMesh = NAME_None;
	WorkData.Cached.bHasCachedTransforms = false;

	WorkData.Collided.Spheres.Empty();
	WorkData.Collided.Capsules.Empty();
	WorkData.Collided.Planars.Empty();
	WorkData.Collided.PhysBodySpheres.Empty();
	WorkData.Collided.PhysBodyCapsules.Empty();
	WorkData.Collided.bValidColliders = false;
	WorkData.Collided.bValidPhysBodyColliders = false;

	// For Avoiding Zero Divide in the first frame
	NodeData.LastDeltaTime = MaxPhysicsDeltaTime; 

	NodeData.bHasEvaluated = false;

	if (Context.AnimInstanceProxy->GetSkelMeshComponent())
	{
		AActor* OwnerActor = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner();
		if (OwnerActor && OwnerActor->IsA(ACharacter::StaticClass()) == false)
		{
			static const FName SequencerBoundTag(TEXT("SequencerBound"));
			static const FName SequencerActorTag(TEXT("SequencerActor"));
			NodeData.bIsSequencerBound = (OwnerActor->ActorHasTag(SequencerBoundTag) || OwnerActor->ActorHasTag(SequencerActorTag));
		}

#if WITH_EDITORONLY_DATA
		UWorld* World = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetWorld();
		check(World);

		if (World->WorldType == EWorldType::Editor ||
			World->WorldType == EWorldType::EditorPreview)
		{
			EditData.bInEditor = true;
		}

		if (World->WorldType == EWorldType::PIE)
		{
			EditData.bPlayInEditor = true;
		}
#endif
	}
}

void FAnimNode_AnimPhys::CacheBones_AnyThread(const FAnimationCacheBonesContext& RESTRICT Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	Base.CacheBones(Context);

	const FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();

	for (auto& Bone : BonesToSimulate)
	{
		Bone.Initialize(RequiredBones);
	}

	for (auto& Bone : BonesToExculude)
	{
		Bone.Initialize(RequiredBones);
	}

	for (auto& Sphere : CollisionSettings.SphereColliders)
	{
		Sphere.DrivingBone.Initialize(RequiredBones);
	}

	for (auto& Capsule : CollisionSettings.CapsuleColliders)
	{
		Capsule.DrivingBone.Initialize(RequiredBones);
	}

	for (auto& Planer : CollisionSettings.PlanarColliders)
	{
		Planer.DrivingBone.Initialize(RequiredBones);
	}
}

void FAnimNode_AnimPhys::Update_AnyThread(const FAnimationUpdateContext& RESTRICT Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)
	Base.Update(Context);

	NodeData.DeltaTime = FMath::Min(MaxPhysicsDeltaTime, Context.GetDeltaTime());
	NodeData.AccumulatedDeltaTime += Context.GetDeltaTime();
}

void FAnimNode_AnimPhys::Evaluate_AnyThread(FPoseContext& RESTRICT Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	Base.Evaluate(Output);

	if (IsAnimPhysValid(Output))
	{
#if WITH_EDITORONLY_DATA
		if (EditData.bInEditor)
		{
			FCSPose<FCompactPose> ComponentPose;
			ComponentPose.InitPose(Output.Pose);

			EditData.ForwardedPose.CopyPose(ComponentPose);
		}
#endif

		EvaluateAnimPhys(Output);
	}
}

DECLARE_CYCLE_STAT(TEXT("AnimPhys_Eval"), STAT_AnimPhys_Eval, STATGROUP_Anim);

void FAnimNode_AnimPhys::EvaluateAnimPhys(FPoseContext& RESTRICT Output)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimPhys_Eval);

	if (WorkData.IsInvalidSimulatedBones(Output.Pose))
	{
		WorkData.BuildSimulatedBones(Output.Pose, BonesToSimulate, BonesToExculude, SetupSettings);
	}

	CheckTeleport(Output);

	ComputeComponentMovement(Output);
	ComputePoseTransform(Output);
	ComputeColliderTransform(Output);

	SimulateBones(Output);

	NodeData.bHasEvaluated = Output.AnimInstanceProxy->GetEvaluationCounter().HasEverBeenUpdated();
	NodeData.CurrentTeleportType = ETeleportType::None;
	NodeData.AccumulatedDeltaTime = 0.0f;
}

bool FAnimNode_AnimPhys::HasPreUpdate() const
{
	return true;
}

DECLARE_CYCLE_STAT(TEXT("AnimPhys_PreUpdate"), STAT_AnimPhys_PreUpdate, STATGROUP_Anim);

void FAnimNode_AnimPhys::PreUpdate(const UAnimInstance* RESTRICT InAnimInstance)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimPhys_PreUpdate);

	if(USkeletalMeshComponent* MeshComponent = GetMeshComponent(InAnimInstance->GetSkelMeshComponent()))
	{
		if (UWorld* World = MeshComponent->GetWorld())
		{
			// Store game time for use in parallel evaluation. This may be the totol time (inc pauses) or the time the game has been unpaused.
			NodeData.WorldTimeSeconds = MeshComponent->PrimaryComponentTick.bTickEvenWhenPaused ? World->UnpausedTimeSeconds : World->TimeSeconds;
		}
			
		ConditionalSetTeleportType(NodeData.PendingDynamicResetTeleportType, NodeData.CurrentTeleportType);
		NodeData.PendingDynamicResetTeleportType = ETeleportType::None;

		WorkData.Cached.bInterpolated = (MeshComponent->IsUsingExternalInterpolation() || (MeshComponent->ShouldUseUpdateRateOptimizations() && MeshComponent->AnimUpdateRateParams != nullptr && MeshComponent->AnimUpdateRateParams->DoEvaluationRateOptimizations()));

		CopyBoneTransformsFromComponent(MeshComponent);
		CopyBoneTransformsFromAttachedComponent(MeshComponent);

		if (WorkData.Collided.bValidColliders == false)
		{
			BuildColliders(MeshComponent);
		}

		if (CollisionSettings.bCollidedWithSimulatedPhysBody && WorkData.Collided.bValidPhysBodyColliders == false)
		{
			BuildPhysBodyColliders(MeshComponent);
		}

		ComputeWind(MeshComponent);
		ComputePhysBody(MeshComponent);
		ComputeFloor(MeshComponent);

#if !UE_BUILD_SHIPPING
		if (CVarDrawDebugAnimPhys.GetValueOnAnyThread())
		{
			DebugDraw(MeshComponent);

#if WITH_EDITORONLY_DATA
			if (EditData.bPlayInEditor && EditData.ObjectPropertyChangedHandle.IsValid() == false)
			{
				EditData.ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FAnimNode_AnimPhys::OnObjectPropertyChanged);
			}
#endif
		}
#endif
	}

	if (IAnimPhysMeshComponentInterface* MeshComponentInterface = Cast<IAnimPhysMeshComponentInterface>(InAnimInstance->GetSkelMeshComponent()))
	{
		NodeData.AnimPhysDisabledStateByOwner = MeshComponentInterface->GetDesiredAnimPhysDisabledState();

		if(BonesToSimulate.IsValidIndex(0))
		{
			WorkData.Forced.Impulse = MeshComponentInterface->GetAccumulatedImpulsesToAnimPhys(BonesToSimulate[0].BoneName);
		}
	}

	if (NodeData.bIsSequencerBound)
	{
		ConditionalSetTeleportType(ETeleportType::TeleportPhysics, NodeData.CurrentTeleportType);
	}
}

void FAnimNode_AnimPhys::ResetDynamics(ETeleportType InTeleportType)
{
	ConditionalSetTeleportType(InTeleportType, NodeData.PendingDynamicResetTeleportType);
}

#if WITH_EDITOR
void FAnimNode_AnimPhys::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEventEvent)
{
	const bool IsAnimPhysCollisionChanged = (Object && Object->IsA(UAnimPhysCollisionData::StaticClass()));
	if (IsAnimPhysCollisionChanged)
	{
		WorkData.Collided.bValidColliders = false;
	}
}

void FAnimNode_AnimPhys::ResetSimulatedBones()
{
	WorkData.Simulated.SimulatedBones.Empty();

	WorkData.Cached.ComponentSpaceTMs.Empty();
	WorkData.Cached.AttachedComponentSpaceTMs.Empty();
	WorkData.Cached.NumComponentSpaceTransforms = 0;
	WorkData.Cached.NumAttachedComponentSpaceTransforms = 0;
	WorkData.Cached.AttachedMesh = NAME_None;
	WorkData.Cached.bHasCachedTransforms = false;

	WorkData.Collided.Spheres.Empty();
	WorkData.Collided.Capsules.Empty();
	WorkData.Collided.Planars.Empty();
	WorkData.Collided.PhysBodySpheres.Empty();
	WorkData.Collided.PhysBodyCapsules.Empty();
	WorkData.Collided.bValidColliders = false;
	WorkData.Collided.bValidPhysBodyColliders = false;
}

void FAnimNode_AnimPhys::ResetColliders()
{
	WorkData.Collided.bValidColliders = false;
	WorkData.Collided.bValidPhysBodyColliders = false;
}
#endif

bool FAnimNode_AnimPhys::IsAnimPhysValid(const FPoseContext& RESTRICT Context) const
{
	if (FAnimWeight::IsRelevant(Alpha) == false)
	{
		return false;
	}

	if (CVarEnableAnimPhys.GetValueOnAnyThread() == 0)
	{
		return false;
	}

	if (IsAnimPhysEnabled() == false)
	{
		return false;
	}

	if (NodeData.DeltaTime <= 0.0f)
	{
		return false;
	}

	if (WorkData.Cached.bHasCachedTransforms == false)
	{
		return false;
	}

	if (NodeData.AnimPhysDisabledStateByOwner == EAnimPhysDisabledState::DisableAll)
	{
		return false;
	}

	const FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();

	for (auto& Bone : BonesToSimulate)
	{
		if (Bone.IsValidToEvaluate(RequiredBones) == false)
		{
			return false;
		}
	}

	return true;
}

bool FAnimNode_AnimPhys::IsAnimPhysEnabled() const
{
	bool bEnabled = true;

	if (SetupSettings.Rule == FAnimPhysRule::EnabledWhenPhysBodyWasSimulated && NodeData.bPhysBodyWasSimulated == false)
	{
		bEnabled = false;
	}

#if WITH_EDITORONLY_DATA
	if (EditData.bInEditor)
	{
		// awalys enabled in editor
		bEnabled = true;
	}
#endif

	return bEnabled;
}

USkeletalMeshComponent* FAnimNode_AnimPhys::GetMeshComponent(USkeletalMeshComponent* RESTRICT OwnerComponent) const
{
	if (OwnerComponent == nullptr)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* LeaderPoseComponent = Cast<USkeletalMeshComponent>(OwnerComponent->LeaderPoseComponent.Get()))
	{
		return LeaderPoseComponent;
	}

	return OwnerComponent;
}

bool FAnimNode_AnimPhys::CanCacheBoneTransformsFrom(USkeletalMeshComponent* RESTRICT MeshComponent) const
{
	if (MeshComponent == nullptr)
	{
		return false;
	}

	if (MeshComponent->IsRegistered() == false)
	{
		return false;
	}

	if (MeshComponent->GetSkeletalMeshAsset() == nullptr)
	{
		return false;
	}

	if (MeshComponent->GetNumComponentSpaceTransforms() == 0)
	{
		return false;
	}

	return true;
}

void FAnimNode_AnimPhys::BuildCachedTransformIndexes(USkeletalMeshComponent* RESTRICT MeshComponent)
{
	check(MeshComponent);

	const FReferenceSkeleton& RefSkeleton = MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();

	TMap<int32, int32> BoneMap;

	const int32 RootBoneIndex(0);
	BoneMap.FindOrAdd(RootBoneIndex);

	for (auto& Bone : BonesToSimulate)
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone.BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			BoneMap.FindOrAdd(RefSkeleton.GetParentIndex(BoneIndex));
		}
	}

	for (const auto& Sphere : CollisionSettings.SphereColliders)
	{
		BoneMap.FindOrAdd(RefSkeleton.FindBoneIndex(Sphere.DrivingBone.BoneName));
	}

	for (const auto& Capsule : CollisionSettings.CapsuleColliders)
	{
		BoneMap.FindOrAdd(RefSkeleton.FindBoneIndex(Capsule.DrivingBone.BoneName));
	}

	for (const auto& Planer : CollisionSettings.PlanarColliders)
	{
		BoneMap.FindOrAdd(RefSkeleton.FindBoneIndex(Planer.DrivingBone.BoneName));
	}

	if (CollisionSettings.bCollidedWithSimulatedPhysBody)
	{
		const UPhysicsAsset* PhysicsAsset = MeshComponent->GetPhysicsAsset();
		const int32 NumBodies = PhysicsAsset ? PhysicsAsset->SkeletalBodySetups.Num() : 0;
		for (int32 BodyIndex = 0; BodyIndex < NumBodies; ++BodyIndex)
		{
			const USkeletalBodySetup* SkeletalBodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex].Get();
			if (SkeletalBodySetup)
			{
				BoneMap.FindOrAdd(RefSkeleton.FindBoneIndex(SkeletalBodySetup->BoneName));
			}
		}
	}

	BoneMap.Remove(INDEX_NONE);

	const int32 NumBones = RefSkeleton.GetNum();
	WorkData.Cached.NumComponentSpaceTransforms = NumBones;
	WorkData.Cached.ComponentSpaceTMs.Empty(BoneMap.Num());

	TArray<int32> ComponentSpaceIndexes;
	BoneMap.GenerateKeyArray(ComponentSpaceIndexes);	
	for (int32 ComponentSpaceIndex : ComponentSpaceIndexes)
	{
		WorkData.Cached.ComponentSpaceTMs.Add(FMeshPoseBoneIndex(ComponentSpaceIndex), FTransform::Identity);
	}

	WorkData.Collided.bValidColliders = false;
}

void FAnimNode_AnimPhys::BuildCachedAttachedTransformIndexes(USkeletalMeshComponent* RESTRICT MeshComponent)
{
	if (CollisionSettings.bCollidedWithAttachedMesh == false)
	{
		return;
	}

	USkeletalMeshComponent* AttachedMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent->GetAttachParent());
	if (AttachedMeshComponent == nullptr)
	{
		return;
	}

	USkeletalMesh* AttachedMeshAsset = AttachedMeshComponent->GetSkeletalMeshAsset();
	if (AttachedMeshAsset == nullptr)
	{
		return;
	}

	const FReferenceSkeleton& AttachedRefSkeleton = AttachedMeshAsset->GetRefSkeleton();

	TMap<int32, int32> AttachedBoneMap;

	UAnimPhysCollisionData* AttachedCollision = AttachedMeshAsset->GetAssetUserData<UAnimPhysCollisionData>();
	if (AttachedCollision)
	{
		for (const auto& Sphere : AttachedCollision->GetSphereColliders())
		{
			AttachedBoneMap.FindOrAdd(AttachedRefSkeleton.FindBoneIndex(Sphere.DrivingBone.BoneName));
		}

		for (const auto& Capsule : AttachedCollision->GetCapsuleColliders())
		{
			AttachedBoneMap.FindOrAdd(AttachedRefSkeleton.FindBoneIndex(Capsule.DrivingBone.BoneName));
		}
	}

	if (CollisionSettings.bCollidedWithSimulatedPhysBody)
	{
		const UPhysicsAsset* PhysicsAsset = AttachedMeshComponent->GetPhysicsAsset();
		const int32 NumBodies = PhysicsAsset ? PhysicsAsset->SkeletalBodySetups.Num() : 0;
		for (int32 BodyIndex = 0; BodyIndex < NumBodies; ++BodyIndex)
		{
			const USkeletalBodySetup* SkeletalBodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex].Get();
			if (SkeletalBodySetup)
			{
				AttachedBoneMap.FindOrAdd(AttachedRefSkeleton.FindBoneIndex(SkeletalBodySetup->BoneName));
			}
		}
	}

	AttachedBoneMap.Remove(INDEX_NONE);

	const int32 NumBones = AttachedRefSkeleton.GetNum();
	WorkData.Cached.NumAttachedComponentSpaceTransforms = NumBones;
	WorkData.Cached.AttachedComponentSpaceTMs.Empty(AttachedBoneMap.Num());
	WorkData.Cached.AttachedMesh = AttachedMeshAsset->GetFName();

	TArray<int32> ComponentSpaceIndexes;
	AttachedBoneMap.GenerateKeyArray(ComponentSpaceIndexes);
	for (int32 ComponentSpaceIndex : ComponentSpaceIndexes)
	{
		WorkData.Cached.AttachedComponentSpaceTMs.Add(FMeshPoseBoneIndex(ComponentSpaceIndex), FTransform::Identity);
	}

	WorkData.Collided.bValidColliders = false;
}

void FAnimNode_AnimPhys::CopyBoneTransformsFromComponent(USkeletalMeshComponent* RESTRICT MeshComponent)
{
	if (CanCacheBoneTransformsFrom(MeshComponent) == false)
	{
		WorkData.Cached.bHasCachedTransforms = false;
		return;
	}

	WorkData.Cached.bHasCachedTransforms = true;

	const TArray<FTransform>& ComponentSpaceTMs = GetAppropriateComponentSpaceTransforms(MeshComponent);
	const int32 NumSpaceBases = ComponentSpaceTMs.Num();

	if (WorkData.Cached.NumComponentSpaceTransforms != NumSpaceBases)
	{
		BuildCachedTransformIndexes(MeshComponent);
	}

	for (auto& CachedComponentSpaceTM : WorkData.Cached.ComponentSpaceTMs)
	{
		const FMeshPoseBoneIndex& ComponentSpaceIndex = CachedComponentSpaceTM.Key;
		if (ComponentSpaceTMs.IsValidIndex(ComponentSpaceIndex.GetInt()))
		{
			CachedComponentSpaceTM.Value = ComponentSpaceTMs[ComponentSpaceIndex.GetInt()];
		}
	}

	const int32 RootBoneIndex(0);
	if (ComponentSpaceTMs.IsValidIndex(RootBoneIndex))
	{
		NodeData.LastRootComponentTransform = ComponentSpaceTMs[RootBoneIndex];
	}
}

void FAnimNode_AnimPhys::CopyBoneTransformsFromAttachedComponent(USkeletalMeshComponent* RESTRICT MeshComponent)
{
	if (CollisionSettings.bCollidedWithAttachedMesh == false)
	{
		return;
	}

	USkeletalMeshComponent* AttachedMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent->GetAttachParent());
	if (AttachedMeshComponent == nullptr)
	{
		return;
	}

	USkeletalMesh* AttachedMeshAsset = AttachedMeshComponent->GetSkeletalMeshAsset();
	if (AttachedMeshAsset == nullptr)
	{
		return;
	}

	const TArray<FTransform>& AttachedComponentSpaceTMs = GetAppropriateComponentSpaceTransforms(AttachedMeshComponent);
	const int32 NumSpaceBases = AttachedComponentSpaceTMs.Num();
	if (WorkData.Cached.NumAttachedComponentSpaceTransforms != NumSpaceBases || WorkData.Cached.AttachedMesh != AttachedMeshAsset->GetFName())
	{
		BuildCachedAttachedTransformIndexes(MeshComponent);
	}

	FTransform BaseTM = AttachedMeshComponent->GetComponentTransform().GetRelativeTransform(MeshComponent->GetComponentTransform());

	for (auto& CachedAttachedComponentSpaceTM : WorkData.Cached.AttachedComponentSpaceTMs)
	{
		const FMeshPoseBoneIndex& ComponentSpaceIndex = CachedAttachedComponentSpaceTM.Key;
		if (AttachedComponentSpaceTMs.IsValidIndex(ComponentSpaceIndex.GetInt()))
		{
			CachedAttachedComponentSpaceTM.Value = AttachedComponentSpaceTMs[ComponentSpaceIndex.GetInt()] * BaseTM;
		}
	}
}

bool FAnimNode_AnimPhys::IsWindEnabled(const USkeletalMeshComponent* RESTRICT MeshComponent) const
{
	if (MeshComponent == nullptr)
	{
		return false;
	}

	if (MeshComponent->GetWorld() == nullptr)
	{
		return false;
	}

	if (ExternalForceSettings.bEnableWind == false)
	{
		return false;
	}

	if (MeshComponent->IsWindEnabled() == false)
	{
		return false;
	}

	if (MeshComponent->WasRecentlyRendered() == false)
	{
		return false;
	}

	return true;
}

void FAnimNode_AnimPhys::ComputeWind(const USkeletalMeshComponent* RESTRICT MeshComponent)
{
	if (IsWindEnabled(MeshComponent))
	{
		FVector BoneWorldPosition = WorkData.Simulated.SimulatedBones.IsValidIndex(0) ? WorkData.Simulated.SimulatedBones[0].PoseComponentSpaceTM.GetLocation() : FVector::ZeroVector;
		BoneWorldPosition = MeshComponent->GetComponentTransform().TransformPosition(BoneWorldPosition);

		FVector WindDirection = FVector::ZeroVector;
		float WindSpeed = 0.0f;
		float WindMinGustAmt = 0.0f;
		float WindMaxGustAmt = 0.0f;

		bool bEnabledWorldWind = true;
		if (ExternalForceSettings.WindChannels.bChannel1 || ExternalForceSettings.WindChannels.bChannel2)
		{
			if (IAnimPhysActorInterface* ActorInterface = Cast<IAnimPhysActorInterface>(MeshComponent->GetOwner()))
			{
				if (ActorInterface->GetWindStrengthCombinedGust(ExternalForceSettings.WindChannels, BoneWorldPosition, WindDirection, WindSpeed))
				{
					bEnabledWorldWind = false;
				}
			}
		}

		if (ExternalForceSettings.WindChannels.bWorld && bEnabledWorldWind)
		{
			UWorld* World = MeshComponent->GetWorld();
			FSceneInterface* Scene = World && World->Scene ? World->Scene : nullptr;
			if (Scene)
			{
				Scene->GetWindParameters_GameThread(BoneWorldPosition, WindDirection, WindSpeed, WindMinGustAmt, WindMaxGustAmt);
			}
		}

		WorkData.Forced.WindVelocity = WindDirection * WindSpeed * ExternalForceSettings.WindScale;
	
		NodeData.bWindWasEnabled = true;
	}
	else if(NodeData.bWindWasEnabled)
	{
		WorkData.Forced.WindVelocity = FVector::ZeroVector;

		NodeData.bWindWasEnabled = false;
	}
}

void FAnimNode_AnimPhys::ComputeComponentMovement(FPoseContext& RESTRICT Output)
{
	FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	WorkData.Moved.WorldToComponent = ComponentTransform.Inverse();

	if (NodeData.CurrentTeleportType == ETeleportType::None)
	{
		WorkData.Moved.WorldLocationDelta = WorkData.Moved.WorldToComponent.TransformPosition(WorkData.Moved.LastComponentTransform.GetLocation());
		if (WorkData.Moved.WorldLocationDelta.SizeSquared() > TeleportDistanceThreshold * TeleportDistanceThreshold)
		{
			WorkData.Moved.WorldLocationDelta = FVector::ZeroVector;
		}

		WorkData.Moved.WorldRotationDelta = WorkData.Moved.WorldToComponent.TransformRotation(WorkData.Moved.LastComponentTransform.GetRotation());
		if (TeleportRotationThreshold >= 0 && FMath::RadiansToDegrees(WorkData.Moved.WorldRotationDelta.GetAngle()) > TeleportRotationThreshold)
		{
			WorkData.Moved.WorldRotationDelta = FQuat::Identity;
		}
	}
	else
	{
		WorkData.Moved.WorldLocationDelta = FVector::ZeroVector;
		WorkData.Moved.WorldRotationDelta = FQuat::Identity;
	}

	WorkData.Moved.LastComponentTransform = ComponentTransform;
}

void FAnimNode_AnimPhys::ComputePoseTransform(FPoseContext& RESTRICT Output)
{
	const bool NeedsToCopyPose = (NodeData.bIsSequencerBound && NodeData.bHasEvaluated == false);
	if (NeedsToCopyPose)
	{
		CopyBoneTransformsFromPose(Output.Pose);
	}

	const bool bResetDynamics = (NodeData.CurrentTeleportType == ETeleportType::ResetPhysics);
	const FBoneContainer& RequiredBones = Output.Pose.GetBoneContainer();

	for (auto& Bone : WorkData.Simulated.SimulatedBones)
	{
		Bone.CompactPoseBoneIndex = RequiredBones.MakeCompactPoseIndex(Bone.MeshPoseBoneIndex);	
		Bone.bValid = false;

		WorkData.CalculatePoseComponentSpace(Output.Pose, SetupSettings, Bone);

		if (Bone.bValid == false)
		{
			continue;
		}

		if (bResetDynamics)
		{
			Bone.ComponentSpaceTM = Bone.PoseComponentSpaceTM;
			Bone.PrevLocation = Bone.PoseComponentSpaceTM.GetLocation();
		}
	}
}

void FAnimNode_AnimPhys::SimulateBones(FPoseContext& RESTRICT Output)
{
	WorkData.Simulated.bDampingEnabled = IsEnableDamping();
	WorkData.Simulated.bStiffnessEnabled = IsEnableStiffness();
	WorkData.Simulated.bGravityEnabled = IsEnableGravity();
	WorkData.Simulated.bWindEnabled = IsEnableWind();
	WorkData.Simulated.bWorldDampingEnabled = IsEnableWorldDamping();

	int32 MaxIterations = 1;

	const bool NeedsToWarmUp = (NodeData.bIsSequencerBound && NodeData.bHasEvaluated == false && EvaluationWarmUpTime > 0.0f);
	if (NeedsToWarmUp)
	{
		MaxIterations = (EvaluationWarmUpTime / MaxPhysicsDeltaTime);
		NodeData.DeltaTime = MaxPhysicsDeltaTime;
	}

	for(int32 NumIterations = 0; NumIterations< MaxIterations; ++NumIterations)
	{
		WorkData.SimulateBones(Output.Pose, NodeData.DeltaTime, NodeData.LastDeltaTime, TargetFramerate, SetupSettings, ExternalForceSettings, SmoothingSettings);

		NodeData.LastDeltaTime = NodeData.DeltaTime;
	}

	WorkData.ApplySimulateBones(Output.Pose);
}

const bool FAnimNode_AnimPhys::IsDisabledState(EAnimPhysDisabledState InState) const
{
	if ((NodeData.AnimPhysDisabledStateByOwner & InState) == EAnimPhysDisabledState::None)
	{
		return false;
	}

	return true;
}

const bool FAnimNode_AnimPhys::IsEnableDamping() const
{
	if (IsDisabledState(EAnimPhysDisabledState::DisableSimulation))
	{
		return false;
	}

	return (NodeData.DeltaTime <= MaxPhysicsDeltaTime);
}

const bool FAnimNode_AnimPhys::IsEnableStiffness() const
{
	if (IsDisabledState(EAnimPhysDisabledState::DisableSimulation))
	{
		return false;
	}

	return (NodeData.bPhysBodyWasSimulated == false);
}

const bool FAnimNode_AnimPhys::IsEnableGravity() const
{
	if (IsDisabledState(EAnimPhysDisabledState::DisableGravity))
	{
		return false;
	}

	return (ExternalForceSettings.Gravity.IsZero() == false || NodeData.bPhysBodyWasSimulated);
}

const bool FAnimNode_AnimPhys::IsEnableWind() const
{
	if (IsDisabledState(EAnimPhysDisabledState::DisableWind))
	{
		return false;
	}

	return (NodeData.bWindWasEnabled && NodeData.StartedSmoothingTimeSeconds == 0.0f);
}

const bool FAnimNode_AnimPhys::IsEnableWorldDamping() const
{
	if (IsDisabledState(EAnimPhysDisabledState::DisableSimulation))
	{
		return false;
	}

	return  (NodeData.CurrentTeleportType == ETeleportType::None);
}

bool FAnimNode_AnimPhys::TryGetCollisionComponentSpaceTransform(FTransform& RESTRICT ComponentSpaceTM, const FAnimPhys_CollidedBase_WorkData& RESTRICT Collider) const
{
	if (Collider.MeshPoseBoneIndex.IsValid() == false)
	{
		return false;
	}

	if (WorkData.Cached.bHasCachedTransforms == false)
	{
		return false;
	}

	if (Collider.bFromAttachedMesh)
	{
		const FTransform* Found = WorkData.Cached.AttachedComponentSpaceTMs.Find(Collider.MeshPoseBoneIndex);
		if (Found == nullptr)
		{
			return false;
		}

		ComponentSpaceTM = (*Found);
	}
	else
	{
		const FTransform* Found = WorkData.Cached.ComponentSpaceTMs.Find(Collider.MeshPoseBoneIndex);
		if (Found == nullptr)
		{
			return false;
		}

		ComponentSpaceTM = (*Found);
	}

	if (Collider.bHasOffset)
	{
		ComponentSpaceTM = Collider.OffsetTransform * ComponentSpaceTM;
	}

	return true;
}

void FAnimNode_AnimPhys::BuildSphereColliders(const FReferenceSkeleton& RefSkeleton, const bool bFromAttachedMesh, const TArray<FAnimPhysSphereCollider>& SphereColliders)
{
	for (const auto& Sphere : SphereColliders)
	{
		const FMeshPoseBoneIndex MeshPoseBoneIndex(RefSkeleton.FindBoneIndex(Sphere.DrivingBone.BoneName));
		if (MeshPoseBoneIndex.IsValid() == false)
		{
			continue;
		}

		FAnimPhys_CollidedSphere_WorkData CollidedSphere;
		CollidedSphere.MeshPoseBoneIndex = MeshPoseBoneIndex;
		CollidedSphere.LimitDistance = SetupSettings.Radius + Sphere.Radius;
		CollidedSphere.LimitDistanceSquared = (CollidedSphere.LimitDistance * CollidedSphere.LimitDistance);
		CollidedSphere.bFromAttachedMesh = bFromAttachedMesh;

		if (Sphere.OffsetLocation.IsZero() == false)
		{
			CollidedSphere.OffsetTransform.AddToTranslation(Sphere.OffsetLocation);
			CollidedSphere.bHasOffset = true;
		}

#if WITH_EDITORONLY_DATA
		CollidedSphere.DebugRadius = Sphere.Radius;
#endif

		WorkData.Collided.Spheres.Add(CollidedSphere);
	}
}

void FAnimNode_AnimPhys::BuildCapsuleColliders(const FReferenceSkeleton& RefSkeleton, const bool bFromAttachedMesh, const TArray<FAnimPhysCapsuleCollider>& CapsuleColliders)
{
	for (const auto& Capsule : CapsuleColliders)
	{
		const FMeshPoseBoneIndex MeshPoseBoneIndex(RefSkeleton.FindBoneIndex(Capsule.DrivingBone.BoneName));
		if (MeshPoseBoneIndex.IsValid() == false)
		{
			continue;
		}

		FAnimPhys_CollidedCapsule_WorkData CollidedCapsule;
		CollidedCapsule.MeshPoseBoneIndex = MeshPoseBoneIndex;
		CollidedCapsule.LimitDistance = SetupSettings.Radius + Capsule.Radius;
		CollidedCapsule.LimitDistanceSquared = (CollidedCapsule.LimitDistance * CollidedCapsule.LimitDistance);
		CollidedCapsule.HalfHeight = (Capsule.Length * 0.5f);
		CollidedCapsule.bFromAttachedMesh = bFromAttachedMesh;

		if (Capsule.OffsetRotation.IsZero() == false)
		{
			CollidedCapsule.OffsetTransform.SetRotation(Capsule.OffsetRotation.Quaternion());
			CollidedCapsule.bHasOffset = true;
		}
		if (Capsule.OffsetLocation.IsZero() == false)
		{
			CollidedCapsule.OffsetTransform.AddToTranslation(Capsule.OffsetLocation);
			CollidedCapsule.bHasOffset = true;
		}

#if WITH_EDITORONLY_DATA
		CollidedCapsule.DebugRadius = Capsule.Radius;
#endif

		WorkData.Collided.Capsules.Add(CollidedCapsule);
	}
}

void FAnimNode_AnimPhys::BuildPlanarColliders(const FReferenceSkeleton& RefSkeleton, const bool bFromAttachedMesh, const TArray<FAnimPhysPlanarCollider>& PlanarColliders)
{
	const float PlanarDepth = FMath::Max(1.0f, SetupSettings.Radius);

	for (const auto& Planar : PlanarColliders)
	{
		const FMeshPoseBoneIndex MeshPoseBoneIndex(RefSkeleton.FindBoneIndex(Planar.DrivingBone.BoneName));
		if (MeshPoseBoneIndex.IsValid() == false)
		{
			continue;
		}

		FAnimPhys_CollidedPlanar_WorkData CollidedPlanar;
		CollidedPlanar.MeshPoseBoneIndex = MeshPoseBoneIndex;
		CollidedPlanar.LimitDistance = PlanarDepth;
		CollidedPlanar.LimitDistanceSquared = (PlanarDepth * PlanarDepth);
		CollidedPlanar.bFromAttachedMesh = bFromAttachedMesh;

		if (Planar.OffsetRotation.IsZero() == false)
		{
			CollidedPlanar.OffsetTransform.SetRotation(Planar.OffsetRotation.Quaternion());
			CollidedPlanar.bHasOffset = true;
		}
		if (Planar.OffsetLocation.IsZero() == false)
		{
			CollidedPlanar.OffsetTransform.AddToTranslation(Planar.OffsetLocation);
			CollidedPlanar.bHasOffset = true;
		}

		WorkData.Collided.Planars.Add(CollidedPlanar);
	}
}

void FAnimNode_AnimPhys::BuildColliders(const USkeletalMeshComponent* RESTRICT MeshComponent)
{
	if (MeshComponent == nullptr)
	{
		return;
	}

	if (MeshComponent->GetSkeletalMeshAsset() == nullptr)
	{
		return;
	}

	WorkData.Collided.Spheres.Empty();
	WorkData.Collided.Capsules.Empty();
	WorkData.Collided.Planars.Empty();

	const FReferenceSkeleton& RefSkeleton = MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	BuildSphereColliders(RefSkeleton, false, CollisionSettings.SphereColliders);
	BuildCapsuleColliders(RefSkeleton, false, CollisionSettings.CapsuleColliders);
	BuildPlanarColliders(RefSkeleton, false, CollisionSettings.PlanarColliders);

	if (CollisionSettings.bCollidedWithAttachedMesh)
	{
		USkeletalMeshComponent* AttachedMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent->GetAttachParent());
		UAnimPhysCollisionData* AttachedCollision = (AttachedMeshComponent && AttachedMeshComponent->GetSkeletalMeshAsset()) ? AttachedMeshComponent->GetSkeletalMeshAsset()->GetAssetUserData<UAnimPhysCollisionData>() : nullptr;
		if (AttachedCollision)
		{
			const FReferenceSkeleton& AttachedRefSkeleton = AttachedMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
			BuildSphereColliders(AttachedRefSkeleton, true, AttachedCollision->GetSphereColliders());
			BuildCapsuleColliders(AttachedRefSkeleton, true, AttachedCollision->GetCapsuleColliders());
		}
	}

	if (CollisionSettings.bCollidedWithFloor)
	{
		const float FloorDepth = FMath::Max(1.0f, SetupSettings.Radius);
		WorkData.Collided.Floor.LimitDistance = FloorDepth;
		WorkData.Collided.Floor.LimitDistanceSquared = (FloorDepth * FloorDepth);
	}

	WorkData.Collided.bValidColliders = true;
}

void FAnimNode_AnimPhys::ComputeSphereColliderTransform(TArray<FAnimPhys_CollidedSphere_WorkData>& RESTRICT CollidedSpheres)
{
	for (auto& CollidedSphere : CollidedSpheres)
	{
		if (CollidedSphere.LimitDistanceSquared <= 0.0f)
		{
			continue;
		}

		FTransform SphereTransform = FTransform::Identity;
		if (TryGetCollisionComponentSpaceTransform(SphereTransform, CollidedSphere) == false)
		{
			continue;
		}

		CollidedSphere.bValid = true;
		CollidedSphere.Center = SphereTransform.GetLocation();

#if WITH_EDITORONLY_DATA
		CollidedSphere.DebugTransform = SphereTransform;
#endif
	}
}

void FAnimNode_AnimPhys::ComputeCapsuleColliderTransform(TArray<FAnimPhys_CollidedCapsule_WorkData>& RESTRICT CollidedCapsules)
{
	for (auto& CollidedCapsule : CollidedCapsules)
	{
		if (CollidedCapsule.LimitDistanceSquared <= 0.0f)
		{
			continue;
		}

		if (CollidedCapsule.HalfHeight <= 0.0f)
		{
			continue;
		}

		FTransform CapsuleTransform = FTransform::Identity;
		if (TryGetCollisionComponentSpaceTransform(CapsuleTransform, CollidedCapsule) == false)
		{
			continue;
		}

		CollidedCapsule.bValid = true;
		CollidedCapsule.SegmentStart = CapsuleTransform.GetLocation() + CapsuleTransform.GetRotation().GetAxisZ() * CollidedCapsule.HalfHeight;
		CollidedCapsule.SegmentEnd = CapsuleTransform.GetLocation() + CapsuleTransform.GetRotation().GetAxisZ() * (-CollidedCapsule.HalfHeight);

#if WITH_EDITORONLY_DATA
		CollidedCapsule.DebugTransform = CapsuleTransform;
#endif
	}
}

void FAnimNode_AnimPhys::ComputeColliderTransform(FPoseContext& RESTRICT Output)
{
	ComputeSphereColliderTransform(WorkData.Collided.Spheres);
	ComputeCapsuleColliderTransform(WorkData.Collided.Capsules);

	for (auto& CollidedPlanar : WorkData.Collided.Planars)
	{
		if (CollidedPlanar.LimitDistanceSquared <= 0.0f)
		{
			continue;
		}

		FTransform PlanarTransform = FTransform::Identity;
		if (TryGetCollisionComponentSpaceTransform(PlanarTransform, CollidedPlanar) == false)
		{
			continue;
		}

		CollidedPlanar.bValid = true;
		CollidedPlanar.Plane = FPlane(PlanarTransform.GetLocation(), PlanarTransform.GetRotation().GetUpVector());

#if WITH_EDITORONLY_DATA
		CollidedPlanar.DebugTransform = PlanarTransform;
#endif
	}

	if (WorkData.Collided.Floor.bValid)
	{
		WorkData.Collided.Floor.Plane = FPlane(WorkData.Moved.WorldToComponent.TransformPosition(WorkData.Collided.Floor.ImpactPoint), WorkData.Moved.WorldToComponent.TransformVector(WorkData.Collided.Floor.ImpactNormal).GetSafeNormal());
	}

	if (CollisionSettings.bCollidedWithSimulatedPhysBody)
	{
		ComputeSphereColliderTransform(WorkData.Collided.PhysBodySpheres);
		ComputeCapsuleColliderTransform(WorkData.Collided.PhysBodyCapsules);
	}
}

void FAnimNode_AnimPhys::CopyBoneTransformsFromPose(const FCompactPose& RESTRICT Pose)
{
	const FBoneContainer& RequiredBones = Pose.GetBoneContainer();

	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Pose);

	for (auto& CachedComponentSpaceTM : WorkData.Cached.ComponentSpaceTMs)
	{
		FCompactPoseBoneIndex CompactPoseBoneIndex = RequiredBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(CachedComponentSpaceTM.Key));
		if (CompactPoseBoneIndex.IsValid() == false)
		{
			continue;
		}

		CachedComponentSpaceTM.Value = CSPose.GetComponentSpaceTransform(CompactPoseBoneIndex) * NodeData.LastRootComponentTransform;
	}
}

bool FAnimNode_AnimPhys::IsAllPhysBodiesSimulated(const USkeletalMeshComponent* RESTRICT MeshComponent) const
{
	const bool bWantsToCheck = (CollisionSettings.bCollidedWithSimulatedPhysBody || SetupSettings.Rule == FAnimPhysRule::EnabledWhenPhysBodyWasSimulated);
	if(bWantsToCheck == false)
	{
		return false;
	}

	if (MeshComponent == nullptr)
	{
		return false;
	}

	if (MeshComponent->IsRegistered() == false)
	{
		return false;
	}

	const UPhysicsAsset* PhysicsAsset = MeshComponent->GetPhysicsAsset();
	if (PhysicsAsset == nullptr)
	{
		return false;
	}

	if (MeshComponent->Bodies.IsEmpty())
	{
		return false;
	}

	for (const FBodyInstance* Body : MeshComponent->Bodies)
	{
		if (Body == nullptr)
		{
			return false;
		}

		if (Body->IsValidBodyInstance() == false)
		{
			return false;
		}

		if (Body->bSimulatePhysics == false)
		{
			return false;
		}
	}

	return true;
}

void FAnimNode_AnimPhys::ComputePhysBody(const USkeletalMeshComponent* RESTRICT MeshComponent)
{
	const bool bIsAllPhysBodiesSimulated = IsAllPhysBodiesSimulated(MeshComponent);

	const bool ShouldBuildPhysBodyColliders = (NodeData.bPhysBodyWasSimulated == false && bIsAllPhysBodiesSimulated);
	if (ShouldBuildPhysBodyColliders)
	{
		NodeData.bPhysBodyWasSimulated = true;
	}

	const bool ShouldInvalidatePhysBodyColliders = (NodeData.bPhysBodyWasSimulated && bIsAllPhysBodiesSimulated == false);
	if (ShouldInvalidatePhysBodyColliders)
	{
		NodeData.bPhysBodyWasSimulated = false;
	}

	if (IsEnableGravity() && NodeData.bPhysBodyWasSimulated)
	{
		WorkData.Forced.GravityZ = UPhysicsSettings::Get()->DefaultGravityZ;
	}
	else
	{
		WorkData.Forced.GravityZ = 0.0f;
	}
}

void FAnimNode_AnimPhys::ComputeFloor(const USkeletalMeshComponent* RESTRICT MeshComponent)
{
	if (CollisionSettings.bCollidedWithFloor == false)
	{
		return;
	}

	FHitResult CurrentFloor;
	CurrentFloor.bBlockingHit = false;

	ACharacter* CharacterOwner = MeshComponent ? Cast<ACharacter>(MeshComponent->GetOwner()) : nullptr;
	UCharacterMovementComponent* Movementcomponent = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	if (Movementcomponent)
	{
		if (Movementcomponent->MovementMode == MOVE_Walking && Movementcomponent->CurrentFloor.bBlockingHit)
		{
			CurrentFloor.bBlockingHit = true;
			CurrentFloor.ImpactPoint = Movementcomponent->CurrentFloor.HitResult.ImpactPoint;
			CurrentFloor.ImpactNormal = Movementcomponent->CurrentFloor.HitResult.ImpactNormal;
		}

		if (Movementcomponent->MovementMode == MOVE_NavWalking && Movementcomponent->CachedProjectedNavMeshHitResult.bBlockingHit)
		{
			CurrentFloor.bBlockingHit = true;
			CurrentFloor.ImpactPoint = Movementcomponent->CachedProjectedNavMeshHitResult.ImpactPoint;
			CurrentFloor.ImpactNormal = Movementcomponent->CachedProjectedNavMeshHitResult.ImpactNormal;
		}

		WorkData.Moved.OnGround = Movementcomponent->IsMovingOnGround();
	}
	else
		WorkData.Moved.OnGround = false;
	{
	}

	if (CurrentFloor.bBlockingHit)
	{
		WorkData.Collided.Floor.ImpactPoint = CurrentFloor.ImpactPoint;
		WorkData.Collided.Floor.ImpactNormal = CurrentFloor.ImpactNormal;
		WorkData.Collided.Floor.bValid = true;
	}
	else
	{
		WorkData.Collided.Floor.bValid = false;
	}
}

void FAnimNode_AnimPhys::BuildPhysBodyCollidersFromPhysicsAsset(const FReferenceSkeleton& RefSkeleton, const UPhysicsAsset* PhysicsAsset, const bool bFromAttachedMesh)
{
	const int32 NumBodies = PhysicsAsset ? PhysicsAsset->SkeletalBodySetups.Num() : 0;
	for (int32 BodyIndex = 0; BodyIndex < NumBodies; ++BodyIndex)
	{
		const USkeletalBodySetup* SkeletalBodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex].Get();
		if (SkeletalBodySetup == nullptr)
		{
			continue;
		}

		const FMeshPoseBoneIndex MeshPoseBoneIndex(RefSkeleton.FindBoneIndex(SkeletalBodySetup->BoneName));
		if (MeshPoseBoneIndex.IsValid() == false)
		{
			continue;
		}

		for (const auto& Sphere : SkeletalBodySetup->AggGeom.SphereElems)
		{
			FAnimPhys_CollidedSphere_WorkData CollidedSphere;
			CollidedSphere.MeshPoseBoneIndex = MeshPoseBoneIndex;
			CollidedSphere.LimitDistance = SetupSettings.Radius + (Sphere.Radius * CollisionSettings.PhysBodyScale);
			CollidedSphere.LimitDistanceSquared = (CollidedSphere.LimitDistance * CollidedSphere.LimitDistance);
			CollidedSphere.OffsetTransform = Sphere.GetTransform();
			CollidedSphere.bHasOffset = true;
			CollidedSphere.bFromAttachedMesh = bFromAttachedMesh;

#if WITH_EDITORONLY_DATA
			CollidedSphere.DebugRadius = (CollidedSphere.LimitDistance - SetupSettings.Radius);
#endif

			WorkData.Collided.PhysBodySpheres.Add(CollidedSphere);
		}

		for (const auto& Capsule : SkeletalBodySetup->AggGeom.SphylElems)
		{
			FAnimPhys_CollidedCapsule_WorkData CollidedCapsule;
			CollidedCapsule.MeshPoseBoneIndex = MeshPoseBoneIndex;
			CollidedCapsule.LimitDistance = SetupSettings.Radius + (Capsule.Radius * CollisionSettings.PhysBodyScale);
			CollidedCapsule.LimitDistanceSquared = (CollidedCapsule.LimitDistance * CollidedCapsule.LimitDistance);
			CollidedCapsule.HalfHeight = (Capsule.Length * 0.5f);
			CollidedCapsule.OffsetTransform = Capsule.GetTransform();
			CollidedCapsule.bHasOffset = true;
			CollidedCapsule.bFromAttachedMesh = bFromAttachedMesh;

#if WITH_EDITORONLY_DATA
			CollidedCapsule.DebugRadius = (CollidedCapsule.LimitDistance - SetupSettings.Radius);
#endif

			WorkData.Collided.PhysBodyCapsules.Add(CollidedCapsule);
		}
	}
}

void FAnimNode_AnimPhys::BuildPhysBodyColliders(const USkeletalMeshComponent* RESTRICT MeshComponent)
{
	if (MeshComponent == nullptr)
	{
		return;
	}

	if (MeshComponent->GetSkeletalMeshAsset() == nullptr)
	{
		return;
	}

	WorkData.Collided.PhysBodySpheres.Empty();
	WorkData.Collided.PhysBodyCapsules.Empty();

	const FReferenceSkeleton& RefSkeleton = MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	const UPhysicsAsset* PhysicsAsset = MeshComponent->GetPhysicsAsset();
	BuildPhysBodyCollidersFromPhysicsAsset(RefSkeleton, PhysicsAsset, false);

	USkeletalMeshComponent* AttachedMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent->GetAttachParent());
	if (CollisionSettings.bCollidedWithAttachedMesh && AttachedMeshComponent && AttachedMeshComponent->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& AttachedRefSkeleton = AttachedMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
		const UPhysicsAsset* AttachedPhysicsAsset = AttachedMeshComponent->GetPhysicsAsset();

		BuildPhysBodyCollidersFromPhysicsAsset(AttachedRefSkeleton, AttachedPhysicsAsset, true);
	}

	WorkData.Collided.bValidPhysBodyColliders = true;
}

void FAnimNode_AnimPhys::CheckTeleport(FPoseContext& RESTRICT Output)
{
	// @ref : FAnimNode_RigidBody::EvaluateSkeletalControl_AnyThread
	if (EvaluationResetTime > 0.0f && NodeData.LastEvalTimeSeconds > 0.0f && NodeData.bHasEvaluated)
	{
		if (NodeData.WorldTimeSeconds - (NodeData.LastEvalTimeSeconds + NodeData.AccumulatedDeltaTime) > EvaluationResetTime)
		{
			ConditionalSetTeleportType(ETeleportType::ResetPhysics, NodeData.CurrentTeleportType);

			if (EvaluationSuspendSimTimeAfterReset > 0.0f)
			{
				NodeData.SuspendedSimTimeSeconds = (NodeData.WorldTimeSeconds + EvaluationSuspendSimTimeAfterReset);
			}
		}
		else if (EvaluationSuspendSimTimeAfterReset > 0.0f && NodeData.WorldTimeSeconds <= NodeData.SuspendedSimTimeSeconds)
		{
			ConditionalSetTeleportType(ETeleportType::TeleportPhysics, NodeData.CurrentTeleportType);
		}
	}

	// Update the evaluation time to the current time
	NodeData.LastEvalTimeSeconds = NodeData.WorldTimeSeconds;

	const bool NeedsToTeleportPhysics = (NodeData.bIsSequencerBound && ExternalForceSettings.Gravity.IsZero());
	if (NeedsToTeleportPhysics)
	{
		ConditionalSetTeleportType(ETeleportType::TeleportPhysics, NodeData.CurrentTeleportType);
	
		FCompactPoseBoneIndex RootBoneIndex(0);
		if (Output.Pose.IsValidIndex(RootBoneIndex))
		{
			const FTransform RootBoneTM = Output.Pose[RootBoneIndex];
			const bool bIsRootBoneIdentity = RootBoneTM.Equals(FTransform::Identity);
			if (bIsRootBoneIdentity != NodeData.bWasRootBoneIdentity)
			{
				ConditionalSetTeleportType(ETeleportType::ResetPhysics, NodeData.CurrentTeleportType);
			}
			NodeData.bWasRootBoneIdentity = bIsRootBoneIdentity;
		}

		const FTransform ActorTransform = Output.AnimInstanceProxy->GetActorTransform();
		if(ActorTransform.Equals(NodeData.LastActorTransform) == false)
		{
			ConditionalSetTeleportType(ETeleportType::ResetPhysics, NodeData.CurrentTeleportType);
		}
		NodeData.LastActorTransform = ActorTransform;
	}

	if (NodeData.CurrentTeleportType == ETeleportType::ResetPhysics)
	{
		NodeData.bHasEvaluated = false;
	}
}

void FAnimNode_AnimPhys::ConditionalSetTeleportType(ETeleportType InTeleportType, ETeleportType& OutTeleportType)
{
	// Request an initialization. Teleport type can only go higher - i.e. if we have requested a reset, then a teleport will still reset fully
	OutTeleportType = ((InTeleportType > OutTeleportType) ? InTeleportType : OutTeleportType);
}

const TArray<FTransform>& FAnimNode_AnimPhys::GetAppropriateComponentSpaceTransforms(USkeletalMeshComponent* RESTRICT ComponentToCopyFrom) const
{
	check(ComponentToCopyFrom);

	const TArray<FTransform>& CachedComponentSpaceTransforms = ComponentToCopyFrom->GetCachedComponentSpaceTransforms();
	const bool bArraySizesMatch = CachedComponentSpaceTransforms.Num() == ComponentToCopyFrom->GetComponentSpaceTransforms().Num();

	if (WorkData.Cached.bInterpolated && bArraySizesMatch)
	{
		return CachedComponentSpaceTransforms;
	}

	return ComponentToCopyFrom->GetComponentSpaceTransforms();
}

#if !UE_BUILD_SHIPPING
void FAnimNode_AnimPhys::DebugDraw(const USkeletalMeshComponent* RESTRICT MeshComponent)
{
	UWorld* World = MeshComponent->GetWorld();
	const float DebugTime = -1.0f;

	FTransform ToWorld = MeshComponent->GetComponentTransform();

	for (auto& Bone : WorkData.Simulated.SimulatedBones)
	{
		const FVector PoseLocation = ToWorld.TransformPosition(Bone.PoseComponentSpaceTM.GetLocation());
		DrawDebugPoint(World, PoseLocation, 5.0f, FColor::White, false, DebugTime);

		const FVector BoneLocation = ToWorld.TransformPosition(Bone.ComponentSpaceTM.GetLocation());
		if (SetupSettings.Radius > 0.0f)
		{
			const FColor Color = (Bone.NumChildren == 0 && Bone.MeshPoseBoneIndex.IsValid() == false) ? FColor::Red : FColor::Yellow;
			DrawDebugSphere(World, BoneLocation, SetupSettings.Radius, 16, Color, false, DebugTime);
		}

		if (WorkData.Simulated.SimulatedBones.IsValidIndex(Bone.ParentIndex))
		{
			const FVector ParentBoneLocation = ToWorld.TransformPosition(WorkData.Simulated.SimulatedBones[Bone.ParentIndex].ComponentSpaceTM.GetLocation());
			DrawDebugLine(World, BoneLocation, ParentBoneLocation, FColor::White, false, DebugTime);
		}
	}

	for (const auto& Sphere : WorkData.Collided.Spheres)
	{
		if (Sphere.bValid == false)
		{
			continue;
		}

		const FColor Color = Sphere.bFromAttachedMesh ? FColor::Cyan : FColor::Blue;
		const float Radius = (Sphere.LimitDistance - SetupSettings.Radius);
		DrawDebugSphere(World, ToWorld.TransformPosition(Sphere.Center), Radius, 16, Color, false, DebugTime);
	}

	for (const auto& Capsule : WorkData.Collided.Capsules)
	{
		if (Capsule.bValid == false)
		{
			continue;
		}

		const FColor Color = Capsule.bFromAttachedMesh ? FColor::Cyan : FColor::Blue;
		const float Radius = (Capsule.LimitDistance - SetupSettings.Radius);
		DrawDebugCylinder(World, ToWorld.TransformPosition(Capsule.SegmentStart), ToWorld.TransformPosition(Capsule.SegmentEnd), Radius, 16, Color, false, DebugTime);
	}

	for (const auto& Sphere : WorkData.Collided.PhysBodySpheres)
	{
		if (Sphere.bValid == false)
		{
			continue;
		}

		const FColor Color = Sphere.bFromAttachedMesh ? FColor::Cyan : FColor::Blue;
		const float Radius = (Sphere.LimitDistance - SetupSettings.Radius);
		DrawDebugSphere(World, ToWorld.TransformPosition(Sphere.Center), Radius, 16, Color, false, DebugTime);
	}

	for (const auto& Capsule : WorkData.Collided.PhysBodyCapsules)
	{
		if (Capsule.bValid == false)
		{
			continue;
		}

		const FColor Color = Capsule.bFromAttachedMesh ? FColor::Cyan : FColor::Blue;
		const float Radius = (Capsule.LimitDistance - SetupSettings.Radius);
		DrawDebugCylinder(World, ToWorld.TransformPosition(Capsule.SegmentStart), ToWorld.TransformPosition(Capsule.SegmentEnd), Radius, 16, Color, false, DebugTime);
	}
}
#endif