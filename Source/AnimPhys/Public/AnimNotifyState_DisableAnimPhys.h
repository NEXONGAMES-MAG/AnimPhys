// Copyright NEXON Games Co., MIT License
#pragma once
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimPhysWorkData.h"
#include "AnimPhysInterface.h"
#include "AnimNotifyState_DisableAnimPhys.generated.h"

class USkeleton;

UCLASS(meta = (DisplayName = "DisableAnimPhys"))
class UAnimNotifyState_DisableAnimPhys : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	TSoftObjectPtr<USkeleton> TargetSkeleton;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	EAnimPhysDisabledState DiresedState = EAnimPhysDisabledState::DisableAll;

	UAnimNotifyState_DisableAnimPhys(const FObjectInitializer& ObjectInitializer);

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	IAnimPhysMeshComponentInterface* TryGetTargetAnimPhysInterface(USkeletalMeshComponent* MeshComp) const;
};
