// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MPAS_RigElement.h"
#include "MPAS_MotionRigElement.generated.h"

/**
 * An abstract class for all rig elements, that are able to carry out movement (Legs, Wings, etc.)
 */
UCLASS()
class MPAS_API UMPAS_MotionRigElement : public UMPAS_RigElement
{
	GENERATED_BODY()
	

//protected:
//
//	// Whether this element is in keep up mode or not
//	bool KeepUpMode = false;
//
//
//public:
//	//...
//
//protected:
//
//	void UpdateNormalMode(float DeltaTime) {}
};
