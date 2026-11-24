# 현재 인수인계

- Unreal 프로젝트의 현재 시작점은 [아키텍처 개요](../10-architecture/overview.md)다. 초기 저장소는 `Client` 게임·에디터 타깃이 있는 C++ 프로젝트 골격이다.
- 이 초기 기록의 Unreal 상태는 소스·설정 파일을 읽어 확인했다. 게임 빌드 또는 플레이 결과가 필요하면 [실행 및 검증 가이드](../40-guides/run-and-verify.md)를 출발점으로 직접 확인한다.
- 문서와 Pi 작업 경계의 선택 이유는 [ADR-0001](../20-decisions/ADR-0001-document-vault-and-pi-guard.md)에 있다. 이후에는 검증된 시작 상태나 다음 작업의 제약이 바뀔 때만 이 노트를 갱신한다.

근거 경로: `Client/Client.uproject`, `Client/Source/Client.Target.cs`, `Client/Source/ClientEditor.Target.cs`

[상위: 작업 기록과 인수인계](index.md)
