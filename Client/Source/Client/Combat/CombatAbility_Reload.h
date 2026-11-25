#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CombatAbility_Reload.generated.h"

/**
 * 재장전 어빌리티. 1.2초 뒤에 탄창을 min(부족분, 예비) 만큼 채운다.
 *
 * 예비 탄약이 0이면 아무 일도 없이 조용히 끝난다(문서: 재장전 실패, 소리·로그 없음).
 * 사망 등으로 취소되면 타이머와 장전 상태를 반드시 정리한다.
 */
UCLASS()
class CLIENT_API UCombatAbility_Reload : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCombatAbility_Reload();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 지속 시간이 끝났을 때 탄약을 옮기고 어빌리티를 끝낸다. */
	void OnReloadFinished();

	FTimerHandle ReloadTimerHandle;
};
