// Copyright Sunnyview Inc. All Rights Reserved.

#include "DollarsMoCapLiveLinkSource.h"

#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkTransformRole.h"

#include "Async/Async.h"
#include "Common/UdpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "DollarsMoCapLiveLinkSource"

const FName FDollarsMoCapLiveLinkSource::SubjectName(TEXT("DollarsMoCap"));

// Dollars MoCap skeleton bone names (must match Unity BoneMapping order)
static const TArray<FName> StandardBoneNames = {
	// Spine (0-5)
	TEXT("pelvis"),       // 0
	TEXT("spine_01"),     // 1
	TEXT("spine_02"),     // 2
	TEXT("spine_04"),     // 3
	TEXT("neck_01"),      // 4
	TEXT("head"),         // 5

	// Left Arm (6-9)
	TEXT("clavicle_l"),   // 6
	TEXT("upperarm_l"),   // 7
	TEXT("lowerarm_l"),   // 8
	TEXT("hand_l"),       // 9

	// Left Fingers (10-24)
	TEXT("thumb_01_l"),   // 10
	TEXT("thumb_02_l"),   // 11
	TEXT("thumb_03_l"),   // 12
	TEXT("index_01_l"),   // 13
	TEXT("index_02_l"),   // 14
	TEXT("index_03_l"),   // 15
	TEXT("middle_01_l"),  // 16
	TEXT("middle_02_l"),  // 17
	TEXT("middle_03_l"),  // 18
	TEXT("ring_01_l"),    // 19
	TEXT("ring_02_l"),    // 20
	TEXT("ring_03_l"),    // 21
	TEXT("pinky_01_l"),   // 22
	TEXT("pinky_02_l"),   // 23
	TEXT("pinky_03_l"),   // 24

	// Right Arm (25-28)
	TEXT("clavicle_r"),   // 25
	TEXT("upperarm_r"),   // 26
	TEXT("lowerarm_r"),   // 27
	TEXT("hand_r"),       // 28

	// Right Fingers (29-43)
	TEXT("thumb_01_r"),   // 29
	TEXT("thumb_02_r"),   // 30
	TEXT("thumb_03_r"),   // 31
	TEXT("index_01_r"),   // 32
	TEXT("index_02_r"),   // 33
	TEXT("index_03_r"),   // 34
	TEXT("middle_01_r"),  // 35
	TEXT("middle_02_r"),  // 36
	TEXT("middle_03_r"),  // 37
	TEXT("ring_01_r"),    // 38
	TEXT("ring_02_r"),    // 39
	TEXT("ring_03_r"),    // 40
	TEXT("pinky_01_r"),   // 41
	TEXT("pinky_02_r"),   // 42
	TEXT("pinky_03_r"),   // 43

	// Left Leg (44-47)
	TEXT("thigh_l"),      // 44
	TEXT("calf_l"),       // 45
	TEXT("foot_l"),       // 46
	TEXT("ball_l"),       // 47

	// Right Leg (48-51)
	TEXT("thigh_r"),      // 48
	TEXT("calf_r"),       // 49
	TEXT("foot_r"),       // 50
	TEXT("ball_r")        // 51
};

// Parent indices for standard skeleton (-1 means root)
static const TArray<int32> StandardBoneParents = {
	// Spine
	-1, // pelvis (root)
	0,  // spine_01 -> pelvis
	1,  // spine_02 -> spine_01
	2,  // spine_04 -> spine_02
	3,  // neck_01 -> spine_04
	4,  // head -> neck_01

	// Left Arm
	3,  // clavicle_l -> spine_04
	6,  // upperarm_l -> clavicle_l
	7,  // lowerarm_l -> upperarm_l
	8,  // hand_l -> lowerarm_l

	// Left Fingers
	9,  // thumb_01_l -> hand_l
	10, // thumb_02_l -> thumb_01_l
	11, // thumb_03_l -> thumb_02_l
	9,  // index_01_l -> hand_l
	13, // index_02_l -> index_01_l
	14, // index_03_l -> index_02_l
	9,  // middle_01_l -> hand_l
	16, // middle_02_l -> middle_01_l
	17, // middle_03_l -> middle_02_l
	9,  // ring_01_l -> hand_l
	19, // ring_02_l -> ring_01_l
	20, // ring_03_l -> ring_02_l
	9,  // pinky_01_l -> hand_l
	22, // pinky_02_l -> pinky_01_l
	23, // pinky_03_l -> pinky_02_l

	// Right Arm
	3,  // clavicle_r -> spine_04
	25, // upperarm_r -> clavicle_r
	26, // lowerarm_r -> upperarm_r
	27, // hand_r -> lowerarm_r

	// Right Fingers
	28, // thumb_01_r -> hand_r
	29, // thumb_02_r -> thumb_01_r
	30, // thumb_03_r -> thumb_02_r
	28, // index_01_r -> hand_r
	32, // index_02_r -> index_01_r
	33, // index_03_r -> index_02_r
	28, // middle_01_r -> hand_r
	35, // middle_02_r -> middle_01_r
	36, // middle_03_r -> middle_02_r
	28, // ring_01_r -> hand_r
	38, // ring_02_r -> ring_01_r
	39, // ring_03_r -> ring_02_r
	28, // pinky_01_r -> hand_r
	41, // pinky_02_r -> pinky_01_r
	42, // pinky_03_r -> pinky_02_r

	// Left Leg
	0,  // thigh_l -> pelvis
	44, // calf_l -> thigh_l
	45, // foot_l -> calf_l
	46, // ball_l -> foot_l

	// Right Leg
	0,  // thigh_r -> pelvis
	48, // calf_r -> thigh_r
	49, // foot_r -> calf_r
	50  // ball_r -> foot_r
};

FDollarsMoCapLiveLinkSource::FDollarsMoCapLiveLinkSource(const FDollarsMoCapLiveLinkConnectionSettings& InConnectionSettings)
	: Client(nullptr)
	, ConnectionSettings(InConnectionSettings)
	, Socket(nullptr)
	, Thread(nullptr)
	, bIsRunning(false)
	, bIsConnected(false)
	, bBindPoseCaptured(false)
	, bIsActive(MakeShared<FThreadSafeBool>(true))
{
}

FDollarsMoCapLiveLinkSource::~FDollarsMoCapLiveLinkSource()
{
	// Mark as inactive to prevent async tasks from accessing this object
	*bIsActive = false;

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}

	if (Socket != nullptr)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void FDollarsMoCapLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;

	// Start the receiving thread
	bIsRunning = true;
	Thread = FRunnableThread::Create(this, TEXT("DollarsMoCapLiveLinkSource"));
}

void FDollarsMoCapLiveLinkSource::Update()
{
	// Called on game thread, can be used for any updates that need to happen on game thread
}

bool FDollarsMoCapLiveLinkSource::IsSourceStillValid() const
{
	return bIsRunning;
}

bool FDollarsMoCapLiveLinkSource::RequestSourceShutdown()
{
	Stop();
	return true;
}

FText FDollarsMoCapLiveLinkSource::GetSourceType() const
{
	return LOCTEXT("SourceType", "Dollars MoCap Live Link");
}

FText FDollarsMoCapLiveLinkSource::GetSourceMachineName() const
{
	return FText::FromString(ConnectionSettings.LocalEndpoint.ToString());
}

FText FDollarsMoCapLiveLinkSource::GetSourceStatus() const
{
	if (bIsConnected)
	{
		return LOCTEXT("StatusConnected", "Connected");
	}
	else if (bIsRunning)
	{
		return LOCTEXT("StatusWaiting", "Waiting for data...");
	}
	else
	{
		return LOCTEXT("StatusDisconnected", "Disconnected");
	}
}

bool FDollarsMoCapLiveLinkSource::Init()
{
	return true;
}

uint32 FDollarsMoCapLiveLinkSource::Run()
{
	// Create UDP socket
	Socket = FUdpSocketBuilder(TEXT("DollarsMoCapSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(ConnectionSettings.LocalEndpoint)
		.WithReceiveBufferSize(1024 * 1024);

	if (Socket == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("DollarsMoCap: Failed to create socket on %s"), *ConnectionSettings.LocalEndpoint.ToString());
		return 0;
	}


	TArray<uint8> ReceivedData;
	ReceivedData.SetNumUninitialized(65536);

	while (bIsRunning)
	{
		int32 BytesRead = 0;
		if (Socket->Recv(ReceivedData.GetData(), ReceivedData.Num(), BytesRead))
		{
			if (BytesRead > 0)
			{
				bIsConnected = true;

				// Convert to string
				ReceivedData[BytesRead] = 0;
				FString JsonString = FString(UTF8_TO_TCHAR(ReceivedData.GetData()));

				// Process on game thread with safe capture
				TSharedPtr<FThreadSafeBool> IsActivePtr = bIsActive;
				AsyncTask(ENamedThreads::GameThread, [this, JsonString, IsActivePtr]()
				{
					if (IsActivePtr.IsValid() && *IsActivePtr)
					{
						HandleReceivedData(JsonString);
					}
				});
			}
		}

		// Small sleep to prevent busy-waiting
		FPlatformProcess::Sleep(0.001f);
	}

	return 0;
}

void FDollarsMoCapLiveLinkSource::Stop()
{
	bIsRunning = false;
}

void FDollarsMoCapLiveLinkSource::Exit()
{
	// Cleanup
}

void FDollarsMoCapLiveLinkSource::HandleReceivedData(const FString& JsonString)
{
	if (!Client)
	{
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("DollarsMoCap: Failed to parse JSON data"));
		return;
	}

	ProcessMoCapData(JsonObject);
}

void FDollarsMoCapLiveLinkSource::CreateSubject(const FName& InSubjectName)
{
	FScopeLock Lock(&SubjectsCriticalSection);

	if (!Client)
	{
		return;
	}

	if (EncounteredSubjects.Contains(InSubjectName))
	{
		return;
	}

	EncounteredSubjects.Add(InSubjectName);

	// Create skeleton data for animation role
	FLiveLinkStaticDataStruct StaticDataStruct(FLiveLinkSkeletonStaticData::StaticStruct());
	FLiveLinkSkeletonStaticData& SkeletonData = *StaticDataStruct.Cast<FLiveLinkSkeletonStaticData>();

	SkeletonData.BoneNames = StandardBoneNames;
	SkeletonData.BoneParents = StandardBoneParents;
	SkeletonData.PropertyNames.Add(FName(TEXT("present")));

	Client->PushSubjectStaticData_AnyThread({SourceGuid, InSubjectName}, ULiveLinkAnimationRole::StaticClass(), MoveTemp(StaticDataStruct));
}

void FDollarsMoCapLiveLinkSource::ProcessMoCapData(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!Client)
	{
		return;
	}

	// Get subject name from JSON or use default
	FName CurrentSubjectName = SubjectName;
	if (JsonObject->HasField(TEXT("name")))
	{
		CurrentSubjectName = FName(*JsonObject->GetStringField(TEXT("name")));
	}

	double PresentNum = 1.0;
	JsonObject->TryGetNumberField(TEXT("present"), PresentNum);
	const float PresentVal = (float)PresentNum;

	// Create subject if not exists
	CreateSubject(CurrentSubjectName);

	// Create frame data
	FLiveLinkFrameDataStruct FrameDataStruct(FLiveLinkAnimationFrameData::StaticStruct());
	FLiveLinkAnimationFrameData& FrameData = *FrameDataStruct.Cast<FLiveLinkAnimationFrameData>();

	// Set world time
	FrameData.WorldTime = FLiveLinkWorldTime(FPlatformTime::Seconds());

	FrameData.PropertyValues.Add(PresentVal);

	// Parse bone transforms from JSON
	const TArray<TSharedPtr<FJsonValue>>* BonesArray;
	if (JsonObject->TryGetArrayField(TEXT("bones"), BonesArray))
	{
		// Capture bind pose on first frame (use UE skeletal mesh ref pose)
		if (!bBindPoseCaptured)
		{
			BindPoseRotations.SetNum(StandardBoneNames.Num());

			// Spine (0-5)
			BindPoseRotations[0] = FRotator(86.366893f, -90.0f, -90.0f).Quaternion();           // pelvis
			BindPoseRotations[1] = FRotator(0.0f, 14.457322f, 0.0f).Quaternion();              // spine_01
			BindPoseRotations[2] = FRotator(0.0f, -3.464470f, 0.0f).Quaternion();             // spine_02
			BindPoseRotations[3] = FRotator(0.0f, -5.866984f, 0.000450f).Quaternion();        // spine_04
			BindPoseRotations[4] = FRotator(0.0f, 23.928404f, 0.0f).Quaternion();             // neck_01
			BindPoseRotations[5] = FRotator(0.000020f, -11.880170f, 0.000096f).Quaternion();  // head

			// Left Arm (6-9)
			BindPoseRotations[6] = FRotator(-80.831226f, -153.124384f, 163.263585f).Quaternion();  // clavicle_l
			BindPoseRotations[7] = FRotator(-46.029604f, 4.358519f, -4.337345f).Quaternion();      // upperarm_l
			BindPoseRotations[8] = FRotator(0.0f, 38.978822f, 0.0f).Quaternion();                  // lowerarm_l
			BindPoseRotations[9] = FRotator(-1.473471f, -1.848916f, -67.770759f).Quaternion();     // hand_l

			// Left Fingers (10-24)
			BindPoseRotations[10] = FRotator(-39.904178f, -20.508676f, 73.564464f).Quaternion();   // thumb_01_l
			BindPoseRotations[11] = FRotator(1.932290f, -23.246006f, 3.530628f).Quaternion();      // thumb_02_l
			BindPoseRotations[12] = FRotator(0.0f, -10.0f, 0.0f).Quaternion();                     // thumb_03_l
			BindPoseRotations[13] = FRotator(0.0f, -23.373f, 0.0f).Quaternion();                   // index_01_l
			BindPoseRotations[14] = FRotator(0.0f, -14.892568f, 0.0f).Quaternion();                // index_02_l
			BindPoseRotations[15] = FRotator(0.0f, -12.516401f, 0.0f).Quaternion();                // index_03_l
			BindPoseRotations[16] = FRotator(0.0f, -31.572682f, 0.0f).Quaternion();                // middle_01_l
			BindPoseRotations[17] = FRotator(0.0f, -20.769210f, 0.0f).Quaternion();                // middle_02_l
			BindPoseRotations[18] = FRotator(0.0f, -10.0f, 0.0f).Quaternion();                     // middle_03_l
			BindPoseRotations[19] = FRotator(0.116938f, -29.414482f, 6.395844f).Quaternion();      // ring_01_l
			BindPoseRotations[20] = FRotator(0.0f, -18.964f, 0.0f).Quaternion();                   // ring_02_l
			BindPoseRotations[21] = FRotator(0.0f, -9.168f, 0.0f).Quaternion();                    // ring_03_l
			BindPoseRotations[22] = FRotator(-0.605043f, -14.833681f, 10.491640f).Quaternion();    // pinky_01_l
			BindPoseRotations[23] = FRotator(0.0f, -21.286999f, 0.0f).Quaternion();                // pinky_02_l
			BindPoseRotations[24] = FRotator(0.0f, -4.917f, 0.0f).Quaternion();                    // pinky_03_l

			// Right Arm (25-28)
			BindPoseRotations[25] = FRotator(-80.831226f, 26.875616f, 163.263585f).Quaternion();   // clavicle_r
			BindPoseRotations[26] = FRotator(-46.029604f, 4.358519f, -4.337345f).Quaternion();     // upperarm_r
			BindPoseRotations[27] = FRotator(0.0f, 38.978822f, 0.0f).Quaternion();                 // lowerarm_r
			BindPoseRotations[28] = FRotator(-1.473471f, -1.848916f, -67.770759f).Quaternion();    // hand_r

			// Right Fingers (29-43)
			BindPoseRotations[29] = FRotator(-39.904178f, -20.508676f, 73.564464f).Quaternion();   // thumb_01_r
			BindPoseRotations[30] = FRotator(1.932290f, -23.246006f, 3.530628f).Quaternion();      // thumb_02_r
			BindPoseRotations[31] = FRotator(0.0f, -10.0f, 0.0f).Quaternion();                     // thumb_03_r
			BindPoseRotations[32] = FRotator(0.0f, -23.373f, 0.0f).Quaternion();                   // index_01_r
			BindPoseRotations[33] = FRotator(0.0f, -14.892568f, 0.0f).Quaternion();                // index_02_r
			BindPoseRotations[34] = FRotator(0.0f, -12.516401f, 0.0f).Quaternion();                // index_03_r
			BindPoseRotations[35] = FRotator(0.0f, -31.572682f, 0.0f).Quaternion();                // middle_01_r
			BindPoseRotations[36] = FRotator(0.0f, -20.769210f, 0.0f).Quaternion();                // middle_02_r
			BindPoseRotations[37] = FRotator(0.0f, -10.0f, 0.0f).Quaternion();                     // middle_03_r
			BindPoseRotations[38] = FRotator(0.116938f, -29.414482f, 6.395844f).Quaternion();      // ring_01_r
			BindPoseRotations[39] = FRotator(0.0f, -18.964f, 0.0f).Quaternion();                   // ring_02_r
			BindPoseRotations[40] = FRotator(0.0f, -9.168f, 0.0f).Quaternion();                    // ring_03_r
			BindPoseRotations[41] = FRotator(-0.605043f, -14.833681f, 10.491640f).Quaternion();    // pinky_01_r
			BindPoseRotations[42] = FRotator(0.0f, -21.286999f, 0.0f).Quaternion();                // pinky_02_r
			BindPoseRotations[43] = FRotator(0.0f, -4.917f, 0.0f).Quaternion();                    // pinky_03_r

			// Left Leg (44-47)
			BindPoseRotations[44] = FRotator(3.125540f, 3.560133f, 8.408539f).Quaternion();        // thigh_l
			BindPoseRotations[45] = FRotator(0.0f, 5.004845f, 0.0f).Quaternion();                  // calf_l
			BindPoseRotations[46] = FRotator(-3.081202f, -2.664105f, -0.004663f).Quaternion();     // foot_l
			BindPoseRotations[47] = FRotator(0.0f, 90.0f, 0.0f).Quaternion();                      // ball_l

			// Right Leg (48-51)
			BindPoseRotations[48] = FRotator(3.125540f, -176.439867f, 8.408539f).Quaternion();     // thigh_r
			BindPoseRotations[49] = FRotator(0.0f, 5.004845f, 0.0f).Quaternion();                  // calf_r
			BindPoseRotations[50] = FRotator(-3.081202f, -2.664105f, -0.004663f).Quaternion();     // foot_r
			BindPoseRotations[51] = FRotator(0.0f, 90.0f, 0.0f).Quaternion();                      // ball_r

			bBindPoseCaptured = true;
		}

		FrameData.Transforms.Reserve(StandardBoneNames.Num());

		int32 BoneIndex = 0;
		for (const TSharedPtr<FJsonValue>& BoneValue : *BonesArray)
		{
			// Stop if we've processed all expected bones
			if (BoneIndex >= StandardBoneNames.Num())
			{
				break;
			}

			const TSharedPtr<FJsonObject>& BoneObject = BoneValue->AsObject();
			if (BoneObject.IsValid())
			{
				FTransform BoneTransform;

				// Parse delta rotation (quaternion) - already converted to UE coordinate system
				if (BoneObject->HasField(TEXT("rotation")))
				{
					const TSharedPtr<FJsonObject>& RotObject = BoneObject->GetObjectField(TEXT("rotation"));
					if (RotObject.IsValid())
					{
						double X = RotObject->GetNumberField(TEXT("x"));
						double Y = RotObject->GetNumberField(TEXT("y"));
						double Z = RotObject->GetNumberField(TEXT("z"));
						double W = RotObject->GetNumberField(TEXT("w"));

						FQuat DeltaRotation(X, Y, Z, W);
						DeltaRotation.Normalize();

						// Apply delta to bind pose: finalRotation = bindPose * delta
						FQuat BindPose = (BoneIndex < BindPoseRotations.Num()) ? BindPoseRotations[BoneIndex] : FQuat::Identity;
						FQuat FinalRotation = BindPose * DeltaRotation;
						FinalRotation.Normalize();

						BoneTransform.SetRotation(FinalRotation);
					}
				}

				// Position: use Unity data for all bones
				if (BoneObject->HasField(TEXT("position")))
				{
					const TSharedPtr<FJsonObject>& PosObject = BoneObject->GetObjectField(TEXT("position"));
					if (PosObject.IsValid())
					{
						double X = PosObject->GetNumberField(TEXT("x"));
						double Y = PosObject->GetNumberField(TEXT("y"));
						double Z = PosObject->GetNumberField(TEXT("z"));
						BoneTransform.SetLocation(FVector(X, Y, Z));
					}
				}

				// Parse scale (optional)
				if (BoneObject->HasField(TEXT("scale")))
				{
					const TSharedPtr<FJsonObject>& ScaleObject = BoneObject->GetObjectField(TEXT("scale"));
					if (ScaleObject.IsValid())
					{
						double X = ScaleObject->GetNumberField(TEXT("x"));
						double Y = ScaleObject->GetNumberField(TEXT("y"));
						double Z = ScaleObject->GetNumberField(TEXT("z"));
						BoneTransform.SetScale3D(FVector(X, Y, Z));
					}
				}

				FrameData.Transforms.Add(BoneTransform);
				BoneIndex++;
			}
		}
	}

	// Ensure we have the correct number of transforms
	while (FrameData.Transforms.Num() < StandardBoneNames.Num())
	{
		FrameData.Transforms.Add(FTransform::Identity);
	}

	// Push frame data
	Client->PushSubjectFrameData_AnyThread({SourceGuid, CurrentSubjectName}, MoveTemp(FrameDataStruct));
}

#undef LOCTEXT_NAMESPACE
