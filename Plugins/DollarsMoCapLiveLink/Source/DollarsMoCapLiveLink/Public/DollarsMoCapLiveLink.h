// Copyright Sunnyview Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class DOLLARSMOCAPLIVELINK_API FDollarsMoCapLiveLinkModule : public IModuleInterface
{
public:
	static FDollarsMoCapLiveLinkModule& Get()
	{
		return FModuleManager::Get().LoadModuleChecked<FDollarsMoCapLiveLinkModule>("DollarsMoCapLiveLink");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("DollarsMoCapLiveLink");
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
