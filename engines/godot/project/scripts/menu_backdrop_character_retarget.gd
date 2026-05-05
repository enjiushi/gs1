extends Node3D

@export var animation_name: StringName = &"HumanM_Idle01"

const TARGET_TO_SOURCE_BONES := {
    "mixamorig_Hips": "B-hips",
    "mixamorig_Spine": "B-spine",
    "mixamorig_Spine1": "B-chest",
    "mixamorig_Spine2": "B-chest",
    "mixamorig_Neck": "B-neck",
    "mixamorig_Head": "B-head",
    "mixamorig_LeftShoulder": "B-shoulder.L",
    "mixamorig_LeftArm": "B-upperArm.L",
    "mixamorig_LeftForeArm": "B-forearm.L",
    "mixamorig_LeftHand": "B-hand.L",
    "mixamorig_LeftHandIndex1": "B-indexFinger01.L",
    "mixamorig_LeftHandIndex2": "B-indexFinger02.L",
    "mixamorig_LeftHandIndex3": "B-indexFinger03.L",
    "mixamorig_LeftHandIndex4": "B-indexFinger03.L",
    "mixamorig_RightShoulder": "B-shoulder.R",
    "mixamorig_RightArm": "B-upperArm.R",
    "mixamorig_RightForeArm": "B-forearm.R",
    "mixamorig_RightHand": "B-hand.R",
    "mixamorig_RightHandIndex1": "B-indexFinger01.R",
    "mixamorig_RightHandIndex2": "B-indexFinger02.R",
    "mixamorig_RightHandIndex3": "B-indexFinger03.R",
    "mixamorig_RightHandIndex4": "B-indexFinger03.R",
    "mixamorig_LeftUpLeg": "B-thigh.L",
    "mixamorig_LeftLeg": "B-shin.L",
    "mixamorig_LeftFoot": "B-foot.L",
    "mixamorig_LeftToeBase": "B-toe.L",
    "mixamorig_LeftToe_End": "B-toe.L",
    "mixamorig_RightUpLeg": "B-thigh.R",
    "mixamorig_RightLeg": "B-shin.R",
    "mixamorig_RightFoot": "B-foot.R",
    "mixamorig_RightToeBase": "B-toe.R",
    "mixamorig_RightToe_End": "B-toe.R",
}

@onready var driver_skeleton: Skeleton3D = $DriverAnimation/Skeleton3D
@onready var driver_animation_player: AnimationPlayer = $DriverAnimation/AnimationPlayer
@onready var target_skeleton: Skeleton3D = $TargetCharacter/Skeleton3D
@onready var target_animation_player: AnimationPlayer = $TargetCharacter/AnimationPlayer

var bone_pairs: Array[Vector2i] = []

func _ready() -> void:
    if target_animation_player != null:
        target_animation_player.stop()

    bone_pairs.clear()
    for target_bone_name in TARGET_TO_SOURCE_BONES.keys():
        var source_bone_name: StringName = TARGET_TO_SOURCE_BONES[target_bone_name]
        var source_bone_idx := driver_skeleton.find_bone(source_bone_name)
        var target_bone_idx := target_skeleton.find_bone(target_bone_name)
        if source_bone_idx >= 0 and target_bone_idx >= 0:
            bone_pairs.append(Vector2i(source_bone_idx, target_bone_idx))

    if driver_animation_player != null and driver_animation_player.has_animation(animation_name):
        driver_animation_player.play(animation_name)

    _apply_retarget_pose()

func _process(_delta: float) -> void:
    _apply_retarget_pose()

func _apply_retarget_pose() -> void:
    for pair in bone_pairs:
        var source_bone_idx := pair.x
        var target_bone_idx := pair.y
        target_skeleton.set_bone_pose_position(target_bone_idx, driver_skeleton.get_bone_pose_position(source_bone_idx))
        target_skeleton.set_bone_pose_rotation(target_bone_idx, driver_skeleton.get_bone_pose_rotation(source_bone_idx))
        target_skeleton.set_bone_pose_scale(target_bone_idx, driver_skeleton.get_bone_pose_scale(source_bone_idx))
