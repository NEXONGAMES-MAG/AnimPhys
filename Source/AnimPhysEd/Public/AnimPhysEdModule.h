// Copyright NEXON Games Co., MIT License
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAnimPhysEdModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

