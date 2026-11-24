#include "Dimension/DemoPlayerController.h"

#include "Components/InputComponent.h"
#include "Dimension/DimensionSubsystem.h"
#include "Engine/World.h"

void ADemoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ADemoPlayerController::OnNextPressed);
	InputComponent->BindKey(EKeys::E, IE_Released, this, &ADemoPlayerController::OnNextReleased);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ADemoPlayerController::OnPrevPressed);
	InputComponent->BindKey(EKeys::Q, IE_Released, this, &ADemoPlayerController::OnPrevReleased);
}

void ADemoPlayerController::OnNextPressed()
{
	GetWorldTimerManager().SetTimer(DimensionHoldTimer, this, &ADemoPlayerController::CommitNext, DimensionHoldSeconds, false);
}

void ADemoPlayerController::OnNextReleased()
{
	// 2초 전에 떼면 타이머가 취소되어 전환이 일어나지 않는다.
	GetWorldTimerManager().ClearTimer(DimensionHoldTimer);
}

void ADemoPlayerController::OnPrevPressed()
{
	GetWorldTimerManager().SetTimer(DimensionHoldTimer, this, &ADemoPlayerController::CommitPrev, DimensionHoldSeconds, false);
}

void ADemoPlayerController::OnPrevReleased()
{
	GetWorldTimerManager().ClearTimer(DimensionHoldTimer);
}

void ADemoPlayerController::CommitNext()
{
	UDimensionSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UDimensionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	const EGameDimension Next = static_cast<EGameDimension>((static_cast<uint8>(Subsystem->GetCurrentDimension()) + 1) % static_cast<uint8>(EGameDimension::Count));
	Subsystem->SetDimension(Next);
}

void ADemoPlayerController::CommitPrev()
{
	UDimensionSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UDimensionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	const uint8 Count = static_cast<uint8>(EGameDimension::Count);
	const uint8 Prev = static_cast<uint8>((static_cast<uint8>(Subsystem->GetCurrentDimension()) + Count - 1) % Count);
	Subsystem->SetDimension(static_cast<EGameDimension>(Prev));
}
