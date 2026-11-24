#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DemoPlayerController.generated.h"

/** Q/E를 홀드해 차원을 순환시키는 데모 컨트롤러. */
UCLASS()
class CLIENT_API ADemoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 이 시간만큼 홀드해야 차원이 바뀐다. */
	static constexpr float DimensionHoldSeconds = 2.0f;

protected:
	virtual void SetupInputComponent() override;

	void OnNextPressed();
	void OnNextReleased();
	void OnPrevPressed();
	void OnPrevReleased();

	void CommitNext();
	void CommitPrev();

	FTimerHandle DimensionHoldTimer;
};
