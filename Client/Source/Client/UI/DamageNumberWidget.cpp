#include "UI/DamageNumberWidget.h"

#include "Combat/CombatConfig.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

void UDamageNumberWidget::SetDamage(float Damage)
{
	LifetimeSeconds = CombatConfig::Tuning().DamageNumberSeconds;

	if (DamageText)
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt32(Damage)));
	}

	ElapsedSeconds = 0.0f;
	bStartPositionCached = false;
}

void UDamageNumberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ElapsedSeconds += InDeltaTime;
	const float Alpha = LifetimeSeconds > 0.0f ? FMath::Clamp(ElapsedSeconds / LifetimeSeconds, 0.0f, 1.0f) : 1.0f;

	SetRenderOpacity(1.0f - Alpha);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		if (!bStartPositionCached)
		{
			StartPosition = CanvasSlot->GetPosition();
			bStartPositionCached = true;
		}

		// 위로 40px 떠오른다. 등장 직후 작게 시작해 커지는 느낌은 폰트 크기 대신 이 이동으로 만든다.
		CanvasSlot->SetPosition(StartPosition + FVector2D(0.0f, -40.0f * Alpha));
	}

	if (Alpha >= 1.0f)
	{
		RemoveFromParent();
	}
}
