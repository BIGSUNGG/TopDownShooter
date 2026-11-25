#pragma once

#include "CoreMinimal.h"
#include "Character/CombatCharacter.h"
#include "DummyCombatCharacter.generated.h"

class UEnemyHealthBarWidget;
class UWidgetComponent;

/**
 * 테스트용 표적.
 *
 * 입력·이동·공격이 없다. 체력 100, 피격 시 데미지·색 플래시·넉백은 공통 캐릭터 규칙을 그대로 쓰고,
 * 사망하면 같은 프레임에 원래 위치·회전·체력으로 돌아온다(문서: 더미).
 * 레벨이 배치를 소유하므로 코드에서 스폰하지 않는다.
 */
UCLASS()
class CLIENT_API ADummyCombatCharacter : public ACombatCharacter
{
	GENERATED_BODY()

public:
	ADummyCombatCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 더미 머리 위 HP 바. 만피면 숨는다(BindWidget 이름은 WBP_EnemyHealthBar 참고). */
	UPROPERTY(VisibleAnywhere, Category = "Combat|HUD")
	TObjectPtr<UWidgetComponent> OverheadHealthBar;

protected:
	/** 체력에 맞춰 HP 바를 갱신하고, 만피면 숨기고 피해를 입으면 보인다. */
	void RefreshOverheadHealthBar();

	/** 지금 HP 바가 보이는 상태인가(전환 순간에만 표시/숨김과 로그를 한다). */
	bool bOverheadBarVisible = false;
	virtual void HandleDeath() override;
	virtual void RespawnFromDeath() override;
	virtual void GrantCombatAbilities() override;

	/** 스폰 시점의 트랜스폼. 부활 위치로 쓴다. */
	FTransform CachedSpawnTransform = FTransform::Identity;
};
