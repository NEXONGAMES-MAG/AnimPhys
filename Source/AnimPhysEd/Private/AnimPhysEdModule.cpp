// Copyright NEXON Games Co., MIT License
#include "AnimPhysEdModule.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "AnimPhysEditMode.h"

#define LOCTEXT_NAMESPACE "AnimPhysEdModule"

void FAnimPhysEdModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FAnimPhysEditMode>("AnimGraph.SkeletalControl.AnimPhys", LOCTEXT("AnimPhysEditMode", "AnimPhys"), FSlateIcon(), false);
}

void FAnimPhysEdModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode("AnimGraph.SkeletalControl.AnimPhys");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAnimPhysEdModule, AnimPhysEd)
