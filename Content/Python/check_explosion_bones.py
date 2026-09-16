import unreal

def check():
    path = '/Game/Animation/AS/Sorceress_Explosion'
    anim = unreal.load_asset(path)
    if not anim:
        unreal.log_error(f"Cannot load {path}")
        return

    unreal.log(f"--- Checking {path} ---")
    unreal.log(f"Length: {anim.sequence_length}")
    unreal.log(f"Enable Root Motion: {anim.get_editor_property('enable_root_motion')}")

    # Check RTG_Wizard settings
    rtg = unreal.load_asset('/Game/Characters/Sorceress/RTG_Wizard')
    if rtg:
        unreal.log(f"--- Checking RTG_Wizard ---")

check()
