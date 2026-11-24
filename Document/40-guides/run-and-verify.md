# 실행 및 검증

## Unreal 프로젝트 열기

1. `Client/Client.uproject`의 `EngineAssociation`에 맞는 Unreal Engine 5.9가 있는지 확인한다. C++ 컴파일이 필요하면 Windows용 Visual Studio 구성은 `Client/.vsconfig`를 참고한다.
2. Unreal Editor에서 `Client/Client.uproject`를 연다. 저장소 루트가 아니라 이 파일이 Unreal 프로젝트 진입점이다.
3. 런타임 기능을 검증할 때는 변경한 기능의 빌드와 에디터 실행 결과를 직접 확인한다. 설정된 게임 기본 맵은 `/Engine/Maps/Templates/OpenWorld`다.

위 절차는 프로젝트 파일을 근거로 정리했다. 이 문서 작업 범위에서는 Unreal 빌드·에디터 실행·게임 동작을 검증하지 않는다.

근거 경로: `Client/Client.uproject`, `Client/.vsconfig`, `Client/Config/DefaultEngine.ini`, `Client/Source/ClientEditor.Target.cs`

## Pi에서 Unreal MCP 연결

1. Unreal Editor에서 MCP 서버를 9090 포트로 켠다. streamable HTTP 엔드포인트는 `http://127.0.0.1:9090/mcp` 하나뿐이다. `/sse`·`/health` 같은 다른 경로는 Unreal 자체 HTTP 서버의 404(`errors.com.epicgames.httpserver.route_handler_not_found`)를 돌려준다.
2. 서버 정의는 프로젝트 루트 `.mcp.json`에 있다(`unreal-mcp`, `exposure: "direct"`). 프로젝트 파일이라 신뢰된 프로젝트에서만 읽히고, 정의가 바뀌면 최초 연결 시 승인을 다시 묻는다.
3. 설정은 로드 시점에 읽힌다. 파일을 고친 뒤에는 `/reload` 또는 새 세션에서 반영된다.
4. 이 서버는 도구 4개(`list_toolsets`, `describe_toolsets`, `describe_tools`, `call_tool`)로 엔진 툴셋을 노출한다. 호출은 `mcp` 게이트웨이(`unreal-mcp_<도구 이름>`)나 `mcp__unreal_mcp` 네임스페이스 프록시로 한다. `call_tool`은 열려 있는 에디터를 직접 바꾸므로 쓰기 도구는 결과를 확인하고 호출한다.

에디터가 꺼져 있으면 9090에 리스너가 없어 연결이 실패하며, 이는 설정 오류가 아니다. 이 프로젝트 파일(`Client/Client.uproject`)에는 MCP 플러그인 항목이 없으므로 9090 서버는 엔진 5.9 쪽 플러그인이 띄우는 것으로 보인다.

근거 경로: `.mcp.json`

## 문서와 메인 Pi 확인

- Obsidian에서 폴더를 여는 방법은 [홈](../Home.md)에 있다.
- 신뢰할 수 있는 이 프로젝트의 저장소 루트에서 `pi --approve`로 시작하면 이 실행에서만 로컬 확장에 프로젝트 신뢰를 부여한다. 프로젝트를 신뢰하지 않으면 실행하지 않는다.
- `Document/Home.md`를 읽을 수 없다는 안내가 나오면 Pi 밖에서 해당 경로의 읽을 수 있는 UTF-8 Markdown 파일을 복원한 뒤 같은 세션에 요청을 다시 보낸다. 다른 도구로 우회하지 않는다.
- 저장소 루트에서 `node --test tools/pi/vault.test.mjs`로 문서 링크와 근거 경로를 확인한다.
- `node --test tools/pi/notes-guard.e2e.test.mjs`로 설치된 Pi의 자동 로드, 주입, 차단·복구, 종료 동작을 확인한다. Pi 1.6.0에서 검증했으며, 테스트는 임시 프로필과 로컬 모의 모델만 사용한다. CLI를 찾지 못하면 설치된 Pi 패키지의 `bin.pi`가 가리키는 파일을 `PI_CLI_JS`로 지정한다. 원시 요청과 RPC 기록을 보존하려면 `NOTES_GUARD_EVIDENCE_DIR`에 저장소 밖의 빈 폴더를 지정한다.

근거 경로: `.pi/extensions/notes-guard.ts`, `tools/pi/vault.test.mjs`, `tools/pi/notes-guard.e2e.test.mjs`

[상위: 가이드](index.md)
