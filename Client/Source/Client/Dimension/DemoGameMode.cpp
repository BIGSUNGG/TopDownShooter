#include "Dimension/DemoGameMode.h"

#include "Dimension/DemoPawn.h"
#include "Dimension/DemoPlayerController.h"

ADemoGameMode::ADemoGameMode()
{
	DefaultPawnClass = ADemoPawn::StaticClass();
	PlayerControllerClass = ADemoPlayerController::StaticClass();
}
