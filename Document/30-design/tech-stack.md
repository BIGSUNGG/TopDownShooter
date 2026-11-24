# 기술 스택

| 영역 | 선택 | 맡는 일 |
| --- | --- | --- |
| 엔진 | Unreal Engine 5.9 | 프로젝트 기반 |
| 게임플레이 | GAS Framework | 캐릭터 능력 구현 |
| 애니메이션 | Motion Matching | 캐릭터 애니메이션 구현 |
| 디버그 UI | Dear ImGui | 에디터에서 디버그 정보 표시 |
| 에디터 자동화 | Unreal Engine MCP | 에이전트가 에디터를 조작 |

- 엔진 버전 5.9는 저장소에서 확인된다. 나머지 네 항목은 확정된 계획이며 아직 프로젝트에 추가되지 않았다.
- GAS, Motion Matching, Dear ImGui를 쓰려면 플러그인·모듈 활성화와 빌드 설정 변경이 필요하다. 추가 시점과 방식은 구현 단계에서 정한다.

근거 경로: `Client/Client.uproject`

[상위: 게임 기획](index.md)
