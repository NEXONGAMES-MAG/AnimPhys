// Copyright NEXON Games Co., MIT License
#include "AnimPhysEditMode.h"
#include "IPersonaPreviewScene.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "AssetEditorModeManager.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"

#define LOCTEXT_NAMESPACE "AnimPhysEditMode"

struct HAnimPhysHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY()

	HAnimPhysHitProxy(const FAnimPhysDebugTarget& InDebugTarget)
		: HHitProxy(HPP_Wireframe)
		, DebugTarget(InDebugTarget)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FAnimPhysDebugTarget DebugTarget;
};
IMPLEMENT_HIT_PROXY(HAnimPhysHitProxy, HHitProxy);

namespace Local
{
	FAnimPhysColliderBase* TryGetAnimPhysCollider(FAnimNode_AnimPhys* InAnimNode, const FAnimPhysDebugTarget& InDebugTarget)
	{
		if (InAnimNode == nullptr)
		{
			return nullptr;
		}

		if (InDebugTarget.Type == EAnimPhysDebugType::SphereCollider)
		{		
			if (InAnimNode->CollisionSettings.SphereColliders.IsValidIndex(InDebugTarget.Index))
			{
				return &InAnimNode->CollisionSettings.SphereColliders[InDebugTarget.Index];
			}
		}
		else if (InDebugTarget.Type == EAnimPhysDebugType::CapsuleCollider)
		{
			if (InAnimNode->CollisionSettings.CapsuleColliders.IsValidIndex(InDebugTarget.Index))
			{
				return &InAnimNode->CollisionSettings.CapsuleColliders[InDebugTarget.Index];
			}
		}
		else if (InDebugTarget.Type == EAnimPhysDebugType::PlanarCollider)
		{
			if (InAnimNode->CollisionSettings.PlanarColliders.IsValidIndex(InDebugTarget.Index))
			{
				return &InAnimNode->CollisionSettings.PlanarColliders[InDebugTarget.Index];
			}
		}


		return nullptr;
	}
}

FAnimPhysEditMode::FAnimPhysEditMode()
	: AnimGraphNode(nullptr)
	, RuntimeAnimNode(nullptr)
{
}

IPersonaPreviewScene& FAnimPhysEditMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

ECoordSystem FAnimPhysEditMode::GetWidgetCoordinateSystem() const
{
	return ECoordSystem::COORD_Local;
}

FVector FAnimPhysEditMode::GetWidgetLocation() const
{
	const UE::Widget::EWidgetMode WidgetMode = GetWidgetMode();
	if (WidgetMode == UE::Widget::WM_None)
	{
		return FVector::ZeroVector;
	}

	if (AnimGraphNode == nullptr)
	{
		return FVector::ZeroVector;
	}

	if (AnimGraphNode->DebugTarget.Type == EAnimPhysDebugType::None)
	{
		return FVector::ZeroVector;
	}

	if (RuntimeAnimNode == nullptr)
	{
		return FVector::ZeroVector;
	}

	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (SkelComp == nullptr)
	{
		return FVector::ZeroVector;
	}

	FAnimPhysColliderBase* TargetCollider = Local::TryGetAnimPhysCollider(&AnimGraphNode->Node, AnimGraphNode->DebugTarget);
	if (TargetCollider == nullptr)
	{
		return FVector::ZeroVector;
	}

	FVector WidgetLoc = FVector::ZeroVector;

#if WITH_EDITORONLY_DATA
	FCSPose<FCompactHeapPose>& MeshBases = RuntimeAnimNode->GetForwardedPose();
	WidgetLoc = ConvertWidgetLocation(SkelComp, MeshBases, AnimGraphNode->DebugTarget.BoneName, TargetCollider->OffsetLocation, EBoneControlSpace::BCS_BoneSpace);
#endif

	return WidgetLoc;
}

UE::Widget::EWidgetMode FAnimPhysEditMode::GetWidgetMode() const
{
	if (GetModeManager() && GetSelectedBone() != NAME_None)
	{
		return GetModeManager()->GetWidgetMode();
	}

	return UE::Widget::WM_None;
}

FName FAnimPhysEditMode::GetSelectedBone() const
{
	if (AnimGraphNode && AnimGraphNode->DebugTarget.Type != EAnimPhysDebugType::None)
	{
		return AnimGraphNode->DebugTarget.BoneName;
	}

	return NAME_None;
}

void FAnimPhysEditMode::EnterMode(UAnimGraphNode_Base* InEditorNode, FAnimNode_Base* InRuntimeNode)
{
	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);

	AnimGraphNode = Cast<UAnimGraphNode_AnimPhys>(InEditorNode);
	RuntimeAnimNode = static_cast<FAnimNode_AnimPhys*>(InRuntimeNode);

	GetModeManager()->SetCoordSystem(GetWidgetCoordinateSystem());
	GetModeManager()->SetWidgetMode(GetWidgetMode());
}

void FAnimPhysEditMode::ExitMode()
{
	FAnimNodeEditMode::ExitMode();

	AnimGraphNode = nullptr;
	RuntimeAnimNode = nullptr;
}

bool FAnimPhysEditMode::SupportsPoseWatch()
{
	return false;
}

void FAnimPhysEditMode::RegisterPoseWatchedNode(UAnimGraphNode_Base* InEditorNode, FAnimNode_Base* InRuntimeNode)
{

}
void FAnimPhysEditMode::DoTranslation(FVector& InTranslation)
{
	if (AnimGraphNode == nullptr)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (SkelComp == nullptr)
	{
		return;
	}

	FAnimPhysColliderBase* TargetCollider = Local::TryGetAnimPhysCollider(&AnimGraphNode->Node, AnimGraphNode->DebugTarget);
	if (TargetCollider == nullptr)
	{
		return;
	}

	FAnimPhysColliderBase* RuntimeTargetCollider = Local::TryGetAnimPhysCollider(RuntimeAnimNode, AnimGraphNode->DebugTarget);
	if (RuntimeTargetCollider == nullptr)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	FCSPose<FCompactHeapPose>& MeshBases = RuntimeAnimNode->GetForwardedPose();
	FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, InTranslation, MeshBases, AnimGraphNode->DebugTarget.BoneName, EBoneControlSpace::BCS_BoneSpace);

	TargetCollider->OffsetLocation += Offset;
	RuntimeTargetCollider->OffsetLocation = TargetCollider->OffsetLocation;

	AnimGraphNode->Node.ResetColliders();
	RuntimeAnimNode->ResetColliders();
#endif
}

void FAnimPhysEditMode::DoRotation(FRotator& InRotation)
{
	if (AnimGraphNode == nullptr)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (SkelComp == nullptr)
	{
		return;
	}

	FAnimPhysColliderBase* TargetCollider = Local::TryGetAnimPhysCollider(&AnimGraphNode->Node, AnimGraphNode->DebugTarget);
	if (TargetCollider == nullptr)
	{
		return;
	}

	FAnimPhysColliderBase* RuntimeTargetCollider = Local::TryGetAnimPhysCollider(RuntimeAnimNode, AnimGraphNode->DebugTarget);
	if (RuntimeTargetCollider == nullptr)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	FCSPose<FCompactHeapPose>& MeshBases = RuntimeAnimNode->GetForwardedPose();
	FQuat DeltaQuat = ConvertCSRotationToBoneSpace(SkelComp, InRotation, MeshBases, AnimGraphNode->DebugTarget.BoneName, EBoneControlSpace::BCS_BoneSpace);

	FQuat PrevQuat(TargetCollider->OffsetRotation);
	FQuat NewQuat = DeltaQuat * PrevQuat;

	TargetCollider->OffsetRotation = NewQuat.Rotator();
	RuntimeTargetCollider->OffsetRotation = TargetCollider->OffsetRotation;

	AnimGraphNode->Node.ResetColliders();
	RuntimeAnimNode->ResetColliders();
#endif
}

void FAnimPhysEditMode::DoScale(FVector& InScale)
{
	if (AnimGraphNode == nullptr)
	{
		return;
	}

	if (RuntimeAnimNode == nullptr)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	if (AnimGraphNode->DebugTarget.Type == EAnimPhysDebugType::SphereCollider)
	{
		if (AnimGraphNode->Node.CollisionSettings.SphereColliders.IsValidIndex(AnimGraphNode->DebugTarget.Index))
		{
			auto& TargetCollider = AnimGraphNode->Node.CollisionSettings.SphereColliders[AnimGraphNode->DebugTarget.Index];
			TargetCollider.Radius += InScale.X;
			TargetCollider.Radius += InScale.Y;
			TargetCollider.Radius += InScale.Z;
			TargetCollider.Radius = FMath::Max(TargetCollider.Radius, 0.0f);

			if (RuntimeAnimNode->CollisionSettings.SphereColliders.IsValidIndex(AnimGraphNode->DebugTarget.Index))
			{
				auto& RuntimeTargetCollider = RuntimeAnimNode->CollisionSettings.SphereColliders[AnimGraphNode->DebugTarget.Index];
				RuntimeTargetCollider.Radius = TargetCollider.Radius;
			}
		}
	}
	else if (AnimGraphNode->DebugTarget.Type == EAnimPhysDebugType::CapsuleCollider)
	{
		if (AnimGraphNode->Node.CollisionSettings.CapsuleColliders.IsValidIndex(AnimGraphNode->DebugTarget.Index))
		{
			auto& TargetCollider = AnimGraphNode->Node.CollisionSettings.CapsuleColliders[AnimGraphNode->DebugTarget.Index];
			TargetCollider.Radius += InScale.X;
			TargetCollider.Radius += InScale.Y;
			TargetCollider.Radius = FMath::Max(TargetCollider.Radius, 0.0f);

			TargetCollider.Length += InScale.Z;
			TargetCollider.Length = FMath::Max(TargetCollider.Length, 0.0f);

			if (RuntimeAnimNode->CollisionSettings.CapsuleColliders.IsValidIndex(AnimGraphNode->DebugTarget.Index))
			{
				auto& RuntimeTargetCollider = RuntimeAnimNode->CollisionSettings.CapsuleColliders[AnimGraphNode->DebugTarget.Index];
				RuntimeTargetCollider.Radius = TargetCollider.Radius;
				RuntimeTargetCollider.Length = TargetCollider.Length;
			}
		}
	}

	AnimGraphNode->Node.ResetColliders();
	RuntimeAnimNode->ResetColliders();
#endif
}

void FAnimPhysEditMode::GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const
{
	if (AnimGraphNode != nullptr)
	{
		AnimGraphNode->GetOnScreenDebugInfo(OutDebugInfo, RuntimeAnimNode, GetAnimPreviewScene().GetPreviewMeshComponent());
	}
}

bool FAnimPhysEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	AnimGraphNode->DebugTarget.Type = EAnimPhysDebugType::None;
	AnimGraphNode->DebugTarget.BoneName = NAME_None;
	AnimGraphNode->DebugTarget.Index = INDEX_NONE;

	if (AnimGraphNode && HitProxy && HitProxy->IsA(HAnimPhysHitProxy::StaticGetType()))
	{
		HAnimPhysHitProxy* AsAnimPhysHitProxy = static_cast<HAnimPhysHitProxy*>(HitProxy);
		AnimGraphNode->DebugTarget = AsAnimPhysHitProxy->DebugTarget;

		if (GetWidgetMode() == UE::Widget::WM_None)
		{
			GetModeManager()->SetWidgetMode(UE::Widget::WM_Translate);
		}

		return true;
	}

	return false;
}

void FAnimPhysEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (AnimGraphNode == nullptr)
	{
		return;
	}

	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (PreviewMeshComponent == nullptr)
	{
		return;
	}
		
	AnimGraphNode->Draw(PDI, PreviewMeshComponent);

	if (RuntimeAnimNode == nullptr)
	{
		return;
	}

	if (AnimGraphNode->bEnableDebugDrawCollision)
	{
		FAnimPhysDebugTarget DebugTarget;

		const auto& Colliders = RuntimeAnimNode->GetColliders();

		if (Colliders.Spheres.Num() == RuntimeAnimNode->CollisionSettings.SphereColliders.Num())
		{
			for (int32 Index = 0; Index < Colliders.Spheres.Num(); ++Index)
			{
				const auto& Sphere = Colliders.Spheres[Index];

				DebugTarget.Type = EAnimPhysDebugType::SphereCollider;
				DebugTarget.Index = Index;
				DebugTarget.BoneName = PreviewMeshComponent->GetBoneName(Sphere.MeshPoseBoneIndex.GetInt());

				PDI->SetHitProxy(new HAnimPhysHitProxy(DebugTarget));
				PDI->DrawPoint(Sphere.DebugTransform.GetLocation(), FLinearColor::Yellow, 5.0f, SDPG_Foreground);
			}
		}

		if (Colliders.Capsules.Num() == RuntimeAnimNode->CollisionSettings.CapsuleColliders.Num())
		{
			for (int32 Index = 0; Index < Colliders.Capsules.Num(); ++Index)
			{
				const auto& Capsule = Colliders.Capsules[Index];

				DebugTarget.Type = EAnimPhysDebugType::CapsuleCollider;
				DebugTarget.Index = Index;
				DebugTarget.BoneName = PreviewMeshComponent->GetBoneName(Capsule.MeshPoseBoneIndex.GetInt());

				PDI->SetHitProxy(new HAnimPhysHitProxy(DebugTarget));
				PDI->DrawPoint(Capsule.DebugTransform.GetLocation(), FLinearColor::Yellow, 5.0f, SDPG_Foreground);
			}
		}

		if (Colliders.Planars.Num() == RuntimeAnimNode->CollisionSettings.PlanarColliders.Num())
		{
			for (int32 Index = 0; Index < Colliders.Planars.Num(); ++Index)
			{
				const auto& Planar = Colliders.Planars[Index];

				DebugTarget.Type = EAnimPhysDebugType::SphereCollider;
				DebugTarget.Index = Index;
				DebugTarget.BoneName = PreviewMeshComponent->GetBoneName(Planar.MeshPoseBoneIndex.GetInt());

				PDI->SetHitProxy(new HAnimPhysHitProxy(DebugTarget));
				PDI->DrawPoint(Planar.DebugTransform.GetLocation(), FLinearColor::Yellow, 5.0f, SDPG_Foreground);
			}
		}
	}
}

void FAnimPhysEditMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	if (Canvas == nullptr)
	{
		return;
	}

	if (AnimGraphNode == nullptr)
	{
		return;
	}

	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (PreviewMeshComponent == nullptr)
	{
		return;
	}

	AnimGraphNode->DrawCanvas(*Viewport, *const_cast<FSceneView*>(View), *Canvas, PreviewMeshComponent);

}

bool FAnimPhysEditMode::ShouldDrawWidget() const
{
	return true;
}

UE::Widget::EWidgetMode FAnimPhysEditMode::GetNextWidgetMode(UE::Widget::EWidgetMode InWidgetMode) const
{
	UE::Widget::EWidgetMode InMode = InWidgetMode;
	switch (InMode)
	{
	case UE::Widget::WM_Translate:
		return UE::Widget::WM_Rotate;
	case UE::Widget::WM_Rotate:
		return UE::Widget::WM_Scale;
	case UE::Widget::WM_Scale:
		return UE::Widget::WM_Translate;
	case UE::Widget::WM_TranslateRotateZ:
	case UE::Widget::WM_2D:
		break;
	}

	return UE::Widget::WM_None;
}

UE::Widget::EWidgetMode FAnimPhysEditMode::FindValidWidgetMode(UE::Widget::EWidgetMode InWidgetMode) const
{
	UE::Widget::EWidgetMode InMode = InWidgetMode;
	UE::Widget::EWidgetMode ValidMode = InMode;
	if (InMode == UE::Widget::WM_None)
	{	// starts from Rotate mode
		ValidMode = UE::Widget::WM_Rotate;
	}

	// find from current widget mode and loop 1 cycle until finding a valid mode
	for (int32 Index = 0; Index < 3; Index++)
	{
		ValidMode = GetNextWidgetMode(ValidMode);
	}

	// if couldn't find a valid mode, returns None
	ValidMode = UE::Widget::WM_None;

	return ValidMode;
}

#undef LOCTEXT_NAMESPACE