#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"

/**
 * 기본 도형만으로 색을 바꾸는 공용 코드. 전용 머티리얼/파티클 에셋을 만들지 않기 위한 것.
 */
namespace CombatVisuals
{
	/** /Engine/BasicShapes/BasicShapeMaterial (동적 인스턴스의 부모). */
	inline const TCHAR* const BaseMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	/**
	 * BasicShapeMaterial 의 벡터 파라미터에 색을 넣는다.
	 *
	 * 이 머티리얼의 색 파라미터 이름은 패키지 안에서 Color / BaseColor 두 후보로만 보이고,
	 * 헤드리스 환경에서는 어느 쪽이 실제 파라미터인지 확정할 수 없다. 그래서 둘 다 설정한다.
	 * 머티리얼에 없는 이름은 조용히 무시되므로 부작용이 없다.
	 */
	inline void SetColor(UMaterialInstanceDynamic* Material, const FLinearColor& Color)
	{
		if (!Material)
		{
			return;
		}

		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
	}
}
