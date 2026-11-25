#include "UI/EnemyHealthBarWidget.h"

#include "Combat/CombatConfig.h"
#include "Components/ProgressBar.h"

void UEnemyHealthBarWidget::SetHealth(float Current, float Max)
{
	const float MaxHealth = FMath::Max(Max, 1.0f);
	const float Percent = FMath::Clamp(Current / MaxHealth, 0.0f, 1.0f);

	if (HealthBarFill)
	{
		HealthBarFill->SetPercent(Percent);
	}

	DelayedBar.Report(Percent, CombatConfig::Tuning().DelayedBarHoldSeconds);

	if (HealthBarDelayed)
	{
		HealthBarDelayed->SetPercent(DelayedBar.DelayedPercent);
	}
}

void UEnemyHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (HealthBarDelayed)
	{
		HealthBarDelayed->SetPercent(
			DelayedBar.Tick(InDeltaTime, CombatConfig::Tuning().DelayedBarCatchUpSeconds));
	}
}
