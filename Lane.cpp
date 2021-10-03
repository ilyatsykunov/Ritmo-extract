/*  Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#include "Lane.h"

#include "WorldController.h"
#include "BaseNote.h"
#include "BaseHoldNote.h"
#include "RitmoLevel/BaseRitmoLevel.h"

ULane::ULane()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULane::BeginPlay()
{
	Super::BeginPlay();
	GameMode = Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode());
	ButtonLeniency = FVector2D(0.1f, 0.1f);
}

void ULane::SetUpComponents()
{
	ParticlesComps.Empty();
	TArray<USceneComponent*> SceneComponents;
	GetChildrenComponents(true, SceneComponents);
	for (auto Cmp : SceneComponents)
	{
		UStaticMeshComponent* MeshCmp = Cast<UStaticMeshComponent>(Cmp);
		if (MeshCmp && Cmp->GetName().ToLower().Contains("ring 0")) // TODO: Rename rings to InputIndicators or something similar
			RingMeshComponent = MeshCmp;
		else if (MeshCmp && Cmp->GetName().ToLower().Contains("ring 1"))
			Ring1MeshComponent = MeshCmp;

		UParticleSystemComponent* ParticleCmp = Cast<UParticleSystemComponent>(Cmp);
		if (ParticleCmp)
			ParticlesComps.Add(ParticleCmp);

		USplineComponent* SplineCmp = Cast<USplineComponent>(Cmp);
		if (SplineCmp && Cmp->GetName().ToLower().Contains("path"))
			MovementPath = SplineCmp;
	}

	OrigButtonLoc = GetButtonWorldLoc();
	ButtonLoc = OrigButtonLoc;
}

void ULane::SetParameters(int NewLaneIdx, int NewMoveSpeed, FVector SizeMultiplier, UMaterialInstanceDynamic* Ring0Mat,
	UMaterialInstanceDynamic* Ring1Mat, UMaterialInstanceDynamic* LaneMat)
{
	SetUpComponents();

	LaneIdx = NewLaneIdx;
	SetMoveSpeed(NewMoveSpeed);

	// Set ring materials
	if (Ring0Mat)
	{
		RingMaterial = Ring0Mat;
		Ring1Material = Ring1Mat;
	}
	else
	{
		RingMaterial = RingMeshComponent->CreateDynamicMaterialInstance(0);
		Ring1Material = Ring1MeshComponent->CreateDynamicMaterialInstance(0);
	}
	RingMeshComponent->SetMaterial(0, RingMaterial);
	Ring1MeshComponent->SetMaterial(0, Ring1Material);

	OrigButtonLoc = GetButtonWorldLoc();
	ButtonLoc = OrigButtonLoc;
	LaneLoc = GetComponentLocation();

	// Set the start of the note movement location and the end depending on the length of the of the lane
	MovementPathLength = MovementPath->GetSplineLength();
	StartLoc = MovementPath->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	NoteBoundaryStartPointPercentage = GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryStartPointIdx);
	EndLoc = MovementPath->GetLocationAtSplinePoint(MovementPath->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	NoteBoundaryEndPointPercentage = GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryEndPointIdx);
	LaneLength = MovementPath->GetSplineLength();

	// Set note boundaries
	UpdateBoundaries();
}

void ULane::SetInitialParticleColor(FLinearColor NewParticleColor)
{
	ParticleColor = NewParticleColor;

	// Set initial particle colours
	for (int i = 0; i < ParticlesComps[0]->GetNumMaterials(); i++)
	{
		UMaterialInstanceDynamic* NewMaterialDynamic = ParticlesComps[0]->CreateDynamicMaterialInstance(i, ParticlesComps[0]->GetMaterial(i));
		NewMaterialDynamic->SetVectorParameterValue(TEXT("Color"), ParticleColor);
		ParticlesComps[0]->SetMaterial(i, NewMaterialDynamic);
	}
	for (int i = 0; i < ParticlesComps[1]->GetNumMaterials(); i++)
	{
		UMaterialInstanceDynamic* NewMaterialDynamic = ParticlesComps[1]->CreateDynamicMaterialInstance(i, ParticlesComps[1]->GetMaterial(i));
		NewMaterialDynamic->SetVectorParameterValue(TEXT("Color"), ParticleColor);
		ParticlesComps[1]->SetMaterial(i, NewMaterialDynamic);
	}

	// Set initial ring colour
	ActiveRingColor = RingIdleColor;
	SwitchRing(ButtonParams::IDLE);
}

void ULane::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GameMode->bIsPlaying)
	{
		NoteSpawn();
		UpdateNotes(DeltaTime);
		CheckIfNoteWithinBounds();

		if (!bButtonIsPressed)
		{
			TouchNotHeld(GameMode->SecondsSinceStart, DeltaTime);
		}

		AnimateRing(DeltaTime);
		UpdateQueue();
	}
}


void ULane::NoteSpawn()
{
	if (NoteIndex < LevelMap.Num() && (LevelMap[NoteIndex].Time - SpawnTimeOffset) <= GameMode->SecondsSinceStart)
	{
		if (LevelMap[NoteIndex].Lanes[LaneIdx] == ENoteType::EMPTY ||
			LevelMap[NoteIndex].Lanes[LaneIdx] == ENoteType::HOLD ||
			LevelMap[NoteIndex].Lanes[LaneIdx] == ENoteType::END_HOLD)
			return;

		// Randomly replace a single note with a special note with a small chance
		RandSwapForSpecial(LevelMap[NoteIndex].Lanes[LaneIdx]);

		switch (LevelMap[NoteIndex].Lanes[LaneIdx])
		{
		case ENoteType::SINGLE:
			{
				ActivateNote(GameMode->NotePool->GetPooledObject(ENoteType::SINGLE));
			}
			break;
		case ENoteType::BEG_HOLD:
			{
				ActivateNote(GameMode->NotePool->GetPooledObject(ENoteType::HOLD));
				HoldNoteIndex++;
			}
			break;
		case ENoteType::SWIPE:
			{
				ActivateNote(GameMode->NotePool->GetPooledObject(ENoteType::SINGLE));
			}
			break;
		case ENoteType::BOMB:
			{
				ActivateNote(GameMode->NotePool->GetPooledObject(ENoteType::BOMB));
			}
			break;
		case ENoteType::IGC:
			{
				ActivateNote(GameMode->NotePool->GetPooledObject(ENoteType::IGC));
			}
			break;
		case ENoteType::RANDOM:
			{
				ActivateNote(GameMode->NotePool->GetPooledObject(ENoteType::RANDOM));
			}
			break;
		default:
			break;
		}
		NoteIndex++;
	}
}

void ULane::CustomStart(float StartTimeValue)
{
	// Update our counters for the notes to start at the needed value
	for (int i = 0; i < LevelMap.Num(); i++)
	{
		if (StartTimeValue < LevelMap[i].Time)
		{
			NoteIndex = i;
			break;
		}
	}

	for (int i = 0; i < HoldNoteData.Num(); i++)
	{
		if (StartTimeValue < HoldNoteData[i].Key)
		{
			HoldNoteIndex = i;
			break;
		}
	}
}

void ULane::RandSwapForSpecial(ENoteType& NoteType)
{
	if (NoteType != ENoteType::SINGLE)
		return;

	if (LevelGeneralParams.bBombsEnabled && LevelGeneralParams.BombSpawnFreq > 0 && FMath::RandRange(0, LevelGeneralParams.BombSpawnFreq) == 0)
		NoteType = ENoteType::BOMB;
	if (GameMode->IgcNoteSpawnFreq > 0 && FMath::RandRange(0, GameMode->IgcNoteSpawnFreq) == 0)
		NoteType = ENoteType::IGC;
	if (GameMode->RandNoteSpawnFreq > 0 && FMath::RandRange(0, GameMode->RandNoteSpawnFreq) == 0)
		NoteType = ENoteType::RANDOM;
}


void ULane::UpdateNotes(float DeltaTime)
{
	for (ABaseNote* Note : Notes)
	{
		// Update the State of the note location (needs to be done before the movement for the hold notes to stretch)
		if (Note->HeadPathPercentage < GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryStartPointIdx) && Note->NoteState.Location != ENoteDistance::InLane)
			Note->UpdateDistance(ENoteDistance::InLane);
		else if (Note->HeadPathPercentage > GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryStartPointIdx) && Note->TailPathPercentage < GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryEndPointIdx) && Note->NoteState.Location != ENoteDistance::InButton)
			Note->UpdateDistance(ENoteDistance::InButton);
		else if (Note->TailPathPercentage >= GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryEndPointIdx) && Note->NoteState.Location != ENoteDistance::PastButton)
			Note->UpdateDistance(ENoteDistance::PastButton);

		// Set the new location of the note
		float TickPercentage = MoveSpeed * DeltaTime / MovementPathLength;
		float Dist = MovementPathLength * Note->RootPathPercentage;
		FVector NewWorldLoc = MovementPath->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		FVector NewWorldTan = MovementPath->GetTangentAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		FRotator NewWorldRot = MovementPath->GetRotationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		Note->MoveTick(NewWorldLoc, NewWorldTan, NewWorldRot, TickPercentage);
	}

	// If a note has been missed 
	for (ABaseNote* Note : Notes)
	{
		FNoteState State = Note->GetState();
		// If tile is missed - call the tile miss delegate
		if (State.Location == ENoteDistance::PastButton && !Note->bToBeDeactivated)
		{
			if (!Note->bIgnoresMiss)
			{
				OnNoteMiss.Broadcast(Note); // Call the NoteMissed function in WorldController
			}
			Note->bToBeDeactivated = true;
			NoteWithinBounds = nullptr;
			break;
		}

		// If the note reaches the end of the lane - deactivate it 
		if (Note->TailPathPercentage >= 1.0f && Note->bToBeDeactivated)
		{
			DeactivateNote(Note);
			break;
		}
	}
}

void ULane::CheckIfNoteWithinBounds()
{
	if (Notes.Num() == 0)
		return;

	ABaseNote* NewNoteWithinBounds = nullptr;
	ButtonParams NewRingParam = ButtonParams::NO_CHANGE;

	for (ABaseNote* Note : Notes)
	{
		if (Note->NoteState.Location == ENoteDistance::InButton)
		{
			NewNoteWithinBounds = Note;
			break;
		}
	}

	NoteWithinBounds = NewNoteWithinBounds;

	// Set button ring mode
	if (NewRingParam == ButtonParams::NO_CHANGE && Notes.Num() > 0)
	{
		NewRingParam = ButtonParams::IDLE;
	}
	// Is it within range of the button but no input is received?
	if (!GetButtonIsPressed() && NoteWithinBounds)
	{
		SetRingWithinBoundsColor(NoteWithinBounds->GetParticleColor());
		NewRingParam = ButtonParams::NOTE_WITHIN_BOUNDS;
	}
	// If no note is within range - stop particle effect
	else if (!GetButtonIsPressed())
	{
		DeactivateButton();
	}
	// If no tile is within range - set button to idle mode
	else if (!GetButtonIsPressed())
	{
		NewRingParam = ButtonParams::IDLE;
	}

	SwitchRing(NewRingParam);
}

void ULane::UpdateQueue()
{
	for (ABaseNote* Note : Notes)
	{
		if (!Note->NoteState.bActive)
		{
			Notes.Remove(Note);
			return;
		}
	}
}

void ULane::NoteHit(ABaseNote* Note)
{
	ActivateParticleGen();
	bInputValid = false;
	DeactivateNote(Note);
	NoteWithinBounds = nullptr;
}

void ULane::NoteMiss(ABaseNote* Note)
{
	NoteWithinBounds = nullptr;
}

void ULane::CompleteMiss()
{
	GameMode->OnPlayerScore.Broadcast(ScoreParams::SCORE_COMPLETE_MISS);
	
	AWorldController* Player = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());
	Player->UpdateStreak(false);
	Player->bShouldLowerAudio = true;
}

void ULane::TouchHeld(float SecondsSinceStart, float DeltaTime)
{
	bButtonIsPressed = true;

	if (bFirstFrame)
	{
		bFirstFrame = false;
		ActivateButton();
		
		if (!NoteWithinBounds)
		{
			bInputValid = false;
			ACompleteMiss.Broadcast();
		}
	}

	if (bButtonIsPressed)
	{
		ButtonPressLength += DeltaTime;

		if (NoteWithinBounds && bInputValid)
			ActivateParticleGen();
	}

	if (NoteWithinBounds && bInputValid)
	{
		NoteWithinBounds->RegisterTouch(SecondsSinceStart, DeltaTime);
	}
}

void ULane::TouchNotHeld(float SecondsSinceStart, float DeltaTime)
{
	if (Cast<ABaseHoldNote>(NoteWithinBounds))
	{
		NoteWithinBounds->RegisterMiss(SecondsSinceStart, DeltaTime);
	}
}

void ULane::TouchReleased(const TEnumAsByte<ETouchIndex::Type> TouchIndex)
{
	Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn())->LanesHeld.Remove(this);

	bInputValid = true;
	bFirstFrame = true;
	bButtonIsPressed = false;
	
	if (NoteWithinBounds)
	{
		NoteWithinBounds->TouchReleased();
	}
}

void ULane::ActivateButton()
{
	if (NoteWithinBounds)
	{
		SetParticleColor(NoteWithinBounds->GetParticleColor());
		SwitchRing(ButtonParams::NOTE_HIT);
	}
	else if (!bActiveSwipeNote)
	{
		SwitchRing(ButtonParams::NOTE_MISS);
	}
}

void ULane::DeactivateButton()
{
	if (!GetButtonIsMoving())
	{
		bButtonHit = false;
	}
}

void ULane::ActivateParticleGen()
{
	for (int i = 0; i < ParticlesComps[ActiveParticleComp]->GetNumMaterials(); i++)
	{
		UMaterialInstanceDynamic* NewMaterialDynamic = ParticlesComps[ActiveParticleComp]->CreateDynamicMaterialInstance(i, ParticlesComps[ActiveParticleComp]->GetMaterial(i));
		NewMaterialDynamic->SetVectorParameterValue(TEXT("Color"), ParticleColor);
		ParticlesComps[ActiveParticleComp]->SetMaterial(i, NewMaterialDynamic);
	}
	ParticlesComps[ActiveParticleComp]->ActivateSystem();

	(ActiveParticleComp >= ParticlesComps.Num() - 1) ? ActiveParticleComp = 0 : ActiveParticleComp++; // Increase the active particle index

}

void ULane::SwitchRing(ButtonParams Event)
{
	switch (Event)
	{
	case ButtonParams::INACTIVE:
		break;
	case ButtonParams::IDLE:
		if (LastRingEvent != ButtonParams::IDLE)
		{
			LastRingEvent = ButtonParams::IDLE;
			Ring1Material->SetVectorParameterValue("Color", ActiveRingColor);
			ActiveRingColor = RingIdleColor;
			RingRadiusValue = 0.0f;
			RingMaterial->SetScalarParameterValue("Radius", RingRadiusValue);
			RingMaterial->SetVectorParameterValue("Color", ActiveRingColor);
			bRingAnimIncrease = true;
		}
		break;
	case ButtonParams::NOTE_WITHIN_BOUNDS:
		if (LastRingEvent != ButtonParams::NOTE_WITHIN_BOUNDS)
		{
			LastRingEvent = ButtonParams::NOTE_WITHIN_BOUNDS;
			Ring1Material->SetVectorParameterValue("Color", ActiveRingColor);
			ActiveRingColor = RingWithinBoundsColor;
			RingRadiusValue = 0.0f;
			RingMaterial->SetScalarParameterValue("Radius", RingRadiusValue);
			RingMaterial->SetVectorParameterValue("Color", ActiveRingColor);
			bRingAnimIncrease = true;
		}
		break;
	case ButtonParams::NOTE_HIT:
		if (LastRingEvent != ButtonParams::NOTE_HIT)
		{
			LastRingEvent = ButtonParams::NOTE_HIT;
			Ring1Material->SetVectorParameterValue("Color", ActiveRingColor);
			ActiveRingColor = RingHitColor;
			RingRadiusValue = 0.0f;
			RingMaterial->SetScalarParameterValue("Radius", RingRadiusValue);
			RingMaterial->SetVectorParameterValue("Color", ActiveRingColor);
			bRingAnimIncrease = true;
		}
		break;
	case ButtonParams::NOTE_MISS:
		if (LastRingEvent != ButtonParams::NOTE_MISS)
		{
			LastRingEvent = ButtonParams::NOTE_MISS;
			Ring1Material->SetVectorParameterValue("Color", ActiveRingColor);
			ActiveRingColor = RingMissColor;
			RingRadiusValue = 0.0f;
			RingMaterial->SetScalarParameterValue("Radius", RingRadiusValue);
			RingMaterial->SetVectorParameterValue("Color", ActiveRingColor);
			bRingAnimIncrease = true;
		}
		break;
	default:
		break;
	}

	OnButtonEvent.Broadcast(LaneIdx, Event, ActiveRingColor);
}

void ULane::LoadNotes(TArray<FLevelMapRow> Rows, TArray<TPair<float, float>> HoldNoteData)
{
	LevelMap = Rows;
	this->HoldNoteData = HoldNoteData;
}

void ULane::AnimateRing(float DeltaTime)
{
	// Fill the ring with new colour
	if (bRingAnimIncrease)
	{
		RingRadiusValue += DeltaTime * 3.0f;

		// If the ring has been completely filled - stop the animation
		if (RingRadiusValue >= 1.0f)
		{
			Ring1Material->SetVectorParameterValue("Color", ActiveRingColor);
			RingRadiusValue = 0.0f;
			bRingAnimIncrease = false;
		}

		RingMaterial->SetScalarParameterValue("Radius", RingRadiusValue);
	}
}

void ULane::DeactivateNote(ABaseNote* const Note)
{
	Note->Reset();
	Notes.Remove(Note);
}

void ULane::ActivateNote(ABaseNote* const Note)
{
	Note->ParentLane = this;
	Note->UpdateDistance(ENoteDistance::InLane);

	if (Note->GetType() == ENoteType::HOLD)
	{
		Note->SetHoldDuration(HoldNoteData[HoldNoteIndex].Value, GameMode->Player->EndLoc - GameMode->Player->StartLoc);
	}

	Note->SetActorLocation(GetStartLoc());

	// Attach the Note to the Lane root so it moves with it
	FAttachmentTransformRules AttachmentRules(FAttachmentTransformRules::KeepWorldTransform);
	Note->AttachToComponent(this, AttachmentRules);

	Note->SetActorHiddenInGame(false);
	Note->SetStopped(false);
	Note->SetActive(true); // Activate the note once it has all of the information it needs

	Notes.Add(Note);
}

void ULane::UpdateBoundaries()
{
	FVector RingBounds = RingMeshComponent->GetStaticMesh()->GetBoundingBox().GetSize() * RingMeshComponent->GetComponentScale();
	ButtonDimensions = FVector2D(RingBounds.X / 2, RingBounds.Y / 2);

	NoteBoundaryStart = MovementPath->GetLocationAtDistanceAlongSpline(GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryStartPointIdx), ESplineCoordinateSpace::Type::World);
	NoteBoundaryEnd = MovementPath->GetLocationAtDistanceAlongSpline(GetPercentageAlongMovementPathAtSplinePoint(NoteBoundaryEndPointIdx), ESplineCoordinateSpace::Type::World);
}

void ULane::ResetLane()
{
	ButtonLoc = OrigButtonLoc;
	MoveSpeed = OwningLevel->MoveSpeed;

	Notes.Empty();
	NoteIndex = 0;
	HoldNoteIndex = 0;

	if (RingMaterial && Ring1Material)
	{
		RingMaterial->SetScalarParameterValue("Radius", 0.0f);
		Ring1Material->SetScalarParameterValue("Radius", 1.0f);
	}

	AWorldController* Player = Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn());

	OnNoteHit.Clear();
	OnNoteHit.AddUniqueDynamic(this, &ULane::NoteHit);
	OnNoteHit.AddUniqueDynamic(Player, &AWorldController::NoteHit);
	OnNoteHit.AddUniqueDynamic(OwningLevel, &ABaseRitmoLevel::NoteHit);

	OnNoteMiss.Clear();
	OnNoteMiss.AddUniqueDynamic(this, &ULane::NoteMiss);
	OnNoteMiss.AddUniqueDynamic(Player, &AWorldController::NoteMissed);
	OnNoteMiss.AddUniqueDynamic(OwningLevel, &ABaseRitmoLevel::NoteMiss);

	ACompleteMiss.Clear();
	ACompleteMiss.AddUniqueDynamic(this, &ULane::CompleteMiss);

	OnButtonEvent.Clear();
	OnButtonEvent.AddUniqueDynamic(OwningLevel, &ABaseRitmoLevel::ButtonEvent);

	GameMode->OnGameReset.AddUniqueDynamic(this, &ULane::ResetLane);
}

void ULane::StartPlaying()
{
	ReceiveStartPlaying();
}

void ULane::StopPlaying()
{
	ReceiveStopPlaying();
}

void ULane::SetParticleColor(FLinearColor Color)
{
	ParticleColor = Color;
}

float ULane::SetMoveSpeed(float NewSpeed)
{
	MoveSpeed = NewSpeed;
	SpawnTimeOffset = ((GetButtonPercentageAlongMovementPath() * MovementPath->GetSplineLength()) / MoveSpeed) * GameMode->GameSpeed;
	ReceiveNewMoveSpeed(NewSpeed);
	UpdateBoundaries();
	return SpawnTimeOffset;
}

float ULane::GetPercentageAlongMovementPathAtSplinePoint(int PointIdx)
{
	return MovementPath->GetDistanceAlongSplineAtSplinePoint(PointIdx) / GetMovementPathLength();
}

FVector ULane::GetLocAtPercentageAlongMovementPath(float Percentage, ESplineCoordinateSpace::Type CoordinateSpace) 
{
	const float Distance = Percentage * MovementPath->GetSplineLength();
	return MovementPath->GetLocationAtDistanceAlongSpline(Distance, CoordinateSpace);
}

FVector ULane::GetTanAtPercentageAlongMovementPath(float Percentage, ESplineCoordinateSpace::Type CoordinateSpace)
{
	const float Distance = Percentage * MovementPath->GetSplineLength();
	return MovementPath->GetTangentAtDistanceAlongSpline(Distance, CoordinateSpace);
}

FRotator ULane::GetRotAtPercentageAlongMovementPath(float Percentage, ESplineCoordinateSpace::Type CoordinateSpace)
{
	const float Distance = Percentage * MovementPath->GetSplineLength();
	return MovementPath->GetRotationAtDistanceAlongSpline(Distance, CoordinateSpace);
}