// Copyright NEXON Games Co., MIT License
#include "AnimGraphNode_AnimPhys.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "Textures/SlateIcon.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "Framework/Commands/UIAction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Animation/AnimInstance.h"
#include "AnimationGraph.h"

#define LOCTEXT_NAMESPACE "AnimPhys"

UAnimGraphNode_AnimPhys::UAnimGraphNode_AnimPhys(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_AnimPhys::GetTooltipText() const
{
	return LOCTEXT("AnimPhys", "AnimPhys");
}


FText UAnimGraphNode_AnimPhys::GetBoneDescription() const
{
	if (Node.BonesToSimulate.IsEmpty())
	{
		return FText();
	}

	if (Node.BonesToSimulate.Num() == 1)
	{
		return FText::FromName(Node.BonesToSimulate[0].BoneName);
	}

	return FText::FromString(Node.BonesToSimulate[0].BoneName.ToString() + FString::Printf(TEXT("+ %d"), (Node.BonesToSimulate.Num()-1)));
}

FText UAnimGraphNode_AnimPhys::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle))
	{
		return GetTooltipText();
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeDescription"), GetTooltipText());
		Args.Add(TEXT("RootBoneName"), GetBoneDescription());

		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_AnimPhys_ListTitle", "{NodeDescription} - Root: {RootBoneName}"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_AnimPhys_Title", "{NodeDescription}\nRoot: {RootBoneName} "), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];
}

void UAnimGraphNode_AnimPhys::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Node.ResetSimulatedBones();

	ReconstructNode();
}

FEditorModeID UAnimGraphNode_AnimPhys::GetEditorMode() const
{
	return "AnimGraph.SkeletalControl.AnimPhys";
}

void UAnimGraphNode_AnimPhys::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{
	if (!SkelMeshComp)
	{
		return;
	}

	FAnimNode_AnimPhys* ActiveNode = GetActiveInstanceNode<FAnimNode_AnimPhys>(SkelMeshComp->GetAnimInstance());
	if (!ActiveNode)
	{
		return;
	}

	if (bEnableDebugDrawBone)
	{
		const float Radius = ActiveNode->SetupSettings.Radius;
		const auto& SimulatedBones = ActiveNode->GetSimulatedBones();
		for (auto& Bone : SimulatedBones)
		{
			const FVector BoneLocation = Bone.ComponentSpaceTM.GetLocation();
			PDI->DrawPoint(BoneLocation, FLinearColor::White, 5.0f, SDPG_Foreground);

			if (Radius > 0.0f)
			{
				const FColor Color = (Bone.NumChildren == 0 && Bone.MeshPoseBoneIndex.IsValid() == false) ? FColor::Red : FColor::Yellow;
				DrawWireSphere(PDI, BoneLocation, Color, Radius, 16, SDPG_World);
			}

			if (SimulatedBones.IsValidIndex(Bone.ParentIndex))
			{
				DrawDashedLine(PDI, BoneLocation, SimulatedBones[Bone.ParentIndex].ComponentSpaceTM.GetLocation(), FLinearColor::White, 1, SDPG_World);
			}
		}
	}

	if (bEnableDebugDrawCollision)
	{
		const auto& Colliders = ActiveNode->GetColliders();
		for (const auto& Sphere : Colliders.Spheres)
		{
			if (Sphere.bValid == false)
			{
				continue;
			}

#if WITH_EDITORONLY_DATA
			DrawSphere(PDI, Sphere.DebugTransform.GetLocation(), FRotator::ZeroRotator, FVector(Sphere.DebugRadius), 24, 6, GEngine->ConstraintLimitMaterialPrismatic->GetRenderProxy(), SDPG_World);
			DrawWireSphere(PDI, Sphere.DebugTransform.GetLocation(), FLinearColor::Black, Sphere.DebugRadius, 24, SDPG_World);
			DrawCoordinateSystem(PDI, Sphere.DebugTransform.GetLocation(), FRotator::ZeroRotator, Sphere.DebugRadius, SDPG_World);
#endif
		}

		for (const auto& Capsule : Colliders.Capsules)
		{
			if (Capsule.bValid == false)
			{
				continue;
			}
#if WITH_EDITORONLY_DATA
			FVector XAxis = Capsule.DebugTransform.GetUnitAxis(EAxis::X);
			FVector YAxis = Capsule.DebugTransform.GetUnitAxis(EAxis::Y);
			FVector ZAxis = Capsule.DebugTransform.GetUnitAxis(EAxis::Z);

			DrawCylinder(PDI, Capsule.DebugTransform.GetLocation(), XAxis, YAxis, ZAxis, Capsule.DebugRadius, Capsule.HalfHeight, 25,	GEngine->ConstraintLimitMaterialPrismatic->GetRenderProxy(), SDPG_World);
			DrawSphere(PDI, Capsule.DebugTransform.GetLocation() + ZAxis * Capsule.HalfHeight, Capsule.DebugTransform.Rotator(), FVector(Capsule.DebugRadius), 24, 6, GEngine->ConstraintLimitMaterialPrismatic->GetRenderProxy(), SDPG_World);
			DrawSphere(PDI, Capsule.DebugTransform.GetLocation() - ZAxis * Capsule.HalfHeight, Capsule.DebugTransform.Rotator(), FVector(Capsule.DebugRadius),24, 6, GEngine->ConstraintLimitMaterialPrismatic->GetRenderProxy(), SDPG_World);

			DrawWireCapsule(PDI, Capsule.DebugTransform.GetLocation(), XAxis, YAxis, ZAxis, FLinearColor::Black, Capsule.DebugRadius, Capsule.HalfHeight + Capsule.DebugRadius, 25, SDPG_World);

			DrawCoordinateSystem(PDI, Capsule.DebugTransform.GetLocation(), Capsule.DebugTransform.Rotator(), Capsule.DebugRadius, SDPG_World);
#endif
		}

		for (const auto& Planar : Colliders.Planars)
		{
			if (Planar.bValid == false)
			{
				continue;
			}

#if WITH_EDITORONLY_DATA
			DrawPlane10x10(PDI, Planar.DebugTransform.ToMatrixWithScale(), 200.0f, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), GEngine->ConstraintLimitMaterialPrismatic->GetRenderProxy(), SDPG_World);
			DrawDirectionalArrow(PDI, FRotationMatrix(FRotator(90.0f, 0.0f, 0.0f)) * Planar.DebugTransform.ToMatrixWithScale(), FLinearColor::Blue, 50.0f, 20.0f, SDPG_World, 0.5f);
#endif
		}
	}
}


void UAnimGraphNode_AnimPhys::ValidateAnimNodePostCompile(class FCompilerResultsLog& MessageLog, class UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex)
{
	if (CompiledClass && CompiledClass->GetTargetSkeleton())
	{
		const FReferenceSkeleton& RefSkeleton = CompiledClass->GetTargetSkeleton()->GetReferenceSkeleton();

		TArray<int32> RootBoneIndexes;
		for (const auto& EachRootBone : Node.BonesToSimulate)
		{
			RootBoneIndexes.AddUnique(RefSkeleton.FindBoneIndex(EachRootBone.BoneName));
		}

		for (const auto& BoneIndex : RootBoneIndexes)
		{
			if (BoneIndex == INDEX_NONE)
			{
				continue;
			}

			TArray<int32> ParentBoneIndexs;

			int32 ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
			while (ParentBoneIndex > 0)
			{
				ParentBoneIndexs.Add(ParentBoneIndex);
				ParentBoneIndex = RefSkeleton.GetParentIndex(ParentBoneIndex);
			}

			for (const auto& BoneIndexToCompare : RootBoneIndexes)
			{
				if (BoneIndexToCompare != BoneIndex &&
					ParentBoneIndexs.Contains(BoneIndexToCompare))
				{
					MessageLog.Error(TEXT("@@ should have only simulated bones parallel. Please check BonesToSimulate."), this);
					return;
				}
			}
		}
	}

}

#undef LOCTEXT_NAMESPACE