import unreal

def inspect_anim(path):
    anim = unreal.load_asset(path)
    if not anim:
        unreal.log_error(f"Asset not found: {path}")
        return
    unreal.log(f"=== Inspecting {path} ===")
    if isinstance(anim, unreal.AnimMontage):
        unreal.log(f"Montage Length: {anim.sequence_length}")
        for section in anim.composite_sections:
            unreal.log(f"Section: {section.section_name} at {section.time}")
        for slot in anim.slot_anim_tracks:
            unreal.log(f"Slot: {slot.slot_name}")
            for anim_track in slot.anim_track.anim_segments:
                unreal.log(f"  Anim Segment: {anim_track.anim_reference.get_path_name() if anim_track.anim_reference else 'None'}")
    elif isinstance(anim, unreal.AnimSequence):
        unreal.log(f"Sequence Length: {anim.sequence_length}, NumFrames: {anim.number_of_sampled_keys}")
        unreal.log(f"Root Motion Enabled: {anim.get_editor_property('enable_root_motion')}")
        unreal.log(f"Force Root Lock: {anim.get_editor_property('force_root_lock')}")

inspect_anim('/Game/Animation/AM/AM_Sorceress_R')
inspect_anim('/Game/Animation/AS/Sorceress_Explosion')
inspect_anim('/Game/Animation/AM/soceress_AM_Sorceress_R')
