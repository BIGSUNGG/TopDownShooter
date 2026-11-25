#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageNumberWidget.generated.h"

class UTextBlock;

/**
 * 피해 숫자. 맞은 지점에서 위로 떠오르며 사라진다(문서: 데미지 숫자 0.8초).
 *
 * WBP_DamageNumber 는 이 클래스를 부모로 하고, `DamageText` 이름의 TextBlock 하나만 두면 된다.
 * 등장 연출은 C++ 이 하므로 위젯 그래프(이벤트 그래프)가 필요 없다.
 */
UCLASS()
class CLIENT_API UDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 피해량을 정하고 수명을 초기화한다. 생성 직후 한 번 호출한다. */
	void SetDamage(float Damage);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** WBP_DamageNumber 의 TextBlock 이름. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DamageText;

private:
	float ElapsedSeconds = 0.0f;
	float LifetimeSeconds = 0.8f;

	/** 떠오르기 시작 위치(캔버스 슬롯 좌표). 첫 틱에 한 번만 기록한다. */
	FVector2D StartPosition = FVector2D::ZeroVector;
	bool bStartPositionCached = false;
};
