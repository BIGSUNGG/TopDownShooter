# 코드 구조

| 경로 | 역할 |
| --- | --- |
| `Client/Source/Client/Client.cpp` | `Client` 기본 게임 모듈 등록 |
| `Client/Source/Client/Dimension/DimensionTypes.h` | `EGameDimension` 열거형과 `Dim:` 태그 생성·파싱(`DimensionTags`) |
| `Client/Source/Client/Dimension/DimensionSubsystem.cpp` | `UDimensionSubsystem` — 액터 태그를 풀어 보임·콜리전 전환, 콘솔 명령 4개 |
| `Client/Source/Client/Dimension/DemoPawn.cpp` | `ADemoPawn` — 캡슐·SpringArm·Plane 이동, WASD 상태 기반 이동 |
| `Client/Source/Client/Dimension/DemoPlayerController.cpp` | `ADemoPlayerController` — Q/E 2초 홀드로 차원 전환 |
| `Client/Source/Client/Dimension/DemoGameMode.cpp` | `ADemoGameMode` — 데모 폰·컨트롤러 기본값 |
| `Client/Source/Client/Client.Build.cs` | 런타임 모듈 빌드 규칙. 공개 의존성: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput` |
| `Client/Source/Client.Target.cs` | 게임 타깃 |
| `Client/Source/ClientEditor.Target.cs` | 에디터 타깃 |
| `Client/Config/DefaultEngine.ini` | 게임 기본 맵, 렌더링·플랫폼 설정 |
| `Client/Config/DefaultInput.ini` | 프로젝트 기본 입력 설정 |
| `Client/Config/DefaultGame.ini` | 프로젝트 기본 게임 설정 |
| `Client/Config/DefaultEditor.ini` | 에디터 기본 설정 파일 |

근거 경로: `Client/Source/Client/Client.cpp`, `Client/Source/Client/Client.Build.cs`, `Client/Source/Client.Target.cs`, `Client/Source/ClientEditor.Target.cs`, `Client/Source/Client/Dimension/DimensionSubsystem.cpp`, `Client/Source/Client/Dimension/DemoPawn.cpp`, `Client/Config/DefaultEngine.ini`, `Client/Config/DefaultInput.ini`, `Client/Config/DefaultGame.ini`, `Client/Config/DefaultEditor.ini`

[상위: 아키텍처](index.md)
