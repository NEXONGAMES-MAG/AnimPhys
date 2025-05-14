// Copyright NEXON Games Co., MIT License
#pragma once
#include "AnimGraphNode_Base.h"
#include "AnimNode_AnimPhys.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "AnimGraphNode_AnimPhys.generated.h"

UENUM(BlueprintType)
enum class EAnimPhysDebugType : uint8
{
	None,
	SphereCollider,
	CapsuleCollider,
	PlanarCollider,
};

USTRUCT()
struct FAnimPhysDebugTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EAnimPhysDebugType Type = EAnimPhysDebugType::None;

	UPROPERTY(EditAnywhere)
	FName BoneName = NAME_None;

	UPROPERTY(EditAnywhere)
	int32 Index = INDEX_NONE;

	bool operator==(const FAnimPhysDebugTarget& Other) const
	{
		return (Type == Other.Type && BoneName == Other.BoneName && Index == Other.Index);
	}
};

UCLASS()
class ANIMPHYSGRAPHNODE_API UAnimGraphNode_AnimPhys : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_AnimPhys Node;

	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawBone = true;

	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawCollision = true;

	UPROPERTY(VisibleAnywhere, Category = Debug)
	FAnimPhysDebugTarget DebugTarget;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const override;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	const FAnimPhysDebugTarget& GetDebugTarget() const { return DebugTarget; }

protected:

	// UAnimGraphNode_Base interface
	virtual FEditorModeID GetEditorMode() const;
	virtual FText GetTooltipText() const override;
	virtual void ValidateAnimNodePostCompile(class FCompilerResultsLog& MessageLog, class UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex) override;
	// End of UAnimGraphNode_Base interface

	FText GetBoneDescription() const;
	const FAnimNode_AnimPhys* GetNode() const { return &Node; }

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTitleTextTable CachedNodeTitles;
};