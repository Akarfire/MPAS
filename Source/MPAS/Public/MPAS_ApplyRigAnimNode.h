#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "MPAS_ApplyRigAnimNode.generated.h"

UENUM(BlueprintType)
enum EMPAS_RigApplicationMode : int
{
    /** Specify a single bone, that is the root of a bone subtree, which the rig will be applied to */
    RootBone UMETA(DisplayName = "Root Bone"),

    /** Specify the list of bones, the rig will be applied to */
    SpecificBones UMETA(DisplayName = "Specific Bones"),

};

USTRUCT(BlueprintType)
struct MPAS_API FMPAS_ApplyRigAnimNode : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
    //FPoseLink InPose;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links, meta=(PinShownByDefault))
    TWeakObjectPtr<class UMPAS_Handler> InHandler = nullptr;

    UPROPERTY(EditAnywhere, Category = "Settings")
    TEnumAsByte<EMPAS_RigApplicationMode> Mode = EMPAS_RigApplicationMode::RootBone;

    UPROPERTY(EditAnywhere, Category = "Settings")
    FBoneReference RootBone;

    UPROPERTY(EditAnywhere, Category = "Settings")
    TArray<FBoneReference> SpecificBoneNames;


    /** Whether and how to modify the translation of this bone. */
    UPROPERTY(EditAnywhere, Category = Translation)
    TEnumAsByte<EBoneModificationMode> TranslationMode = EBoneModificationMode::BMM_Replace;

    /** Whether and how to modify the translation of this bone. */
    UPROPERTY(EditAnywhere, Category = Rotation)
    TEnumAsByte<EBoneModificationMode> RotationMode = EBoneModificationMode::BMM_Replace;

    /** Whether and how to modify the translation of this bone. */
    UPROPERTY(EditAnywhere, Category = Scale)
    TEnumAsByte<EBoneModificationMode> ScaleMode = EBoneModificationMode::BMM_Additive;

    /** Reference frame to apply Translation in. */
    UPROPERTY(EditAnywhere, Category = Translation)
    TEnumAsByte<enum EBoneControlSpace> TranslationSpace = EBoneControlSpace::BCS_WorldSpace;

    /** Reference frame to apply Rotation in. */
    UPROPERTY(EditAnywhere, Category = Rotation)
    TEnumAsByte<enum EBoneControlSpace> RotationSpace = EBoneControlSpace::BCS_WorldSpace;

    /** Reference frame to apply Scale in. */
    UPROPERTY(EditAnywhere, Category = Scale)
    TEnumAsByte<enum EBoneControlSpace> ScaleSpace = EBoneControlSpace::BCS_BoneSpace;


    // FAnimNode_Base interface
    virtual void GatherDebugData(FNodeDebugData& DebugData) override;
    // End of FAnimNode_Base interface

    // FAnimNode_SkeletalControlBase interface
    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
    virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
    // End of FAnimNode_SkeletalControlBase interface

    FMPAS_ApplyRigAnimNode();

protected:
    UPROPERTY(Transient)
    TArray<FBoneReference> CachedBones;

protected:
    void RecursiveGatherSubtree(TArray<FBoneReference>& OutSubtree, FBoneReference CurrentRoot, const FBoneContainer& RequiredBones);
};


UCLASS()
class MPAS_API UMPAS_ApplyRigAnimGraphNode : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, Category = "Settings")
    FMPAS_ApplyRigAnimNode Node;

    //~ Begin UEdGraphNode Interface.
    virtual FLinearColor GetNodeTitleColor() const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetControllerDescription() const override;
    //~ End UEdGraphNode Interface.

    //~ Begin UAnimGraphNode_Base Interface
    virtual FString GetNodeCategory() const override;
    //~ End UAnimGraphNode_Base Interface

    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; };

    UMPAS_ApplyRigAnimGraphNode(const FObjectInitializer& ObjectInitializer);
};