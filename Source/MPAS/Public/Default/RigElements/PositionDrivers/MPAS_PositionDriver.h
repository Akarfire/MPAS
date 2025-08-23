// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/RigElements/MPAS_VoidRigElement.h"
#include "MPAS_PositionDriver.generated.h"

USTRUCT()
struct FMPAS_PositionDrivenElementData
{
	GENERATED_USTRUCT_BODY()

	UMPAS_RigElement* RigElement;

	int32 LocationStackID;
	int32 LocationLayerID;

	int32 RotationStackID;
	int32 RotationLayerID;

	FMPAS_PositionDrivenElementData(UMPAS_RigElement* InRigElement, int32 InLocationStackID, int32 InLocationLayerID, int32 InRotationStackID, int32 InRotationLayerID):
		RigElement(InRigElement), LocationStackID(InLocationStackID), LocationLayerID(InLocationLayerID), RotationStackID(InRotationStackID), RotationLayerID(InRotationLayerID)
	{}
};


/**
 * 
 */
UCLASS()
class MPAS_API UMPAS_PositionDriver : public UMPAS_VoidRigElement
{
	GENERATED_BODY()

// Data
protected:

	// A map of all elements affected by this position driver
	TMap<UMPAS_RigElement*, FMPAS_PositionDrivenElementData> DrivenElements; 
	
public:


// Methods
protected:


public:	

	UMPAS_PositionDriver() {}


	// VIRTUAL, Calculates the required transform (location and rotation) for the specified element, 
	// If the specified element is not one of the affected by this Position Driver, then the result depends on the implementation
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MPAS|Elements|PositionDriver")
	void CalculateElementTransform(FVector& OutLocation, FRotator& OutRotation, UMPAS_RigElement* InRigElement);
	virtual void CalculateElementTransform_Implementation(FVector& OutLocation, FRotator& OutRotation, UMPAS_RigElement* InRigElement)
	{
		OutLocation = GetComponentLocation();
		OutRotation = GetComponentRotation();
	}

	// VIRTUAL, Called right after the position driver has gathered data about it's driven elements
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MPAS|Elements|PositionDriver")
	void OnPositionDriverInitialized();
	virtual void OnPositionDriverInitialized_Implementation() {}
	


	// DATA GETTERS

	// Returns an array, containing pointers to all of the rig elements that are affected by this position driver
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Elements|PositionDriver")
	TArray<UMPAS_RigElement*> GetDrivenElementsList();

	// CALLED BY THE HANDLER
	// 
	// Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler) override;

	// Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;

};
