#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Dimension/DimensionTypes.h"
#include "DimensionSubsystem.generated.h"

class FAutoConsoleCommandWithWorldAndArgs;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDimensionChanged, EGameDimension);

/**
 * 현재 차원을 보관하고, 액터 Tags("Dim:A" 등)에 따라 액터의 표시/충돌 상태를 전환한다.
 */
UCLASS()
class CLIENT_API UDimensionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	EGameDimension GetCurrentDimension() const;

	/** 차원을 바꾼다. 같은 값이면 아무 일도 하지 않는다. */
	void SetDimension(EGameDimension NewDimension);

	/** 차원이 실제로 바뀐 뒤 발행된다. */
	FOnDimensionChanged OnDimensionChanged;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void ApplyDimension();

	void CmdSet(const TArray<FString>& Args, UWorld* World);
	void CmdNext(const TArray<FString>& Args, UWorld* World);
	void CmdPrev(const TArray<FString>& Args, UWorld* World);
	void CmdStatus(const TArray<FString>& Args, UWorld* World);

	EGameDimension CurrentDimension = EGameDimension::A;

	TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> SetCommand;
	TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> NextCommand;
	TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> PrevCommand;
	TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> StatusCommand;
};
