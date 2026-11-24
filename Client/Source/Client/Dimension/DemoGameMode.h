#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DemoGameMode.generated.h"

/** 데모 폰/컨트롤러만 지정하는 게임 모드. 맵 World Settings에서 선택해 쓴다. */
UCLASS()
class CLIENT_API ADemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADemoGameMode();
};
