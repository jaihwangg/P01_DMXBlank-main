// Copyright Sunnyview Inc. All Rights Reserved.

#include "DollarsMoCapLiveLinkSourceFactory.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "DollarsMoCapLiveLinkSourceFactory"

namespace DollarsMoCapLiveLinkSourceFactoryUI
{
	class SConnectionPanel : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SConnectionPanel) {}
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, UDollarsMoCapLiveLinkSourceFactory::FOnLiveLinkSourceCreated InOnSourceCreated, const UDollarsMoCapLiveLinkSourceFactory* InFactory)
		{
			OnSourceCreated = InOnSourceCreated;
			Factory = InFactory;

			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.FillWidth(0.5f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PortLabel", "Port Number:"))
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.FillWidth(0.5f)
					[
						SNew(SNumericEntryBox<uint16>)
						.AllowSpin(true)
						.MinValue(1)
						.MaxValue(65535)
						.MinSliderValue(1)
						.MaxSliderValue(65535)
						.Value(this, &SConnectionPanel::GetPortValue)
						.OnValueChanged(this, &SConnectionPanel::OnPortValueChanged)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.OnClicked(this, &SConnectionPanel::OnCreateClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateButton", "Create"))
					]
				]
			];
		}

	private:
		TOptional<uint16> GetPortValue() const
		{
			return Settings.LocalEndpoint.Port;
		}

		void OnPortValueChanged(uint16 NewValue)
		{
			Settings.LocalEndpoint.Port = NewValue;
		}

		FReply OnCreateClicked()
		{
			if (Factory)
			{
				TSharedPtr<ILiveLinkSource> NewSource = MakeShared<FDollarsMoCapLiveLinkSource>(Settings);
				OnSourceCreated.ExecuteIfBound(NewSource, FString::Printf(TEXT(":%d"), Settings.LocalEndpoint.Port));
			}
			return FReply::Handled();
		}

		FDollarsMoCapLiveLinkConnectionSettings Settings;
		UDollarsMoCapLiveLinkSourceFactory::FOnLiveLinkSourceCreated OnSourceCreated;
		const UDollarsMoCapLiveLinkSourceFactory* Factory = nullptr;
	};
}

FText UDollarsMoCapLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "Dollars MoCap Live Link");
}

FText UDollarsMoCapLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Create a source for receiving motion capture data from Dollars MoCap");
}

TSharedPtr<SWidget> UDollarsMoCapLiveLinkSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const
{
	return SNew(DollarsMoCapLiveLinkSourceFactoryUI::SConnectionPanel, OnLiveLinkSourceCreated, this);
}

TSharedPtr<ILiveLinkSource> UDollarsMoCapLiveLinkSourceFactory::CreateSource(const FString& ConnectionString) const
{
	FDollarsMoCapLiveLinkConnectionSettings Settings;

	// Parse connection string (expected format: ":port")
	if (!ConnectionString.IsEmpty() && ConnectionString.StartsWith(TEXT(":")))
	{
		uint16 Port = FCString::Atoi(*ConnectionString.Mid(1));
		if (Port > 0)
		{
			Settings.LocalEndpoint.Port = Port;
		}
	}

	return MakeShared<FDollarsMoCapLiveLinkSource>(Settings);
}

void UDollarsMoCapLiveLinkSourceFactory::CreateSourceFromSettings(FDollarsMoCapLiveLinkConnectionSettings InSettings, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const
{
	TSharedPtr<FDollarsMoCapLiveLinkSource> NewSource = MakeShared<FDollarsMoCapLiveLinkSource>(InSettings);
	OnLiveLinkSourceCreated.ExecuteIfBound(NewSource, FString::Printf(TEXT(":%d"), InSettings.LocalEndpoint.Port));
}

#undef LOCTEXT_NAMESPACE
