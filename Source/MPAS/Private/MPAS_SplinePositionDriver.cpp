// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/RigElements/PositionDrivers/MPAS_SplinePositionDriver.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

// Calculates location and rotation of a spline point
void UMPAS_SplinePositionDriver::CalculateSplinePointPosition(FVector& OutLocation, FRotator& OutRotation, int32 SplinePoint)
{
	if (SplinePoint >= SplinePoints.Num()) return;

	// Location

	if (SplinePoints[SplinePoint].PositionMode == EMPAS_SplinePointPositionMode::SplineFraction)
		OutLocation = Spline->GetLocationAtTime(SplinePoints[SplinePoint].PositionValue, ESplineCoordinateSpace::World, true);

	else if (SplinePoints[SplinePoint].PositionMode == EMPAS_SplinePointPositionMode::Distance)
		OutLocation = Spline->GetLocationAtDistanceAlongSpline(SplinePoints[SplinePoint].PositionValue, ESplineCoordinateSpace::World);

	// Rotation

	if (SplinePoints[SplinePoint].PositionMode == EMPAS_SplinePointPositionMode::SplineFraction)
		OutRotation = Spline->GetRotationAtTime(SplinePoints[SplinePoint].PositionValue, ESplineCoordinateSpace::World, true) + SplinePoints[SplinePoint].AdditiveRotation;

	else if (SplinePoints[SplinePoint].PositionMode == EMPAS_SplinePointPositionMode::Distance)
		OutRotation = Spline->GetRotationAtDistanceAlongSpline(SplinePoints[SplinePoint].PositionValue, ESplineCoordinateSpace::World) + SplinePoints[SplinePoint].AdditiveRotation;
}

// Scans the spline and registers all of the control points
void UMPAS_SplinePositionDriver::ScanSplineControlPoints()
{
	if (!Spline) return;

	ControlPoints.Empty();

	FVector PreviousPointLocation = GetComponentLocation();
	FQuat PreviousPointRotation = GetComponentQuat();
	for (int i = 0; i < Spline->GetNumberOfSplinePoints(); i++)
	{
		FMPAS_ControlPointData ControlPointData = DefaultControlPointData;

		if (ControlPointPositionModeOverrides.Contains(i))
			ControlPointData = ControlPointPositionModeOverrides[i];

		ControlPointData.LocationVectorStackID = RegisterVectorStack("ControlPointLocation_" + FString::FromInt(i));
		RegisterVectorLayer(ControlPointData.LocationVectorStackID, "BaseLocation", EMPAS_LayerBlendingMode::Normal, EMPAS_LayerCombinationMode::Add, 1.f, 0, true);
		
		ControlPointData.IntendedRelativeOffset = UKismetMathLibrary::Quat_UnrotateVector(PreviousPointRotation, Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World) - PreviousPointLocation);

		ControlPoints.Add(ControlPointData);

		PreviousPointLocation = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		PreviousPointRotation = Spline->GetQuaternionAtSplinePoint(i, ESplineCoordinateSpace::World);
	}
}

// Manually links a spline component to the position driver
void UMPAS_SplinePositionDriver::LinkSplineComponent(USplineComponent* InSpline)
{
	if (Spline == InSpline) return;

	Spline = InSpline;
	ScanSplineControlPoints();
}


// VIRTUAL, Called right after the position driver has gathered data about it's driven elements
void UMPAS_SplinePositionDriver::OnPositionDriverInitialized_Implementation()
{
	// Automatically fetching Spline Component with tag: "<PositionDriverName>_Spline" (Example: "SplinePositionDriver_Spline")

	TArray<UActorComponent*> Components = GetOwner()->GetComponentsByTag(USplineComponent::StaticClass(), FName(DriverName.ToString() + "_Spline"));

	if (Components.Num() > 0)
		Spline = Cast<USplineComponent>(Components[0]);

	if (!Spline) return; // If there is no spline, then don't continue the initialization

	// Scanning spline control points

	if (Spline)
	{
		ScanSplineControlPoints();
	}

	// Assinging driven elements to spline points

	FVector TempLocation;
	FRotator TempRotation;

	// To each point we assign the one rig element, that is closest to it
	for (int i = 0; i < SplinePoints.Num(); i++)
	{
		float MinDistance = 10e9f;
		UMPAS_RigElement* ClosestDrivenElement = nullptr;

		CalculateSplinePointPosition(TempLocation, TempRotation, i);

		for (auto Element : DrivenElements)
		{
			float Distance = FVector::Distance(TempLocation, Element.Key->GetComponentLocation());
			if (Distance < MinDistance && !SplinePointAssignment.Contains(Element.Key))
			{
				MinDistance = Distance;
				ClosestDrivenElement = Element.Key;
			}
		}

		SplinePointAssignment.Add(ClosestDrivenElement, i);
	}
}

// VIRTUAL, Calculates the required transform (location and rotation) for the specified element, 
// If the specified element is not one of the affected by this Position Driver, (0, 0, 0)-s are returned
void UMPAS_SplinePositionDriver::CalculateElementTransform_Implementation(FVector& OutLocation, FRotator& OutRotation, UMPAS_RigElement* InRigElement)
{
	// If this rig element is not driven by this driver
	if (!DrivenElementsList.Contains(InRigElement) || !Spline)
	{
		OutLocation = FVector(0, 0, 0);
		OutRotation = FRotator(0, 0 ,0);
		return;
	}

	CalculateSplinePointPosition(OutLocation, OutRotation, SplinePointAssignment[InRigElement]);
}


// INTENTION DRIVING

// Returns control point's configuration data (Control Points - Points that define the shape of the spline)
FMPAS_ControlPointData UMPAS_SplinePositionDriver::GetControlPointsSettings(int32 InControlPoint)
{
	if (InControlPoint < 0 || InControlPoint >= ControlPoints.Num())
		return FMPAS_ControlPointData();

	return ControlPoints[InControlPoint];
}

// Changes control point's configuration data (Control Points - Points that define the shape of the spline)
void UMPAS_SplinePositionDriver::SetControlPointSettings(int32 InControlPoint, const FMPAS_ControlPointData& InSettings)
{
	if (InControlPoint < 0 || InControlPoint >= ControlPoints.Num()) return;

	FMPAS_ControlPointData Cache = ControlPoints[InControlPoint];
	ControlPoints[InControlPoint] = InSettings;

	// Keeping some things that are not supposed to be modified
	ControlPoints[InControlPoint].IntendedRelativeOffset = Cache.IntendedRelativeOffset;
	ControlPoints[InControlPoint].LocationVectorStackID = Cache.LocationVectorStackID;
	//...
}


// Modifies control point's location in world space 
// If Force Movement flag is set, the spline point will imedeately be moved to the new location, otherwise only the indended offset will be modifed
void UMPAS_SplinePositionDriver::SetControlPointLocation(int32 InControlPoint, const FVector& InLocation, bool ForceMovement)
{
	if (!Spline || InControlPoint < 0 || InControlPoint >= ControlPoints.Num()) return;

	// Calculating new intended relative offset for the control point

	FVector PreviousPointLocation = GetComponentLocation();
	if (InControlPoint > 0)
		PreviousPointLocation = Spline->GetLocationAtSplinePoint(InControlPoint - 1, ESplineCoordinateSpace::World);

	ControlPoints[InControlPoint].IntendedRelativeOffset = InLocation - PreviousPointLocation;

	// Forcing new location if needed
	if (ForceMovement)
	{
		SetVectorSourceValue(ControlPoints[InControlPoint].LocationVectorStackID, 0, this, InLocation);

		// Applying stack value to the spline point
		Spline->SetLocationAtSplinePoint(InControlPoint, CalculateVectorStackValue(ControlPoints[InControlPoint].LocationVectorStackID), ESplineCoordinateSpace::World);
	}
}

// Sets a new control point's tangent value in relative space (relative to the control point)
void UMPAS_SplinePositionDriver::SetControlPointTangentRelative(int32 InControlPoint, const FVector& InArrivingTangentRelativeLocation, const FVector& InLeavingTangentRelativeLocation)
{
	if (!Spline || InControlPoint < 0 || InControlPoint >= ControlPoints.Num()) return;

	Spline->SetTangentsAtSplinePoint(InControlPoint, InArrivingTangentRelativeLocation, InLeavingTangentRelativeLocation, ESplineCoordinateSpace::Local);
}

// Sets a new control point's tangent value in world space
void UMPAS_SplinePositionDriver::SetControlPointTangentWorld(int32 InControlPoint, const FVector& InArrivingTangentWorldLocation, const FVector& InLeavingTangentWorldLocation)
{
	if (!Spline || InControlPoint < 0 || InControlPoint >= ControlPoints.Num()) return;

	Spline->SetTangentsAtSplinePoint(InControlPoint, InArrivingTangentWorldLocation, InLeavingTangentWorldLocation, ESplineCoordinateSpace::World);
}

// Control point effects

// Adds a new effect layer to the specified control point
// Effect layers can modify control point's location in runtime
int32 UMPAS_SplinePositionDriver::AddControlPointProceduralEffectLayer(int32 InControlPoint, const FString& InLayerName, EMPAS_LayerBlendingMode InBlendingMode, EMPAS_LayerCombinationMode InCombinationMode, float InBlendingFactor, int32 InPriority, bool InForceAllElementActive)
{
	if (!Spline || InControlPoint < 0 || InControlPoint >= ControlPoints.Num()) return -1;

	return RegisterVectorLayer(ControlPoints[InControlPoint].LocationVectorStackID, InLayerName, InBlendingMode, InCombinationMode, InBlendingFactor, InPriority, InForceAllElementActive);
}

// Adds (or modifies) the value on the specified effect layer of the specific control point
void UMPAS_SplinePositionDriver::SetControlPointProceduralEffectSourceValue(int32 InControlPoint, int32 InLayerID, UMPAS_RigElement* InSource, const FVector& Value)
{
	if (!Spline || InControlPoint < 0 || InControlPoint >= ControlPoints.Num() || InLayerID == 0) return;

	SetVectorSourceValue(ControlPoints[InControlPoint].LocationVectorStackID, InLayerID, InSource, Value);
}

// Retunrs the value on the specified effect layer of the specific control point
const FVector& UMPAS_SplinePositionDriver::GetControlPointProceduralEffectSourceValue(int32 InControlPoint, int32 InLayerID, UMPAS_RigElement* InSource)
{
	if (!Spline || InControlPoint < 0 || InControlPoint >= ControlPoints.Num()) FVector::ZeroVector;

	return GetVectorSourceValue(ControlPoints[InControlPoint].LocationVectorStackID, InLayerID, InSource);
}



// CALLED BY THE HANDLER : Updating Rig Element every tick
void UMPAS_SplinePositionDriver::UpdateRigElement(float DeltaTime)
{
	Super::UpdateRigElement(DeltaTime);

	if (!Spline) return;

	// Auto Tangent Smoothing

	for (int i = 0; i < ControlPoints.Num(); i++)
	{
		if (ControlPoints[i].AutoTangentSmoothing && Spline->GetNumberOfSplinePoints() > 1)
		{
			// Calculating tangent direction

			FVector TangentDirection;

			// First point case
			if (i == 0)
				TangentDirection = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World) - Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);

			// Last point case
			else if (i == Spline->GetNumberOfSplinePoints() - 1)
				TangentDirection = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World) - Spline->GetLocationAtSplinePoint(i - 1, ESplineCoordinateSpace::World);

			// General case
			TangentDirection = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World) - Spline->GetLocationAtSplinePoint(i - 1, ESplineCoordinateSpace::World);

			// Normalization
			TangentDirection.Normalize();

			// Tangent calculation and application
			FVector Tangent = TangentDirection * ControlPoints[i].TangetDistanceFactor;

			Spline->SetTangentAtSplinePoint(i, Tangent, ESplineCoordinateSpace::Local);
		}
	}


	// Updating spline shape dynamics

	FVector PreviousPointLocation = GetComponentLocation();
	FQuat PreviousPointRotation = GetComponentQuat();

	for (int i = 0; i < ControlPoints.Num(); i++)
	{
		FVector Offset;
		if (i > 0)
			Offset = PreviousPointRotation.RotateVector(ControlPoints[i].IntendedRelativeOffset);
		else
			Offset = ControlPoints[i].IntendedRelativeOffset;

		if (ControlPoints[i].PositionMode == EMPAS_ControlPointPositionMode::Rigid)
		{
			SetVectorSourceValue(ControlPoints[i].LocationVectorStackID, 0, this, PreviousPointLocation + Offset);
		}

		else if (ControlPoints[i].PositionMode == EMPAS_ControlPointPositionMode::Soft)
		{
			// Caching

			FVector PointLocation = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			FQuat PointQuat = Spline->GetQuaternionAtSplinePoint(i, ESplineCoordinateSpace::World);

			FVector DeformationVector = PointLocation - (PreviousPointLocation + Offset);
			FVector LocalizedDeformationVector = UKismetMathLibrary::Quat_UnrotateVector(PointQuat, DeformationVector);

			// Data

			FVector ClampedVector = DeformationVector; // <- Calculating this

			// Forward deformation

			float ForwardDeformation = LocalizedDeformationVector.X;
			float Limitation = ControlPoints[i].SOFT_MaxDeformation * ControlPoints[i].SOFT_FreedomAxes.X;

			if (abs(ForwardDeformation) > Limitation)
				ClampedVector += PointQuat.RotateVector(FVector(1, 0, 0) * UKismetMathLibrary::SignOfFloat(ForwardDeformation) * Limitation - FVector(1, 0, 0) * ForwardDeformation);

			// Right deformation

			float RightDeformation = LocalizedDeformationVector.Y;
			Limitation = ControlPoints[i].SOFT_MaxDeformation * ControlPoints[i].SOFT_FreedomAxes.Y;

			if (abs(RightDeformation) > Limitation)
				ClampedVector += PointQuat.RotateVector(FVector(0, 1, 0) * UKismetMathLibrary::SignOfFloat(RightDeformation) * Limitation - FVector(0, 1, 0) * RightDeformation);

			// Vertical deformation

			float VerticalDeformation = LocalizedDeformationVector.Z;
			Limitation = ControlPoints[i].SOFT_MaxDeformation * ControlPoints[i].SOFT_FreedomAxes.Z;

			if (abs(VerticalDeformation) > Limitation)
				ClampedVector += PointQuat.RotateVector(FVector(0, 0, 1) * UKismetMathLibrary::SignOfFloat(VerticalDeformation) * Limitation - FVector(0, 0, 1) * VerticalDeformation);
			
			// Applying
			SetVectorSourceValue(ControlPoints[i].LocationVectorStackID, 0, this, PreviousPointLocation + Offset + ClampedVector);
		}

		else // World Space case
		{
			// Doing nothing
		}

		// Applying stack value to the spline point
		Spline->SetLocationAtSplinePoint(i, CalculateVectorStackValue(ControlPoints[i].LocationVectorStackID), ESplineCoordinateSpace::World);

		// Loop stuff
		PreviousPointLocation = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		PreviousPointRotation = UKismetMathLibrary::Quat_Slerp(Spline->GetQuaternionAtSplinePoint(i, ESplineCoordinateSpace::World), GetComponentQuat(), 0.f);
	}
}
