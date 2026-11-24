#pragma once

#include "CoreMinimal.h"
#include "DimensionTypes.generated.h"

/** 데모에서 순환하는 차원. Count는 순환 계산용 경계값이다. */
UENUM(BlueprintType)
enum class EGameDimension : uint8
{
	A = 0,
	B = 1,
	C = 2,
	Count = 3
};

/** 액터 Tags에 붙이는 차원 태그 규칙: "Dim:A" / "Dim:B" / "Dim:C" */
namespace DimensionTags
{
	inline const TCHAR* const TagPrefix = TEXT("Dim:");
	inline const TCHAR* const GroupTagPrefix = TEXT("DimGroup:");

	inline const TCHAR* ToDisplayString(EGameDimension Dimension)
	{
		if (Dimension == EGameDimension::A)
		{
			return TEXT("A");
		}
		if (Dimension == EGameDimension::B)
		{
			return TEXT("B");
		}
		if (Dimension == EGameDimension::C)
		{
			return TEXT("C");
		}
		return TEXT("?");
	}

	inline FString MakeDimTag(EGameDimension Dimension)
	{
		return FString(TagPrefix) + ToDisplayString(Dimension);
	}

	/** "Dim:<A|B|C>" 형태만 성공한다. Count를 포함한 나머지는 false. */
	inline bool TryParseDimTag(const FString& Tag, EGameDimension& OutDimension)
	{
		const FString Prefix(TagPrefix);
		if (!Tag.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return false;
		}

		const FString Suffix = Tag.Mid(Prefix.Len());
		if (Suffix == TEXT("A"))
		{
			OutDimension = EGameDimension::A;
			return true;
		}
		if (Suffix == TEXT("B"))
		{
			OutDimension = EGameDimension::B;
			return true;
		}
		if (Suffix == TEXT("C"))
		{
			OutDimension = EGameDimension::C;
			return true;
		}
		return false;
	}
}
