// Copyright NEXON Games Co., MIT License
#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimPhysInterface.h"
#include "AnimNotify_AddImpulseToAnimPhys.generated.h"

class USkeleton;

UCLASS(meta = (DisplayName = "AddImpulseToAnimPhys"))
class UAnimNotify_AddImpulseToAnimPhys : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	TSoftObjectPtr<USkeleton> TargetSkeleton;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	FVector Impulse = FVector::ZeroVector;

	UAnimNotify_AddImpulseToAnimPhys(const FObjectInitializer& ObjectInitializer);

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	IAnimPhysMeshComponentInterface* TryGetTargetAnimPhysInterface(USkeletalMeshComponent* MeshComp) const;
};
