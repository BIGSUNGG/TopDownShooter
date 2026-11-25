#pragma once

#include "CoreMinimal.h"

/**
 * 지연 바 상태(플레이어 HUD 와 적 HP 바가 같이 쓴다).
 *
 * 체력이 줄면 HoldSeconds 동안 현재 값에 멈춰 있다가 CatchUpSeconds 비율로 따라 내려간다.
 * 회복·리스폰처럼 값이 늘면 지연 없이 즉시 따라온다.
 */
struct FCombatDelayedBar
{
	/** 현재 목표 비율(체력 바가 보여주는 값). */
	float Percent = 1.0f;

	/** 지연 바가 보여주는 비율. Percent 를 늦게 따라간다. */
	float DelayedPercent = 1.0f;

	/** 지연 바가 움직이지 않고 기다리는 남은 시간. */
	float HoldRemainingSeconds = 0.0f;

	/** 초기화. 바를 만들거나 리스폰할 때 부른다. */
	void Reset(float InPercent)
	{
		Percent = InPercent;
		DelayedPercent = InPercent;
		HoldRemainingSeconds = 0.0f;
	}

	/** 체력 변화를 알린다. 줄었으면 HoldSeconds 만큼 기다린 뒤 따라온다. */
	void Report(float InPercent, float HoldSeconds)
	{
		Percent = InPercent;

		if (InPercent >= DelayedPercent)
		{
			DelayedPercent = InPercent;
			HoldRemainingSeconds = 0.0f;
			return;
		}

		HoldRemainingSeconds = HoldSeconds;
	}

	/** 매 프레임 진행. 보간한 지연 비율을 돌려준다. */
	float Tick(float DeltaSeconds, float CatchUpSeconds)
	{
		if (HoldRemainingSeconds > 0.0f)
		{
			HoldRemainingSeconds = FMath::Max(HoldRemainingSeconds - DeltaSeconds, 0.0f);
			return DelayedPercent;
		}

		DelayedPercent = FMath::FInterpConstantTo(
			DelayedPercent, Percent, DeltaSeconds, 1.0f / FMath::Max(CatchUpSeconds, KINDA_SMALL_NUMBER));

		return DelayedPercent;
	}
};
