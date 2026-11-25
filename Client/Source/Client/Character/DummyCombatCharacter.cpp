#include "Character/DummyCombatCharacter.h"

#include "Combat/CombatAttributeSet.h"
#include "Combat/CombatConfig.h"
#include "Combat/CombatLog.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UI/EnemyHealthBarWidget.h"

ADummyCombatCharacter::ADummyCombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 표적은 움직이지 않는다.
	bIsDummy = true;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
		MovementComponent->bRunPhysicsWithNoController = false;
	}

	// 머리 위 HP 바. 화면 공간 위젯이라 항상 카메라를 향하고 거리와 무관하게 같은 크기로 읽힌다.
	OverheadHealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadHealthBar"));
	OverheadHealthBar->SetupAttachment(GetCapsuleComponent());
	OverheadHealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	OverheadHealthBar->SetDrawSize(FVector2D(160.0f, 16.0f));
	OverheadHealthBar->SetPivot(FVector2D(0.5f, 0.5f));
	OverheadHealthBar->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	OverheadHealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverheadHealthBar->SetGenerateOverlapEvents(false);
	OverheadHealthBar->SetVisibility(false);
	OverheadHealthBar->SetWidgetClass(UEnemyHealthBarWidget::StaticClass());

	// 에디터에서 만든 배치가 있으면 그 WBP 를 쓴다(없으면 C++ 위젯만으로도 바는 동작한다).
	static ConstructorHelpers::FClassFinder<UEnemyHealthBarWidget> BarClassFinder(TEXT("/Game/Combat/UI/WBP_EnemyHealthBar"));
	if (BarClassFinder.Succeeded())
	{
		OverheadHealthBar->SetWidgetClass(BarClassFinder.Class);
	}
}

void ADummyCombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	bIsDummy = true;
	CachedSpawnTransform = GetActorTransform();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}
}

void ADummyCombatCharacter::Tick(float DeltaSeconds)
{
	// 조준선·이동 입력이 없으므로 공통 틱(상태 갱신)만 돌린다. 틱 자체는 피격 연출을 위해 유지한다.
	Super::Tick(DeltaSeconds);

	RefreshOverheadHealthBar();
}

void ADummyCombatCharacter::RefreshOverheadHealthBar()
{
	if (!OverheadHealthBar)
	{
		return;
	}

	const float MaxHealth = FMath::Max(GetMaxHealth(), 1.0f);
	const float Health = GetHealth();
	const bool bShouldShow = Health < MaxHealth - 0.01f;

	if (bShouldShow != bOverheadBarVisible)
	{
		bOverheadBarVisible = bShouldShow;
		OverheadHealthBar->SetVisibility(bShouldShow, true);

		UE_LOG(LogCombat, Log, TEXT("COMBAT_ENEMY_HEALTHBAR_%s target=%s health=%.1f/%.1f"),
			bShouldShow ? TEXT("SHOW") : TEXT("HIDE"), *GetName(), Health, MaxHealth);
	}

	if (!bShouldShow)
	{
		return;
	}

	// 표시 상태에서만 갱신한다. 위젯 생성은 컴포넌트가 맡는다.
	if (UEnemyHealthBarWidget* Bar = Cast<UEnemyHealthBarWidget>(OverheadHealthBar->GetUserWidgetObject()))
	{
		Bar->SetHealth(Health, MaxHealth);
	}
}

void ADummyCombatCharacter::GrantCombatAbilities()
{
	// 표적은 사격·재장전 능력을 받지 않는다.
}

void ADummyCombatCharacter::HandleDeath()
{
	if (IsDead())
	{
		return;
	}

	UE_LOG(LogCombat, Log, TEXT("COMBAT_DUMMY_DEATH dummy=%s"), *GetName());

	// 사망 처리 자체는 공통 경로를 따르되(사망 상태·로드), 부활 타이머 대신 같은 프레임에 되돌린다.
	Super::HandleDeath();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}

	RespawnFromDeath();
}

void ADummyCombatCharacter::RespawnFromDeath()
{
	const FCombatTuning& Tuning = CombatConfig::Tuning();

	SetActorTransform(CachedSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCombatAttributeSet* Attributes = GetCombatAttributeSet())
	{
		Attributes->SetHealth(Tuning.MaxHealth);
	}

	// 공통 사망 플래그를 직접 되돌린다(Super::RespawnFromDeath 는 플레이어용 시작 지점을 쓴다).
	bDead = false;
	bAiming = false;
	bSprinting = false;
	FlashElapsedSeconds = -1.0f;
	KnockbackRemainingSeconds = 0.0f;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}

	SyncHudState();

	UE_LOG(LogCombat, Log, TEXT("COMBAT_DUMMY_RESPAWN dummy=%s health=%.1f"), *GetName(), GetHealth());
}
