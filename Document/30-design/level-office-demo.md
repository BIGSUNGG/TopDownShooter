# 레벨 명세: 오피스 데모 (Lvl_OfficeDemo)

기술 데모용 탑다운 실내 레벨. 차원 시스템이 레벨 위에서 어떻게 동작하는지 확인하는 것이 목적이며, 완성도 높은 아트 레벨이 아니다. 실제 게임 레벨로 교체될 때 이 문서는 규약과 검증 방법의 기준으로 남는다.

- 맵 경로: `/Game/Maps/Lvl_OfficeDemo` (World Partition 꺼짐, 액터가 .umap에 내장)
- 실측 크기: 36.2 × 36.2 m, 단일 층, 천장 없음
- 생성 액터 88개 · 공간 9개 · 문 12개 · 차원 전용 통로 3개

## 공간 구성

좌표는 cm. 벽 두께 20, 벽 높이 300.

| 공간 | X 범위 | Y 범위 | 비고 |
| --- | --- | --- | --- |
| Restroom (화장실) | -1800 ~ -600 | -1800 ~ -600 | 문 2 |
| Lobby (로비) | -1800 ~ -600 | -600 ~ 600 | 문 1, 문 9(정문, 서쪽 외곽) |
| Pantry (탕비실) | -1800 ~ -600 | 600 ~ 1800 | 문 3 |
| Corridor (메인 복도) | -600 ~ -200 | -1800 ~ 1800 | 폭 400 (캡슐 지름 68의 약 5.8배) |
| OfficeA (오피스 A) | -200 ~ 800 | -1800 ~ -600 | 문 5, 차원 통로 P1·P3 |
| OfficeB (오피스 B) | 800 ~ 1800 | -1800 ~ -600 | 문 6, 차원 통로 P3 |
| CoreHall (코어 홀) | -200 ~ 800 | -600 ~ 900 | 중심 오브젝트, 문 4·7, 차원 통로 P1·P2 |
| ServerRoom (서버실) | 800 ~ 1800 | -600 ~ 900 | 문 6·8 |
| MeetingRoom (회의실) | -200 ~ 1800 | 900 ~ 1800 | 문 7·8, 회의 테이블 |

문 번호(문 1~9)는 `WALLS` 개구부의 식별자이며 개구부 개수가 아니다. 개구부는 모두 폭 200(≥1.2m), 복도 폭 400(≥2.4m)로 기본 캡슐(r 34, h 88)이 지나간다.
- PlayerStart는 복도 중앙 `(-400, 0, 90)`에 1개. 세 차원 모두 반경 1.0m 안에 장애물이 없다.

## 차원 태그 규약

액터 태그만으로 현재 차원의 보임·콜리전을 계산한다. 별도 데이터 애셋이나 레벨별 코드가 필요 없다.

| 태그 | 의미 |
| --- | --- |
| `Dim:A` / `Dim:B` / `Dim:C` | 해당 차원에서만 보이고 콜리전이 켜진다 |
| (태그 없음) | 모든 차원에 공통. `Generated:OfficeDemo`·`Nav` 같은 운영 태그만 붙은 액터도 공통이다 |
| `DimGroup:Center01` | 중심 오브젝트 그룹. 차원별로 다른 형태가 같은 자리에 겹쳐 있다 |

- 중심 오브젝트는 `DimGroup:Center01` + `Dim:<차원>`을 함께 갖는다. A는 납작한 큐브(400×200×100), B는 기둥형 큐브(200×200×280), C는 원뿔(280×280×280)로 같은 XY(300, 150)에 놓인다.
- 차원 전용 통로 3개: P1(OfficeA ↔ CoreHall, `y=-600`의 `x=300..500`, 차원 A에서만 열림), P2(복도 ↔ CoreHall, `x=-200`의 `y=500..700`, B에서만), P3(OfficeA ↔ OfficeB, `x=800`의 `y=-1400..-1200`, C에서만). 구현은 "열린 차원에서는 보이지 않는 문막이"이며, 문막이가 `Dim:` 태그로 차원에 묶여 있다.
- 차원 전용 엄폐물 8개(차원 A 큐브 3, B 큐브 3, C 실린더 2).
- 차원 반응 메시는 `can_ever_affect_navigation=False`로 정적 네비게이션에 영향을 주지 않는다. 네비게시는 한 번만 베이크한다.

## 재질과 조명

- `M_Blockout` 1개(BaseColor·Roughness 파라미터, Roughness 0.7) + 단색 인스턴스 4개: `MI_Floor`(0.34 회색), `MI_Wall`(0.74 밝은 회색), `MI_Cover`(진한 파랑), `MI_Dim`(주황).
- 지오메트리는 `/Engine/BasicShapes/`의 Cube·Cylinder·Cone·Plane만 쓴다.
- 조명: DirectionalLight 6 lux(STATIONARY, pitch -60), SkyLight 2, 방마다 PointLight 1개(300 cd, 감쇠 1400).
- 노출은 unbound PostProcessVolume에서 EV100 2.5로 고정한다. 자동 노출 적응이 켜져 있으면 같은 카메라에서도 차원별 밝기가 달라져 스크린샷 비교가 흔들린다. 초기 값(PointLight 2000 cd, 노출 미고정)은 실내를 하얗게 날려 레벨을 읽을 수 없었다.

## 카메라와 입력

- `ADemoPawn`은 캡슐(r 34, h 88) + SpringArm(2000, pitch -75) + Camera + FloatingPawnMovement(최대 400). 좌표계는 월드 축 고정이며 회전은 쓰지 않는다.
- WASD는 `BindKey`의 눌림/뗌 이벤트로 상태를 모아 Tick에서 `AddMovementInput`을 호출한다. 디지털 키는 `BindAxisKey`가 아니며(엔진이 `AxisKey.IsAxis1D()` ensure를 발생시킨다) 이 경로로 바꿨다.
- Q/E는 2초 홀드로 이전/다음 차원. 구현은 `ADemoPlayerController`의 타이머이고, 홀드를 일찍 놓으면 취소된다.

## 재생성과 검증

두 스크립트 모두 에디터의 Python에서 실행한다(파이썬 콘솔 또는 MCP `PythonScriptingTools.execute_python`). 엔진 API를 쓰는 에디터 스크립트지 배치 프로그램이 아니다.

| 스크립트 | 동작 |
| --- | --- |
| `tools/unreal/build_office_demo.py` | 저장된 맵을 열고(미저장 편집은 버림) `Generated:OfficeDemo` 액터를 지운 뒤 재질·지오메트리·조명·PlayerStart·네비게이션을 다시 만든다. 여러 번 실행해도 결과가 같다 |
| `tools/unreal/verify_office_demo.py` | 레벨을 검사해 `Document/50-worklog/level-office-demo/inventory.json`을 쓰고, 실패하면 0이 아닌 코드로 끝난다 |

검증 항목: 공간 9개, 천장 0개, 차원 A/B/C 각 1개 이상, `DimGroup:Center01` 3형태, PlayerStart 1개와 세 차원 클리어런스, 메시 경로가 모두 `/Engine/BasicShapes/`, footprint ≤ 40 × 40 m, non-partitioned.

### 이 레벨을 다시 만들 때 걸리는 지점

- C++를 바꿨으면 에디터를 닫고 빌드한 뒤 다시 열어야 한다. 열린 채로 빌드하면 DLL이 잠겨 링크가 실패한다.
- 에디터를 강제 종료했으면 다음 실행에서 "패키지 복원" 모달이 뜨고, 그것이 떠 있는 동안 MCP의 모든 에디터 도구가 "slow task" 오류로 거부된다. 복원할 필요가 없으면 ESC로 닿고 천천히 다시 호출하면 된다.
- PIE 중에는 `get_all_level_actors()`가 빈 배열을 돌려준다. 레벨을 조사하려면 PIE를 멈추고 호출해야 한다.

## 스크린샷

PIE에서 같은 카메라로 찍은 차원별 화면. 차원 전환은 Q/E 2.4초 홀드로 실제 실행했고 전환 로그는 [전환 기록](../50-worklog/level-office-demo/pie-switching.log)에 있다. 같은 세션에서 0.6초 홀드를 놓으면 전환이 취소되고 2.4초 홀드에서만 전환되는 것까지 확인했다.

| 차원 | 화면 | 보이는 차원 전용 액터 |
| --- | --- | --- |
| A | [dim-a.png](../50-worklog/level-office-demo/dim-a.png) | 납작한 중심 큐브, 엄폐물 3, 통로 P1 열림 |
| B | [dim-b.png](../50-worklog/level-office-demo/dim-b.png) | 기둥형 중심 큐브, 엄폐물 3, 통로 P2 열림 |
| C | [dim-c.png](../50-worklog/level-office-demo/dim-c.png) | 원뿔 중심 오브젝트, 엄폐물 2, 통로 P3 열림 |

## 알려진 한계

- 전환 시 플레이어가 새 차원의 지오메트리와 겹치면 끼인다. 실패 처리·밀어내기는 이번 범위 밖이며 [ADR-0002](../20-decisions/ADR-0002-dimension-level-authoring.md)에 기록했다.
- 회전 시점, 엄폐물 파괴, 상호작용(F)은 이 레벨에 없다.
- 기본 맵(`GameDefaultMap`)은 바꾸지 않았다. 맵을 열고 PIE를 실행해야 한다.

근거 경로: `tools/unreal/build_office_demo.py`, `tools/unreal/verify_office_demo.py`, `Document/50-worklog/level-office-demo/inventory.json`, `Client/Source/Client/Dimension/DimensionSubsystem.cpp`, `Client/Source/Client/Dimension/DemoPawn.cpp`, `Client/Source/Client/Dimension/DemoPlayerController.cpp`

[상위: 게임 기획](index.md)
