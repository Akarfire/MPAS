#include "MPAS_ApplyRigAnimNode.h"
#include "MPAS_Handler.h"
#include "Animation/AnimInstanceProxy.h"

FMPAS_ApplyRigAnimNode::FMPAS_ApplyRigAnimNode()
{
}

void FMPAS_ApplyRigAnimNode::GatherDebugData(FNodeDebugData& DebugData)
{
    FString DebugLine = DebugData.GetNodeName(this);

    DebugData.AddDebugItem(DebugLine);

    Super::GatherDebugData(DebugData);
}

void FMPAS_ApplyRigAnimNode::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
    Super::InitializeBoneReferences(RequiredBones);

    CachedBones.Reset();

	if (Mode == EMPAS_RigApplicationMode::RootBone)
	{
		FBoneReference EmptyReference = FBoneReference();
		if (RootBone == EmptyReference) return;

		CachedBones.Add(RootBone);
		RecursiveGatherSubtree(CachedBones, RootBone, RequiredBones);
		for (auto& Bone : CachedBones)
			Bone.Initialize(RequiredBones);
	}

	else if (Mode == EMPAS_RigApplicationMode::SpecificBones)
	{
		FBoneReference EmptyReference = FBoneReference();
		SpecificBoneNames.Remove(EmptyReference);

		for (int32 i = 0; i < SpecificBoneNames.Num(); i++)
		{
			SpecificBoneNames[i].Initialize(RequiredBones);
			CachedBones.Add(SpecificBoneNames[i]);
		}
	}
}

void FMPAS_ApplyRigAnimNode::RecursiveGatherSubtree(TArray<FBoneReference>& OutSubtree, FBoneReference CurrentRoot, const FBoneContainer& RequiredBones)
{
	TArray<int32> ChildBones;
	RequiredBones.GetSkeletonAsset()->GetChildBones(RequiredBones.GetSkeletonAsset()->GetReferenceSkeleton().FindBoneIndex(CurrentRoot.BoneName), ChildBones);

	for (auto& Child : ChildBones)
	{
		FBoneReference Reference = FBoneReference();
		Reference.BoneName = RequiredBones.GetSkeletonAsset()->GetReferenceSkeleton().GetBoneName(Child);

		OutSubtree.Add(Reference);
	
		RecursiveGatherSubtree(OutSubtree, Reference, RequiredBones);
	}
}

void FMPAS_ApplyRigAnimNode::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
    check(OutBoneTransforms.Num() == 0);
    if (!InHandler.IsValid()) return;

    const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
    FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	for (auto& Bone : CachedBones)
	{
		FCompactPoseBoneIndex CompactIndex = Bone.GetCompactPoseIndex(BoneContainer);

		FTransform HandlerBoneTransform;
		bool HasTransform = InHandler->GetSingleBoneTransform(HandlerBoneTransform, Bone.BoneName);

		if (!HasTransform) continue;

		FTransform NewBoneTransform = Output.Pose.GetComponentSpaceTransform(CompactIndex);

        
		if (ScaleMode != BMM_Ignore)
		{
			// Convert to Bone Space.
			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, ScaleSpace);

			if (ScaleMode == BMM_Additive)
			{
				NewBoneTransform.SetScale3D(NewBoneTransform.GetScale3D() * HandlerBoneTransform.GetScale3D());
			}
			else
			{
				NewBoneTransform.SetScale3D(HandlerBoneTransform.GetScale3D());
			}

			// Convert back to Component Space.
			FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, ScaleSpace);
		}

		if (RotationMode != BMM_Ignore)
		{
			// Convert to Bone Space.
			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, RotationSpace);

			const FQuat BoneQuat(HandlerBoneTransform.GetRotation());
			if (RotationMode == BMM_Additive)
			{
				NewBoneTransform.SetRotation(BoneQuat * NewBoneTransform.GetRotation());
			}
			else
			{
				NewBoneTransform.SetRotation(BoneQuat);
			}

			// Convert back to Component Space.
			FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, RotationSpace);
		}

		if (TranslationMode != BMM_Ignore)
		{
			// Convert to Bone Space.
			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, TranslationSpace);

			if (TranslationMode == BMM_Additive)
			{
				NewBoneTransform.AddToTranslation(HandlerBoneTransform.GetTranslation());
			}
			else
			{
				NewBoneTransform.SetTranslation(HandlerBoneTransform.GetTranslation());
			}

			// Convert back to Component Space.
			FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, TranslationSpace);
		}

		//FAnimationRuntime::FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, EBoneControlSpace::BCS_WorldSpace);

        //NewBoneTransform.SetScale3D(HandlerBoneTransform.GetScale3D());
        //NewBoneTransform.SetRotation(HandlerBoneTransform.GetRotation());
        //NewBoneTransform.SetTranslation(HandlerBoneTransform.GetTranslation());

        //FAnimationRuntime::FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTransform, CompactIndex, EBoneControlSpace::BCS_WorldSpace);

        OutBoneTransforms.Add(FBoneTransform(CompactIndex, NewBoneTransform));
    }

}

bool FMPAS_ApplyRigAnimNode::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
    return InHandler.IsValid();
}





#define LOCTEXT_NAMESPACE "A3Nodes"

UMPAS_ApplyRigAnimGraphNode::UMPAS_ApplyRigAnimGraphNode(const FObjectInitializer& ObjectInitializer)
    :Super(ObjectInitializer)
{
}

FLinearColor UMPAS_ApplyRigAnimGraphNode::GetNodeTitleColor() const
{
    return FLinearColor::Red;
}

FText UMPAS_ApplyRigAnimGraphNode::GetTooltipText() const
{
    return LOCTEXT("MPAS_ApplyRig", "MPAS_ApplyRig");
}

FText UMPAS_ApplyRigAnimGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("MPAS_ApplyRig", "MPAS_ApplyRig");
}

FText UMPAS_ApplyRigAnimGraphNode::GetControllerDescription() const
{
    return LOCTEXT("MPAS_ApplyRig", "MPAS_ApplyRig");
}

FString UMPAS_ApplyRigAnimGraphNode::GetNodeCategory() const
{
    return TEXT("MPAS_ApplyRig");
}

#undef LOCTEXT_NAMESPACE