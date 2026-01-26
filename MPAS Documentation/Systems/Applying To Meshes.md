#System
### Animation Graph Node
#ToDo

### Backend

Bone transforms that are applied to the skeletal mesh are buffered by [MPAS Handler](../Structure/MPAS%20Handler.md) in `BoneTransforms` field:

```c++
TMap<FName, FTransform> BoneTransforms
```
where `FName` key represents the name of the bone and `FTransform` value - the buffered bone transform, that will be applied to the specified bone.

Any Rig Element can access/modify this buffer using the following interface:

```c++
class UMPAS_Handler
{
	// Sets transform of a single bone
	void SetBoneTransform(FName InBone, FTransform InTransform);

	// Sets individual transform components of a single bone
	void SetBoneLocation(FName InBone, FVector InLocation);
	void SetBoneRotation(FName InBone, FRotator InRotation);
	void SetBoneScale(FName InBone, FVector InScale);

	// Returns data about all bone transforms
	const TMap<FName, FTransform>& GetBoneTransforms();

	// Returns data about single bone transform
	FTransform GetSingleBoneTransform(FName InBone);
}
```

*Usually, bones affected by the Rig Element are specified in Element's parameters. The way the transforms are modified depends on the specific Rig Element.*

Bone transforms are applied when the skeletal mesh animation blueprint is updated.