// Copyright NEXON Games Co., MIT License
#pragma once
#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Animation/BoneSocketReference.h"
#include "AnimPhysCollisionData.generated.h"

USTRUCT()
struct FAnimPhysColliderBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FBoneReference DrivingBone;

	UPROPERTY(EditAnywhere)
	FVector OffsetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "-360", ClampMax = "360"))
	FRotator OffsetRotation = FRotator::ZeroRotator;
};

USTRUCT()
struct FAnimPhysSphereCollider : public FAnimPhysColliderBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float Radius = 0.0f;
};

USTRUCT()
struct FAnimPhysCapsuleCollider : public FAnimPhysColliderBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float Radius = 0.0f;

	UPROPERTY(EditAnywhere)
	float Length = 0.0f;
};

USTRUCT()
struct FAnimPhysPlanarCollider : public FAnimPhysColliderBase
{
	GENERATED_BODY()
};

UCLASS()
class ANIMPHYS_API UAnimPhysCollisionData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UAnimPhysCollisionData(const FObjectInitializer& ObjectInitializer);

	const TArray<FAnimPhysSphereCollider>& GetSphereColliders() const { return SphereColliders; }
	const TArray<FAnimPhysCapsuleCollider>& GetCapsuleColliders() const { return CapsuleColliders; }
	
private:
	UPROPERTY(EditAnywhere)
	TArray<FAnimPhysSphereCollider> SphereColliders;

	UPROPERTY(EditAnywhere)
	TArray<FAnimPhysCapsuleCollider> CapsuleColliders;
};
