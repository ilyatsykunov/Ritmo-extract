/*  Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/

#include "BaseRitmoLevel.h"
#include "RitmoLevelMeta.h"
#include "SplineMeshHoldNote.h"
#include "ObjectPool.h"
#include "../WorldController.h"

// Sets default values
ABaseRitmoLevel::ABaseRitmoLevel()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");

	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	CameraComponent->SetupAttachment(RootComponent);


	ConstructorHelpers::FObjectFinder<UMaterialInterface> ppGlitch(TEXT("MaterialInstanceConstant'/Game/Materials/PostProcessing/M_PP_Glitch_Inst.M_PP_Glitch_Inst'"));
	ConstructorHelpers::FObjectFinder<UMaterialInterface> ppBGBlur(TEXT("Material'/Game/Materials/PostProcessing/PP_BGBlur.PP_BGBlur'"));

	ppMatArray.Add(ppGlitch.Object);
	ppMatArray.Add(ppBGBlur.Object);
}

void ABaseRitmoLevel::SetUpComponents()
{
	GetComponents<ULane>(Lanes);
	for (ULane* Lane : Lanes)
		Lane->OwningLevel = this;
}

void ABaseRitmoLevel::LoadLevel(FRitmoLevelPlayData& LevelMeta, FSongData& SongMeta, FVector SizeMultiplier)
{
	SetUpComponents();
	ReceiveComponentSetup();
}

void ABaseRitmoLevel::ResetLevel()
{
	ppMatDynamicArray.Empty();

	// Generate dynamic post processing materials 
	ppMatDynamicArray.Add(UMaterialInstanceDynamic::Create(ppMatArray[ppMatArrayIndex], nullptr));
	ppMatDynamicArray[ppMatArrayIndex]->SetScalarParameterValue("Intensity", 0.0f);
	ppMatDynamicArray[ppMatArrayIndex]->SetScalarParameterValue("Speed", 0.0f);
	CameraComponent->PostProcessSettings.AddBlendable(ppMatDynamicArray[ppMatArrayIndex], 1.0f);
	// Background blur post processing material
	ppMatDynamicArray.Add(UMaterialInstanceDynamic::Create(ppMatArray[1], nullptr));
	ppMatDynamicArray[1]->SetScalarParameterValue("Intensity", 0.0f);
	CameraComponent->PostProcessSettings.AddBlendable(ppMatDynamicArray[1], 1.0f);

	for (ULane* Lane : Lanes)
	{
		Lane->ResetLane();
	}

	Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode())->NotePool->OnNoteSpawned.AddUniqueDynamic(this, &ABaseRitmoLevel::NativeReceiveNoteSpawn);

	OnButtonPress.Clear();
	OnButtonPress.AddUniqueDynamic(this, &ABaseRitmoLevel::ActivateButton);

	OnButtonLift.Clear();
	OnButtonLift.AddUniqueDynamic(this, &ABaseRitmoLevel::DeactivateButton);

}

void ABaseRitmoLevel::NativeReceiveNoteSpawn(ABaseNote* Note)
{
	if (Note->IsA(ASplineMeshHoldNote::StaticClass()))
	{
		Cast<ASplineMeshHoldNote>(Note)->OnSegmentSpawned.AddUniqueDynamic(this, &ABaseRitmoLevel::NativeReceiveSegmentSpawned);
	}
	ReceiveNoteSpawn(Note);
}

void ABaseRitmoLevel::NativeReceiveSegmentSpawned(USplineMeshComponent* Segment)
{
	FLinearColor BodyColor;
	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Segment->GetMaterial(0), Segment);

	// Get the color from the existing note and apply it to the segment
	FMaterialParameterInfo MatParamInfo;
	MatParamInfo.Name = "Color";

	Cast<ASplineMeshHoldNote>(Segment->GetAttachParent()->GetOwner())->BodyMeshCmp->GetMaterial(0)->GetVectorParameterValue(MatParamInfo, BodyColor);
	DynamicMaterial->SetVectorParameterValue(FName("Color"), BodyColor);
	Segment->SetMaterial(0, DynamicMaterial);

	ReceiveNoteSegmentSpawn(Segment);
}

// Called when the game starts or when spawned
void ABaseRitmoLevel::BeginPlay()
{
	Super::BeginPlay();

	CameraComponent->SetActive(true, true);
	GetWorld()->GetFirstPlayerController()->SetViewTargetWithBlend(this, 0.0f, EViewTargetBlendFunction::VTBlend_Linear, 0.0f, false);

	if (CameraTransforms.Num() && CameraTransformIndex < CameraTransforms.Num())
		CameraComponent->SetWorldLocationAndRotation(CameraTransforms[CameraTransformIndex].Location, CameraTransforms[CameraTransformIndex].Rotation, false, nullptr, ETeleportType::None);
}

// Called every frame
void ABaseRitmoLevel::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Cast<ARhythmGameGameMode>(GetWorld()->GetAuthGameMode())->bIsPlaying)
	{
		if (CameraParams.bCamCanShake)
			CameraShake(DeltaTime);

		ppEffectsTick(DeltaTime);
	}
}

// Called when the game is unpaused
void ABaseRitmoLevel::StartPlaying()
{
	ReceiveStartPlaying();
	Cast<AWorldController>(GetWorld()->GetFirstPlayerController()->GetPawn())->StartPlaying();
}

void ABaseRitmoLevel::StopPlaying()
{
	ReceiveStopPlaying();
}

void ABaseRitmoLevel::ButtonEvent(int LaneIdx, ButtonParams Event, FLinearColor Color)
{
	ReceiveButtonEvent(Lanes[LaneIdx], Event, Color);
}

void ABaseRitmoLevel::NoteHit(ABaseNote* Note)
{
	switch (Note->GetType())
	{
	case ENoteType::EMPTY:
		ppGenerateEffect(ppEffectLength);
		break;
	case ENoteType::SINGLE:
		ppGenerateEffect(ppEffectLength);
		break;
	case ENoteType::HOLD:
		ppGenerateEffect(ppEffectLength);
		break;
	case ENoteType::SWIPE:
		ppGenerateEffect(ppEffectLength);
		break;
	case ENoteType::BOMB:
		ppGenerateEffect(ppEffectLength * 3);
		return;
	case ENoteType::RANDOM:
		ppGenerateEffect(ppEffectLength);
		break;
	case ENoteType::IGC:
		ppGenerateEffect(ppEffectLength);
		break;
	}

	ReceiveNoteHit(Note);
}

void ABaseRitmoLevel::NoteMiss(ABaseNote* Note)
{

}

void ABaseRitmoLevel::TouchHeld(float DeltaTime)
{

}

void ABaseRitmoLevel::ActivateButton(ULane* Lane)
{
	Lane->ActivateButton();
	ReceiveActivateButton(Lanes.Find(Lane));
}

void ABaseRitmoLevel::DeactivateButton(ULane* Lane)
{
	Lane->DeactivateButton();
	ReceiveDectivateButton(Lanes.Find(Lane));
}

void ABaseRitmoLevel::SetMoveSpeed(float NewSpeed)
{
	MoveSpeed = NewSpeed;

	for (ULane* Lane : Lanes)
	{
		Lane->SetMoveSpeed(NewSpeed);
	}

	ReceiveNewMoveSpeed(NewSpeed);
}

void ABaseRitmoLevel::NewCamTransform(FVector CamLoc, FRotator CamRot, float CamFov)
{

}



void ABaseRitmoLevel::ppEffectsTick(float DeltaTime)
{
	//Gradually decrease effect over time
	if (ppEffectSpeed > 0.0f)
	{
		ppEffectAmount -= DeltaTime;
		ppEffectSpeed -= DeltaTime;

		if (ppEffectSpeed <= 0.0f)
		{
			ppEffectAmount = 0.0f;
			ppEffectSpeed = 0.0f;
		}
		ppMatDynamicArray[ppMatArrayIndex]->SetScalarParameterValue("Intensity", ppEffectAmount * 2.0f);
		ppMatDynamicArray[ppMatArrayIndex]->SetScalarParameterValue("Speed", ppEffectSpeed * 2.0f);
	}
}

void ABaseRitmoLevel::ppGenerateEffect(float NewppEffectAmount)
{
	ppEffectAmount = NewppEffectAmount;
	ppEffectSpeed = ppEffectLength;
}

void ABaseRitmoLevel::CameraShake(float DeltaTime)
{
	if (ppEffectAmount <= 0.0f || ppEffectSpeed <= 0.0f)
		return;

	static bool	bUp = false;
	static bool	bRollPos = false;

	FVector CurLoc = CameraComponent->GetComponentLocation();
	FRotator CurRot = CameraComponent->GetComponentRotation();

	//Depending on the bool value of bUP - move either down or up 
	if (bUp)
	{
		CurLoc.Z += DeltaTime * ppEffectSpeed * 12.5f;
		FVector NewLoc = FVector(CurLoc.X, CurLoc.Y, CurLoc.Z);
		CameraComponent->SetWorldLocation(NewLoc);
	}
	else
	{
		CurLoc.Z -= DeltaTime * ppEffectSpeed * 12.5f;
		FVector NewLoc = FVector(CurLoc.X, CurLoc.Y, CurLoc.Z);
		CameraComponent->SetWorldLocation(NewLoc);
	}

	//If current X position is bigger than max X position -> decrease X position
	(CurLoc.X >= CamShakeLocMax.X) ? CurLoc.X -= DeltaTime * ppEffectSpeed : CurLoc.X += DeltaTime * ppEffectSpeed;
	//If current Y position is bigger than max Y position -> decrease Y position
	(CurLoc.Y >= CamShakeLocMax.Y) ? CurLoc.Y -= DeltaTime * ppEffectSpeed : CurLoc.Y += DeltaTime * ppEffectSpeed;

	FVector OrigCamLoc = CameraTransforms.Num() ? CameraTransforms[RuntimeCamTransformIndex].Location : CameraComponent->GetComponentLocation();

	//Set which way the camera should move (Up or down)
	if (CurLoc.Z >= OrigCamLoc.Z + CamShakeLocMax.Z && bUp)
	{
		bUp = false;
		CamShakeLocMax.X = FMath::RandRange(OrigCamLoc.X - (OrigCamLoc.X / ppEffectAmount), OrigCamLoc.X + (OrigCamLoc.X / ppEffectAmount));
		CamShakeLocMax.Y = FMath::RandRange(OrigCamLoc.Y - (OrigCamLoc.Y / ppEffectAmount), OrigCamLoc.Y + (OrigCamLoc.Y / ppEffectAmount));
		CamShakeLocMax.Z = FMath::RandRange(ppEffectMaxAmount * -1, 0.0f);
	}
	if (CurLoc.Z <= OrigCamLoc.Z - CamShakeLocMax.Z && !bUp)
	{
		bUp = true;
		CamShakeLocMax.X = FMath::RandRange(OrigCamLoc.X - (OrigCamLoc.X / ppEffectAmount), OrigCamLoc.X + (OrigCamLoc.X / ppEffectAmount));
		CamShakeLocMax.Y = FMath::RandRange(OrigCamLoc.Y - (OrigCamLoc.Y / ppEffectAmount), OrigCamLoc.Y + (OrigCamLoc.Y / ppEffectAmount));
		CamShakeLocMax.Z = FMath::RandRange(0.0f, ppEffectMaxAmount);
	}
	NewCamTransform(CurLoc, CurRot, CameraComponent->FieldOfView);

}

#if WITH_EDITOR
void ABaseRitmoLevel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Single Note
	if (PlayData.SingleNoteClass)
	{
		FName Single = PlayData.SingleNoteClass->GetFName();

		if (Single == FName("StaticMeshNote"))
		{
			for (FNoteMeta& Note : PlayData.SingleNotesMeta)
			{
				Note.SetUsingType("SingleStatic");
			}
		}
		else if (Single == FName("SpriteNote"))
		{
			for (FNoteMeta& Note : PlayData.SingleNotesMeta)
			{
				Note.SetUsingType("SingleSprite");
			}
		}
		else if (Single == FName("SkeletalMeshNote"))
		{
			for (FNoteMeta& Note : PlayData.SingleNotesMeta)
			{
				Note.SetUsingType("SingleSkeletal");
			}
		}
	}
	else
	{
		for (FNoteMeta& Note : PlayData.SingleNotesMeta)
		{
			Note.SetUsingType("None");
		}
	}

	// Hold Note
	if (PlayData.HoldNoteClass)
	{
		FName Hold = PlayData.HoldNoteClass->GetFName();

		if (Hold == FName("StaticMeshHoldNote"))
		{
			for (FHoldNoteMeta& Note : PlayData.HoldNotesMeta)
			{
				for (FNoteMeta& Inner : Note.ComponentMeta)
					Inner.SetUsingType("HoldStatic");
			}
		}
		else if (Hold == FName("SpriteHoldNote"))
		{
			for (FHoldNoteMeta& Note : PlayData.HoldNotesMeta)
			{
				for (FNoteMeta& Inner : Note.ComponentMeta)
					Inner.SetUsingType("HoldSprite");
			}
		}
		else if (Hold == FName("SkeletalMeshHoldNote"))
		{
			for (FHoldNoteMeta& Note : PlayData.HoldNotesMeta)
			{
				for (FNoteMeta& Inner : Note.ComponentMeta)
					Inner.SetUsingType("HoldSkeletal");
			}
		}
		else if (Hold == FName("SplineMeshHoldNote"))
		{
			for (FHoldNoteMeta& Note : PlayData.HoldNotesMeta)
			{
				for (FNoteMeta& Inner : Note.ComponentMeta)
					Inner.SetUsingType("HoldSpline");
			}
		}
	}
	else
	{
		for (FHoldNoteMeta& Note : PlayData.HoldNotesMeta)
		{
			for (FNoteMeta& Inner : Note.ComponentMeta)
				Inner.SetUsingType("None");
		}
	}
	// Special Notes
	TArray<FNoteMeta>* Arr = &PlayData.SwipeNotesMeta;
	for (int i = 0; i < 4; i++)
	{
		if (i == 0)
			Arr = &PlayData.SwipeNotesMeta;
		else if (i == 1)
			Arr = &PlayData.BombNotesMeta;
		else if (i == 2)
			Arr = &PlayData.RandNotesMeta;
		else if (i == 3)
			Arr = &PlayData.IgcNotesMeta;

		if (PlayData.SpecialNoteClass)
		{
			FName Special = PlayData.SpecialNoteClass->GetFName();

			if (Special == FName("StaticMeshNote"))
			{
				for (FNoteMeta& Note : *Arr)
				{
					Note.SetUsingType("SingleStatic");
				}
			}
			else if (Special == FName("SpriteNote"))
			{
				for (FNoteMeta& Note : *Arr)
				{
					Note.SetUsingType("SingleSprite");
				}
			}
			else if (Special == FName("SkeletalMeshNote"))
			{
				for (FNoteMeta& Note : *Arr)
				{
					Note.SetUsingType("SingleSkeletal");
				}
			}
		}
		else
		{
			for (FNoteMeta& Note : *Arr)
			{
				Note.SetUsingType("None");
			}
		}
	}


}

#endif

