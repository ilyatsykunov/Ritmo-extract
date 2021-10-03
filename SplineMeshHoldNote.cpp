/*  Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#include "SplineMeshHoldNote.h"

#include "Lane.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SplineComponent.h"

ASplineMeshHoldNote::ASplineMeshHoldNote()
{
	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents, true);
	Root = (SceneComponents.Max() > 0) ? SceneComponents[0] : CreateDefaultSubobject<USceneComponent>("Root");

	BodySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BodySpline"));
	BodySpline->SetupAttachment(Root);
	BodySpline->ClearSplinePoints();

	HeadMeshCmp = CreateDefaultSubobject<USplineMeshComponent>(TEXT("HeadMesh"));
	HeadMeshCmp->SetupAttachment(BodySpline);
	HeadMeshCmp->SetMobility(EComponentMobility::Movable);
	Components.Add(HeadMeshCmp);

	BodyMeshCmp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMeshCmp->SetupAttachment(BodySpline);
	BodyMeshCmp->SetMobility(EComponentMobility::Movable);

	TailMeshCmp = CreateDefaultSubobject<USplineMeshComponent>(TEXT("TailMesh"));
	TailMeshCmp->SetupAttachment(BodySpline);
	TailMeshCmp->SetMobility(EComponentMobility::Movable);
	Components.Add(TailMeshCmp);
}

void ASplineMeshHoldNote::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASplineMeshHoldNote::BeginPlay()
{
	Super::BeginPlay();

	BaseHead = HeadMeshCmp;
	BaseBody = BodyMeshCmp;
	BaseTail = TailMeshCmp;
}

FVector ASplineMeshHoldNote::GetTopLocation()
{
	return BodySpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
}

FVector ASplineMeshHoldNote::GetBottomLocation()
{
	return BodySpline->GetLocationAtSplinePoint(BodySpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
}

void ASplineMeshHoldNote::SetupComponents()
{
}

float ASplineMeshHoldNote::GetXLength()
{
	return BodySpline->GetSplineLength();
}

void ASplineMeshHoldNote::SetParameters(const FHoldNoteMeta NewNoteMeta, const FVector MeshSizeMultiplier)
{
	Super::SetParameters(NewNoteMeta, MeshSizeMultiplier);

	if (NewNoteMeta.NoteType == ENoteType::EMPTY)
		return;

	Type = ENoteType::HOLD;
	HeadMeshCmp->SetStaticMesh(NewNoteMeta.ComponentMeta[0].StaticMesh);
	BodyMeshCmp->SetStaticMesh(NewNoteMeta.ComponentMeta[1].StaticMesh);
	TailMeshCmp->SetStaticMesh(NewNoteMeta.ComponentMeta[2].StaticMesh);

	HeadMaterial = UMaterialInstanceDynamic::Create(NewNoteMeta[0].StaticMesh->GetMaterial(0), nullptr);
	HeadMaterial->SetVectorParameterValue("Color", NewNoteMeta.MainColor);
	HeadMaterial->SetVectorParameterValue("SecondColor", NewNoteMeta.ParticleColor);
	HeadMeshCmp->SetMaterial(0, HeadMaterial);

	BodyMaterial = UMaterialInstanceDynamic::Create(NewNoteMeta[1].StaticMesh->GetMaterial(0), nullptr);
	BodyMaterial->SetVectorParameterValue("Color", NewNoteMeta.MainColor);
	BodyMaterial->SetVectorParameterValue("SecondColor", NewNoteMeta.ParticleColor);
	BodyMeshCmp->SetMaterial(0, BodyMaterial);

	TailMaterial = UMaterialInstanceDynamic::Create(NewNoteMeta[2].StaticMesh->GetMaterial(0), nullptr);
	TailMaterial->SetVectorParameterValue("Color", NewNoteMeta.MainColor);
	TailMaterial->SetVectorParameterValue("SecondColor", NewNoteMeta.ParticleColor);
	TailMeshCmp->SetMaterial(0, TailMaterial);

	ParticleColor = NewNoteMeta.ParticleColor;
	StartScale = HeadMeshCmp->GetComponentScale();
}

void ASplineMeshHoldNote::SetActive(bool IsActive)
{
	Super::SetActive(IsActive);

	if (!IsActive)
		return;

	const float HeadLength = HeadMeshCmp->GetStaticMesh()->GetBounds().GetBox().GetSize().X * StartScale.X;
	const float TailLength = TailMeshCmp->GetStaticMesh()->GetBounds().GetBox().GetSize().X * StartScale.X;

	const float TotalNoteLength = ParentLane->GetMoveSpeed() * (EndTime - ParentLane->GetSpawnTimeOffset() - GameMode->SecondsSinceStart);
	const float TotalBodyLength = TotalNoteLength - HeadLength - TailLength;

	const float SingleBodyLength = 100.0f; // TODO: Replace magic number with LaneLength / number of points on the lane path
	const int BodyNum = FMath::CeilToInt(TotalBodyLength / SingleBodyLength);

	const int MeshesNum = BodyNum + 2; // Number of body meshes + head mesh + tail mesh
	const int PointsNum = MeshesNum + 1;

	// Spawn spline points
	for (int i = 0; i < PointsNum; i++)
	{
		BodySpline->AddSplineLocalPoint(FVector(0.0f, 0.0f, 0.0f));
		SplinePointsMeta.Add(FSplinePointMeta());
		
		// Head point
		if (i == 0)
			SplinePointsMeta[i].MaxPercentage = HeadLength / ParentLane->GetMovementPathLength();
		// Body point - assign it a constant max % 
		else if (i < BodyNum)
			SplinePointsMeta[i].MaxPercentage = SingleBodyLength / ParentLane->GetMovementPathLength();
		// Last body point - assign it whatever is left of the TotalBodyLength / SingleBodyLength as the max %
		else if (i == BodyNum)
			SplinePointsMeta[i].MaxPercentage = int(TotalBodyLength) % int(SingleBodyLength) / ParentLane->GetMovementPathLength();
		// Tail point
		else if (i == PointsNum - 2)
			SplinePointsMeta[i].MaxPercentage = TailLength / ParentLane->GetMovementPathLength();

		if (SplinePointsMeta[i].MaxPercentage > 1.0f)
			SplinePointsMeta[i].MaxPercentage = 1.0f;
	}

	// Spawn spline meshes
	for (int i = 0; i < MeshesNum; i++)
	{
		USplineMeshComponent* NewCmp = nullptr;

		// Head mesh
		if (i == 0)
		{
			NewCmp = HeadMeshCmp;
		}
		// Body mesh
		else if (i < MeshesNum - 1)
		{
			const FName Name = *FString(TEXT("BodyMesh ")).Append(FString::FromInt(i - 1));
			NewCmp = SpawnSplineMesh(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, BodyMeshCmp->GetStaticMesh(), Name);

			// Broadcast the segment so we can make changes to it if we need to
			OnSegmentSpawned.Broadcast(NewCmp);

		}
		// Tail Mesh
		else if (i == MeshesNum - 1)
		{
			NewCmp = TailMeshCmp;
		}		
		NewCmp->SetStartAndEnd(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector);
		NewCmp->SetWorldScale3D(StartScale);
		SplineMeshCmps.Add(NewCmp);
	}

	HeadPathPercentage = HeadMeshCmp->GetStaticMesh()->GetBounds().GetBox().GetSize().X * StartScale.X / 2 / GetParentLane()->GetMovementPath()->GetSplineLength();
	RootPathPercentage = 0.0f;
	TailPathPercentage = 0.0f - TailMeshCmp->GetStaticMesh()->GetBounds().GetBox().GetSize().X * StartScale.X / 2 / GetParentLane()->GetMovementPath()->GetSplineLength();
}

void ASplineMeshHoldNote::Reset()
{
	Super::Reset();

	// Clear the spline
	ActiveSplinePoint = 0;
	SplinePointsMeta.Empty();
	for (int i = 1; i < SplineMeshCmps.Num() - 1; i++) // Destroy every body component, keeping the head and tail components
		SplineMeshCmps[i]->DestroyComponent();
	SplineMeshCmps.Empty();
	HeadMeshCmp->SetStartAndEnd(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector);
	TailMeshCmp->SetStartAndEnd(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector);
	BodySpline->ClearSplinePoints();

	for (USceneComponent* Component : Components)
		ResetComponent(Component);

	//OnSegmentSpawned.Clear();
}

void ASplineMeshHoldNote::MoveTick(FVector NewWorldLoc, FVector NewWorldTan, FRotator NewWorldRot, float TickPercentage)
{
	(HeadPathPercentage + TickPercentage) < 1.0f ? HeadPathPercentage += TickPercentage : 1.0f;
	(RootPathPercentage + TickPercentage) < 1.0f ? RootPathPercentage += TickPercentage : 1.0f;

	// Each point will either stop at the end of the movement path or at the button
	float MaxPercentage = 1.0f;

	if (NoteState.Location == ENoteDistance::InButton && ParentLane->GetButtonIsPressed() && ParentLane->IsInputValid())
		MaxPercentage = ParentLane->GetButtonPercentageAlongMovementPath();

	// Move each spline point and each spline mesh forward
	for (int i = 0; i < BodySpline->GetNumberOfSplinePoints(); i++)
	{
		// Move the point forward making sure it does not go over the MaxPercentage
		if (i <= ActiveSplinePoint)
			(SplinePointsMeta[i].Percentage + TickPercentage < MaxPercentage) ? SplinePointsMeta[i].Percentage += TickPercentage : SplinePointsMeta[i].Percentage = MaxPercentage;

		const FVector PointWorldLoc = ParentLane->GetLocAtPercentageAlongMovementPath(SplinePointsMeta[i].Percentage, ESplineCoordinateSpace::World);

		BodySpline->SetLocationAtSplinePoint(i, PointWorldLoc, ESplineCoordinateSpace::World);

		const FVector NewLocalLoc = BodySpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
		const FVector NewLocalTan = BodySpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);

		// Head mesh (For head and tail the start and end tangents are the same)
		if (i == 0)
		{
			SplineMeshCmps[i]->SetStartPosition(NewLocalLoc);
			SplineMeshCmps[i]->SetStartTangent(NewLocalTan);
			SplineMeshCmps[i]->SetEndTangent(NewLocalTan);
		}
		else if (i == 1)
		{
			SplineMeshCmps[i - 1]->SetEndPosition(NewLocalLoc);

			SplineMeshCmps[i]->SetStartPosition(NewLocalLoc);
			SplineMeshCmps[i]->SetStartTangent(NewLocalTan);
		}
		// Body meshes
		else if (i < SplineMeshCmps.Num() - 1)
		{
			SplineMeshCmps[i - 1]->SetEndPosition(NewLocalLoc);
			SplineMeshCmps[i - 1]->SetEndTangent(NewLocalTan);

			SplineMeshCmps[i]->SetStartPosition(NewLocalLoc);
			SplineMeshCmps[i]->SetStartTangent(NewLocalTan);
		}
		// For the tail mesh the start and end positions are flipped (because the tail mesh is always flipped. For head and tail the start and end tangents are the same)
		else if (i == BodySpline->GetNumberOfSplinePoints() - 2)
		{
			SplineMeshCmps[i - 1]->SetEndPosition(NewLocalLoc);
			SplineMeshCmps[i - 1]->SetEndTangent(NewLocalTan);

			SplineMeshCmps[i]->SetEndPosition(NewLocalLoc);
		}
		else if (i == BodySpline->GetNumberOfSplinePoints() - 1)
		{
			SplineMeshCmps[i - 1]->SetStartPosition(NewLocalLoc);
			SplineMeshCmps[i - 1]->SetStartTangent(NewLocalTan * -1);
			SplineMeshCmps[i - 1]->SetEndTangent(NewLocalTan * -1);
		}
	}

	// Start stretching out the next spline point in the next frame either when the current point reaches the end (if no input) or the button (if input true)
	if (((SplinePointsMeta[ActiveSplinePoint].Percentage >= SplinePointsMeta[ActiveSplinePoint].MaxPercentage && ActiveSplinePoint < BodySpline->GetNumberOfSplinePoints() - 1)) ||
		(FMath::IsNearlyEqual(SplinePointsMeta[ActiveSplinePoint].Percentage, MaxPercentage, 0.01f) && ActiveSplinePoint < BodySpline->GetNumberOfSplinePoints() - 1 && NoteState.Location == ENoteDistance::InButton && ParentLane->GetButtonIsPressed() && ParentLane->IsInputValid()))
	{
		ActiveSplinePoint++;
	}

	// Move the tail
	if (ActiveSplinePoint == BodySpline->GetNumberOfSplinePoints() - 1 && TailMeshCmp->bHiddenInGame)
		TailMeshCmp->SetHiddenInGame(false);
	if (!TailMeshCmp->bHiddenInGame)
		TailPathPercentage = SplinePointsMeta[BodySpline->GetNumberOfSplinePoints() - 1].Percentage;

	// If the HEAD has reached the end of the path, but the TAIL has not - wait for it before removing the note
	if (NoteState.Location == ENoteDistance::PastButton && TailPathPercentage < MaxPercentage && !NoteState.bStationary)
		NoteState.bStationary = true;
	else if (TailPathPercentage >= MaxPercentage && NoteState.bStationary)
		NoteState.bStationary = false;
}

USplineMeshComponent* ASplineMeshHoldNote::SpawnSplineMesh(FVector LocalStartLoc, FVector LocalStartTan, FVector LocalEndLoc, FVector LocalEndTan, UStaticMesh* Mesh, FName Name)
{
	USplineMeshComponent* NewCmp = NewObject<USplineMeshComponent>(BodySpline, USplineMeshComponent::StaticClass(), Name);
	NewCmp->SetMobility(EComponentMobility::Movable);
	NewCmp->AttachToComponent(BodySpline, FAttachmentTransformRules::KeepRelativeTransform, Name);
	NewCmp->RegisterComponent();
	NewCmp->SetStartAndEnd(LocalStartLoc, LocalStartTan, LocalEndLoc, LocalEndTan);
	NewCmp->SetStaticMesh(Mesh);

	return NewCmp;
}

void ASplineMeshHoldNote::RegisterTouch(float CurrentTime, float DeltaTime)
{
	Super::RegisterTouch(CurrentTime, DeltaTime);

	// As Super::RegisterTouch can potentially deactivate the tile we need to check if we have a parent lane.
	// If  we don't, then the tile has been deactivated. So don't do anything else
	if (ParentLane && ParentLane->IsInputValid())
	{
		if (HeadPathPercentage > ParentLane->GetButtonPercentageAlongMovementPath())
		{

		}
	}
}

float ASplineMeshHoldNote::GetOffsetRadius()
{
	return (HeadMeshCmp->GetStaticMesh()->GetBounds().GetBox().GetSize().X * HeadMeshCmp->GetComponentScale().X) / 2;
}

FVector ASplineMeshHoldNote::GetFullSize()
{
	return FVector(((HoldTimeRequired / ParentLane->GetSpawnTimeOffset()) * ParentLane->GetLaneLength()) + (2 * GetOffsetRadius()), 0.f, 0.f);
}

void ASplineMeshHoldNote::TouchReleased()
{
	Super::TouchReleased();
}
