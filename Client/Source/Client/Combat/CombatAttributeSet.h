#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CombatAttributeSet.generated.h"

/**
 * ATTRIBUTE_ACCESSORS 는 엔진 주석에만 있는 이름이고, 실제로 정의된 매크로는
 * ATTRIBUTE_ACCESSORS_BASIC 이다(AttributeSet.h). 프로젝트 코드가 문서·관례에서 쓰는 이름을
 * 그대로 쓸 수 있도록 얇게 연결한다.
 */
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) ATTRIBUTE_ACCESSORS_BASIC(ClassName, PropertyName)
#endif

/**
 * 체력 속성. 피해는 GameplayEffect(SetByCaller)로만 들어오고, 값은 항상 [0, MaxHealth]로 자른다.
 */
UCLASS()
class CLIENT_API UCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCombatAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Attributes")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Attributes")
	FGameplayAttributeData MaxHealth;

	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health)
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
