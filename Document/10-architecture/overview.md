# 프로젝트 개요

TopDownShooter의 Unreal 프로젝트는 `Client/`에 있다. `Client/Client.uproject`는 엔진 5.9와 런타임 모듈 `Client` 하나를 선언한다. `ModelingToolsEditorMode` 플러그인은 Editor 타깃에만 허용된다.

저장소에서 확인되는 것은 C++ 프로젝트 골격이다. `Client.cpp`는 기본 게임 모듈을 등록하고, 설정된 게임 기본 맵은 엔진 템플릿 `/Engine/Maps/Templates/OpenWorld`다. 현재 추적된 소스에는 별도의 게임플레이 구현이 없다. 저장소 이름만으로 기능·규칙을 추정하지 않는다.

근거 경로: `Client/Client.uproject`, `Client/Source/Client/Client.cpp`, `Client/Config/DefaultEngine.ini`

[코드 구조](code-structure.md) · [상위: 아키텍처](index.md)
