// Copyright NEXON Games Co., MIT License
#include "AnimNotify_AddImpulseToAnimPhys.h"

UAnimNotify_AddImpulseToAnimPhys::UAnimNotify_AddImpulseToAnimPhys(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimNotify_AddImpulseToAnimPhys::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (IAnimPhysMeshComponentInterface* MeshComponentInterface = TryGetTargetAnimPhysInterface(MeshComp))
	{
		MeshComponentInterface->ApplyImpulseToAnimPhys(BoneName, Impulse);
	}
}

IAnimPhysMeshComponentInterface* UAnimNotify_AddImpulseToAnimPhys::TryGetTargetAnimPhysInterface(USkeletalMeshComponent* MeshComp) const
{
	if (TargetSkeleton.IsNull())
	{
		return nullptr;
	}

	if (MeshComp == nullptr)
	{
		return nullptr;
	}

	FSoftObjectPath PathToSkeleton = (MeshComp->GetSkeletalMeshAsset()) ? MeshComp->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	if (TargetSkeleton.ToSoftObjectPath() == PathToSkeleton)
	{
		return Cast<IAnimPhysMeshComponentInterface>(MeshComp);
	}

	for (USceneComponent* ChildComponent : MeshComp->GetAttachChildren())
	{
		USkeletalMeshComponent* ChildMeshComponent = Cast<USkeletalMeshComponent>(ChildComponent);
		if (ChildMeshComponent == nullptr)
		{
			continue;
		}

		PathToSkeleton = (ChildMeshComponent->GetSkeletalMeshAsset()) ? ChildMeshComponent->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
		if (TargetSkeleton.ToSoftObjectPath() == PathToSkeleton)
		{
			return Cast<IAnimPhysMeshComponentInterface>(ChildMeshComponent);
		}
	}

	return nullptr;
}