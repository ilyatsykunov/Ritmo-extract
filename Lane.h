/*  This is a Lane component class which the notes move on.

	Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#pragma once

// Ritmo classes
#include "RhythmGameGameMode.h"
#include "../RitmoLevelMeta.h"

// Unreal includes
#include "Engine.h"
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Keep this last
#include "Lane.generated.h"

class ABaseNote;
class ABaseRitmoLevel;

UENUM(BlueprintType)
enum ButtonParams
{
	NO_CHANGE = -2,
	INACTIVE = -1,
	IDLE = 0,
	NOTE_WITHIN_BOUNDS = 1,
	NOTE_HIT = 2,
	NOTE_MISS = 3
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNoteFinished, ABaseNote*, Note);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNoteMiss, ABaseNote*, Note);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCompleteMiss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnButtonEvent, int, LaneIdx, ButtonParams, Event, FLinearColor, Color);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RHYTHMGAME_API ULane : public USceneComponent
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)		bool bDebugMessages;


	// Sets default values for this component's properties
	ULane();

	/* ############################################# EVENTS ############################################# */

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "StartPlaying"))
		void ReceiveStartPlaying();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "StopPlaying"))
		void ReceiveStopPlaying();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (DisplayName = "NewMoveSpeed"))
		void ReceiveNewMoveSpeed(float NewSpeed);

	/* ############################################# PUBLIC FUNCTIONS ############################################# */

	virtual void			BeginPlay() override;
	virtual void			TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	/* Sets the parameters of the lane in correspondence with the target screen resolution and the active level
	* @param NewLaneIdx		- The index of the lane (0-2)
	* @param NewMoveSpeed	- The default speed of the notes
	* @param SizeMultiplier - Multiplied by the current scale, to adjust to the screen size
	* @param Ring0Mat		- Material of ring 0
	* @param Ring1Mat		- Material of ring 1
	* @param LaneMat		- Material of the lane
	*/
	virtual void			SetParameters(int NewLaneIdx, int NewMoveSpeed, FVector SizeMultiplier, UMaterialInstanceDynamic* Ring0Mat,	UMaterialInstanceDynamic* Ring1Mat, UMaterialInstanceDynamic* LaneMat = nullptr);


	/* Moves the notes in its array and manages their location states, also handles note misses where they reach the end of the lane
	* @param DeltaTime - DeltaTime..
	*/
	void					UpdateNotes(float DeltaTime);


	/*
	* What colour are the particles
	* @param ParticleColor - LinearColor of the particles
	*/
	virtual void			SetInitialParticleColor(FLinearColor ParticleColor);


	/* Fired when the game is unpaused
	*/ 
	virtual void			StartPlaying();


	/* Fired when the game is paused / exits
	*/
	virtual void			StopPlaying();


	/* When a note has finished i.e. hit completely 
	* @param Note - The note that was finished
	*/
	UFUNCTION()
	virtual void			NoteHit(ABaseNote* Note);

	/* When a note was missed i.e. not hit in time or not enough of the note was hit 
	* @param Note - The note was that was missed
	*/
	UFUNCTION()
	virtual void			NoteMiss(ABaseNote* Note);

	/*	When the user presses a button but no note is within the bounds
	*/
	UFUNCTION()
	virtual void			CompleteMiss();


	/* Each frame a button is held on this lane, if a note is within the button range, pass along the values
	* @param SecondsSinceStart - How long since the start of the level has passed
	* @param DeltaTime		   - DeltaTime..
	*/
	UFUNCTION()
	void TouchHeld(float SecondsSinceStart, float DeltaTime);

	/* Each frame a button is not held on this lane, if a note is within the button range, pass along the values. This isn't currently used
	* @param SecondsSinceStart - How long since the start of the level has passed
	* @param DeltaTime		   - DeltaTime..
	*/
	UFUNCTION()
	void TouchNotHeld(float SecondsSinceStart, float DeltaTime);

	/* When we release the button after pressing / holding it
	*/
	UFUNCTION()
	void TouchReleased(const TEnumAsByte<ETouchIndex::Type> TouchIndex);


	/* When we press the button - switch the ring state and fire particles if necessary
	*/
	UFUNCTION()
	virtual void			ActivateButton();

	/* If we need to manually deactivate the button - currently only sets bButtonHit to false
	*/
	virtual void			DeactivateButton();


	/*	When we hit a note, we need to set the particle colour to the hit note's colour - this is called to set it
	* @param Color	- The new colour for the particles
	*/
	virtual  void			SetParticleColor(FLinearColor Color);


	/* Each frame we hit a note - we keep the particles going
	*/
	virtual void			ActivateParticleGen();


	/* When we start a new level / restart. Set default values
	*/
	UFUNCTION() virtual void			ResetLane();

	/* Not sure what use this is, this will probably be removed / changed
	*/
	virtual float			SetMoveSpeed(float NewSpeed);

	/**/
	
	void SetGameSpeed(float NewSpeed);

	/* When a note has been hit completely or missed we need to remove it from the lane.
	* @param Note - The note to remove
	*/
	virtual void			DeactivateNote(ABaseNote* const Note);

	/* Given a note, set it up to use the lane
	* @param Note - The note to add
	*/
	virtual void			ActivateNote(ABaseNote* const Note);

	/* This should be removed - not needed anymore
	*/
	void					UpdateQueue();

	/* Given an event - change the ring accordingly 
	*/
	void					SwitchRing(ButtonParams Event);

	/* After we've loaded the map, give the notes to each lane
	* @param Rows			- Array containing the note types and times to spawn them at
	* @param HoldNoteData	- Information about the duration of hold notes we refer to when spawning them 
	*/
	void					LoadNotes(TArray<FLevelMapRow> Rows, TArray<TPair<float, float>> HoldNoteData);


	/* Each lane is responsible for spawning its own notes, it does so here each frame
	*/
	void					NoteSpawn();


	/* If we want to start at a point that isn't 0s (test mode) we can set that up here
	*/
	void					CustomStart(float StartTimeValue);

	/* Every note has a chance to be a bomb, igc or random note, this decides it
	* @param NoteType& - The note to potentially change
	*/
	void					RandSwapForSpecial(ENoteType& NoteType);

	/* Returns the % of how far the point is along the movement path of this lane
	*/
	float					GetPercentageAlongMovementPathAtSplinePoint(int Point);

	/* Returns the location of the input % along the movement path of this lane
	*/
	FVector					GetLocAtPercentageAlongMovementPath(float Percentage, ESplineCoordinateSpace::Type CoordinateSpace);

	/* Returns the location of the input % along the movement path of this lane
	*/
	FVector					GetTanAtPercentageAlongMovementPath(float Percentage, ESplineCoordinateSpace::Type CoordinateSpace);

	/* Returns the rotation of the input % along the movement path of this lane
	*/
	FRotator				GetRotAtPercentageAlongMovementPath(float Percentage, ESplineCoordinateSpace::Type CoordinateSpace);


	/* ############################################# ACCESSORS  ############################################# */

	UFUNCTION(BlueprintCallable)	inline FVector					GetEndLoc()								{ return EndLoc; }
	UFUNCTION(BlueprintCallable)	inline FVector					GetStartLoc()							{ return StartLoc; }
	UFUNCTION(BlueprintCallable)	inline int						GetLaneIdx()							{ return LaneIdx; }
	UFUNCTION(BlueprintCallable)	inline float					GetMoveSpeed()							{ return MoveSpeed; }
	UFUNCTION(BlueprintCallable)	inline bool 					GetReversed()							{ return bReversed; }
	UFUNCTION(BlueprintCallable)	inline float					GetLaneLength()							{ return LaneLength; }					// Returns the size of the lane mesh along the X axis 
	UFUNCTION(BlueprintCallable)	inline bool						IsInputValid()							{ return bInputValid; }
	UFUNCTION(BlueprintCallable)	inline bool 					GetButtonHit()							{ return bButtonHit; }
	UFUNCTION(BlueprintCallable)	inline FVector2D				GetButtonDimensions()					{ return ButtonDimensions; }
	UFUNCTION(BlueprintCallable)	inline float					GetSpawnTimeOffset()					{ return SpawnTimeOffset; }
	UFUNCTION(BlueprintCallable)	inline bool						GetButtonIsMoving()						{ return bButtonIsMoving; }
	UFUNCTION(BlueprintCallable)	inline bool 					GetButtonIsPressed()					{ return bButtonIsPressed; }
	UFUNCTION(BlueprintCallable)	inline FVector2D				GetButtonViewportLoc()					{ return ButtonViewportLoc; }
	UFUNCTION(BlueprintCallable)	inline FVector 					GetNoteBoundaryStart()					{ return GetLocAtPercentageAlongMovementPath(GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryStartPointIdx), ESplineCoordinateSpace::World); }
	UFUNCTION(BlueprintCallable)	inline FVector 					GetNoteBoundaryEnd()					{ return GetLocAtPercentageAlongMovementPath(GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryEndPointIdx), ESplineCoordinateSpace::World); }
	UFUNCTION(BlueprintCallable)	inline float 					GetButtonPressLength()					{ return ButtonPressLength; }
	UFUNCTION(BlueprintCallable)	inline FVector2D				GetButtonViewportDimensionsN()			{ return ButtonViewportDimensionsN; } 	// Get negative dimensions of the button (left and top points)
	UFUNCTION(BlueprintCallable)	inline FVector2D				GetButtonViewportDimensionsP()			{ return ButtonViewportDimensionsP; } 	// Get positive dimensions of the button (right and bottom points)
	// Returns the location of the root of the input indicator (ring) in world space
	UFUNCTION(BlueprintCallable)	inline FVector					GetRingWorldLoc()						{ return RingMeshComponent->GetComponentLocation(); }
	UFUNCTION(BlueprintCallable)	inline FVector					GetButtonWorldLoc()						{ return GetLocAtPercentageAlongMovementPath(GetButtonPercentageAlongMovementPath(), ESplineCoordinateSpace::World); }
	UFUNCTION(BlueprintCallable)	inline float					GetNoteBoundaryStartPointIdx()			{ return NoteBoundaryStartPointIdx; }
	UFUNCTION(BlueprintCallable)	inline float					GetNoteBoundaryEndPointIdx()			{ return NoteBoundaryEndPointIdx; }
	// Returns float between 0 and 1 that represents the position of the middle of the button as % along the movement path
	UFUNCTION(BlueprintCallable)	inline float					GetButtonPercentageAlongMovementPath()	{ return (GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryEndPointIdx) - GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryStartPointIdx)) / 2 + GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryStartPointIdx); }
	UFUNCTION(BlueprintCallable)	inline USplineComponent*		GetMovementPath()						{ return MovementPath; }
	UFUNCTION(BlueprintCallable)	inline float					GetMovementPathLength()					{ return MovementPathLength; }

	/* ############################################# MODIFIERS  ############################################# */

	void SetButtonViewportLoc(FVector2D NewLoc) { ButtonViewportLoc = NewLoc; }
	// Set positive dimensions of the button (right and bottom points)
	void SetButtonViewportDimensionsP(FVector2D NewButtonViewportDimensionsP) { ButtonViewportDimensionsP = NewButtonViewportDimensionsP; }
	// Set negative dimensions of the button (left and top points)
	void SetButtonViewportDimensionsN(FVector2D NewButtonViewportDimensionsN) { ButtonViewportDimensionsN = NewButtonViewportDimensionsN; }

	UFUNCTION(BlueprintCallable)	void SetRingIdleColor(FLinearColor NewColor) { RingIdleColor = NewColor; }
	UFUNCTION(BlueprintCallable)	void SetRingWithinBoundsColor(FLinearColor NewColor) { RingWithinBoundsColor = NewColor; }
	UFUNCTION(BlueprintCallable)	void SetRingHitColor(FLinearColor NewColor) { RingHitColor = NewColor; }
	UFUNCTION(BlueprintCallable)	void SetRingMissColor(FLinearColor NewColor) { RingMissColor = NewColor; }

	/* ############################################# DELEGATES ############################################# */

	// When a note has not been held enough of a duration to be considered 'hit'
	UPROPERTY(BlueprintAssignable)			FOnNoteMiss					OnNoteMiss;

	// When we hold past the end of a note
	UPROPERTY(BlueprintAssignable)			FOnNoteFinished				OnNoteHit;

	// When we press the button but do not hit anything. This is ACompleteMiss instead of OnCompleteMiss because something is overriding the function I bind to it (causing a guaranteed crash when called) with something else and I can't find out where
	UPROPERTY(BlueprintAssignable)			FOnCompleteMiss				ACompleteMiss;

	// Called on user input or when a note either enters or leaves the bounds 
	UPROPERTY(BlueprintAssignable)			FOnButtonEvent				OnButtonEvent;

	/* ############################################# PUBLIC VARIABLES ############################################# */

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)				TArray<ABaseNote*>		Notes;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)				TArray<ABaseNote*>		ReverseNotes;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)				ABaseNote*				NoteWithinBounds;
	UPROPERTY(BlueprintReadOnly)								bool					bHoldingNote;	// This is for debugging really and isn't used for anything gameplay related
	UPROPERTY()													bool					bActiveSwipeNote;

protected:

	/* ############################################# PROTECTED FUNCTIONS ############################################# */

	//Attaches components of this object to member variables and sets their initial parameters
	virtual void			SetUpComponents();
	// Reduces the duration of swipe notes, and makes them reverse back into their StartLoc once the duration is 0
	void					DrawSwipeNotes(float DeltaTime);
	// Update the boundaries within which notes can be hit
	void					UpdateBoundaries();
	// Sets the bNoteWithinBounds to true if any note is within the bounds of the button
	void					CheckIfNoteWithinBounds();
	// Gradually fills the button ring with a new colour
	void					AnimateRing(float DeltaTime);

	/* ########################################## GENERAL ############################################### */

	UPROPERTY()							int								LaneIdx;
	UPROPERTY()							FVector							LaneLoc;
	UPROPERTY()							float							LaneLength;	
	UPROPERTY()							FVector							StartLoc;
	UPROPERTY()							FVector							EndLoc;
	UPROPERTY()							float							MoveSpeed;
	UPROPERTY()							USplineComponent*				MovementPath;
	UPROPERTY()							float							MovementPathLength;

	UPROPERTY()							float							SpawnTimeOffset;

	// <Time value of entry, duration>
	TArray<TPair<float, float>>									HoldNoteData;
	int															HoldNoteIndex = 0;
	TArray<FLevelMapRow>										LevelMap;
	int															NoteIndex = 0;


	/* ########################################## OBJECT POINTERS ####################################### */

	UPROPERTY()			UStaticMeshComponent*					ButtonMeshComponent;
	UPROPERTY()			UStaticMeshComponent*					RingMeshComponent;
	UPROPERTY()			UStaticMeshComponent*					Ring1MeshComponent;

	/* ############################################ BOOLEANS ############################################ */

	UPROPERTY()						bool						bButtonHit;
	UPROPERTY()						bool						bReversed;
	UPROPERTY()						bool						bRingAnimIncrease;
	UPROPERTY()						bool						bButtonIsMoving = false;
	UPROPERTY(BlueprintReadOnly)	bool						bButtonIsPressed = false;
	UPROPERTY(BlueprintReadOnly)	bool						bFirstFrame = true;
	UPROPERTY(BlueprintReadOnly)	bool						bInputValid = true;	// We only want to hit one note at a time per touch / hold

	/* ############################################# BUTTON ############################################# */

	UPROPERTY(EditAnywhere)			int							NoteBoundaryStartPointIdx = -1;
	UPROPERTY(VisibleAnywhere)		float						NoteBoundaryStartPointPercentage;
	UPROPERTY(EditAnywhere)			int							NoteBoundaryEndPointIdx = -1;
	UPROPERTY(VisibleAnywhere)		float						NoteBoundaryEndPointPercentage;
	UPROPERTY()						FVector2D					ButtonDimensions;
	UPROPERTY()						FVector2D					ButtonViewportLoc;
	UPROPERTY()						FVector2D					ButtonLeniency; // (UP, DOWN)
	UPROPERTY()						FVector2D					ButtonViewportDimensionsN;	// Negative dimensions of the button (left and top points)
	UPROPERTY()						FVector2D					ButtonViewportDimensionsP;	// Positive dimensions of the button (right and bottom points)
	UPROPERTY()						float						ButtonPressLength;

	/* ####################################### BUTTON ANIMATION ######################################### */

	UPROPERTY()						FLinearColor				ActiveRingColor;
	UPROPERTY()						int							LastRingEvent = ButtonParams::INACTIVE;
	UPROPERTY(EditAnywhere)			FLinearColor				RingIdleColor;
	UPROPERTY(EditAnywhere)			FLinearColor				RingHitColor;
	UPROPERTY(EditAnywhere)			FLinearColor				RingMissColor;
	UPROPERTY()						FLinearColor				RingWithinBoundsColor;
	UPROPERTY()						float						ButtonLength = 0.1f;
	UPROPERTY()						float						ButtonSpeed = 500.0f;
	UPROPERTY()						FVector						ButtonLoc;
	UPROPERTY()						FVector						OrigButtonLoc;
	UPROPERTY()						UMaterialInstanceDynamic*	RingMaterial;
	UPROPERTY()						UMaterialInstanceDynamic*	Ring1Material;
	UPROPERTY()						float						RingRadiusValue = 0.0f;
	UPROPERTY(VisibleAnywhere)		FVector						NoteBoundaryStart;
	UPROPERTY(VisibleAnywhere)		FVector						NoteBoundaryEnd;

	/* ####################################### BUTTON PARTICLES ######################################### */

	UPROPERTY()						TArray<UParticleSystemComponent*>				ParticlesComps;
	UPROPERTY()						int												ActiveParticleComp = 0;
	UPROPERTY()						FLinearColor									ParticleColor;

	public:
	UPROPERTY(BlueprintReadOnly)	ABaseRitmoLevel*								OwningLevel;
	UPROPERTY()						ARhythmGameGameMode*							GameMode;

	UPROPERTY()						FRitmoLevelGeneralParams						LevelGeneralParams;
};
