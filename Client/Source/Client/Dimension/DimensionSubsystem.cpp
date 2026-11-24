#include "Dimension/DimensionSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/SubsystemCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogDimension, Log, All);

void UDimensionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 생성자에서 등록하면 CDO도 같은 이름의 명령을 등록해 실제 인스턴스와 중복된다.
	SetCommand = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("dimension.set"),
		TEXT("Switch to a dimension. Usage: dimension.set <A|B|C>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &ThisClass::CmdSet));

	NextCommand = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("dimension.next"),
		TEXT("Switch to the next dimension."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &ThisClass::CmdNext));

	PrevCommand = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("dimension.prev"),
		TEXT("Switch to the previous dimension."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &ThisClass::CmdPrev));

	StatusCommand = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
		TEXT("dimension.status"),
		TEXT("Log the current dimension."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(this, &ThisClass::CmdStatus));
}

EGameDimension UDimensionSubsystem::GetCurrentDimension() const
{
	return CurrentDimension;
}

void UDimensionSubsystem::SetDimension(EGameDimension NewDimension)
{
	if (NewDimension == CurrentDimension)
	{
		return;
	}

	CurrentDimension = NewDimension;
	ApplyDimension();
	OnDimensionChanged.Broadcast(CurrentDimension);
}

void UDimensionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 최초 상태(A)를 액터에 반영한다.
	ApplyDimension();
}

bool UDimensionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDimensionSubsystem::ApplyDimension()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 VisibleCount = 0;
	int32 HiddenCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		TArray<EGameDimension> ActorDimensions;
		for (const FName& Tag : Actor->Tags)
		{
			EGameDimension ParsedDimension = EGameDimension::A;
			if (DimensionTags::TryParseDimTag(Tag.ToString(), ParsedDimension))
			{
				ActorDimensions.AddUnique(ParsedDimension);
			}
		}

		// 차원 태그가 없는 액터는 모든 차원에 공통이므로 건드리지 않는다.
		if (ActorDimensions.Num() == 0)
		{
			continue;
		}

		const bool bVisible = ActorDimensions.Contains(CurrentDimension);
		Actor->SetActorHiddenInGame(!bVisible);
		Actor->SetActorEnableCollision(bVisible);

		if (bVisible)
		{
			++VisibleCount;
		}
		else
		{
			++HiddenCount;
		}
	}

	UE_LOG(LogDimension, Log, TEXT("switched to %s: visible=%d hidden=%d"), DimensionTags::ToDisplayString(CurrentDimension), VisibleCount, HiddenCount);
}

void UDimensionSubsystem::CmdSet(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogDimension, Warning, TEXT("usage: dimension.set <A|B|C>"));
		return;
	}

	EGameDimension NewDimension = EGameDimension::A;
	if (!DimensionTags::TryParseDimTag(FString(DimensionTags::TagPrefix) + Args[0].ToUpper(), NewDimension))
	{
		UE_LOG(LogDimension, Warning, TEXT("unknown dimension '%s'. usage: dimension.set <A|B|C>"), *Args[0]);
		return;
	}

	SetDimension(NewDimension);
}

void UDimensionSubsystem::CmdNext(const TArray<FString>& Args, UWorld* World)
{
	const uint8 NextIndex = static_cast<uint8>((static_cast<uint8>(CurrentDimension) + 1) % static_cast<uint8>(EGameDimension::Count));
	SetDimension(static_cast<EGameDimension>(NextIndex));
}

void UDimensionSubsystem::CmdPrev(const TArray<FString>& Args, UWorld* World)
{
	const uint8 Count = static_cast<uint8>(EGameDimension::Count);
	const uint8 PrevIndex = static_cast<uint8>((static_cast<uint8>(CurrentDimension) + Count - 1) % Count);
	SetDimension(static_cast<EGameDimension>(PrevIndex));
}

void UDimensionSubsystem::CmdStatus(const TArray<FString>& Args, UWorld* World)
{
	UE_LOG(LogDimension, Log, TEXT("current dimension: %s"), DimensionTags::ToDisplayString(CurrentDimension));
}
