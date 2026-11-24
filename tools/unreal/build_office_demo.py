"""오피스 데모 레벨 빌더 (UE 5.9 에디터 Python 전용).

에디터 Output Log의 Python 모드에서 exec(open(<이 파일>).read()) 또는
Tools > Execute Python Script로 실행한다. 실행 전용 스크립트이며 헤드리스에서는 동작하지 않는다.

좌표 단위는 cm, 원점은 레벨 중심, X=북(+), Y=동(+), Z=상승(+).
재실행하면 태그 'Generated:OfficeDemo' 액터만 지우고 다시 만든다(멱등).
"""

# pyright: reportAttributeAccessIssue=false
# unreal 모듈은 에디터 안에서만 존재하는 런타임 모듈이라 정적 타입 스텁이 없다. 위 규칙만 끈다.

import unreal

MAP_PATH = '/Game/Maps/Lvl_OfficeDemo'
MATERIAL_DIR = '/Game/Materials'
BASE_MATERIAL = MATERIAL_DIR + '/M_Blockout'
GENERATED_TAG = 'Generated:OfficeDemo'

WALL_THICKNESS = 20.0
WALL_HEIGHT = 300.0
BLOCKER_THICKNESS = 16.0
SHAPE_SIZE = 100.0  # 엔진 기본 도형의 기준 크기(cm)

CUBE = '/Engine/BasicShapes/Cube'
CYLINDER = '/Engine/BasicShapes/Cylinder'
CONE = '/Engine/BasicShapes/Cone'
PLANE = '/Engine/BasicShapes/Plane'

# 재질 인스턴스는 부모 M_Blockout의 BaseColor만 다르게 쓴다.
COLORS = {
    'MI_Floor': (0.34, 0.34, 0.37, 1.0),
    'MI_Wall': (0.74, 0.74, 0.77, 1.0),
    'MI_Cover': (0.05, 0.12, 0.35, 1.0),
    'MI_Dim': (0.92, 0.35, 0.05, 1.0),
}

# 방 내부 영역: 이름 -> (x0, x1, y0, y1)
ROOMS = {
    'Restroom': (-1800.0, -600.0, -1800.0, -600.0),
    'Lobby': (-1800.0, -600.0, -600.0, 600.0),
    'Pantry': (-1800.0, -600.0, 600.0, 1800.0),
    'Corridor': (-600.0, -200.0, -1800.0, 1800.0),
    'OfficeA': (-200.0, 800.0, -1800.0, -600.0),
    'OfficeB': (800.0, 1800.0, -1800.0, -600.0),
    'CoreHall': (-200.0, 800.0, -600.0, 900.0),
    'ServerRoom': (800.0, 1800.0, -600.0, 900.0),
    'MeetingRoom': (-200.0, 1800.0, 900.0, 1800.0),
}

# 벽: (축, 고정 좌표, 시작, 끝, 개구부[(a, b), ...])
# 'X'는 X가 고정된 벽(법선=X, Y축으로 뻗음), 'Y'는 Y가 고정된 벽(법선=Y, X축으로 뻗음).
WALLS = [
    ('X', -1800.0, -1800.0, 1800.0, [(-200.0, 200.0)]),                      # 외곽 서쪽 + 문9 정문
    ('X', 1800.0, -1800.0, 1800.0, []),                                      # 외곽 동쪽
    ('Y', -1800.0, -1800.0, 1800.0, []),                                     # 외곽 남쪽
    ('Y', 1800.0, -1800.0, 1800.0, []),                                      # 외곽 북쪽
    ('X', -600.0, -1800.0, 1800.0, [(-1300.0, -1100.0), (-100.0, 100.0), (1100.0, 1300.0)]),  # 문2, 문1, 문3
    ('X', -200.0, -1800.0, 900.0, [(-1300.0, -1100.0), (100.0, 300.0), (500.0, 700.0)]),      # 문5, 문4, P2
    ('X', 800.0, -1800.0, 900.0, [(-1400.0, -1200.0)]),                      # P3
    ('Y', -600.0, -1800.0, 1800.0, [(300.0, 500.0), (1200.0, 1400.0)]),      # P1, 문6
    ('Y', 600.0, -1800.0, -600.0, []),                                       # Lobby | Pantry
    ('Y', 900.0, -200.0, 1800.0, [(300.0, 500.0), (1200.0, 1400.0)]),        # 문7, 문8
]

# 차원 전용 통로를 막는 벽: (축, 고정 좌표, 개구부 시작, 끝, 이 벽을 남기는 차원 태그들)
PASSAGE_BLOCKERS = [
    ('Y', -600.0, 300.0, 500.0, ('B', 'C')),         # P1은 차원 A에서만 열린다
    ('X', -200.0, 500.0, 700.0, ('A', 'C')),         # P2는 차원 B에서만
    ('X', 800.0, -1400.0, -1200.0, ('A', 'B')),      # P3은 차원 C에서만
]

# 중심 오브젝트(모두 같은 XY=300,150에 겹쳐 배치): (차원, 메시, 크기, 위치)
CENTER_OBJECTS = [
    ('A', CUBE, (400.0, 200.0, 100.0), (300.0, 150.0, 50.0)),
    ('B', CUBE, (200.0, 200.0, 280.0), (300.0, 150.0, 140.0)),
    ('C', CONE, (280.0, 280.0, 280.0), (300.0, 150.0, 140.0)),
]

# 차원 전용 엄폐물: (차원, 메시, 크기, 위치)
COVERS = [
    ('A', CUBE, (200.0, 20.0, 150.0), (-50.0, -1500.0, 75.0)),
    ('A', CUBE, (200.0, 20.0, 150.0), (300.0, -1500.0, 75.0)),
    ('A', CUBE, (200.0, 20.0, 150.0), (-50.0, -800.0, 75.0)),
    ('B', CUBE, (220.0, 40.0, 120.0), (1100.0, -1300.0, 60.0)),
    ('B', CUBE, (220.0, 40.0, 120.0), (1500.0, -1300.0, 60.0)),
    ('B', CUBE, (220.0, 40.0, 120.0), (1300.0, -1000.0, 60.0)),
    ('C', CYLINDER, (160.0, 160.0, 180.0), (200.0, 1600.0, 90.0)),
    ('C', CYLINDER, (160.0, 160.0, 180.0), (1400.0, 1600.0, 90.0)),
]

# 방마다 PointLight 1개. CoreHall 조명은 중심 오브젝트(300,150)와 겹치지 않게 남쪽으로 비켜 둔다.
POINT_LIGHTS = {
    'Restroom': (-1200.0, -1200.0),
    'Lobby': (-1200.0, 0.0),
    'Pantry': (-1200.0, 1200.0),
    'Corridor': (-400.0, 0.0),
    'OfficeA': (300.0, -1200.0),
    'OfficeB': (1300.0, -1200.0),
    'CoreHall': (300.0, 700.0),
    'ServerRoom': (1300.0, 150.0),
    'MeetingRoom': (800.0, 1350.0),
}


def _level_editor():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def _actors_api():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _tags(actor):
    return {str(tag) for tag in actor.tags}


def _basic_shape(path):
    # 엔진 기본 도형은 모두 원점 중심 100cm 기준(Plane 100x100, 나머지 100^3)이라 크기를 100으로 나눠 스케일한다.
    return unreal.EditorAssetLibrary.load_asset(path)


def _tag(actor, label, extra_tags=()):
    actor.tags = [GENERATED_TAG] + list(extra_tags)
    if label:
        actor.set_actor_label(label)
    return actor


def _spawn(mesh, size, location, material, extra_tags=(), label='', dim=None):
    """기본 도형을 size(cm)로 스케일한 스태틱 메시 액터를 만든다."""
    actor = _actors_api().spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, 0.0))
    component = actor.static_mesh_component
    component.set_static_mesh(_basic_shape(mesh))
    component.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*[side / SHAPE_SIZE for side in size]))
    parts = list(extra_tags)
    if dim:
        parts.append('Dim:' + dim)
    _tag(actor, label, parts)
    if dim:
        # 차원 반응 액터는 내비게이션에 영향을 주지 않는다(차원 전환 시 내비 재빌드 없이 통행 변경).
        # 비트필드 프로퍼티라 Python 애트리뷰트가 없어 set_editor_property로만 설정된다.
        component.set_editor_property('can_ever_affect_navigation', False)
    return actor


def _wall_segment(axis, coord, start, end, material):
    length = end - start
    middle = (start + end) / 2.0
    if axis == 'X':
        return _spawn(CUBE, (WALL_THICKNESS, length, WALL_HEIGHT), (coord, middle, WALL_HEIGHT / 2.0),
                      material)
    return _spawn(CUBE, (length, WALL_THICKNESS, WALL_HEIGHT), (middle, coord, WALL_HEIGHT / 2.0),
                  material)


def _wall_run(axis, coord, start, end, openings, material):
    """문 개구부 목록을 기준으로 벽 한 줄을 세그먼트로 나눠 생성한다."""
    cursor = start
    for opening_start, opening_end in sorted(openings):
        if opening_start > cursor:
            _wall_segment(axis, coord, cursor, opening_start, material)
        cursor = opening_end
    if cursor < end:
        _wall_segment(axis, coord, cursor, end, material)


def _blockers(axis, coord, start, end, dims, material):
    """차원 전용 통로 개구부를 막는 벽. 법선 방향 ±8cm로 밀어 두 벽이 겹치지 않게 둔다."""
    middle = (start + end) / 2.0
    for index, dim in enumerate(dims):
        fixed = coord + (index - (len(dims) - 1) / 2.0) * BLOCKER_THICKNESS
        if axis == 'X':
            _spawn(CUBE, (BLOCKER_THICKNESS, end - start, WALL_HEIGHT), (fixed, middle, WALL_HEIGHT / 2.0),
                   material, label='Blocker_' + dim, dim=dim)
        else:
            _spawn(CUBE, (end - start, BLOCKER_THICKNESS, WALL_HEIGHT), (middle, fixed, WALL_HEIGHT / 2.0),
                   material, label='Blocker_' + dim, dim=dim)


def _furniture():
    """공통 가구: (메시, 크기, 위치, 라벨). 차원 태그는 붙이지 않는다."""
    items = [
        (CUBE, (400.0, 150.0, 110.0), (-1600.0, -350.0, 55.0), 'Furniture_Lobby_Reception'),
        (CUBE, (500.0, 180.0, 75.0), (800.0, 1350.0, 37.5), 'Furniture_MeetingRoom_Table'),
        (CUBE, (400.0, 100.0, 110.0), (-1200.0, 1700.0, 55.0), 'Furniture_Pantry_Counter'),
    ]
    for column, x in enumerate((-60.0, 300.0, 600.0)):
        for row, y in enumerate((-1700.0, -950.0)):
            items.append((CUBE, (150.0, 70.0, 75.0), (x, y, 37.5), f'Furniture_OfficeA_Desk_{column}{row}'))
    for column, x in enumerate((1000.0, 1400.0)):
        for row, y in enumerate((-1600.0, -800.0)):
            items.append((CUBE, (150.0, 70.0, 75.0), (x, y, 37.5), f'Furniture_OfficeB_Desk_{column}{row}'))
    chairs = [(650.0, 1200.0), (800.0, 1200.0), (950.0, 1200.0), (650.0, 1500.0),
              (800.0, 1500.0), (950.0, 1500.0), (500.0, 1350.0), (1100.0, 1350.0)]
    for index, (x, y) in enumerate(chairs):
        items.append((CUBE, (60.0, 60.0, 90.0), (x, y, 45.0), f'Furniture_MeetingRoom_Chair_{index}'))
    for index, x in enumerate((-1600.0, -1300.0, -1000.0)):
        items.append((CUBE, (20.0, 180.0, 200.0), (x, -800.0, 100.0), f'Furniture_Restroom_Partition_{index}'))
    for index, y in enumerate((-1500.0, 1500.0)):
        items.append((CYLINDER, (120.0, 120.0, 120.0), (-400.0, y, 60.0), f'Furniture_Corridor_Plant_{index}'))
    return items


def _instance(name, color, parent):
    path = MATERIAL_DIR + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        instance = unreal.EditorAssetLibrary.load_asset(path)
    else:
        instance = _asset_tools().create_asset(name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
                                               unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance, 'BaseColor', unreal.LinearColor(*color))
    unreal.EditorAssetLibrary.save_loaded_asset(instance)
    return instance


def _ensure_materials():
    """M_Blockout + MI 4개를 준비한다. 이미 있으면 재사용하고 BaseColor만 갱신한다."""
    if not unreal.EditorAssetLibrary.does_asset_exist(BASE_MATERIAL):
        unreal.EditorAssetLibrary.make_directory(MATERIAL_DIR)
        material = _asset_tools().create_asset('M_Blockout', MATERIAL_DIR, unreal.Material,
                                               unreal.MaterialFactoryNew())
        base_color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -400, 0)
        base_color.set_editor_property('parameter_name', 'BaseColor')
        roughness = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -400, 200)
        roughness.set_editor_property('parameter_name', 'Roughness')
        roughness.set_editor_property('default_value', 0.7)
        unreal.MaterialEditingLibrary.connect_material_property(
            base_color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(
            roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
    base = unreal.EditorAssetLibrary.load_asset(BASE_MATERIAL)
    return {name: _instance(name, color, base) for name, color in COLORS.items()}


def _build_geometry(materials):
    for name, (x0, x1, y0, y1) in ROOMS.items():
        _spawn(PLANE, (x1 - x0, y1 - y0, 1.0), ((x0 + x1) / 2.0, (y0 + y1) / 2.0, 0.0),
               materials['MI_Floor'], extra_tags=['Room:' + name], label='Room_' + name)
    for axis, coord, start, end, openings in WALLS:
        _wall_run(axis, coord, start, end, openings, materials['MI_Wall'])
    for axis, coord, start, end, dims in PASSAGE_BLOCKERS:
        _blockers(axis, coord, start, end, dims, materials['MI_Cover'])


def _build_objects(materials):
    for dim, mesh, size, location in CENTER_OBJECTS:
        _spawn(mesh, size, location, materials['MI_Dim'], extra_tags=['DimGroup:Center01'],
               label='Center01_' + dim, dim=dim)
    for dim, mesh, size, location in COVERS:
        _spawn(mesh, size, location, materials['MI_Cover'], extra_tags=['Cover'],
               label='Cover_' + dim, dim=dim)
    for mesh, size, location, label in _furniture():
        _spawn(mesh, size, location, materials['MI_Wall'], extra_tags=['Furniture'], label=label)


def _light_component(actor):
    # ALight의 공통 프로퍼티 light_component가 Directional/Sky/Point 모두에서 노출된다(5.9 확인).
    component = actor.light_component
    component.set_editor_property('mobility', unreal.ComponentMobility.STATIONARY)
    return component


def _build_lights():
    # 고정 노출(EV100 1)에서 밝기가 맞는 값이다. 2000cd는 실내를 하얗게 날려 버렸다.
    sun = _actors_api().spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 600.0),
        unreal.Rotator(pitch=-60.0, yaw=0.0, roll=0.0))
    _light_component(sun).set_editor_property('intensity', 6.0)
    _tag(sun, 'Light_Directional')

    sky = _actors_api().spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0))
    _light_component(sky).set_editor_property('intensity', 2.0)
    _tag(sky, 'Light_Sky')

    for name, (x, y) in POINT_LIGHTS.items():
        light = _actors_api().spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, 260.0))
        component = _light_component(light)
        component.set_editor_property('intensity', 300.0)
        component.set_editor_property('attenuation_radius', 1400.0)
        _tag(light, 'Light_' + name)


def _build_post_process():
    """노출을 고정해 차원 스크린샷이 자동 노출 적응에 흔들리지 않게 한다."""
    volume = _actors_api().spawn_actor_from_class(
        unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 600.0))
    _tag(volume, 'PostProcess_Exposure')
    volume.set_editor_property('unbound', True)
    settings = volume.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_min_brightness', True)
    settings.set_editor_property('auto_exposure_min_brightness', 2.5)
    settings.set_editor_property('override_auto_exposure_max_brightness', True)
    settings.set_editor_property('auto_exposure_max_brightness', 2.5)
    settings.set_editor_property('override_auto_exposure_bias', True)
    settings.set_editor_property('auto_exposure_bias', 0.0)
    volume.set_editor_property('settings', settings)


def _spawn_player_start():
    # 복도 중앙에서 시작해야 중심 오브젝트와 세 차원 통로가 한 화면에 들어온다.
    start = _actors_api().spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-400.0, 0.0, 90.0), unreal.Rotator(0.0, 0.0, 0.0))
    _tag(start, 'PlayerStart_OfficeDemo', ['Spawn:PlayerStart'])


def _spawn_nav_bounds():
    volume = _actors_api().spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0.0, 0.0, 150.0))
    _tag(volume, 'NavMeshBounds_OfficeDemo', ['Nav'])
    # 기본 브러시는 200cm 큐브(반지름 100). 실제 바운드를 읽어 3900x3900x300 상자로 맞춘다.
    _, base = volume.get_actor_bounds(False)
    base = unreal.Vector(max(base.x, 100.0), max(base.y, 100.0), max(base.z, 100.0))
    volume.set_actor_scale3d(unreal.Vector(1900.0 / base.x, 1900.0 / base.y, 150.0 / base.z))


def _demo_game_mode():
    try:
        game_mode = getattr(unreal, 'DemoGameMode', None) or unreal.load_class(
            None, '/Script/Client.DemoGameMode')
    except Exception:
        game_mode = None
    if game_mode is None:
        # 조용히 넘어가면 검증 항목이 무너지므로 빌드를 중단한다.
        raise RuntimeError('GameMode /Script/Client.DemoGameMode를 찾지 못했습니다.'
                           'ClientEditor 빌드가 끝나고 에디터가 재시작됐는지 확인하세요.')
    return game_mode


def _apply_world_settings():
    # WorldSettings는 get_all_level_actors()에 포함되지 않아 월드에서 직접 꺼낸다.
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings() if world else None
    if settings is None:
        raise RuntimeError('WorldSettings를 찾지 못했습니다.')
    settings.set_editor_property('default_game_mode', _demo_game_mode())


def _remove_generated():
    actors = _actors_api()
    removed = 0
    for actor in list(actors.get_all_level_actors()):
        if GENERATED_TAG in _tags(actor):
            actors.destroy_actor(actor)
            removed += 1
    if removed:
        print(f'[OfficeDemo] 기존 생성 액터 {removed}개 삭제')


def _report():
    generated = [actor for actor in _actors_api().get_all_level_actors() if GENERATED_TAG in _tags(actor)]
    counts = {dim: sum(1 for actor in generated if 'Dim:' + dim in _tags(actor)) for dim in ('A', 'B', 'C')}
    doors = sum(len(openings) for _, _, _, _, openings in WALLS)
    print(f'[OfficeDemo] 생성 액터 {len(generated)}개 / 방 {len(ROOMS)}개 / '
          f'차원 태그 A:{counts["A"]} B:{counts["B"]} C:{counts["C"]} / 문 {doors}개')


def main():
    level = _level_editor()
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        # 저장본에서 다시 시작해 이전 실행의 미저장 편집이 결과물을 오염시키지 않게 한다(멱등 재빌드).
        level.load_level(MAP_PATH, unreal.LevelEditorUnsavedChangesPolicy.DISCARD)
    else:
        level.new_level(MAP_PATH, False)

    _remove_generated()
    materials = _ensure_materials()
    _build_geometry(materials)
    _build_objects(materials)
    _build_lights()
    _build_post_process()
    _spawn_player_start()
    _spawn_nav_bounds()
    _apply_world_settings()
    level.save_current_level()
    _report()


if __name__ == '__main__':
    main()
