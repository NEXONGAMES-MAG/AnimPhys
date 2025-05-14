// Copyright NEXON Games Co., MIT License
#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UnrealWidget.h"
#include "AnimNodeEditMode.h"
#include "BonePose.h"
#include "AnimGraphNode_AnimPhys.h"

class FAnimPhysEditMode : public FAnimNodeEditMode
{
public:
	FAnimPhysEditMode();

	/** IAnimNodeEditMode interface */
	virtual ECoordSystem GetWidgetCoordinateSystem() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	virtual FName GetSelectedBone() const override;
	virtual void DoTranslation(FVector& InTranslation) override;
	virtual void DoRotation(FRotator& InRotation) override;
	virtual void DoScale(FVector& InScale) override;
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;
	virtual bool SupportsPoseWatch() override;
	virtual void RegisterPoseWatchedNode(UAnimGraphNode_Base* InEditorNode, FAnimNode_Base* InRuntimeNode) override;

	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override;

	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool ShouldDrawWidget() const override;

private:
	UE::Widget::EWidgetMode FindValidWidgetMode(UE::Widget::EWidgetMode InWidgetMode) const;
	UE::Widget::EWidgetMode GetNextWidgetMode(UE::Widget::EWidgetMode InWidgetMode) const;

protected:
	class UAnimGraphNode_AnimPhys* AnimGraphNode;
	struct FAnimNode_AnimPhys* RuntimeAnimNode;
};