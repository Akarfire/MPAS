// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StacksAndLayers.generated.h"

// ENUMS

// Blending mode for location/rotation/... layers in stacks
UENUM(BlueprintType)
enum class EMPAS_LayerBlendingMode : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply")
};

// The rule by which the elements in a single layer are combined
UENUM(BlueprintType)
enum class EMPAS_LayerCombinationMode : uint8
{
	Add UMETA(DisplayName = "Add"),
	Multiply UMETA(DisplayName = "Multiply"),
	Average UMETA(DisplayName = "Average")
};


// SOURCE INTERFACE

UINTERFACE(MinimalAPI, Blueprintable)
class USourceInterface : public UInterface
{
	GENERATED_BODY()
};

/* Actual Interface declaration. */
class ISourceInterface : public IInterface
{
	GENERATED_BODY()

public:
	// Is this source active?
	UFUNCTION(BlueprintNativeEvent, Category = "MPAS|StacksAndLayers|Source")
	bool IsSourceActive() const;
};


// CLASS TEMPLATES

// Zero value template
template<typename T>
struct TMPAS_Zero;

template<typename T, typename LHS, typename RHS>
struct TMPAS_Add
{
	static T Add(const LHS& lhs, const RHS& rhs)
	{
		return rhs + lhs;
	}
};

template<typename T, typename LHS, typename RHS>
struct TMPAS_Multiply
{
	static T Multiply(const LHS& lhs, const RHS& rhs)
	{
		return rhs * lhs;
	}
};

template<typename T, typename LHS, typename RHS>
struct TMPAS_Divide
{
	static T Divide(const LHS& lhs, const RHS& rhs)
	{
		return lhs / rhs;
	}
};

template<typename T>
struct TMPAS_Interpolate
{
	static T Interpolate(const T& Start, const T& Finish, float Factor)
	{
		return Start + Factor * (Finish - Start);
	}
};

template<typename T>
struct FMPAS_LayerBase
{
	// Whether the layer affects the associated stack or not
	bool Enabled = true;

	// The way the layer is applied on top of underlying layers 
	EMPAS_LayerBlendingMode BlendingMode;

	// Blending factor used when applying this layer on top of underlying layers
	float BlendingFactor = 1.f;

	// The way layer elements are combined to get the resulting value
	EMPAS_LayerCombinationMode CombinationMode;

	// Layer elements with pointers to their sources, allowing for non-destructive addition and removal of elements to/from the layer
	TMap<TWeakObjectPtr<UObject>, T> LayerElements;

	// Higher priority -> higher in the stack
	int Priority = 0;

	// Usually only active elements are used in calculating the resulting value, this flag forces the use of all layer elements
	bool ForceAllElementsActive = false;

	// Name of the layer, used for debugging
	FName Name = "";


	FMPAS_LayerBase(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
		float InBlendingFactor = 1.f,
		EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
		int InPriority = 0,
		bool InForceAllElementsActive = false,
		FName InName = "") :
		BlendingMode(InBlendingMode), BlendingFactor(InBlendingFactor), CombinationMode(InLayerCombinationMode), Priority(InPriority), 
		ForceAllElementsActive(InForceAllElementsActive), Name(InName) {}


	// Calculates the resulting value from the layer elements
	T CalculateLayerValue(bool& OutHasActiveElements) const
	{
		T OutValue = TMPAS_Zero<T>::Get();
		int32 ActiveElementCount = 0;

		bool FirstElement = false;

		switch (CombinationMode)
		{

		case EMPAS_LayerCombinationMode::Average:

			for (auto& Source : LayerElements)
			{
				if (!Source.Key.IsValid()) continue;

				bool SourceActive = true;
				if (Source.Key->GetClass()->ImplementsInterface(USourceInterface::StaticClass()))
					SourceActive = ISourceInterface::Execute_IsSourceActive(Source.Key.Get());

				bool CurrentActive = ForceAllElementsActive || SourceActive;

				if (CurrentActive)
					OutValue = TMPAS_Add<T, T, T>::Add(OutValue, Source.Value);
				ActiveElementCount += CurrentActive;
			}

			if (ActiveElementCount > 0)
				OutValue = TMPAS_Divide<T, T, int32>::Divide(OutValue, ActiveElementCount);

			break;

		case EMPAS_LayerCombinationMode::Add:

			for (auto& Source : LayerElements)
			{
				if (!Source.Key.IsValid()) continue;

				bool SourceActive = true;
				if (Source.Key->GetClass()->ImplementsInterface(USourceInterface::StaticClass()))
					SourceActive = ISourceInterface::Execute_IsSourceActive(Source.Key.Get());

				bool CurrentActive = ForceAllElementsActive || SourceActive;

				if (CurrentActive)
					OutValue = TMPAS_Add<T, T, T>::Add(OutValue, Source.Value);
				ActiveElementCount += CurrentActive;
			}
			break;

		case EMPAS_LayerCombinationMode::Multiply:

			FirstElement = true;
			for (auto& Source : LayerElements)
			{
				if (!Source.Key.IsValid()) continue;

				bool SourceActive = true;
				if (Source.Key->GetClass()->ImplementsInterface(USourceInterface::StaticClass()))
					SourceActive = ISourceInterface::Execute_IsSourceActive(Source.Key.Get());

				bool CurrentActive = ForceAllElementsActive || SourceActive;

				ActiveElementCount += CurrentActive;
				if (CurrentActive)
				{
					if (FirstElement)
					{
						OutValue = Source.Value;
						FirstElement = false;
					}

					else
						OutValue = TMPAS_Multiply<T, T, T>::Multiply(OutValue, Source.Value);
				}
			}
			break;

		default:
			break;
		}

		OutHasActiveElements = ActiveElementCount > 0;
		return OutValue;
	}

	// Adds/Updates a sourced value in the layer.
	void SetSourceValue(UObject* InSource, const T& InSourceValue)
	{
		LayerElements.Add(InSource, InSourceValue);
	}

	// Removes a source value from the layer. If succeded: returns true, false - overwise
	bool RemoveSourceValue(UObject* InSource)
	{
		return LayerElements.Remove(InSource) != 0;
	}

	// Returns the value associated with the specified source, returns default value if source isn't valid
	const T& GetSourceValue(UObject* InSource) const
	{
		const T* ValueP = LayerElements.Find(InSource);
		if (ValueP)
			return *ValueP;

		return TMPAS_Zero<T>::Get();
	}
};


template<typename T>
struct FMPAS_StackBase
{
	// [LayerID] -> <Layer>
	TArray<FMPAS_LayerBase<T>> Layers;

	// [Execution Step] -> <LayerID>
	TArray<int32> StackOrder;

	// Since normal layers override everything that lies beneath them, it is logical to start compuatation from the top-most normal layer with blending factor of 1.0f
	// But that layer may not be active for any number of reasons, so we cache all of the normal layers with blending factor of 1.0f in this array, so we can check if they are active in runtime
	// Layers are identified by their stack order id and placed top to bottom (0 - the top-most such layer)
	TArray<int32> StartingLayersCache;

	// Name of the stack, mainly used for debugging
	FName StackName = "";


	FMPAS_StackBase(const FName& InName = "") : StackName(InName) {}

	// Number of layers in the stack
	int32 Num() const { return Layers.Num(); }

	// Access to the stack layers
	FMPAS_LayerBase<T>& operator[] (int32 InID) { return Layers[InID]; }


	// Adds a new layer into the stack and updates the stack order
	int32 AddLayer(FMPAS_LayerBase<T> InLayer)
	{
		// Storing the layer
		int32 ID = Layers.Add(InLayer);

		// Updating stack order
		int32 LastIndex = StackOrder.Add(ID);

		// Insertion sort for just the last element (works in O(N), N - number of elements in the stack)
		int32 i = LastIndex - 1;
		while (i >= 0 && Layers[StackOrder[i]].Priority > InLayer.Priority)
		{
			StackOrder[i + 1] = StackOrder[i];
			i--;
		}

		StackOrder[i + 1] = ID;
		RecalculateStartingLayerCache();

		return ID;
	}

	// Removes a layer from the stack
	bool RemoveLayer(int32 InLayerID)
	{
		if (!(InLayerID >= 0 && InLayerID < Num())) return false;

		Layers.RemoveAt(InLayerID);
		StackOrder.Remove(InLayerID);

		RecalculateStartingLayerCache();

		return true;
	}

	// Recalucates StartingLayersCache array, should be called whenether a new layer is added/removed or some normal layer's blending factor is changed
	void RecalculateStartingLayerCache()
	{
		StartingLayersCache.Empty(Num());

		for (int32 i = Num() - 1; i >= 0; i--)
			if (Layers[StackOrder[i]].BlendingMode == EMPAS_LayerBlendingMode::Normal && Layers[StackOrder[i]].BlendingFactor == 1.f)
				StartingLayersCache.Add(i);
	}

	// Calculates the final value of the stack based on it's layers
	T CalculateStackValue() const
	{
		T FinalValue;

		int32 StartingLayer = -1;
		for (int32 i = 0; i < StartingLayersCache.Num(); i++)
		{
			int32 StackOrderID = StartingLayersCache[i];

			const FMPAS_LayerBase<T>& Layer = Layers[StackOrder[StackOrderID]];

			if (!Layer.Enabled) continue;

			bool HasActiveElements = false;
			FinalValue = Layer.CalculateLayerValue(HasActiveElements);

			if (HasActiveElements && Layer.BlendingFactor == 1.0f)
			{
				StartingLayer = StackOrderID + 1; // +1 because we have already calculated this layer's value and stored it in FinalValue
				break;
			}
		}

		if (StartingLayer == -1) // Rare case, when there are no normal layers with blending factor of 1.0f present
		{
			FinalValue = TMPAS_Zero<T>::Get();
			StartingLayer = 0;
		}

		for (int32 LayerOrderID = StartingLayer; LayerOrderID < Num(); LayerOrderID++)
		{
			const FMPAS_LayerBase<T>& Layer = Layers[StackOrder[LayerOrderID]];

			if (!Layer.Enabled) continue;

			bool HasActiveElements = false;
			T LayerValue = Layer.CalculateLayerValue(HasActiveElements);

			if (HasActiveElements)
			{
				switch (Layer.BlendingMode)
				{
				case EMPAS_LayerBlendingMode::Normal:
					FinalValue = TMPAS_Interpolate<T>::Interpolate(FinalValue, LayerValue, Layer.BlendingFactor);
					break;

				case EMPAS_LayerBlendingMode::Add:
					FinalValue = TMPAS_Interpolate<T>::Interpolate(FinalValue, TMPAS_Add<T, T, T>::Add(FinalValue, LayerValue), Layer.BlendingFactor);
					break;

				case EMPAS_LayerBlendingMode::Multiply:
					FinalValue = TMPAS_Interpolate<T>::Interpolate(FinalValue, TMPAS_Multiply<T, T, T>::Multiply(FinalValue, LayerValue), Layer.BlendingFactor);
					break;

				default: break;
				}
			}
		}

		return FinalValue;
	}
};


// APPLIED STRUCT WRAPPERS
// Needed for blueprint exposure

// VECTORS

template<>
struct TMPAS_Zero<FVector>
{
	static const FVector& Get() { return FVector::ZeroVector; }
};

USTRUCT(BlueprintType)
struct FMPAS_VectorLayer
{
	GENERATED_USTRUCT_BODY()

	// Actual layer data
	FMPAS_LayerBase<FVector> LayerData;

	FMPAS_VectorLayer(FMPAS_LayerBase<FVector> InLayerData) : LayerData(InLayerData) {}

	FMPAS_VectorLayer(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
		float InBlendingFactor = 1.f,
		EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
		int InPriority = 0,
		bool InForceAllElementsActive = false,
		FName InName = "") :

		LayerData(InBlendingMode, InBlendingFactor, InLayerCombinationMode, InPriority, InForceAllElementsActive, InName) {}

	// Adds/Updates a sourced value in the layer.
	void SetSourceValue(UObject* InSource, const FVector& InSourceValue) { LayerData.SetSourceValue(InSource, InSourceValue); }

	// Removes a source value from the layer. If succeded: returns true, false - overwise
	bool RemoveSourceValue(UObject* InSource) { return LayerData.RemoveSourceValue(InSource); }

	// Returns the value associated with the specified source, returns default value if source isn't valid
	const FVector& GetSourceValue(UObject* InSource) const { return LayerData.GetSourceValue(InSource); }
};

USTRUCT(BlueprintType)
struct FMPAS_VectorStack
{
	GENERATED_USTRUCT_BODY()

	// Actual stack data
	FMPAS_StackBase<FVector> StackData;


	FMPAS_VectorStack(const FName& InName = "") : StackData(InName) {}

	// Adds a layer to the StackData and stores a layer wrapper at the same index
	int32 AddLayer(const FMPAS_VectorLayer& InLayer)
	{
		return StackData.AddLayer(InLayer.LayerData);
	}

	// Removes a layer from the stack
	bool RemoveLayer(int32 InLayerID)
	{
		if (!StackData.RemoveLayer(InLayerID)) return false;
		return true;
	}

	// Number of layers in the stack
	int32 Num() const { return StackData.Num(); }

	// Name of the stack
	const FName& Name() const { return StackData.StackName; }

	// Access to the stack layers
	FMPAS_LayerBase<FVector>& operator[](int32 InLayerID) { return StackData[InLayerID]; }
};


// ROTATORS

template<>
struct TMPAS_Zero<FRotator>
{
	static const FRotator& Get() { return FRotator::ZeroRotator; }
};

template<>
struct TMPAS_Multiply<FRotator, FRotator, FRotator>
{
	static FRotator Multiply(const FRotator& lhs, const FRotator& rhs)
	{
		return FRotator(rhs.Pitch * lhs.Pitch, rhs.Yaw * lhs.Yaw, rhs.Roll * lhs.Roll);
	}
};

template<>
struct TMPAS_Divide<FRotator, FRotator, int32>
{
	static FRotator Divide(const FRotator& lhs, const int32& rhs)
	{
		return FRotator(lhs.Pitch / rhs, lhs.Yaw / rhs, lhs.Roll / rhs);
	}
};

template<>
struct TMPAS_Interpolate<FRotator>
{
	static FRotator Interpolate(const FRotator& Start, const FRotator& Finish, double Factor)
	{
		return Finish; // INTERPOLATE IS DISABLED FOR ROTATORS
	}
};

USTRUCT(BlueprintType)
struct FMPAS_RotatorLayer
{
	GENERATED_USTRUCT_BODY()

	// Actual layer data
	FMPAS_LayerBase<FRotator> LayerData;

	FMPAS_RotatorLayer(FMPAS_LayerBase<FRotator> InLayerData) : LayerData(InLayerData) {}

	FMPAS_RotatorLayer(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
		float InBlendingFactor = 1.f,
		EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
		int InPriority = 0,
		bool InForceAllElementsActive = false,
		FName InName = "") :

		LayerData(InBlendingMode, InBlendingFactor, InLayerCombinationMode, InPriority, InForceAllElementsActive, InName) {
	}

	// Adds/Updates a sourced value in the layer.
	void SetSourceValue(UObject* InSource, const FRotator& InSourceValue) { LayerData.SetSourceValue(InSource, InSourceValue); }

	// Removes a source value from the layer. If succeded: returns true, false - overwise
	bool RemoveSourceValue(UObject* InSource) { return LayerData.RemoveSourceValue(InSource); }

	// Returns the value associated with the specified source, returns default value if source isn't valid
	const FRotator& GetSourceValue(UObject* InSource) const { return LayerData.GetSourceValue(InSource); }
};

USTRUCT(BlueprintType)
struct FMPAS_RotatorStack
{
	GENERATED_USTRUCT_BODY()

	// Actual stack data
	FMPAS_StackBase<FRotator> StackData;

	FMPAS_RotatorStack(const FName& InName = "") : StackData(InName) {}

	// Adds a layer to the StackData and stores a layer wrapper at the same index
	int32 AddLayer(const FMPAS_RotatorLayer& InLayer)
	{
		return StackData.AddLayer(InLayer.LayerData);
	}

	// Removes a layer from the stack
	bool RemoveLayer(int32 InLayerID)
	{
		if (!StackData.RemoveLayer(InLayerID)) return false;
		return true;
	}

	// Number of layers in the stack
	int32 Num() const { return StackData.Num(); }

	// Name of the stack
	const FName& Name() const { return StackData.StackName; }

	// Access to the stack layers
	FMPAS_LayerBase<FRotator>& operator[](int32 InLayerID) { return StackData[InLayerID]; }
};


/**
 * 
 */
UCLASS()
class MPAS_API UStacksAndLayers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// VECTORS

	// Create a stack structure
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static FMPAS_VectorStack MakeStack_Vector(const FName& InName) { return FMPAS_VectorStack(InName); }

	// Create a layer structure
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static FMPAS_VectorLayer MakeLayer_Vector(	EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
												float InBlendingFactor = 1.f,
												EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
												int InPriority = 0,
												bool InForceAllElementsActive = false,
												FName InName = "") 
	{
		return FMPAS_VectorLayer(InBlendingMode, InBlendingFactor, InLayerCombinationMode, InPriority, InForceAllElementsActive, InName);
	}


	// Calculates resulting value of the layer
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static FVector CalculateLayer_Vector(const FMPAS_VectorLayer& InLayer) { bool plug = false; return InLayer.LayerData.CalculateLayerValue(plug); }

	// Calculates resulting value of the layer
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static FVector CalculateLayerInStack_Vector(bool& OutHasActiveElements, UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].CalculateLayerValue(OutHasActiveElements); }

	// Calculate resulting value of the stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static FVector CalculateStack_Vector( FMPAS_VectorStack InStack) { return InStack.StackData.CalculateStackValue(); }


	// Registers a new layer in the given stack and returns it's ID
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static int32 AddLayer_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, const FMPAS_VectorLayer& InLayer) { return InStack.AddLayer(InLayer.LayerData); }

	// Removes a layer from the given stack, return true if successful, false - if not
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static bool RemoveLayer_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack.RemoveLayer(InLayerID); }

	// Finds the layer with the specified name exists inside the stack, returns -1 if no layer with such name exists
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|VectorStacks")
	static int GetLayerIdByName_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, const FName& InLayerName)
	{
		for (int32 i = 0; i < InStack.StackData.Layers.Num(); i++)
			if (InStack.StackData.Layers[i].Name == InLayerName)
				return i;
		return -1;
	}

	// Returns the number of layers in the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static int32 GetStackSize_Vector(const FMPAS_VectorStack& InStack) { return InStack.Num(); }

	// Returns the name of the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static const FName& GetStackName_Vector(const FMPAS_VectorStack& InStack) { return InStack.Name(); }

	// Returns the name of the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static const FName& GetLayerName_Vector(const FMPAS_VectorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack.StackData.Layers[InLayerID].Name; }


	// Sets the value of a source in the given Layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static void SetLayerElementValue_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID, UObject* InSource, FVector InSourceValue) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].SetSourceValue(InSource, InSourceValue); }

	// Removes the value of a source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static bool RemoveLayerElementValue_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID, UObject* InSource) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].RemoveSourceValue(InSource); }

	// Returns the value of a location source in the given Stack and Layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|VectorStacks")
	static const FVector& GetLayerElementValue_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID, UObject* InSource) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].GetSourceValue(InSource); }
	
	// Returns a map of all layer elements
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|VectorStacks")
	static TMap<UObject*, FVector> GetLayerElements_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID) 
	{ 
		TMap<UObject*, FVector> Output;

		for (auto& Element : InStack[InLayerID].LayerElements)
			Output.Add(Element.Key.Get(), Element.Value);

		return Output; 
	}


	// Enables/Disables the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static void SetLayerEnabled_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID, bool InNewEnabled) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].Enabled = InNewEnabled; }

	// Whenether the specified layer is enabled or not
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|VectorStacks")
	static bool GetLayerEnabled_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].Enabled; }


	// Modifies blending factor of the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static void SetLayerBlendingFactor_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID, float InNewBlendingFactor) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].BlendingFactor = InNewBlendingFactor; }

	// Returns the blending factor of the specified layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|VectorStacks")
	static float GetLayerBlendingFactor_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].BlendingFactor; }


	// Modifies blending mode of the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static void SetLayerBlendingMode_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID, EMPAS_LayerBlendingMode InNewBlendingMode) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].BlendingMode = InNewBlendingMode; }

	// Returns the blending mode of the specified layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|VectorStacks")
	static EMPAS_LayerBlendingMode GetLayerBlendingMode_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].BlendingMode; }


	// Modifies combination of the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|VectorStacks")
	static void SetLayerCombinationMode_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID, EMPAS_LayerCombinationMode InNewCombinationMode) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].CombinationMode = InNewCombinationMode; }

	// Returns the combination mode of the specified layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|VectorStacks")
	static EMPAS_LayerCombinationMode GetLayerCombinationMode_Vector(UPARAM(ref) FMPAS_VectorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].CombinationMode; }



	// ROTATORS

	// Create a stack structure
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static FMPAS_RotatorStack MakeStack_Rotator(const FName& InName) { return FMPAS_RotatorStack(InName); }

	// Create a layer structure
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static FMPAS_RotatorLayer MakeLayer_Rotator(EMPAS_LayerBlendingMode InBlendingMode = EMPAS_LayerBlendingMode::Normal,
		float InBlendingFactor = 1.f,
		EMPAS_LayerCombinationMode InLayerCombinationMode = EMPAS_LayerCombinationMode::Average,
		int InPriority = 0,
		bool InForceAllElementsActive = false,
		FName InName = "")
	{
		return FMPAS_RotatorLayer(InBlendingMode, InBlendingFactor, InLayerCombinationMode, InPriority, InForceAllElementsActive, InName);
	}

	// Calculates resulting value of the layer
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static FRotator CalculateLayer_Rotator(const FMPAS_RotatorLayer& InLayer) { bool plug = false; return InLayer.LayerData.CalculateLayerValue(plug); }

	// Calculates resulting value of the layer
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static FRotator CalculateLayerInStack_Rotator(bool& OutHasActiveElements, UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].CalculateLayerValue(OutHasActiveElements); }

	// Calculate resulting value of the stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static FRotator CalculateStack_Rotator( FMPAS_RotatorStack InStack) { return InStack.StackData.CalculateStackValue(); }


	// Registers a new layer in the given stack and returns it's ID
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static int32 AddLayer_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, const FMPAS_RotatorLayer& InLayer) { return InStack.AddLayer(InLayer.LayerData); }

	// Removes a layer from the given stack, return true if successful, false - if not
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static bool RemoveLayer_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID) { return InStack.RemoveLayer(InLayerID); }

	// Finds the layer with the specified name exists inside the stack, returns -1 if no layer with such name exists
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static int GetLayerIdByName_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, const FName& InLayerName)
	{
		for (int32 i = 0; i < InStack.StackData.Layers.Num(); i++)
			if (InStack.StackData.Layers[i].Name == InLayerName)
				return i;
		return -1;
	}


	// Returns the number of layers in the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static int32 GetStackSize_Rotator(const FMPAS_RotatorStack& InStack) { return InStack.Num(); }

	// Returns the name of the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static const FName& GetStackName_Rotator(const FMPAS_RotatorStack& InStack) { return InStack.Name(); }

	// Returns the name of the given stack
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static const FName& GetLayerName_Rotator(const FMPAS_RotatorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack.StackData.Layers[InLayerID].Name; }


	// Sets the value of a source in the given Layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static void SetLayerElementValue_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID, UObject* InSource, FRotator InSourceValue) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].SetSourceValue(InSource, InSourceValue); }

	// Removes the value of a source in the given Stack and Layer, if succeded: returns true, false - overwise
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static bool RemoveLayerElementValue_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID, UObject* InSource) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].RemoveSourceValue(InSource); }

	// Returns the value of a location source in the given Stack and Layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static const FRotator& GetLayerElementValue_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID, UObject* InSource) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].GetSourceValue(InSource); }

	// Returns a map of all layer elements
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static TMap<UObject*, FRotator> GetLayerElements_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID)
	{
		TMap<UObject*, FRotator> Output;

		for (auto& Element : InStack[InLayerID].LayerElements)
			Output.Add(Element.Key.Get(), Element.Value);

		return Output;
	}


	// Enables/Disables the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static void SetLayerEnabled_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID, bool InNewEnabled) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].Enabled = InNewEnabled; }

	// Whenther the specified layer is enabled or not
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static bool GetLayerEnabled_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].Enabled; }


	// Modifies blending factor of the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static void SetLayerBlendingFactor_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID, float InNewBlendingFactor) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].BlendingFactor = InNewBlendingFactor; }

	// Returns the blending factor of the specified layer is enabled or not
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static float GetLayerBlendingFactor_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].BlendingFactor; }


	// Modifies blending mode of the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static void SetLayerBlendingMode_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID, EMPAS_LayerBlendingMode InNewBlendingMode) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].BlendingMode = InNewBlendingMode; }

	// Returns the blending mode of the specified layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static EMPAS_LayerBlendingMode GetLayerBlendingMode_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].BlendingMode; }


	// Modifies combination of the specified layer
	UFUNCTION(BlueprintCallable, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static void SetLayerCombinationMode_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID, EMPAS_LayerCombinationMode InNewCombinationMode) { check(InLayerID >= 0 && InLayerID < InStack.Num()); InStack[InLayerID].CombinationMode = InNewCombinationMode; }

	// Returns the combination mode of the specified layer
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MPAS|StacksAndLayers|RotatorStacks")
	static EMPAS_LayerCombinationMode GetLayerCombinationMode_Rotator(UPARAM(ref) FMPAS_RotatorStack& InStack, int32 InLayerID) { check(InLayerID >= 0 && InLayerID < InStack.Num()); return InStack[InLayerID].CombinationMode; }

};
