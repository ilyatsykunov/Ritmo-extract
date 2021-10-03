/* This class is an example of the note functionality. This specific note class uses the SplineMeshComponent class to
   create note of varying length that can bend around corners and exactly follow the movement path 
   
	Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#pragma once

#include "CoreMinimal.h"
#include "Play/BaseHoldNote.h"
#include "EnumTypes.h"

#include "SplineMeshHoldNote.generated.h"

class USplineComponent;
class USplineMeshComponent;

USTRUCT(BlueprintType)
struct FSplinePointMeta
{
	GENERATED_USTRUCT_BODY()

	FSplinePointMeta() {}

	// How much of the path the point has travelled: 0 - the start of the path, 1 - the end of the path
	UPROPERTY()	float Percentage = 0.0f;
	// Once this spline point reaches this % - increment the ActiveSplinePoint and start expanding the next spline point
	UPROPERTY()	float MaxPercentage = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewBodyComponentSpawned, USplineMeshComponent*, Segment);

/**
 * 
 */
UCLASS()
class RHYTHMGAME_API ASplineMeshHoldNote : public ABaseHoldNote
{
	GENERATED_BODY()
	

public:

	UPROPERTY(BlueprintAssignable)					FOnNewBodyComponentSpawned		OnSegmentSpawned;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)	EButtonBehaviour				ButtonBehaviour = EButtonBehaviour::STRETCH;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		USceneComponent*				Root;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		USplineMeshComponent*			HeadMeshCmp;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		USplineMeshComponent*			TailMeshCmp;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		UStaticMeshComponent*			BodyMeshCmp;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		USplineComponent*				BodySpline;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		TArray<FSplinePointMeta>		SplinePointsMeta;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		int								ActiveSplinePoint = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)		TArray<USplineMeshComponent*>	SplineMeshCmps;


	ASplineMeshHoldNote();
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void SetupComponents() override;
	virtual float GetXLength() override;
	virtual void SetParameters(const FHoldNoteMeta NewNoteMeta, const FVector MeshSizeMultiplier) override;
	virtual void Reset() override;
	virtual void MoveTick(FVector NewWorldLoc, FVector NewWorldTan, FRotator NewWorldRot, float TickPercentage) override;
	virtual void RegisterTouch(float CurrentTime, float DeltaTime) override;
	virtual FVector GetTopLocation() override;
	virtual FVector GetBottomLocation() override;
	virtual float	GetOffsetRadius() override;
	virtual void SetActive(bool IsActive) override;

	/*
	* As the size of the tile on screen can change as it moves through the game - this reports the maximum size, which is used for scoring
	* For real time length see GetXLength
	* and checking bounds
	*/
	virtual FVector GetFullSize() override;


	/*
	* When we release touch input while this tile is the NoteWithinBounds
	*/
	virtual void TouchReleased();



	USplineMeshComponent* SpawnSplineMesh(FVector LocalStartLoc, FVector LocalStartTan, FVector LocalEndLoc, FVector LocalEndTan, UStaticMesh* Mesh, FName Name);

	/*
	* Moves the tail and adjusts the spline points of the body acording to the movement path
	*/
	void MoveSpline(float TickPercentage);

};
