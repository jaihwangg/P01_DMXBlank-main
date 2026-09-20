// Copyright Sunnyview Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

class ILiveLinkClient;
class FJsonObject;

struct FDollarsMoCapLiveLinkConnectionSettings
{
	FIPv4Endpoint LocalEndpoint;

	FDollarsMoCapLiveLinkConnectionSettings()
		: LocalEndpoint(FIPv4Address::Any, 12351)
	{
	}
};

class DOLLARSMOCAPLIVELINK_API FDollarsMoCapLiveLinkSource : public ILiveLinkSource, public FRunnable
{
public:
	FDollarsMoCapLiveLinkSource(const FDollarsMoCapLiveLinkConnectionSettings& InConnectionSettings);
	virtual ~FDollarsMoCapLiveLinkSource();

	// ILiveLinkSource interface
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual void Update() override;
	virtual bool IsSourceStillValid() const override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	void HandleReceivedData(const FString& JsonString);
	void ProcessMoCapData(const TSharedPtr<FJsonObject>& JsonObject);
	void CreateSubject(const FName& SubjectName);

private:
	ILiveLinkClient* Client;
	FGuid SourceGuid;

	FDollarsMoCapLiveLinkConnectionSettings ConnectionSettings;

	FSocket* Socket;
	FRunnableThread* Thread;
	FThreadSafeBool bIsRunning;
	FThreadSafeBool bIsConnected;

	TSet<FName> EncounteredSubjects;
	FCriticalSection SubjectsCriticalSection;

	// Bind pose storage (captured on first frame)
	TArray<FQuat> BindPoseRotations;
	bool bBindPoseCaptured;

	// Shared flag for tracking object lifetime in async tasks
	TSharedPtr<FThreadSafeBool> bIsActive;

	static const FName SubjectName;
};
