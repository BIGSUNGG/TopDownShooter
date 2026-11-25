#include "Dimension/DemoGameMode.h"

#include "Character/PlayerCombatCharacter.h"
#include "Dimension/DemoPlayerController.h"

ADemoGameMode::ADemoGameMode()
{
	// 기본 폰/컨트롤러는 C++ 에서만 지정한다(Client/Config 는 건드리지 않는다).
	// 레벨(Lvl_OfficeDemo)의 World Settings 는 이 게임 모드를 그대로 가리키므로 클래스 이름을 유지한다.
	DefaultPawnClass = APlayerCombatCharacter::StaticClass();
	PlayerControllerClass = ADemoPlayerController::StaticClass();
}
