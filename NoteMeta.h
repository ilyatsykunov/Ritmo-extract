/* The following structs hold references to a note's mesh assets as well as other parameters that will be set during a note's spawn  

	Copyright (C) 2020-2021 Ilya Tsykunov (ilya@ilyatsykunov.com)
*/
#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "PaperSprite.h"
#include "NoteMap.h"

#include "NoteMeta.generated.h"

// Once we decide to make more complex use of meshes these could be turned into a classes of their own which derive from the NoteMeta class

// Holds information about the decorative part of a tile that needs to be spawned
USTRUCT(BlueprintType, Blueprintable)
struct FNoteMeta
{
	GENERATED_USTRUCT_BODY()

		FNoteMeta()
		: StaticMesh(nullptr), SkeletalMesh(nullptr), Sprite(nullptr), NoteType(ENoteType::EMPTY),
		MainColor(FLinearColor::White), ParticleColor(FLinearColor::White), RotationRange(FRotator(0.0f, 0.0f, 0.0f)),
		bUsingSingleStatic(false), bUsingSingleSprite(false), bUsingSingleSkeletal(false), bUsingHoldStatic(false),
		bUsingHoldSprite(false), bUsingHoldSkeletal(false), bUsingHoldSpline(false)
	{ }

	FNoteMeta(UStaticMesh* NewStaticMesh, USkeletalMesh* NewSkeletalMesh, UPaperSprite* NewSprite, ENoteType NewNoteType,
			FLinearColor NewMainColor, FLinearColor NewParticleColor, FRotator NewRotationRange)
			: 
		StaticMesh(NewStaticMesh), SkeletalMesh(NewSkeletalMesh), Sprite(NewSprite), NoteType(NewNoteType),
		MainColor(NewMainColor), ParticleColor(NewParticleColor), RotationRange(NewRotationRange),
		bUsingSingleStatic(false), bUsingSingleSprite(false), bUsingSingleSkeletal(false), bUsingHoldStatic(false),
		bUsingHoldSprite(false), bUsingHoldSkeletal(false), bUsingHoldSpline(false)
	{ }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUsingSingleStatic || bUsingHoldStatic", EditConditionHides))
		UStaticMesh* StaticMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUsingSingleSkeletal || bUsingHoldSkeletal", EditConditionHides))
		USkeletalMesh* SkeletalMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bUsingSingleSprite || bUsingHoldSprite", EditConditionHides))
		UPaperSprite* Sprite;
	UPROPERTY(BlueprintReadOnly)					ENoteType					NoteType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)		FLinearColor				MainColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)		FLinearColor				ParticleColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)		FRotator					RotationRange;

	UPROPERTY()
		bool bUsingSingleStatic = false;

	UPROPERTY()
		bool bUsingSingleSprite = false;

	UPROPERTY()
		bool bUsingSingleSkeletal = false;

	UPROPERTY()
		bool bUsingHoldStatic = false;

	UPROPERTY()
		bool bUsingHoldSprite = false;

	UPROPERTY()
		bool bUsingHoldSkeletal = false;

	UPROPERTY()
		bool bUsingHoldSpline = false;


	void SetUsingType(FString _NoteType)
	{
		if (_NoteType == "SingleStatic")
		{
			bUsingSingleStatic = true;
			bUsingSingleSprite = false;
			bUsingSingleSkeletal = false;
			bUsingHoldStatic = false;
			bUsingHoldSprite = false;
			bUsingHoldSkeletal = false;
			bUsingHoldSpline = false;
		}
		else if (_NoteType == "HoldStatic" || _NoteType == "HoldSpline")
		{
			bUsingSingleStatic = false;
			bUsingSingleSprite = false;
			bUsingSingleSkeletal = false;
			bUsingHoldStatic = true;
			bUsingHoldSprite = false;
			bUsingHoldSkeletal = false;
			bUsingHoldSpline = false;
		}
		else if (_NoteType == "SingleSkeletal")
		{
			bUsingSingleStatic = false;
			bUsingSingleSprite = false;
			bUsingSingleSkeletal = true;
			bUsingHoldStatic = false;
			bUsingHoldSprite = false;
			bUsingHoldSkeletal = false;
			bUsingHoldSpline = false;
		}
		else if (_NoteType == "HoldSkeletal")
		{
			bUsingSingleStatic = false;
			bUsingSingleSprite = false;
			bUsingSingleSkeletal = false;
			bUsingHoldStatic = false;
			bUsingHoldSprite = false;
			bUsingHoldSkeletal = true;
			bUsingHoldSpline = false;
		}
		else if (_NoteType == "SingleSprite")
		{
			bUsingSingleStatic = false;
			bUsingSingleSprite = true;
			bUsingSingleSkeletal = false;
			bUsingHoldStatic = false;
			bUsingHoldSprite = false;
			bUsingHoldSkeletal = false;
			bUsingHoldSpline = false;
		}
		else if (_NoteType == "HoldSprite")
		{
			bUsingSingleStatic = false;
			bUsingSingleSprite = false;
			bUsingSingleSkeletal = false;
			bUsingHoldStatic = false;
			bUsingHoldSprite = true;
			bUsingHoldSkeletal = false;
			bUsingHoldSpline = false;
		}
		else if (_NoteType == "HoldSpline")
		{
			bUsingSingleStatic = false;
			bUsingSingleSprite = false;
			bUsingSingleSkeletal = false;
			bUsingHoldStatic = false;
			bUsingHoldSprite = false;
			bUsingHoldSkeletal = false;
			bUsingHoldSpline = true;
		}
		else
		{
			bUsingSingleStatic = false;
			bUsingSingleSprite = false;
			bUsingSingleSkeletal = false;
			bUsingHoldStatic = false;
			bUsingHoldSprite = false;
			bUsingHoldSkeletal = false;
		}
	}
};

// Holds information about all parts of a hold tile model
USTRUCT(BlueprintType, Blueprintable)
struct FHoldNoteMeta
{
	FHoldNoteMeta()
		: ComponentMeta({}), NoteType(ENoteType::EMPTY), StartTime(0.0f), EndTime(0.0f),
		MainColor(FLinearColor::White), ParticleColor(FLinearColor::White)
	{ }

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)  	TArray<FNoteMeta>			ComponentMeta;
	UPROPERTY(BlueprintReadOnly)					ENoteType					NoteType;
	UPROPERTY(BlueprintReadOnly)					float						StartTime;
	UPROPERTY(BlueprintReadOnly)					float						EndTime;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)		FLinearColor				MainColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)		FLinearColor				ParticleColor;

	FNoteMeta operator[](int Index) const
	{
		if (ComponentMeta.Num() > 0 && (Index < ComponentMeta.Num() || Index == 0))
			return ComponentMeta[Index];
		else
			return FNoteMeta();
	}
};