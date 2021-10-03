/*  Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#include "PatchController.h"

#include "Play/WorldController.h"


// Sets default values for this component's properties
UPatchController::UPatchController()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPatchController::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITOR
	PlatformName = "Windows";
#endif

#if PLATFORM_ANDROID 
	PlatformName = "Android";
#endif 

#if PLATFORM_IOS
	PlatformName = "iOS";
#endif 

	bFirstAttemptToPatch = true;
	InitPatching();
}


// Called every frame
void UPatchController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CheckTimeOut(DeltaTime);
}

void UPatchController::CheckTimeOut(float DeltaTime)
{

	static uint64 LastBytesDownloadedNum = 0;

	if (bIsPatchManifestUpToDate && LevelDownloadList.Num() > 0 || SongDownloadList.Num() > 0)
	{
		// Get the loading stats
		TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();
		FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

		// Check how long it has been since the last packet of data was downloaded
		if (LastBytesDownloadedNum == LoadingStats.BytesDownloaded)
		{
			// Timout every download and throw error message if it has been more than 10 seconds
			if (SecondsSinceLastByteWasReceived >= 10.0f)
			{
				ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
				GameMode->ThrowDebugMessage(200, EDebugMessageType::Type::ERROR, "10s patch controller timeout", true);

				for (int32 LevelID : LevelDownloadList)
					OnLevelDownloadFailure.Broadcast(LevelID);
				for (int32 SongID : SongDownloadList)
					OnSongDownloadFailure.Broadcast(SongID);

				bDownloadTimeOut = true;
				SecondsSinceLastByteWasReceived = 0.0f;
			}

			SecondsSinceLastByteWasReceived += DeltaTime;
		}
		// If download resumed after a time out - update all cache indicators and the play button
		else if (bDownloadTimeOut)
		{
			for (int32 LevelID : LevelDownloadList)
				OnLevelDownloadStart.Broadcast(LevelID);
			for (int32 SongID : SongDownloadList)
				OnSongDownloadStart.Broadcast(SongID);

			SecondsSinceLastByteWasReceived = 0.0f;
		}

		LastBytesDownloadedNum = LoadingStats.BytesDownloaded;
	}
}

void UPatchController::Shutdown()
{
#if WITH_EDITOR
	return;
#endif

	FChunkDownloader::Shutdown();
}

void UPatchController::InitPatching()
{
#if WITH_EDITOR
	return;
#endif

	bIsPatchingGame = true;

	HttpModule = &FHttpModule::Get();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule->CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UPatchController::OnPatchVersionResponse);
	Request->OnRequestProgress().BindUObject(this, &UPatchController::OnPatchVersionProgress);

	// Todo: make this url a member variable
	Request->SetURL(FString::Printf(TEXT("https://ritmolevels.s3.eu-west-2.amazonaws.com/ContentBuild.txt")));
	Request->SetVerb("GET");
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
	Request->SetHeader("Content-Type", TEXT("application/json"));
	Request->ProcessRequest();
}

void UPatchController::OnPatchVersionProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	int32 FullSize = Request->GetContentLength();

#if WITH_EDITOR
	UE_LOG(LogTemp, Warning, TEXT("Sent: %i, Received: %i, FullSize: %i"), BytesSent, BytesReceived, FullSize);
#endif
}

void UPatchController::OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bResponseSuccess)
{
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();

	if (!bResponseSuccess || !Response.IsValid())
	{
		// The first patching attempt is always initiated by the game to make the initial update of the manifest file
		// Show "Failed to update the game" error on the first occasion
		if (bFirstAttemptToPatch)
		{
			ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
			GameMode->ThrowDebugMessage(201, EDebugMessageType::Type::ERROR, Response->GetContentAsString(), true);
		}
		// On all other ocassions - it is the user trying to download content when their manifest file is not up to date
		// Show "No internet connection" error
		else
		{
			ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
			GameMode->ThrowDebugMessage(200, EDebugMessageType::Type::ERROR, Response->GetContentAsString(), true);
		}

		bNoInternet = true;
		bIsPatchingGame = false;
		bIsPatchManifestUpToDate = false;
		bFirstAttemptToPatch = false;
		OnPatchReady.Broadcast(false);
		return;
	}

	bNoInternet = false;
	const FString DeploymentName = "Ritmo-Live";
	const FString ContentBuildID = Response->GetContentAsString(); // ID string from the ContentBuild.txt file
	const int MaxStreams = 8;
	Downloader->Initialize(PlatformName, MaxStreams);
	Downloader->LoadCachedBuild(DeploymentName);

	// Called when the Downloader fished downloading the new patch file
	TFunction<void(bool)> ManifestCompleteCallback = [this](bool bSuccess)
	{
		if (!bSuccess)
		{
			ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
			GameMode->ThrowDebugMessage(201, EDebugMessageType::Type::ERROR, GetPatchStatus().LastError.ToString(), true);
		}

		bIsPatchManifestUpToDate = bSuccess;

		bIsPatchingGame = false;

		// Call a delegate to notify that we are ready to start patching
		OnPatchReady.Broadcast(bSuccess);
	};

	// Update the manifest file and call ManifestCompleteCallback
	Downloader->UpdateBuild(DeploymentName, ContentBuildID, ManifestCompleteCallback);
}

int32 UPatchController::AssetIDtoChunkID(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID)
{
	int32 ChunkID = 0;

	switch (AssetType)
	{
	case EAssetType::LEVEL:
	{
		AWorldController* WC = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());
		ChunkID = WC->LevelLibrary->GetLevelMeta(AssetID).ChunkID;
	}
	break;
	case EAssetType::SONG:
	{
		AWorldController* WC = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());
		ChunkID = WC->SongLibrary->Songs[AssetID].ChunkID;
	}
	break;
	default:
		break;
	}

	return ChunkID;
}

bool UPatchController::IsChunkCached(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID)
{
#if WITH_EDITOR
	return true;
#endif

	const int32 ChunkID = AssetIDtoChunkID(AssetType, AssetID);

	switch (AssetType)
	{
	case EAssetType::LEVEL:
	{
		// If a level is available by default and comes with the game - it doesn't need to be downloaded
		AWorldController* WC = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());
		if (AssetID >= WC->LevelLibrary->Levels.Num())
			return false;
		if (WC->LevelLibrary->GetLevelMeta(AssetID).bPreloaded || WC->LevelLibrary->GetLevelMeta(AssetID).LevelBP.IsValid())
			return true;
	}
	break;
	case EAssetType::SONG:
	{
		// If a song is available by default and comes with the game - it doesn't need to be downloaded
		AWorldController* WC = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());
		if (AssetID >= WC->SongLibrary->Songs.Num())
			return false;
		if (WC->SongLibrary->Songs[AssetID].bPreloaded || WC->SongLibrary->Songs[AssetID].SoundWave.IsValid())
			return true;
	}
	break;
	default:
		break;
	}

	if (!bIsPatchManifestUpToDate)
		return false;

	// ALL of the game content is stored in Chunk 0 by default, we only want chunk IDs that are >= 1
	if (ChunkID <= 0)
		return false;

	// If the chunk is mounted or cached - return true
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();
	return Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Mounted || Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Cached;
}

bool UPatchController::IsChunkMounted(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID)
{
#if WITH_EDITOR
	return true;
#endif

	switch (AssetType)
	{
	case EAssetType::LEVEL:
	{
		// If a level is available by default and comes with the game - it doesn't need to be downloaded
		AWorldController* WC = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());
		if (WC->LevelLibrary->GetLevelMeta(AssetID).bPreloaded || WC->LevelLibrary->GetLevelMeta(AssetID).LevelBP.IsValid())
			return true;
	}
	break;
	case EAssetType::SONG:
	{
		// If a song is available by default and comes with the game - it doesn't need to be downloaded
		AWorldController* WC = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());
		if (WC->SongLibrary->Songs[AssetID].bPreloaded || WC->SongLibrary->Songs[AssetID].SoundWave.IsValid())
			return true;
	}
	break;
	default:
		break;
	}

	const int32 ChunkID = AssetIDtoChunkID(AssetType, AssetID);

	// If the chunk is mounted - return true
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();
	return Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Mounted;
}

bool UPatchController::DownloadSingleLevel(int32 LevelID)
{
#if WITH_EDITOR
	return false;
#endif

	int32 ChunkID = AssetIDtoChunkID(EAssetType::LEVEL, LevelID);
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	if (bNoInternet || !bIsPatchManifestUpToDate)
	{
		// Try to update the manifest file again, in case the user now has access to the internet
		InitPatching();
		return false;
	}
	if (bIsPatchingGame)
	{
		ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
		GameMode->ThrowDebugMessage(208, EDebugMessageType::Type::WARNING, "", true);
		return false;
	}
	if (IsChunkMounted(EAssetType::Type::LEVEL, LevelID) || Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Downloading)
		return false;

	LevelDownloadList.AddUnique(LevelID);

	// Called when the chunk is downloaded and mounted
	TFunction<void(bool)> LevelMountCompleteCallback = [this](bool bSuccess)
	{
		if (!bSuccess)
		{
			ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
			GameMode->ThrowDebugMessage(204, EDebugMessageType::Type::ERROR, GetPatchStatus().LastError.ToString(), true);
		}

		FinishedDownloadingChunk();
	};

	// Make the level pak file available for use by downloading and mounting them in the memory
	Downloader->MountChunk(ChunkID, LevelMountCompleteCallback);
	OnLevelDownloadStart.Broadcast(LevelID);
	return true;
}

bool UPatchController::DownloadSingleSong(int32 SongID)
{
#if WITH_EDITOR
	return false;
#endif

	int32 ChunkID = AssetIDtoChunkID(EAssetType::SONG, SongID);

	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	if (bNoInternet || !bIsPatchManifestUpToDate)
	{
		// Try to update the manifest file again, in case the user now has access to the internet
		InitPatching();
		return false;
	}
	if (bIsPatchingGame)
	{
		ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
		GameMode->ThrowDebugMessage(208, EDebugMessageType::Type::WARNING, "", true);
		return false;
	}
	if (IsChunkMounted(EAssetType::Type::SONG, SongID) || Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Downloading)
		return false;

	SongDownloadList.AddUnique(SongID);

	// Called when the chunk is downloaded and mounted
	TFunction<void(bool)> SongMountCompleteCallback = [this](bool bSuccess)
	{
		if (!bSuccess)
		{
			ARhythmGameGameMode* GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
			GameMode->ThrowDebugMessage(205, EDebugMessageType::Type::ERROR, GetPatchStatus().LastError.ToString(), true);
		}

		FinishedDownloadingChunk();
	};

	// Make the level pak file available for use by downloading and mounting them in the memory
	Downloader->MountChunk(ChunkID, SongMountCompleteCallback);
	OnSongDownloadStart.Broadcast(SongID);
	return true;
}

FPPatchStats UPatchController::GetPatchStatus()
{
	FPPatchStats Stats;

#if WITH_EDITOR
	return Stats;
#endif

	// Get the loading stats
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();
	FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

	// Copy the info into the output stats.
	// ChunkDownloader tracks bytes downloaded as uint64s, which have no BP support,
	// So we divide to MB and cast to int32 (signed) to avoid overflow and interpretation errors.
	Stats.FilesDownloaded = LoadingStats.TotalFilesToDownload;
	Stats.MBDownloaded = (int32)(LoadingStats.BytesDownloaded / (1024 * 1024));
	Stats.TotalMBToDownload = (int32)(LoadingStats.TotalBytesToDownload / (1024 * 1024));
	Stats.DownloadPercent = (float)LoadingStats.BytesDownloaded / (float)LoadingStats.TotalBytesToDownload;
	Stats.LastError = LoadingStats.LastError;
	
	return Stats;
}

bool UPatchController::IsChunkDownloadActive(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID)
{
	return !bDownloadTimeOut && 
		((AssetType == EAssetType::Type::LEVEL && LevelDownloadList.Contains(AssetID) ||
		(AssetType == EAssetType::Type::SONG && SongDownloadList.Contains(AssetID))));
}

void UPatchController::FinishedDownloadingChunk()
{
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// Check all levels
	for (int i = 0; i < LevelDownloadList.Num(); i++)
	{
		int32 LevelID = LevelDownloadList[i];
		int32 ChunkID = AssetIDtoChunkID(EAssetType::Type::LEVEL, LevelID);

		FChunkDownloader::EChunkStatus ChunkStatus = Downloader->GetChunkStatus(ChunkID);
		switch (ChunkStatus)
		{
		case FChunkDownloader::EChunkStatus::Mounted:
			{
				OnLevelDownloadSuccess.Broadcast(LevelID);
				LevelDownloadList.RemoveAt(i);
			}
			break;
		case FChunkDownloader::EChunkStatus::Partial:
			{
				OnLevelDownloadFailure.Broadcast(LevelID);
				LevelDownloadList.RemoveAt(i);
			}
			break;
		case FChunkDownloader::EChunkStatus::Remote:
			{
				OnLevelDownloadFailure.Broadcast(LevelID);
				LevelDownloadList.RemoveAt(i);
			}
			break;
		case FChunkDownloader::EChunkStatus::Unknown:
			{
				OnLevelDownloadFailure.Broadcast(LevelID);
				LevelDownloadList.RemoveAt(i);
			}
			break;
		}
	}

	// Check all songs
	for (int i = 0; i < SongDownloadList.Num(); i++)
	{
		int32 SongID = SongDownloadList[i];
		int32 ChunkID = AssetIDtoChunkID(EAssetType::Type::SONG, SongID);

		FChunkDownloader::EChunkStatus ChunkStatus = Downloader->GetChunkStatus(ChunkID);
		switch (ChunkStatus)
		{
		case FChunkDownloader::EChunkStatus::Mounted:
			{
				OnSongDownloadSuccess.Broadcast(SongID);
				SongDownloadList.RemoveAt(i);
			}
			break;
		case FChunkDownloader::EChunkStatus::Partial:
			{
				OnSongDownloadFailure.Broadcast(SongID);
				SongDownloadList.RemoveAt(i);
			}
			break;
		case FChunkDownloader::EChunkStatus::Remote:
			{
				OnSongDownloadFailure.Broadcast(SongID);
				SongDownloadList.RemoveAt(i);
			}
			break;
		case FChunkDownloader::EChunkStatus::Unknown:
			{
				OnSongDownloadFailure.Broadcast(SongID);
				SongDownloadList.RemoveAt(i);
			}
			break;
		}
	}
}