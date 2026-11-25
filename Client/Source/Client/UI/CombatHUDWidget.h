#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/DelayedBar.h"
#include "CombatHUDWidget.generated.h"

class ACombatCharacter;
class UCanvasPanel;
class UDamageNumberWidget;
class UImage;
class UProgressBar;
class UTextBlock;

/**
 * 플레이어 HUD. 모든 수치와 연출은 C++ 이 처리하고 WBP 는 배치만 담당한다.
 *
 * WBP_HUD 는 이 클래스를 부모로 하고 다음 이름의 위젯을 둔다.
 * - `HealthBarDelayed` (ProgressBar, 필수): 피해를 맞은 뒤 늦게 따라 줄어드는 바
 * - `HealthBarFill` (ProgressBar, 필수): 현재 체력 바
 * - `AmmoText` (TextBlock, 필수): 탄약 표시
 * - `StateText` (TextBlock, 선택): 재장전·조준 상태 표시
 * - `DamageNumberLayer` (CanvasPanel, 선택): 피해 숫자가 붙는 레이어
 * - `DamageFlash` (Image, 선택): 피격 화면 플래시
 * 크로스헤어는 WBP 배치가 아니라 NativePaint 로 직접 그린다(커서를 매 프레임 따라가야 하므로).
 */
UCLASS()
class CLIENT_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 피해 숫자 위젯 클래스. WBP_HUD 클래스 기본값으로 WBP_DamageNumber 를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|HUD")
	TSubclassOf<UDamageNumberWidget> DamageNumberWidgetClass;

	/** 크로스헤어 중앙 점 크기(화면 픽셀). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HUD")
	float CrosshairDotPixels = 6.0f;

	/** 크로스헤어 홈대 길이(화면 픽셀). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HUD")
	float CrosshairArmLengthPixels = 22.0f;

	/** 크로스헤어 홈대 두께(화면 픽셀). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HUD")
	float CrosshairArmThicknessPixels = 4.0f;

	/** 크로스헤어 색. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HUD")
	FLinearColor CrosshairColor = FLinearColor(0.35f, 1.0f, 1.0f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth, float Delta);

	UFUNCTION()
	void HandleAmmoChanged(int32 Magazine, int32 Reserve);

	UFUNCTION()
	void HandleDamaged(float Damage, FVector WorldLocation);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBarFill;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBarDelayed;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> DamageNumberLayer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DamageFlash;


private:
	/** 소유 폰(플레이어)에 델리게이트를 연결하고 현재 값으로 초기화한다. */
	void BindToPlayer();

	/** 탄약·상태 텍스트를 현재 값으로 갱신한다. */
	void RefreshText();

	/** 맞은 월드 위치에 피해 숫자를 띄운다. */
	void SpawnDamageNumber(float Damage, const FVector& WorldLocation);

	/** 커서 위치(위젯 로컬 좌표), 현재 탄퍼짐의 십자 간격(px), 화면 1픽셀에 해당하는 로컬 크기를 계산한다. */
	void ComputeCrosshair(FVector2D& OutCursorViewportPx, float& OutGapViewportPx) const;

	TWeakObjectPtr<ACombatCharacter> BoundCharacter;

	/** 지연 바 상태(문서: 유지 0.5초, 2.5초 추격). */
	FCombatDelayedBar DelayedBar;

	/** 크로스헤어 로그를 다시 쓸 때까지의 남은 시간(로그 폭주 방지). */
	float CrosshairLogCooldown = 0.0f;

	/** 마지막으로 기록한 크로스헤어 간격(px). */
	float LastLoggedGapPixels = -1.0f;

	/** NativePaint 호출 횟수(진단 로그 스로틀용). */
	mutable int32 PaintLogCounter = 0;

	/** 마지막으로 그린 크로스헤어 상태. Slate 는 다시 그리라고 할 때만 NativePaint 를 호출한다. */
	float LastPaintedGapPixels = -1.0f;
	FVector2D LastPaintedCursor = FVector2D(-1.0f, -1.0f);

	/** 피격 플래시 남은 시간. */
	float FlashRemainingSeconds = 0.0f;
};
