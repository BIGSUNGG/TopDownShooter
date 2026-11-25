#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CombatAbility_Fire.generated.h"

/**
 * 사격 어빌리티.
 *
 * 연사 간격(0.15초)은 UCombatFireCooldownEffect 를 쿨다운 GE 로 커밋해 표현한다.
 * 탄창이 0이거나 구르기·재장전·사망 상태면 CanActivateAbility 가 막는다(로그만 남기지 않는다).
 * 서버·리스너 없이도(headless) 동작하도록 LocalOnly + InstancedPerActor 로 둔다.
 */
UCLASS()
class CLIENT_API UCombatAbility_Fire : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCombatAbility_Fire();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
