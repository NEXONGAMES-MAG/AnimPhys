// Copyright NEXON Games Co., MIT License
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AnimPhysWorkData.h"
#include "AnimPhysInterface.generated.h"

UINTERFACE(MinimalApi, meta = (CannotImplementInterfaceInBlueprint))
class UAnimPhysMeshComponentInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IAnimPhysMeshComponentInterface : public IInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual void PushDisableAnimPhys(EAnimPhysDisabledState InState) = 0;
	virtual void PopDisableAnimPhys() = 0;
	virtual EAnimPhysDisabledState GetDesiredAnimPhysDisabledState() const = 0;

	virtual void ApplyImpulseToAnimPhys(const FName& InBoneName, const FVector& InImpulse) = 0;
	virtual const FVector& GetAccumulatedImpulsesToAnimPhys(const FName& InBoneName) const = 0;
};

UINTERFACE(MinimalApi, meta = (CannotImplementInterfaceInBlueprint))
class UAnimPhysActorInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IAnimPhysActorInterface : public IInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual bool GetWindStrengthCombinedGust(const FWindChannels& InWindChannels, const FVector& InPosition, FVector& OutDirection, float& OutSpeed) = 0;
};