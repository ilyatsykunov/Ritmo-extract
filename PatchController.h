/*  This class is responsible for downloading songs and levels to the user's device and then updating them when needed.

	Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#pragma once

// Unreal includes
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "ChunkDownloader.h"

// Keep this last
#include "PatchController.generated.h"

// patching delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPatchCompleteDelegate, bool, Succeeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChunkMountedDelegate, int32, ChunkID, bool, Succeeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAssetDownloadEndDelegate, int32, AssetID);

UENUM(BlueprintType)
namespace EAssetType
{
	enum Type
	{
		SONG,
		LEVEL
	};
}

UENUM(BlueprintType)
namespace EAssetStatus
{
	enum Status
	{
		NONE, // No local caching has started
		MOUNTED, // chunk is cached locally and mounted in RAM
		CACHED, // chunk is fully cached locally but not mounted
		DOWNLOADING, // chunk is partially cached locally, not mounted, download in progress
		PARTIAL, // chunk is partially cached locally, not mounted, download NOT in progress
		UNKNOWN // no paks are included in this chunk, can consider it either an error or fully mounted depending
	};
}

// Reports game patching stats that can be used for progress feedback
USTRUCT(BlueprintType)
struct FPPatchStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
		int32 FilesDownloaded;
	UPROPERTY(BlueprintReadOnly)
		int32 TotalFilesToDownload;
	UPROPERTY(BlueprintReadOnly)
		float DownloadPercent;
	UPROPERTY(BlueprintReadOnly)
		int32 MBDownloaded;
	UPROPERTY(BlueprintReadOnly)
		int32 TotalMBToDownload;
	UPROPERTY(BlueprintReadOnly)
		FText LastError;
};

/*
	Patching is currently only available in shipping builds. All functions return false or void when called in the editor.
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RHYTHMGAME_API UPatchController : public UActorComponent
{
	GENERATED_BODY()

public:	

	/* ############################################# DELEGATES ###################################################### */

	// Fired when the patching manifest has been queried and we're ready to decide whether to patch the game or not*/
	UPROPERTY(BlueprintAssignable) FPatchCompleteDelegate OnPatchReady;

	// Fired when the patching process succeeds or fails 
	UPROPERTY(BlueprintAssignable) FPatchCompleteDelegate OnPatchComplete;

	UPROPERTY(BlueprintAssignable) FAssetDownloadEndDelegate OnLevelDownloadStart;
	UPROPERTY(BlueprintAssignable) FAssetDownloadEndDelegate OnLevelDownloadSuccess;
	UPROPERTY(BlueprintAssignable) FAssetDownloadEndDelegate OnLevelDownloadFailure;
	UPROPERTY(BlueprintAssignable) FAssetDownloadEndDelegate OnSongDownloadStart;
	UPROPERTY(BlueprintAssignable) FAssetDownloadEndDelegate OnSongDownloadSuccess;
	UPROPERTY(BlueprintAssignable) FAssetDownloadEndDelegate OnSongDownloadFailure;

	/* ############################################# PUBLIC FUNCTIONS ###################################################### */


	// Sets default values for this component's properties
	UPatchController();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called when the user exits the game
	UFUNCTION(BlueprintCallable) void Shutdown();

	// Updates the BuildManifest file to the most recent version
	UFUNCTION(BlueprintCallable) void InitPatching();

	// Starts the game patching process, updating any chunks that have previously been downloaded. Returns false if the BuildManifest is not up to date
	UFUNCTION(BlueprintCallable) bool PatchGame();

	/* Converts either a LevelLibrary ID or SongLibrary ID to a global Chunk ID. 
	*  All of the downloadable assets are given a Chunk ID in order for them to be packaged into PAK (.pak) files,
	*  and it is what is used for downloading and storing assets.
	*/
	UFUNCTION(BlueprintCallable) int32 AssetIDtoChunkID(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID);

	/* Downloads and mounts a single pak file into the active memory
	* @param ChunkID - ID of the PrimaryAssetLabel inside the pak file
	*/
	//UFUNCTION(BlueprintCallable) bool DownloadSingleChunk(int32 ChunkID);

	/* Downloads and mounts a single level into the active memory
	* @param LevelID - ID of the level in the LevelLibrary.
	* @return - true if download was started, false if not
	*/
	UFUNCTION(BlueprintCallable) bool DownloadSingleLevel(int32 LevelID);

	/* Downloads and mounts a single song into the active memory
	* @param SongID - ID of the song in the AudioLibrary
	* @return - true if download was started, false if not
	*/
	UFUNCTION(BlueprintCallable) bool DownloadSingleSong(int32 SongID);

	/* Checks if a pak file can be found on the device
	* @param AssetType - What kind of assets us the pak file for
	* @param AssetID - ID of the asset (SongID or LevelID)
	*/
	UFUNCTION(BlueprintCallable) bool IsChunkCached(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID);

	/* Checks if a pak file is loaded into the working memory
	* @param AssetType - What kind of assets us the pak file for
	* @param AssetID - ID of the asset (SongID or LevelID)
	*/
	UFUNCTION(BlueprintCallable) bool IsChunkMounted(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID);

	UFUNCTION(BlueprintCallable) bool IsChunkDownloadActive(TEnumAsByte<EAssetType::Type> AssetType, int32 AssetID);

	// Returns a patching status report we can use to populate progress bars, etc
	UFUNCTION(BlueprintCallable) FPPatchStats GetPatchStatus();

	/* ############################################# ACCESSOR FUNCTIONS ###################################################### */

	UFUNCTION(BlueprintCallable) bool IsPatchManifestUpToDate() { return bIsPatchManifestUpToDate; }

	UFUNCTION(BlueprintCallable) bool IsConnectedToInternet() { return !bNoInternet; }

	
protected:

	/* ############################################# PROTECTED FUNCTIONS ###################################################### */

	// Called when the game starts
	virtual void BeginPlay() override;
	// Called every frame when a download is active. Notifies the user if they have problems with connection
	void CheckTimeOut(float DeltaTime);
	// Receives the BuildManifest version query response
	void OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bResponseSuccess);
	// Watches the patching process
	void OnPatchVersionProgress(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);
	// Called every time a level/song/etc is finished downloading, whether successfully or not
	// Finds out which asset has been downloaded and calls relevant delegates to notify of this
	void FinishedDownloadingChunk();

	/* ############################################# PROTECTED VARIABLES ###################################################### */

	FHttpModule* HttpModule;

	FString PlatformName = "Android";

	// Whether we have the most recent version of the BuildManifest 
	bool bIsPatchManifestUpToDate = false;
	// Whether the BuildManifest is currently beign updated
	bool bIsPatchingGame;
	// Whether the BuildManifest is currently beign updated
	bool bNoInternet;
	// Whether a level is currently being downloaded
	bool bIsDownloadingSingleChunk;
	// The first patching attempt is always initiated by the game to make the initial update of the content build file
	bool bFirstAttemptToPatch;
	// Stores chunk IDs of levels that are currently being downloaded
	TArray<int32> LevelDownloadList;
	// Stores chunk IDs of songs that are currently being downloaded
	TArray<int32> SongDownloadList;
	// All chunks for downloading and mounting
	TArray<int32> ChunkDownloadList;
	// Time count since the last piece of data was downloaded. If this time goes over 10 seconds, all downloads will get cancelled
	// and user will be notified that they need to be connected to the internet
	float SecondsSinceLastByteWasReceived;
	bool bDownloadTimeOut;
};
