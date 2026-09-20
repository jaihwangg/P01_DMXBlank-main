// Copyright Sunnyview Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkSourceFactory.h"
#include "DollarsMoCapLiveLinkSource.h"
#include "DollarsMoCapLiveLinkSourceFactory.generated.h"

UCLASS()
class DOLLARSMOCAPLIVELINK_API UDollarsMoCapLiveLinkSourceFactory : public ULiveLinkSourceFactory
{
	GENERATED_BODY()

public:
	virtual FText GetSourceDisplayName() const override;
	virtual FText GetSourceTooltip() const override;

	virtual EMenuType GetMenuType() const override { return EMenuType::SubPanel; }
	virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
	virtual TSharedPtr<ILiveLinkSource> CreateSource(const FString& ConnectionString) const override;

private:
	void CreateSourceFromSettings(FDollarsMoCapLiveLinkConnectionSettings InSettings, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const;
};
