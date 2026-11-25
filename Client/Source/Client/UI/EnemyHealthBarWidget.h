#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/DelayedBar.h"
#include "EnemyHealthBarWidget.generated.h"

class UProgressBar;

/**
 * 적(더미) 머리 위에 붙는 월드 HP 바.
 *
 * WBP_EnemyHealthBar 는 이 클래스를 부모로 하고 다음 이름의 위젯을 둔다.
 * - `HealthBarDelayed` (ProgressBar, 필수): 피해를 맞은 뒤 늦게 따라 줄어드는 바
 * - `HealthBarFill` (ProgressBar, 필수): 현재 체력 바
 *
 * 표시/숨김은 더미가 결정한다(문서: 만피면 숨긴다).
 */
UCLASS()
class CLIENT_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 체력을 반영한다. 줄었으면 지연 바가 잠시 멈춘 뒤 따라 내려간다. */
	void SetHealth(float Current, float Max);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBarFill;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBarDelayed;

private:
	FCombatDelayedBar DelayedBar;
};
