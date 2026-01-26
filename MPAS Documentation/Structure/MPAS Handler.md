#Basic #System

A central component of any MPAS rig.
### Functionality

* Rig setup;
* Provides access to rig's data;
* Applying & syncing bone transforms;
* Intention driving;
* Handling input from a controller;
* Custom rig parameters;
* Propagation.
* Timer and Timeline handling;

### Rig Data & Setup

#### Data

Basic rig data includes:
* `UMPAS_Core* Core` - a pointer to the rig's core component;
* `TMap<FName, FMPAS_RigElementData> RigData` - contains [Rig Structure](Rig%20Structure.md) data.
* `TArray<FName> CoreElements` - lists all elements, that are directly attached to the core element.

Structural data about a single [Rig Element](Rig%20Element.md) is represented by a `FMPAS_RigElementData` structure: 

```c++
struct FMPAS_RigElementData
{
	// Name of the element in the RigData
	FName Name;

	// Pointer to the corresponding component
	class UMPAS_RigElement* RigElement;

	// Name of the element, which is a parent to this one. Contains 'Core' if the element is directly attached to the core
	FName ParentComponent;

	// List of Rig elemenets, attached to this one
	TArray<FName> ChildElements;

	// Adds a child element to the list
	void AddChildElement(const FName& InChildElementName);
};
```

#### Setup Procedure

Rig setup procedure is started on `BeginPlay` and consists of 4 stages:

1. *Scanning*;
2. *Initialization*;
3. *Linking*;
4. *Post-Linking*;

When all 4 stages are completed, `SetupComplete` flag is set to true and `OnRigSetupComplete` is called (calls `OnRigSetupFinished` on every existing [Intention Driver](../Systems/Intention%20Driving.md)).

##### 1. *Scanning* (`ScanRig`)
```c++
void ScanRig();
```

1. Locates rig's core component.
2. Recursively locates rig elements, starting from direct children of the core component.

```c++
void ScanElement(class UMPAS_RigElement* RigElement, const FName& ParentElementName);
``` 

`ScanElement` obtains information about a single rig element and stores it inside of `RigData` field. `ScanElement` is recursively called on all direct children of the processed element. 

If the processed element is a `PositionDriver`, it is additionally stored in `Map<FName, class UMPAS_PositionDriver*> PositionDrivers`.

##### 2. *Initialization* (`InitRig`)
```c++
void InitRig();
```

For each rig element stored in `RigData`:
1. Calls `InitRigElement(this)`;
2. Detaches the element from the root component.

Initialization stage is meant for:
* Inner setup of the rig elements (registering vector and rotation stacks/layers);
* Integration into the rig (creation of timers, timelines, custom parameters, which can be used for indirect communication with other rig elements);

**During initialization rig elements MUST NOT access information about other elements in the rig.**

##### 3. *Linking* (`LinkRig`)
```c++
void LinkRig();
```

For each rig element stored in `RigData` calls `LinkRigElement(this)`.

During linking stage rig elements become aware of other elements in the rig. This stage is meant for inter-element integration (registering vector/rotation layers in other rig elements, etc.).

##### 4. *Post-Linking* (`PostLinkSetupRig`)
For each rig element stored in `RigData` calls `PostLinkSetupRig(this)`. This stage is meant for logic, that must happen at the end of rig setup process.

### Rig Update

Every component tick MPAS Handler performs the *Update* procedure, that includes multiple stages:
#### 1. *Rig Update*
Loops over all Rig Elements and calls `UpdateRigElement` on them.

#### 2. *Intention Driver Update*
Loops over all [Intention Drivers](../Systems/Intention%20Driving.md) and calls `UpdateStateMachine` on them if they are *Active*.

### Bone Transforms
[Applying To Meshes](../Systems/Applying%20To%20Meshes.md)
[Hybrid Animation](../Systems/Hybrid%20Animation.md)

### Intention Driving
[Intention Driving](../Systems/Intention%20Driving.md)

### Position Drivers
[Position Driving](../Systems/Position%20Driving.md)

Handler provides a `GetPositionDrivers()` method for accessing all position drivers in the rig (which are stored in `PositionDrivers` field).

```c++
// <Driver Name, Pointer>
TMap<FName, class UMPAS_PositionDriver*> PositionDrivers;
```

```c++
const TMap<FName, class UMPAS_PositionDriver*>& GetPositionDrivers() { return PositionDrivers; }
```

### Input

Handler provides an interface for handling input values. These values are then provided to Intention Drivers.

#### Movement Input

Vector input value, that determines desired movement direction.

```c++
FVector MovementInputDirection;
```

```c++
void SetMovementInputDirection(FVector InMovementInputDirection);
FVector GetMovementInputDirection();
```

#### Rotation Input

Rotator input value, that determines desired rotation of the core/rig.

```c++
FRotator InputTargetRotation;
```

```c++
void SetInputTargetRotation(FRotator InInputTargetRotation);
FRotator GetInputTargetRotation();
```


### Custom Parameters
[Custom Parameters](../Systems/Custom%20Parameters.md)

### Other
#### Propagation

*Pick an element in the rig, then pick it's adjacent elements, then their adjacent elements and so on, until you reach the propagation depth.*

Propagation settings are determined by a `FMPAS_PropagationSettings` structure

```c++
struct FMPAS_PropagationSettings
{
	// Propagation depth, if set to 0 the propagation will be unlimited (will cover all connected elements)
	int32 Depth = 0;

	// Should an element propogate to it's children
	bool PropogateToChildren = true;

	// Should an element propogate to it's parent
	bool PropogateToParent = true;

	// Enables/Disables a filter, based on rig element's component tags
	bool EnableTagFilter = false;

	// If true, the filter will switch from a White-List to a Black-List mode
	bool FilterBlackListMode = false;

	// Rig Element component tags to filter
	TArray<FName> TagFilter;
};
```

To use propagation use `PropogateFromElement` method.

```c++
void PropogateFromElement(TArray<FName>& OutPropagation, FName InStartingElement, FMPAS_PropagationSettings InPropagationSettings);
```

`OutPropagation` array will contain all processed elements (including the starting one) in order of processing


#### Timers & Timelines plugin integration

Handler contains an `USTT_TimerController*` pointer (from `Scarlet_TimerAndTimelines` plugin) - `TimerController`.

On `BeginPlay` handler makes an attempt to find an existing `USTT_TimerController` component on the owning actor. If none is found a new `USTT_TimerController` component is created.