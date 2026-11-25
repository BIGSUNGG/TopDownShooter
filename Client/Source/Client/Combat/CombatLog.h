#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/** 전투 로그. 메시지는 "COMBAT_*" 접두사를 써서 로그 검사기가 grep으로 찾을 수 있게 한다. */
DECLARE_LOG_CATEGORY_EXTERN(LogCombat, Log, All);
