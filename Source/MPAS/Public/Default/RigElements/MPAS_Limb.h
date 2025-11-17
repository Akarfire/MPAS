// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MPAS_VoidRigElement.h"
#include "MPAS_Limb.generated.h"


// Defines a single segment in UMPAS_Limb
USTRUCT(BlueprintType)
struct FMPAS_LimbSegmentData
{
	GENERATED_USTRUCT_BODY()

	// The length of the limb segment
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Length;

	// Rotational limits for each rotation axis
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator AngularLimits_Min;

	// Rotational limits for each rotation axis
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator AngularLimits_Max;

	// Name of the bone, corresponding to this limb segment
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BoneName;

	// NON-Length plane extent of the mesh of this segment (used for visualization)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D SegmentMeshExtent;

	FMPAS_LimbSegmentData(): Length(0), AngularLimits_Min(FRotator(-360, -360, -360)), AngularLimits_Max(FRotator(360, 360, 360)), BoneName(FName()), SegmentMeshExtent(FVector2D(1, 1)) {}
};

/*
 * Defines AngularLimits and SegmentMeshExtent for a single segment in UMPAS_Limb
 * Needed to configure FetchFromMesh mode of the limb, as these parameters are impossible to extract from a mesh
 */
USTRUCT(BlueprintType)
struct FMPAS_AdditionalLimbSegmentData
{
	GENERATED_USTRUCT_BODY()

	// Rotational limits for each rotation axis
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator AngularLimits_Min;

	// Rotational limits for each rotation axis
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator AngularLimits_Max;

	// NON-Length plane extent of the mesh of this segment (used for visualization)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D SegmentMeshExtent;

	FMPAS_AdditionalLimbSegmentData(): AngularLimits_Min(FRotator(-360, -360, -360)), AngularLimits_Max(FRotator(360, 360, 360)), SegmentMeshExtent(FVector2D(1, 1)) {}
};



// Contains the state of a single limb segment
USTRUCT(BlueprintType)
struct FMPAS_LimbSegmentState
{
	GENERATED_USTRUCT_BODY()

	// The root location of the segment
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location;

	// The rotation of the segment
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Rotation;

	FMPAS_LimbSegmentState(): Location(FVector(0, 0, 0)), Rotation(FRotator(0, 0, 0)) {}
};


// How should a limb pole target location be calcualted
UENUM(BlueprintType)
enum class EMPAS_LimbPoleTargetLocationMode : uint8
{
	// Pole target location will be calculated automatically, using settings provided in FMPAS_LimbPoleTarget
	AutoCalculation UMETA(DisplayName="Auto Calculation"),

	// Use a custom location stack as a pole target (stack is located in the limb component)
	PoleTargetLocationStack UMETA(DisplayName="Pole Target Location Stack")
};

// Contains settings of a sinlge pole target, assigned to a bone segment
USTRUCT(BlueprintType)
struct FMPAS_LimbPoleTarget
{
	GENERATED_USTRUCT_BODY()

	// How should a limb pole target location be calcualted
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMPAS_LimbPoleTargetLocationMode LocationMode;

	// Offset of the pole target in relativity to limb origin and limb target 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector AUTO_PoleTargetOffset;

	// ID of a postion stack, created for this pole target (if LocationMode is set to PoleTargetLocationStack)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 STACK_VectorStackID;

	FMPAS_LimbPoleTarget(): LocationMode(EMPAS_LimbPoleTargetLocationMode::AutoCalculation), AUTO_PoleTargetOffset(FVector(0, 0, 0)), STACK_VectorStackID(0) {}
};


// The algorithm that will be used to position/animate the limb
UENUM(BlueprintType)
enum class EMPAS_LimbSolvingAlgorithm : uint8
{
	// PoleFABRIK IK - custom version of FABRIK IK, sligtly slower, but implements support for pole targets, making it the most usable algorithm out of the ones presented here
	PoleFABRIK_IK UMETA(DisplayName="PoleFABRIK IK"),

	// PoleFABRIK IK - custom version of FABRIK IK, sligtly slower, but implements support for pole targets, making it the most usable algorithm out of the ones presented here
	// Supports angular limits
	PoleFABRIK_Limited_IK UMETA(DisplayName = "PoleFABRIK Limited IK"),

	// Simply rotate the limb towards the target, efficient for single-segment limbs
	RotateToTarget UMETA(DisplayName="Rotate To Target"),

	// Implements a light-weight FABRIK IK algorithm
	FABRIK_IK UMETA(DisplayName="FABRIK IK"),

	// Implements FABRIK algorithm with angular limits
	FABRIK_Limited_IK UMETA(DisplayName = "FABRIK LIMITED IK"),

	// Implements CCD IK algorithm
	CCD_IK UMETA(DisplayName="CCD IK"),
	
	// Turns the limb into a telescopic multi-stage piston for mechanical effects, all segments extend at the same time
	PistonMulti UMETA(DisplayName = "Piston Multi"),

	// Turns the limb into a telescopic multi-stage piston for mechanical effects, segments extend one by one
	PistonSequential UMETA(DisplayName = "Piston Sequential")

	//// Gauss-Seidel IK approximation method (paper link: https://arxiv.org/pdf/2211.00330)
	//Gauss_Seidel UMETA(DisplayName="Gauss-Seidel")
};

// What should be used as a target of the limb
UENUM(BlueprintType)
enum class EMPAS_LimbTargetType : uint8
{
	// Use the first child component of the limb as a target
	FirstChildComponent UMETA(DisplayName="First Child Component"),

	// Use a custom vector stack as a target (stack is located in the limb component)
	TargetVectorStack UMETA(DisplayName="Target Vector Stack")
};

// How should the limb segments be set up?
UENUM(BlueprintType)
enum class EMPAS_LimbSetupType : uint8
{
	// Segments will be fetched from a bone chain of a specified skeletal mesh
	FetchFromMesh UMETA(DisplayName="Fetch From Mesh"),

	// User defined custom chain
	CustomChain UMETA(DisplayName="Custom Chain")	
};


// LIMB

/**
 * A segmented limb component, that can be animated using Inverse Kinematics or similiar algorithms
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPAS_API UMPAS_Limb : public UMPAS_VoidRigElement
{
	GENERATED_BODY()
	
public:
	UMPAS_Limb();

// PARAMETERS
public:

	// The algoritm that is going to be used to solve (position / animate) the limb
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb")
	EMPAS_LimbSolvingAlgorithm Algoritm;

	// What should be used as a target of the limb
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb")
	EMPAS_LimbTargetType TargetType;

	// How should the limb segments be set up?
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb")
	EMPAS_LimbSetupType SetupType;

	// EXPERIMENTAL How fast the limb transitions to a new calculated location ( 0 - means instant transition), can cause weird behavior with RollRecalculation
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|Interpolation")
	float LimbInterpolationSpeed = 0.f;

	// Should inverse kinematics algorithms run on a background thread or on the main thread?
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb")
	bool EnableAsyncCalculation = true;

	/* List of pole targets, assigned to the chain: < First segment the pole target is to be applied to : FMPAS_LimbPoleTarget >
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb")
	TMap<int32, FMPAS_LimbPoleTarget> PoleTargets;

	// Should the limb recalculate segment roll rotation axis after solving the limb? (Should be on in the absulute majority of cases)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|Rotation")
	bool EnableRollRecalculation = true;

	// Used to fix Limb rotation (play around with it) (works only if EnableRollRecalculation is set to True) - Additional roll that is added during segment roll recalculation
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|Rotation")
	float LimbRoll = 0.f;

	// The maximum number of inverse kinematics iterations that can happen in a single update
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|InverseKinematics")
	int32 IK_MaxIterations = 32;
	
	// Inverse kinematics location error tollerance, higher values mean less iterations on average (better perfomance, less accurate results)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|InverseKinematics")
	float IK_ErrorTollerance = 10.f;

	// Bone, that marks the beginning of the fetched chain
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|FetchFromMesh")
	FName Fetch_OriginBone;

	// Bone, that marks the end of the fetched chain
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|FetchFromMesh")
	FName Fetch_TipBone;

	// If set to true, then origin position will be mathed with the origin bone
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|FetchFromMesh")
	bool Fetch_OriginPosition = true;

	/*
	 * Defines AngularLimits and SegmentMeshExtent for each single segment in UMPAS_Limb
	 * If not enough entries are specified, extra segments will use the last entry
	 * If no entries are specified, segments will default to values:
	 * 			-  AngularLimits = FRotator(90, 90, 90);
	 * 			-  SegmentMeshExtent =FVector2D(1, 1);
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|FetchFromMesh")
	TArray<FMPAS_AdditionalLimbSegmentData> AdditionalSegmentData;


	// An array of all limb segments, the order of the elements defines the order of segments in the limb
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Default|Limb|CustomChain")
	TArray<FMPAS_LimbSegmentData> Segments;


// DATA
protected:

	// Used as a target if TargetType is set to FirstChildComponent
	USceneComponent*  TargetComponent;
	
	// Used as an ID of the target vector stack if TargetType is set to TargetVectorStack
	int32 TargetStackID;

	// Current segment configuration
	TArray<FMPAS_LimbSegmentState> CurrentState;

	// Segment configuration the limb has to assume, by interpolating to it from the current state
	TArray<FMPAS_LimbSegmentState> TargetState;

	// Whether the limb is in a process of asynchronoulsy solving it's state;
	bool CurrentlySolving;

	// Mesh, from which the bone chain will be fetched
	USkeletalMeshComponent* Fetch_MeshComponent;

	// Whether the limb was succesfully initialized
	bool Initialized;

	// Total lenght of all limb segments
	float MaxExtent;



// INTERFACE
public:

	// Returns the ID of the target vector stack, used if TargetType is set to TargetVectorStack
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Elements|Limb")
	int32 GetTargetStackID() { return TargetStackID; }

	// Returns the current configuration of the segments
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Elements|Limb")
	const TArray<FMPAS_LimbSegmentState>& GetCurrentState() { return CurrentState; }

	// The point is space (World Space), which the limb is trying to reach
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Elements|Limb")
	FVector GetLimbTarget();

	// Whether the limb was succesfully initialized
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Elements|Limb")
	bool GetInitialized() { return Initialized; }
	
	// Reinitializes limb's segments and state
	UFUNCTION(BlueprintCallable, Category="MPAS|Elements|Limb")
	void ReinitLimb();

	// Changes the element the limb origin is attached to (by default it is the limb parent)
	UFUNCTION(BlueprintCallable, Category = "MPAS|Elements|Limb")
	void OverrideAttachmentParent(UMPAS_RigElement* NewParent);


	// Returns the mesh, from which the bone chain will be fetched, !IF SetupType is set to FetchFromMesh!
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="MPAS|Elements|Limb")
	USkeletalMeshComponent* GetFetchMeshComponent() { return Fetch_MeshComponent; }

	// Sets the mesh, from which the bone chain will be fetched, !IF SetupType is set to FetchFromMesh!
	UFUNCTION(BlueprintCallable, Category="MPAS|Elements|Limb")
	void SetFetchMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent) { Fetch_MeshComponent = InSkeletalMeshComponent; }


// BACKGROUND
protected:

	// Initializes limb's segments and state
	void InitLimb();

	// Solves the limb by applying the specified algorithm to the segments
	void SolveLimb();

	// A call back from the background thread, that indicates that the limb has finished solving it's state
	void FinishSolving(TArray<FMPAS_LimbSegmentState> ResultingState);

	// Updates the current state of the specified segment
	void WriteSegmentState(int32 InSegment, const FMPAS_LimbSegmentState& InState);

	// Handles interpolation of the current state to the target state
	void InterpolateLimb(float DeltaTime);

	// Calculates resulting location of the pole target in world space
	FVector CalculatePoleTargetLocation(const FMPAS_LimbPoleTarget& InPoleTargetSettings);

	
	// Algorithms
	// 'static' because they are going to run in a background thread

	// Rotate To Target
	static TArray<FMPAS_LimbSegmentState> Solve_RotateToTarget(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState);

	// FABRIK IK
	static TArray<FMPAS_LimbSegmentState> Solve_FABRIK_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance);
	
	// FABRIK Limited
	static TArray<FMPAS_LimbSegmentState> Solve_FABRIK_Limited_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance);

	// CCD IK
	static TArray<FMPAS_LimbSegmentState> Solve_CCD_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, int32 InMaxIterations, float InTollerance);

	// PoleFABRIK IK - custom version of FABRIK IK, sligtly slower, but implements support for pole targets, making it the most usable algorithm out of the ones presented here
	static TArray<FMPAS_LimbSegmentState> Solve_PoleFABRIK_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, const TArray<FVector>& InPoleTargets, int32 InMaxIterations, float InTollerance, const FVector& InUpVector);

	// PoleFABRIK Limited - custom version of FABRIK IK, sligtly slower, but implements support for pole targets, making it the most usable algorithm out of the ones presented here
	static TArray<FMPAS_LimbSegmentState> Solve_PoleFABRIK_Limited_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, const TArray<FVector>& InPoleTargets, int32 InMaxIterations, float InTollerance, const FVector& InUpVector);

	// Turns the limb into a telescopic multi-stage piston for mechanical effects, all segments extend at the same time
	static TArray<FMPAS_LimbSegmentState> Solve_Piston_Multi(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, float InLimbMaxExtent);

	// Turns the limb into a telescopic multi-stage piston for mechanical effects, segments extend one by one
	static TArray<FMPAS_LimbSegmentState> Solve_Piston_Sequential(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState);

	//// Gauss-Seidel IK (parer link: https://arxiv.org/pdf/2211.00330)
	//static TArray<FMPAS_LimbSegmentState> Solve_Gauss_Seidel_IK(const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FMPAS_LimbSegmentState>& InCurrentState, const TArray<FVector>& InPoleTargets, int32 InMaxIterations, float InTollerance, const FVector& InUpVector);


	// Recalculating segment roll rotation
	static void RecalculateRoll(TArray<FMPAS_LimbSegmentState>& InOutState, float InLimbRoll, const FVector& InOriginLocation, const FVector& InTargetLocation, const TArray<FMPAS_LimbSegmentData>& InSegments, const TArray<FVector>& InPoleTargets, const FVector& InUpVector);

	// CALLED BY THE HANDLER
	// Initializing Rig Element
	virtual void InitRigElement(class UMPAS_Handler* InHandler) override;

	// CALLED BY THE HANDLER : Contains the logic that links this element with other elements in the rig
	virtual void LinkRigElement(class UMPAS_Handler* InHandler) override;


	// Updating Rig Element every tick
	virtual void UpdateRigElement(float DeltaTime) override;

	// CALLED BY THE HANDLER : Synchronizes Rig Element to the most recently fetched bone transforms
	virtual void SyncToFetchedBoneTransforms(float DeltaTime) override;


	// Math/Utilities

	// Finds rotation between two vectors
	static FRotator GetRotationBetweenVectors(const FVector& InVector_1, const FVector& InVector_2, bool InVectorsNormalized = true);

	// Projects location vector on to a "solution plane" (plane constructed from 3 points: Limb Origin, Limb Target and Pole Target)
	static FVector ProjectLocationOnToSolutionPlane(const FVector& InLocation, const FVector& InLimbOrigin, const FVector& InLimbTarget, const FVector& InPoleTarget);


// DEBUGGING
public:

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|Elements|Limb|Debug")
	FVector DEBUG_GetPoleTargetLocation(int32 InPoleTargetID) { return CalculatePoleTargetLocation(PoleTargets[InPoleTargetID]); }

};	
