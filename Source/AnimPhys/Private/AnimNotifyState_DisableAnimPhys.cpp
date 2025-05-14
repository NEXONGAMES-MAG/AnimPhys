// Copyright NEXON Games Co., MIT License
#include "AnimNotifyState_DisableAnimPhys.h"

UAnimNotifyState_DisableAnimPhys::UAnimNotifyState_DisableAnimPhys(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimNotifyState_DisableAnimPhys::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (IAnimPhysMeshComponentInterface* MeshComponentInterface = TryGetTargetAnimPhysInterface(MeshComp))
	{
		MeshComponentInterface->PushDisableAnimPhys(DiresedState);
	}
}

void UAnimNotifyState_DisableAnimPhys::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (IAnimPhysMeshComponentInterface* MeshComponentInterface = TryGetTargetAnimPhysInterface(MeshComp))
	{
		MeshComponentInterface->PopDisableAnimPhys();
	}
}

IAnimPhysMeshComponentInterface* UAnimNotifyState_DisableAnimPhys::TryGetTargetAnimPhysInterface(USkeletalMeshComponent* MeshComp) const
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