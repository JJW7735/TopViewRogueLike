import unreal

def run():
    asset_path = '/Game/Characters/Sorceress/IK_Wizard'
    ik_rig = unreal.load_asset(asset_path)
    if not ik_rig:
        unreal.log_error(f"IK_Wizard를 찾을 수 없습니다: {asset_path}")
        return

    controller = unreal.IKRigController.get_controller(ik_rig)

    # 1. 언리얼 5 내장 템플릿 자동 분석 시도 (Mixamo 뼈대 자동 감지)
    applied = controller.apply_auto_generated_retarget_definition()
    
    # 2. 기존 체인 목록 확인
    existing_chains = [c.chain_name for c in controller.get_retarget_chains()]
    unreal.log(f">> 현재 등록된 체인들: {existing_chains}")

    # 3. 필수 체인이 누락되어 있다면 추가
    controller.set_retarget_root('Hips')

    chains = [
        ('Spine', 'Spine', 'Spine2'),
        ('Head', 'Neck', 'Head'),
        ('LeftArm', 'LeftShoulder', 'LeftHand'),
        ('RightArm', 'RightShoulder', 'RightHand'),
        ('LeftLeg', 'LeftUpLeg', 'LeftFoot'),
        ('RightLeg', 'RightUpLeg', 'RightFoot')
    ]

    for chain_name, start_bone, end_bone in chains:
        if chain_name not in existing_chains:
            controller.add_retarget_chain(chain_name, start_bone, end_bone, '')
            unreal.log(f">> 본 체인 추가 성공: {chain_name} ({start_bone} ~ {end_bone})")

    # 4. 에셋 저장
    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log("🎉 IK_Wizard 뼈대 체인 설정이 100% 완료되었습니다!")

run()
