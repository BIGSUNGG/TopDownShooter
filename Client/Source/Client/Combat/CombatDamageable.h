#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatDamageable.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class UCombatDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 피해를 받을 수 있는 대상. 투사체는 이 인터페이스만 알고 피해를 전달한다.
 * 구현체는 자기 AbilitySystemComponent 로 GameplayEffect 를 적용한다(피해 경로 단일화).
 */
class CLIENT_API ICombatDamageable
{
	GENERATED_BODY()

public:
	/**
	 * @param Damage        적용할 피해량(양수). 0 이하면 무시한다.
	 * @param Direction     발사 방향(넉백 방향).
	 * @param Causer        발사한 액터(없을 수 있다).
	 * @param HitLocation   명중 지점(데미지 숫자·임팩트 위치).
	 */
	virtual void ApplyCombatDamage(float Damage, const FVector& Direction, AActor* Causer, const FVector& HitLocation) = 0;
};
