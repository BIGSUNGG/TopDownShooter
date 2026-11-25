#include "UI/CombatHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Character/CombatCharacter.h"
#include "Combat/CombatConfig.h"
#include "Combat/CombatLog.h"
#include "Combat/CombatMath.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/DamageNumberWidget.h"

void UCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 크로스헤어는 NativePaint 로 직접 그린다. Slate 는 무효화된 위젯만 다시 그리므로
	// 이 위젯을 항상 다시 그리는(volatile) 위젯으로 둔다.
	if (TSharedPtr<SWidget> SafeWidget = GetCachedWidget())
	{
		SafeWidget->ForceVolatile(true);
	}

	BindToPlayer();
}

void UCombatHUDWidget::NativeDestruct()
{
	if (ACombatCharacter* Character = BoundCharacter.Get())
	{
		Character->OnHealthChanged.RemoveDynamic(this, &UCombatHUDWidget::HandleHealthChanged);
		Character->OnAmmoChanged.RemoveDynamic(this, &UCombatHUDWidget::HandleAmmoChanged);
		Character->OnDamaged.RemoveDynamic(this, &UCombatHUDWidget::HandleDamaged);
	}

	Super::NativeDestruct();
}

void UCombatHUDWidget::BindToPlayer()
{
	ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwningPlayerPawn());
	if (!Character)
	{
		return;
	}

	BoundCharacter = Character;
	Character->OnHealthChanged.AddDynamic(this, &UCombatHUDWidget::HandleHealthChanged);
	Character->OnAmmoChanged.AddDynamic(this, &UCombatHUDWidget::HandleAmmoChanged);
	Character->OnDamaged.AddDynamic(this, &UCombatHUDWidget::HandleDamaged);

	const float MaxHealth = FMath::Max(Character->GetMaxHealth(), 1.0f);
	const float Percent = FMath::Clamp(Character->GetHealth() / MaxHealth, 0.0f, 1.0f);

	if (HealthBarFill)
	{
		HealthBarFill->SetPercent(Percent);
	}

	DelayedBar.Reset(Percent);

	if (HealthBarDelayed)
	{
		HealthBarDelayed->SetPercent(DelayedBar.DelayedPercent);
	}

	RefreshText();
}

void UCombatHUDWidget::HandleHealthChanged(float NewHealth, float MaxHealth, float Delta)
{
	const float Percent = MaxHealth > 0.0f ? FMath::Clamp(NewHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;

	if (HealthBarFill)
	{
		HealthBarFill->SetPercent(Percent);
	}

	DelayedBar.Report(Percent, CombatConfig::Tuning().DelayedBarHoldSeconds);
}

void UCombatHUDWidget::HandleAmmoChanged(int32 Magazine, int32 Reserve)
{
	RefreshText();
}

void UCombatHUDWidget::HandleDamaged(float Damage, FVector WorldLocation)
{
	FlashRemainingSeconds = CombatConfig::Tuning().HitFlashReturnSeconds;
	SpawnDamageNumber(Damage, WorldLocation);
}

void UCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ACombatCharacter* Character = BoundCharacter.Get();
	const float MaxHealth = Character ? FMath::Max(Character->GetMaxHealth(), 1.0f) : 1.0f;
	const float Percent = Character ? FMath::Clamp(Character->GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f;

	// 문서: 피해를 맞으면 0.5초 멈췄다가 2.5초에 걸쳐 현재 체력까지 따라 내려간다.
	const float DelayedPercent = DelayedBar.Tick(InDeltaTime, CombatConfig::Tuning().DelayedBarCatchUpSeconds);

	if (HealthBarDelayed)
	{
		HealthBarDelayed->SetPercent(DelayedPercent);
	}

	if (CrosshairLogCooldown > 0.0f)
	{
		CrosshairLogCooldown = FMath::Max(CrosshairLogCooldown - InDeltaTime, 0.0f);
	}

	// PIE 증거용 로그. 간격이 1px 이상 바뀔 때만 0.5초에 한 번 적는다.
	FVector2D CrosshairCursor = FVector2D::ZeroVector;
	float CrosshairGap = 0.0f;
	ComputeCrosshair(CrosshairCursor, CrosshairGap);

	// Slate 는 무효화 요청이 있을 때만 NativePaint 를 다시 호출한다.
	// 그래서 커서나 간격이 실제로 바뀌었을 때 직접 다시 그리라고 알린다.
	const bool bCrosshairMoved = FVector2D::Distance(CrosshairCursor, LastPaintedCursor) > 0.5f;
	if (bCrosshairMoved || FMath::Abs(CrosshairGap - LastPaintedGapPixels) > 0.25f || LastPaintedGapPixels < 0.0f)
	{
		LastPaintedCursor = CrosshairCursor;
		LastPaintedGapPixels = CrosshairGap;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	if (CrosshairLogCooldown <= 0.0f && FMath::Abs(CrosshairGap - LastLoggedGapPixels) >= 1.0f)
	{
		LastLoggedGapPixels = CrosshairGap;
		CrosshairLogCooldown = 0.5f;
		const ACombatCharacter* LogCharacter = BoundCharacter.Get();
		UE_LOG(LogCombat, Log, TEXT("COMBAT_CROSSHAIR gap=%.1f spread=%.2f"),
			CrosshairGap, LogCharacter ? LogCharacter->GetCurrentSpreadDeg() : 0.0f);
	}

	if (DamageFlash)
	{
		const float FlashSeconds = FMath::Max(CombatConfig::Tuning().HitFlashReturnSeconds, KINDA_SMALL_NUMBER);
		DamageFlash->SetRenderOpacity(FMath::Clamp(FlashRemainingSeconds / FlashSeconds, 0.0f, 1.0f));
	}

	if (FlashRemainingSeconds > 0.0f)
	{
		FlashRemainingSeconds = FMath::Max(FlashRemainingSeconds - InDeltaTime, 0.0f);
	}

	RefreshText();
}

void UCombatHUDWidget::ComputeCrosshair(FVector2D& OutCursorViewportPx, float& OutGapViewportPx) const
{
	OutCursorViewportPx = FVector2D::ZeroVector;
	OutGapViewportPx = 0.0f;

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	const ACombatCharacter* Character = BoundCharacter.Get();
	const float SpreadDeg = Character ? Character->GetCurrentSpreadDeg() : 0.0f;

	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	const float FovAngle = PlayerController->PlayerCameraManager ? PlayerController->PlayerCameraManager->GetFOVAngle() : 90.0f;

	// 간격은 화면상 각도에 해당하므로 뷰포트 픽셀 단위로 바꾼다. 위젯 로컬 단위 환산은 NativePaint 가 기하로 한다.
	OutGapViewportPx = CombatMath::CrosshairGapPixels(SpreadDeg, ViewportSize.X * 0.5f, FovAngle * 0.5f);

	// 커서가 뷰포트 밖에 있으면 화면 중앙에 둔다(조준점은 항상 보여야 한다).
	FVector2D CursorPixels = ViewportSize * 0.5f;
	float CursorX = 0.0f;
	float CursorY = 0.0f;
	if (PlayerController->GetMousePosition(CursorX, CursorY))
	{
		CursorPixels = FVector2D(CursorX, CursorY);
	}

	OutCursorViewportPx = CursorPixels;
}

int32 UCombatHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 PaintedLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FVector2D CursorViewportPx = FVector2D::ZeroVector;
	float GapViewportPx = 0.0f;
	ComputeCrosshair(CursorViewportPx, GapViewportPx);

	// 뷰포트 픽셀 -> 위젯 로컬 단위. 위젯 크기와 뷰포트 크기가 다를 수 있어(레터박스, DPI) 실제 기하로 환산한다.
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	const double LocalPerPx = (ViewportSize.X > 0.0) ? LocalSize.X / ViewportSize.X : 1.0;
	const float PixelScale = static_cast<float>(LocalPerPx);

	FVector2D CursorPos = CursorViewportPx * LocalPerPx;
	const float Gap = GapViewportPx * PixelScale;

	// 뷰포트를 벗어난 커서는 화면 중앙으로 되돌린다.
	if (CursorPos.X < 0.0 || CursorPos.Y < 0.0 || CursorPos.X > LocalSize.X || CursorPos.Y > LocalSize.Y)
	{
		CursorPos = LocalSize * 0.5;
	}

	// 진단용: 실제로 그리는 값과 호출 빈도를 남긴다(1초에 한 번).
	if (++PaintLogCounter % 60 == 0)
	{
		const ACombatCharacter* PaintCharacter = BoundCharacter.Get();
		UE_LOG(LogCombat, Log, TEXT("COMBAT_CROSSHAIR_PAINT gap=%.1f spread=%.2f cursor=%.0f,%.0f scale=%.2f"),
			Gap, PaintCharacter ? PaintCharacter->GetCurrentSpreadDeg() : 0.0f, CursorPos.X, CursorPos.Y, PixelScale);
	}
	if (Gap <= 0.0f && CursorPos.IsNearlyZero())
	{
		return PaintedLayer;
	}

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (!Brush)
	{
		return PaintedLayer;
	}

	// 십자 조준점: 중앙 점 + 위/아래/좌/우 홈대. 두께는 화면 픽셀 기준이라 배율이 달라도 또렷하게 보인다.
	const FLinearColor& Color = CrosshairColor;
	const float DotScaled = CrosshairDotPixels * PixelScale;
	const float ArmLengthScaled = CrosshairArmLengthPixels * PixelScale;
	const float ArmThicknessScaled = CrosshairArmThicknessPixels * PixelScale;
	const FVector2D DotSize(DotScaled, DotScaled);

	auto DrawBlock = [&](const FVector2D& InCenter, const FVector2f& InSize)
	{
		const FVector2f TopLeft(static_cast<float>(InCenter.X) - InSize.X * 0.5f, static_cast<float>(InCenter.Y) - InSize.Y * 0.5f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			PaintedLayer,
			AllottedGeometry.ToPaintGeometry(InSize, FSlateLayoutTransform(TopLeft)),
			Brush,
			ESlateDrawEffect::None,
			Color);
	};

	DrawBlock(CursorPos, FVector2f(static_cast<float>(DotSize.X), static_cast<float>(DotSize.Y)));

	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const FVector2f Size = (Axis == 0)
			? FVector2f(ArmThicknessScaled, ArmLengthScaled)
			: FVector2f(ArmLengthScaled, ArmThicknessScaled);
		for (int32 Direction = -1; Direction <= 1; Direction += 2)
		{
			const FVector2D Offset = (Axis == 0)
				? FVector2D(0.0, Direction * (Gap + ArmLengthScaled * 0.5f))
				: FVector2D(Direction * (Gap + ArmLengthScaled * 0.5f), 0.0);

			DrawBlock(CursorPos + Offset, Size);
		}
	}

	return PaintedLayer;
}

void UCombatHUDWidget::RefreshText()
{
	const ACombatCharacter* Character = BoundCharacter.Get();

	if (AmmoText)
	{
		const int32 Magazine = Character ? Character->GetAmmoInMagazine() : 0;
		const int32 Reserve = Character ? Character->GetAmmoReserve() : 0;
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Magazine, Reserve)));
	}

	if (StateText)
	{
		FString Label;
		if (Character && Character->IsReloading())
		{
			Label = TEXT("재장전 중");
		}
		else if (Character && Character->IsAiming())
		{
			Label = TEXT("조준");
		}
		StateText->SetText(FText::FromString(Label));
	}
}

void UCombatHUDWidget::SpawnDamageNumber(float Damage, const FVector& WorldLocation)
{
	if (!DamageNumberLayer || !DamageNumberWidgetClass)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!PlayerController || !UGameplayStatics::ProjectWorldToScreen(PlayerController, WorldLocation, ScreenPosition))
	{
		return;
	}

	UDamageNumberWidget* Number = CreateWidget<UDamageNumberWidget>(PlayerController, DamageNumberWidgetClass);
	if (!Number)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = DamageNumberLayer->AddChildToCanvas(Number))
	{
		CanvasSlot->SetAutoSize(true);

		// 뷰포트 스케일을 나눠야 4K/DPI 환경에서도 커서 지점에 붙는다.
		const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
		CanvasSlot->SetPosition(ScreenPosition / ViewportScale - FVector2D(12.0f, 16.0f));
	}

	// 숫자 폭의 절반만큼 왼쪽으로 밀어 맞은 지점을 가운데로 맞춘다(자동 크기라 위치만 조정한다).
	Number->SetDamage(Damage);
}
