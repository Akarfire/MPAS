#System 

### Concept

**Intention drivers** are used to dynamically control behavior and parameters of [Rig Element](../Structure/Rig%20Element.md)s based on the input from the controller and the environment. **Intention drivers** are represented by State Machines, that have access to rig's [MPAS Handler](../Structure/MPAS%20Handler.md). A single rig can have multiple **Intention Drivers** working simultaneously.

### Implementation

**Intention Drivers** are derived from `USSM_StateMachine` class, that comes with `ScarletStateMachines` plugin.

In order to grant easy access to rig's data two base classes are implemented:

```c++
class UMPAS_IntentionStateMachine : public USSM_StateMachine
{
	// Returns the pointer to the associated MPAS handler
	class UMPAS_Handler* GetHandler()
	
	// Returns the name of the State Machine in the Intention Driver
	FName GetName();
	
	// Active flag access
	void SetActive(bool NewActive);
	bool IsActive();
	
	
	// CALLED BY THE HANDLER: Called once the rig has finished it's Scanning, Initialization and Linking stages
	virtual void OnRigSetupFinished_Implementation()
	
	// CALLED BY THE HANDLER: Links the state machine to the Handler
	void LinkToHandler(class UMPAS_Handler* InHandler, FName InName);
```

```c++
class UMPAS_IntentionStateBase : public USSM_StateBase
{
	// Returns the pointer to the Handler that is associated with the State Machine
	class UMPAS_Handler* GetHandler() { return Handler; }
};
```

All **Intention Drivers** and their states MUST be derived from these two classes!

**Intentions Drivers** that associated with a certain rig have their pointers stored inside of the rig's [MPAS Handler](../Structure/MPAS%20Handler.md)

```c++
class UMPAS_Handler
{
	TMap<FName, UMPAS_IntentionStateMachine*> IntentionStateMachines;
};
```
where `FName` key represents the unique name of the **Intention Driver**, which is used for accessing it from outside the rig.

**Intention Drivers** have a special *Active flag*, which tells the rig's Handler whether the driver needs to be updated or not. Active **Intention Drivers** are updated every rig's update.

### Usage

Adding an **Intention Driver** to the rig and accessing already registered drivers is done via the following interface:

```c++
class UMPAS_Handler
{
// Adds a Intention State Machine to the Intention Driver
	bool AddIntentionStateMachine( TSubclassOf<UMPAS_IntentionStateMachine> InStateMachineClass, FName InStateMachineName);
	
	// Returns a pointer to the requested Intention Driver, returns nullptr if failed to find
	UMPAS_IntentionStateMachine* GetIntentionDriver(FName InStateMachineName);
	
	// Activates / Deactivates selected Intention State Machine
	void SetIntentionStateMachineActive(FName InStateMachineName, bool NewActive);
	
	// Whether the selected Intention State Machine is active or not
	bool IsIntentionStateMachineActive(FName InStateMachineName);
};
```

### Communicating with Rig Elements

There are two general ways of communicating with Rig Elements from inside of the Intention Driver.

#### 1. "INTENTION" Custom Parameters
Some rig elements register [Custom Parameters](Custom%20Parameters.md), that affect their behavior. Usually such parameters start with "INTENTION_". Intention Drivers can change these parameters, thus controlling Rig Elements. 

*Note: such parameters usually affect ALL elements of the same type.*

#### 2. Direct Interfacing
By accessing [MPAS Handler](../Structure/MPAS%20Handler.md)'s `RigData`, **Intention Drivers** can directly interface with individual rig elements, accessing their data and modifying parameters using that data.