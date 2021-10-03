/*  This is the base class for a level in Ritmo. It consists of 3 lane components which the notes move down
	This repository is only for demonstration purposes so some of the includes aren't actually here.

	Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#pragma once

// Ritmo classes
#include "/RitmoLevelMeta.h"
#include "Lane.h"

// Unreal includes
#include "Engine.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "Components/SplineMeshComponent.h"
#include "EnumTypes.h"

// Keep this last
#include "BaseRitmoLevel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReceiveButtonPress, ULane*, TouchedLane);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReceiveButtonLift, ULane*, TouchedLane);

struct FSongData;

USTRUCT(BlueprintType)
struct FRitmoTransform
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
		FVector Location;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
		FRotator Rotation;
};

UCLASS()
class RHYTHMGAME_API ABaseRitmoLevel : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABaseRitmoLevel();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;


	/* ############################################# EVENTS ############################################# */

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Component Setup"))
		void ReceiveComponentSetup();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Start Playing"))
		void ReceiveStartPlaying();
	/* This is where you should call Start Playing for the game to begin */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Post-Load"))
		void ReceiveFinishedLoading();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Stop Playing"))
		void ReceiveStopPlaying();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "New Move Speed"))
		void ReceiveNewMoveSpeed(float NewSpeed);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Button Event"))
		void ReceiveButtonEvent(ULane* Lane, ButtonParams Event, FLinearColor Color);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Activate Button"))
		void ReceiveActivateButton(int LaneIdx);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Deactivate Button"))
		void ReceiveDectivateButton(int LaneIdx);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Note Hit"))
		void ReceiveNoteHit(ABaseNote* Note);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Note Miss"))
		void ReceiveNoteMiss(ABaseNote* Note);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Note Spawned"))
		void ReceiveNoteSpawn(ABaseNote* Note);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Hold Note Segment Spawned"))
		void ReceiveNoteSegmentSpawn(USplineMeshComponent* Segment);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "Camera Switch"))
		void ReceiveCameraSwitch(int Idx);


	/* ############################################# PUBLIC FUNCTIONS ############################################# */

	/* Called on each frame to handle camera shake
	*/
	virtual void CameraShake(float DeltaTime);

	/* Called to load the level
	* @param LevelMeta - Struct containing references to mesh assets that will be used by the notes
	* @param SongMeta - Struct containing info about the song: sound wave, level map, etc
	* @param SizeMultiplier - Used for scailing the game world to better suit the user's device
	*/
	virtual void LoadLevel(FRitmoLevelPlayData& LevelMeta, FSongData& SongMeta, FVector SizeMultiplier);

	/* When we resume the game from a pause state
	*/
	UFUNCTION(BlueprintCallable)
		virtual void StartPlaying();

	/* When we pause or exit the game
	*/
	virtual void StopPlaying();

	/* Called on user input or when a note either enters or leaves the bounds 
	*/
	UFUNCTION() virtual void ButtonEvent(int LaneIdx, ButtonParams Event, FLinearColor Color);

	/* When the user hits a note - this will be passed in from WorldController
	*/
	UFUNCTION() virtual void NoteHit(ABaseNote* Note);

	/* Called on incorrect input / when a tile was not hit in time
	*/
	UFUNCTION() virtual void NoteMiss(ABaseNote* Note);

	/* Each frame the user holds within the button bounds
	*/
	virtual void TouchHeld(float DeltaTime);

	/* When the user touches the button
	*/
	UFUNCTION() virtual void ActivateButton(ULane* Lane);

	/* When the user released / navigates away from the button with their touch
	*/
	UFUNCTION() virtual void DeactivateButton(ULane* Lane);

	virtual void SetMoveSpeed(float NewSpeed);

	/* Called every time the camera location, rotation or fov is changed
	*/
	virtual void NewCamTransform(FVector CamLoc, FRotator CamRot, float CamFov);

	/* Used when restarting level - resets all values in this level
	*/
	UFUNCTION() virtual void ResetLevel();

	/* When a note spawns - alert the level. If you need to do anything to notes at runtime before they're seen. Do it here 
	* @param Note - The new note spawned
	*/
	UFUNCTION() virtual void NativeReceiveNoteSpawn(ABaseNote* Note);

	/* When a hold note spawns a segment i.e. when it occupies a longer duration.   If you need to do anything to hold note segments at runtime before they're seen. Do it here 
	* @param Segment - The new segment spawned
	*/
	UFUNCTION() virtual void NativeReceiveSegmentSpawned(USplineMeshComponent* Segment);

	/* ############################################# ACCESSORS  ############################################# */

	UFUNCTION(BlueprintCallable) TArray<ULane*> GetLanes() { return Lanes; }
	UFUNCTION(BlueprintCallable) float			GetMoveSpeed() { return MoveSpeed;  }

	/* ############################################# DELEGATES ############################################# */

	UPROPERTY(BlueprintAssignable)								FReceiveButtonPress				OnButtonPress;
	UPROPERTY(BlueprintAssignable)								FReceiveButtonLift				OnButtonLift;

	/* ############################################# GENERAL FUNCTIONS ############################################# */

	/* Returns quaternion rotation of a given euler rotation
	* @param Rot - Euler rotation
	*/
	UFUNCTION(BlueprintCallable) FQuat EulerToQuat(FRotator Rot) { return Rot.Quaternion(); }

	/* Returns relative quaternion rotation of a given object
	* @param Cmp - Component to get rotation of
	*/
	UFUNCTION(BlueprintCallable) FQuat GetRelativeRotationOfAnObject(USceneComponent* Cmp) { return Cmp->GetRelativeRotation().Quaternion(); }

	/* Returns world quaternion rotation of a given object
	* @param Cmp - Component to get rotation of
	*/
	UFUNCTION(BlueprintCallable) FQuat GetWorldRotationOfAnObject(USceneComponent* Cmp) { return Cmp->GetComponentRotation().Quaternion(); }

	/* Sets the relative rotation of a given scene component
	* @param Cmp - Component to set the rotation of
	* @param Rot - Quaternion rotation
	*/
	UFUNCTION(BlueprintCallable) void SetRelativeRotationOfComponent(USceneComponent* Cmp, FQuat Rot) { Cmp->SetRelativeRotation(Rot); }

	/* Sets the world rotation of a given scene component
	* @param Cmp - Component to set the rotation of
	* @param Rot - Quaternion rotation
	*/
	UFUNCTION(BlueprintCallable) void SetWorldRotationOfComponent(USceneComponent* Cmp, FQuat Rot) { Cmp->SetWorldRotation(Rot); }

	/* ############################################# PUBLIC GENERAL VARIABLES ############################################# */

	// Note move speed
	UPROPERTY(EditDefaultsOnly)															float						MoveSpeed;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)										bool						bDelayStart = false;
	// How long to wait at the start of the game before notes are spawned
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)										float						StartDelay = 4.0f;

	// Contains references to note assets that will be used by the level 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly,  Category = "Note Settings")			FRitmoLevelPlayData			PlayData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")					UCameraComponent*			CameraComponent;
	UPROPERTY(EditDefaultsOnly)															FRitmoLevelCameraParams		CameraParams;
	// Transforms for changing camera location / rotation.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Camera | Default")		TArray<FRitmoTransform>		CameraTransforms;
	// The index of the initial camera transform
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Camera | Default")		int							CameraTransformIndex;
	// The index of the current camera transform (used by the camera switch event)
	UPROPERTY(BlueprintReadOnly, Category = "Camera | Default")							int							RuntimeCamTransformIndex;
	
	/* ############################################# POST PROCESSING ######################################################### */

	// Index of the glitch effect
	UPROPERTY(BlueprintReadOnly, Category = "Camera | PostProcessing")					int	  	ppMatArrayIndex;
	// Length and intensity of the glitch effect and camera shake
	UPROPERTY(EditAnywhere, Category = "Camera | PostProcessing")						float 	ppEffectLength = 0.33f;
	UPROPERTY(EditAnywhere, Category = "Camera | PostProcessing")						float 	ppEffectMaxAmount;
	UPROPERTY()																			float 	ppEffectAmount;
	UPROPERTY()																			float 	ppEffectSpeed;
	UPROPERTY()																			FVector	CamShakeLocMax;


	// Holds the materials that we will use to create at runtime
	UPROPERTY(BlueprintReadOnly, Category = "Camera | PostProcessing")					TArray<UMaterialInterface*>			ppMatArray;

	// Used at runtime to hold materials we create. Index 0 must be the Glitch and Index 1 must be the Background Blur
	UPROPERTY(BlueprintReadOnly, Category = "Camera | PostProcessing")					TArray<UMaterialInstanceDynamic*>	ppMatDynamicArray;


	/* Gradually decreases the glitch and camera shake effect over time
	*/
	virtual void ppEffectsTick(float DeltaTime);
	
	/* Starts the glitch and camera shake effect
	*/
	virtual void ppGenerateEffect(float NewppEffectAmount);



protected:

	/* Attaches components of this object to member variables and sets their initial parameters
	*/
	virtual void SetUpComponents();

	/* ############################################# PROTECTED GENERAL VARIABLES ############################################# */

	UPROPERTY(BlueprintReadWrite)								TArray<ULane*>				Lanes;
	UPROPERTY(BlueprintReadWrite)								FVector						ViewportSizeMultiplier;


	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (DisplayName = "Show Debug Messages"))				bool	bDebugMessages;


#if WITH_EDITOR
	/* When the user sets their PlayData params, depending on which mesh type they use (Static/Skeletal/Spline/Sprite)
	   show them only the relevant variables
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
