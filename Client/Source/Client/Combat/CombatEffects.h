#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "CombatEffects.generated.h"

/** SetByCaller 키. 피해 GameplayEffect 와 그 값을 채우는 코드가 같은 이름을 쓴다. */
namespace CombatEffectKeys
{
	/** float. 체력은 더하기 연산이므로 피해량은 음수로 전달한다. */
	inline const FName Damage = FName(TEXT("Combat.Damage"));
}

/**
 * 피해 GameplayEffect(Instant).
 *
 * 런타임에 NewObject 로 만든 GE 인스턴스는 MakeOutgoingSpec(TSubclassOf)에 넘길 수 없어서
 * 클래스(CDO)로 정의한다. 체력에 SetByCaller 만큼 더한다.
 */
UCLASS()
class CLIENT_API UCombatDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UCombatDamageEffect();
};

/**
 * 사격 쿨다운 GameplayEffect. 지속 시간이 곧 연사 간격이다.
 * UGameplayAbility::CooldownGameplayEffectClass 는 클래스(CDO)를 받으므로 역시 클래스로 정의한다.
 */
UCLASS()
class CLIENT_API UCombatFireCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UCombatFireCooldownEffect();
};
