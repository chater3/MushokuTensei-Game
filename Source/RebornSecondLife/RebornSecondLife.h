#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRebornSecondLifeModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
