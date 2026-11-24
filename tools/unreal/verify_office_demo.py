"""오피스 데모 레벨 검증 (UE 5.9 에디터 Python 전용).

현재 열린 레벨의 실제 액터를 읽어 계약을 확인하고, 결과를
Document/50-worklog/level-office-demo/inventory.json 에 UTF-8 JSON으로 남긴다.
실패하면 메시지를 출력하고 비0으로 종료한다.

검증은 레벨에서 읽은 값만 사용한다(빌더의 레이아웃 데이터를 재사용하지 않는다).
"""

# pyright: reportAttributeAccessIssue=false
# unreal 모듈은 에디터 안에서만 존재하는 런타임 모듈이라 정적 타입 스텁이 없다. 위 규칙만 끈다.

import json
import os

import unreal

GENERATED_TAG = 'Generated:OfficeDemo'
ROOM_NAMES = ['Restroom', 'Lobby', 'Pantry', 'Corridor', 'OfficeA', 'OfficeB',
              'CoreHall', 'ServerRoom', 'MeetingRoom']
DIMS = ('A', 'B', 'C')
BASIC_SHAPES = '/Engine/BasicShapes/'
CLEARANCE_RADIUS = 100.0
CEILING_MAX_THICKNESS = 25.0
CEILING_MIN_AREA = 400.0 * 400.0
CEILING_MIN_Z = 150.0
FOOTPRINT_LIMIT = 4000.0  # 40x40m 상한(목표 기준). 전체 폭을 cm로 잰다.


def _tags(actor):
    return {str(tag) for tag in actor.tags}


def _is_mesh(actor):
    return isinstance(actor, unreal.StaticMeshActor)


def _hits_circle(px, py, actor, radius):
    """액터 XY 바운드가 (px, py) 중심 반경 radius 원과 겹치는지."""
    origin, extent = actor.get_actor_bounds(False)
    dx = max(abs(px - origin.x) - extent.x, 0.0)
    dy = max(abs(py - origin.y) - extent.y, 0.0)
    return dx * dx + dy * dy < radius * radius


def _mesh_path(actor):
    mesh = actor.static_mesh_component.get_editor_property('static_mesh')
    return mesh.get_path_name() if mesh else ''


def _inventory_path():
    # 프로젝트 디렉터리는 <저장소>/Client/ 이므로 그 부모가 저장소 루트다.
    repo_root = os.path.dirname(os.path.normpath(unreal.Paths.project_dir()))
    return os.path.join(repo_root, 'Document', '50-worklog', 'level-office-demo', 'inventory.json')


def collect():
    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    tagged = [(actor, _tags(actor)) for actor in actors]
    meshes = [(actor, tags) for actor, tags in tagged if _is_mesh(actor)]

    rooms_found = [name for name in ROOM_NAMES if any('Room:' + name in tags for _, tags in tagged)]

    # 2) 천장 판: 두께 25cm 이하 + XY 면적 400x400 이상 + 액터 Z 중심 150 초과
    ceiling_count = 0
    for actor, _ in meshes:
        origin, extent = actor.get_actor_bounds(False)
        if (2.0 * extent.z) <= CEILING_MAX_THICKNESS \
                and (2.0 * extent.x) * (2.0 * extent.y) >= CEILING_MIN_AREA \
                and origin.z > CEILING_MIN_Z:
            ceiling_count += 1

    # 7) footprint는 실제 지오메트리(충돌 컴포넌트) 기준으로 잰다.
    #    조명의 감쇠 구체(attenuation radius)는 레벨 크기가 아니다. 원점 기준 절반값이 아니라 전체 폭을 잰다.
    min_x = min_y = float('inf')
    max_x = max_y = float('-inf')
    for actor in actors:
        origin, extent = actor.get_actor_bounds(True)
        min_x = min(min_x, origin.x - extent.x)
        max_x = max(max_x, origin.x + extent.x)
        min_y = min(min_y, origin.y - extent.y)
        max_y = max(max_y, origin.y + extent.y)
    footprint_x = max_x - min_x
    footprint_y = max_y - min_y

    dim_counts = {dim: sum(1 for _, tags in tagged if 'Dim:' + dim in tags) for dim in DIMS}
    center_counts = {dim: sum(1 for _, tags in tagged if 'DimGroup:Center01' in tags and 'Dim:' + dim in tags)
                     for dim in DIMS}

    player_starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    # 바닥(Room:*)은 걸어 다니는 면이므로 통행 장애물로 보지 않는다.
    obstacles = [(actor, tags) for actor, tags in meshes
                 if not any(tag.startswith('Room:') for tag in tags)]
    start = player_starts[0].get_actor_location() if player_starts else None
    clearances = {}
    for dim in DIMS:
        # 이 차원에서 존재하는 장애물 = 차원 태그가 없는 공통 액터 + 자기 차원 액터
        present = [actor for actor, tags in obstacles
                   if not any(tag.startswith('Dim:') and tag != 'Dim:' + dim for tag in tags)]
        clearances[dim] = start is not None and all(
            not _hits_circle(start.x, start.y, actor, CLEARANCE_RADIUS) for actor in present)

    mesh_paths = sorted({_mesh_path(actor) for actor, _ in meshes})
    mesh_path_violations = sum(1 for path in mesh_paths if not path.startswith(BASIC_SHAPES))
    map_dir = os.path.join(unreal.Paths.project_content_dir(), 'Maps', 'Lvl_OfficeDemo')
    generated_actor_count = sum(1 for _, tags in tagged if GENERATED_TAG in tags)

    footprint_m = [round(footprint_x / 100.0, 2), round(footprint_y / 100.0, 2)]

    checks = {
        'rooms_ok': len(rooms_found) == len(ROOM_NAMES),
        'ceiling_count': ceiling_count,
        'dim_counts': dim_counts,
        'center_group_counts': center_counts,
        'player_start_count': len(player_starts),
        'player_start_clearances': clearances,
        'mesh_path_violations': mesh_path_violations,
        'footprint_cm': {'x': int(round(footprint_x)), 'y': int(round(footprint_y))},
        'footprint': footprint_m,
        'is_partitioned': os.path.isdir(map_dir),
        'generated_actor_count': generated_actor_count,
    }

    failed = []
    if not checks['rooms_ok']:
        missing = ','.join(name for name in ROOM_NAMES if name not in rooms_found)
        failed.append('rooms: ' + missing)
    if ceiling_count:
        failed.append(f'ceiling_count={ceiling_count}')
    for dim in DIMS:
        if dim_counts[dim] < 1:
            failed.append(f'dim_count_{dim}={dim_counts[dim]}')
        if center_counts[dim] != 1:
            failed.append(f'center_group_{dim}={center_counts[dim]}')
        if not clearances[dim]:
            failed.append(f'player_start_clearance_{dim}')
    if len(player_starts) != 1:
        failed.append(f'player_start_count={len(player_starts)}')
    if mesh_path_violations:
        failed.append(f'mesh_path_violations={mesh_path_violations}')
    if footprint_x > FOOTPRINT_LIMIT or footprint_y > FOOTPRINT_LIMIT:
        failed.append(f'footprint={int(footprint_x)}x{int(footprint_y)}')
    if checks['is_partitioned']:
        failed.append('is_partitioned=True')
    if generated_actor_count <= 0:
        failed.append('generated_actor_count=0')

    return {
        # 계약이 직접 읽는 평면 키(검증 명령이 이 이름을 쓴다).
        'ceiling_count': ceiling_count,
        'player_start_count': len(player_starts),
        'footprint': footprint_m,
        'is_partitioned': checks['is_partitioned'],
        'mesh_paths': mesh_paths,
        'dim_counts': dim_counts,
        'center_group_counts': center_counts,
        'rooms': ROOM_NAMES,
        'checks': checks,
        'ok': not failed,
        'failed': failed,
    }


def main():
    data = collect()
    path = _inventory_path()
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as handle:
            json.dump(data, handle, ensure_ascii=False, indent=2)
    except OSError as error:
        # 인벤토리를 남기지 못하면 검증 자체가 무효이므로 조용히 넘기지 않고 실패한다.
        print(f'[OfficeDemo] inventory 기록 실패: {error}')
        raise SystemExit(1) from None
    print('[OfficeDemo] inventory: ' + path)

    try:
        assert data['ok'], '검증 실패: ' + ', '.join(data['failed'])
    except AssertionError as error:
        print(error)
        raise SystemExit(1) from None

    checks = data['checks']
    footprint = checks['footprint_cm']
    print(f"[OfficeDemo] 검증 통과: 방 {len(data['rooms'])}개, 생성 액터 {checks['generated_actor_count']}개, "
          f"천장 {checks['ceiling_count']}개, footprint {footprint['x']}x{footprint['y']}cm, "
          f"파티션 {checks['is_partitioned']}")

if __name__ == '__main__':
    main()
