// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Default/RigElements/PositionDrivers/MPAS_PositionDriver.h"
#include "MPAS_SplinePositionDriver.generated.h"


// Spline points strucutres

UENUM(BlueprintType)
enum class EMPAS_SplinePointPositionMode : uint8
{
	SplineFraction UMETA(DisplayName = "Spline Fraction"),
	Distance UMETA(DisplayName = "Distance")
};

USTRUCT(BlueprintType)
struct FMPAS_SplinePointData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EMPAS_SplinePointPositionMode PositionMode;

	// Depends on the PositionMode
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float PositionValue;

	// Rotation value that is added to the resulting transform
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FRotator AdditiveRotation;
};

// Control points strucutres

UENUM(BlueprintType)
enum class EMPAS_ControlPointPositionMode : uint8
{
	Rigid UMETA(DisplayName = "Rigid"),
	Soft UMETA(DisplayName = "Soft"),
	WorldSpace UMETA(DisplayName = "World Space")
};

USTRUCT(BlueprintType)
struct FMPAS_ControlPointData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AutoTangentSmoothing")
	bool AutoTangentSmoothing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AutoTangentSmoothing")
	float TangetDistanceFactor = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EMPAS_ControlPointPositionMode PositionMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SoftPositionMode")
	float SOFT_MaxDeformation = 100.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SoftPositionMode")
	FVector SOFT_FreedomAxes = FVector(1, 1, 1);

	// Desired offset from the previous control point
	//UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector IntendedRelativeOffset; 

	/*UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector ArrivingTangent;*/
};


/**
 * Position Driver in a form of a spline
 * 
 * CONTROL POINTS - Points that define the shape of the spline;
 * SPLINE POINTS - Points that define the position of elements, that are attached to the spline
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MPAS_API UMPAS_SplinePositionDriver : public UMPAS_PositionDriver
{
	GENERATED_BODY()
	
protected:

	// Pointer to the spline component, linked to this position driver
	class USplineComponent* Spline;

	// To which spline point is the given Rig Element assigned?
	TMap<UMPAS_RigElement*, int32> SplinePointAssignment;

	// Intention Driving data for each of the spline's control points
	TArray<FMPAS_ControlPointData> ControlPoints;

public:

	// Defines points, driven elements are going to attach to
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|SplinePositionDriver")
	TArray<FMPAS_SplinePointData> SplinePoints;

	// Default control point configuration
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|SplinePositionDriver")
	FMPAS_ControlPointData DefaultControlPointData;

	// Per point override for control points (APPLIED ONCE, USE FUNCTIONS TO CHANGE IN RUNTIME)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default|SplinePositionDriver")
	TMap<int32, FMPAS_ControlPointData> ControlPointPositionModeOverrides;


protected:

	// Calculates location and rotation of a spline point
	void CalculateSplinePointPosition(FVector& OutLocation, FRotator& OutRotation, int32 SplinePoint);

	// Scans the spline and registers all of the control points
	void ScanSplineControlPoints();

public:

	// Manually links a spline component to the position driver
	UFUNCTION(BlueprintCallable, Category="MPAS|Elements|PositionDriver|Spline")
	void LinkSplineComponent(class USplineComponent* InSpline);

	// Returns a pointer to the spline component, that is linked to this position driver
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|PositionDriver|Spline")
	class USplineComponent* GetSplineComponent() { return Spline; }


	// INTENTION DRIVING

	// Returns control point's configuration data (Control Points - Points that define the shape of the spline)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Category = "MPAS|Elements|PositionDriver|Spline")
	FMPAS_ControlPointData GetControlPointsSettings(int32 InControlPoint);

	// Changes control point's configuration data (Control Points - Points that define the shape of the spline)
	UFUNCTION(BlueprintCallable, Category = Category = "MPAS|Elements|PositionDriver|Spline")
	void SetControlPointSettings(int32 InControlPoint, const FMPAS_ControlPointData& InSettings);


	// Modifies control point's location in world space 
	// If Force Movement flag is set, the spline point will imedeately be moved to the new location, otherwise only the indended offset will be modifed
	UFUNCTION(BlueprintCallable, Category= Category = "MPAS|Elements|PositionDriver|Spline")
	void SetControlPointLocation(int32 InControlPoint, const FVector& InLocation, bool ForceMovement = true);

	// Sets a new control point's tangent value in relative space (relative to the control point)
	UFUNCTION(BlueprintCallable, Category = Category = "MPAS|Elements|PositionDriver|Spline")
	void SetControlPointTangentRelative(int32 InControlPoint, const FVector& InArrivingTangentRelativeLocation, const FVector& InLeavingTangentRelativeLocation);

	// Sets a new control point's tangent value in world space
	UFUNCTION(BlueprintCallable, Category = Category = "MPAS|Elements|PositionDriver|Spline")
	void SetControlPointTangentWorld(int32 InControlPoint, const FVector& InArrivingTangentWorldLocation, const FVector& InLeavingTangentWorldLocation);



	// VIRTUAL, Called right after the position driver has gathered data about it's driven elements
	virtual void OnPositionDriverInitialized_Implementation() override;

	// VIRTUAL, Calculates the required transform (location and rotation) for the specified element, 
	// If the specified element is not one of the affected by this Position Driver, (0, 0, 0)-s are returned
	virtual void CalculateElementTransform_Implementation(FVector& OutLocation, FRotator& OutRotation, UMPAS_RigElement* InRigElement) override;


	// CALLED BY THE HANDLER : Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;
};
