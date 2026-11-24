# 코드 구조

| 경로 | 역할 |
| --- | --- |
| `Client/Source/Client/Client.cpp` | `Client` 기본 게임 모듈 등록 |
| `Client/Source/Client/Client.Build.cs` | 런타임 모듈 빌드 규칙. 공개 의존성: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput` |
| `Client/Source/Client.Target.cs` | 게임 타깃 |
| `Client/Source/ClientEditor.Target.cs` | 에디터 타깃 |
| `Client/Config/DefaultEngine.ini` | 게임 기본 맵, 렌더링·플랫폼 설정 |
| `Client/Config/DefaultInput.ini` | 프로젝트 기본 입력 설정 |
| `Client/Config/DefaultGame.ini` | 프로젝트 기본 게임 설정 |
| `Client/Config/DefaultEditor.ini` | 에디터 기본 설정 파일 |

근거 경로: `Client/Source/Client/Client.cpp`, `Client/Source/Client/Client.Build.cs`, `Client/Source/Client.Target.cs`, `Client/Source/ClientEditor.Target.cs`, `Client/Config/DefaultEngine.ini`, `Client/Config/DefaultInput.ini`, `Client/Config/DefaultGame.ini`, `Client/Config/DefaultEditor.ini`

[상위: 아키텍처](index.md)
