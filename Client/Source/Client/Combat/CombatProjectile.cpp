#include "Combat/CombatProjectile.h"

#include "Combat/CombatConfig.h"
#include "Combat/CombatDamageable.h"
#include "Combat/CombatLog.h"
#include "Combat/CombatVisuals.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ACombatProjectile::ACombatProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(4.0f);
	CollisionSphere->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionSphere->SetGenerateOverlapEvents(false);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionSphere;

	// 기본 구(100uu)를 4cm로 줄인 시각 메시. 충돌에는 관여하지 않는다.
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionSphere);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetRelativeScale3D(FVector(0.04f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterialFinder(CombatVisuals::BaseMaterialPath);
	if (BaseMaterialFinder.Succeeded())
	{
		BaseMaterial = BaseMaterialFinder.Object;
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	// 8000uu/s 에서도 벽을 건너뛰지 않도록 스윕 충돌을 켠다.
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f;
}

void ACombatProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionSphere->OnComponentHit.AddDynamic(this, &ACombatProjectile::OnProjectileHit);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ACombatProjectile::OnProjectileStopped);

	if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this))
	{
		VisualMesh->SetMaterial(0, Material);
		CombatVisuals::SetColor(Material, FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
	}
}

void ACombatProjectile::InitProjectile(const FVector& Direction, AActor* InCauser)
{
	CauserActor = InCauser;
	DamageAmount = CombatConfig::Tuning().PistolDamage;

	const FCombatTuning& Tuning = CombatConfig::Tuning();
	const FVector NormalizedDirection = Direction.GetSafeNormal();

	ProjectileMovement->InitialSpeed = Tuning.ProjectileSpeedUUPerSec;
	ProjectileMovement->MaxSpeed = Tuning.ProjectileSpeedUUPerSec;
	ProjectileMovement->Velocity = NormalizedDirection * Tuning.ProjectileSpeedUUPerSec;
	ProjectileMovement->Activate(true);

	SetActorRotation(NormalizedDirection.Rotation());

	// 사거리/속도 = 최대 비행 시간. 그 뒤에는 스스로 사라진다.
	if (Tuning.ProjectileSpeedUUPerSec > 0.0f)
	{
		SetLifeSpan(Tuning.WeaponRangeUU / Tuning.ProjectileSpeedUUPerSec);
	}
}

void ACombatProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bImpactHandled)
	{
		return;
	}

	FinishImpact(Hit.ImpactPoint, OtherActor);
}

void ACombatProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	if (bImpactHandled)
	{
		return;
	}

	FinishImpact(ImpactResult.ImpactPoint, ImpactResult.GetActor());
}

void ACombatProjectile::FinishImpact(const FVector& ImpactLocation, AActor* OtherActor)
{
	if (bImpactHandled)
	{
		return;
	}
	bImpactHandled = true;

	const FVector Direction = GetVelocity().GetSafeNormal();
	const bool bIsSelf = OtherActor == nullptr || OtherActor == this || OtherActor == CauserActor;

	if (!bIsSelf)
	{
		if (ICombatDamageable* Victim = Cast<ICombatDamageable>(OtherActor))
		{
			Victim->ApplyCombatDamage(DamageAmount, Direction, CauserActor, ImpactLocation);
			UE_LOG(LogCombat, Verbose, TEXT("COMBAT_PROJECTILE_HIT victim=%s damage=%.1f"), *OtherActor->GetName(), DamageAmount);
		}
	}

	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACombatImpactBurst* Burst = World->SpawnActor<ACombatImpactBurst>(ACombatImpactBurst::StaticClass(), ImpactLocation, FRotator::ZeroRotator, SpawnParameters))
		{
			Burst->StartBurst(ImpactLocation, FLinearColor(1.0f, 0.9f, 0.4f, 1.0f));
		}

		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactLocation);
		}
	}

	Destroy();
}

ACombatImpactBurst::ACombatImpactBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	BurstMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BurstMesh"));
	BurstMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BurstMesh->SetCastShadow(false);
	BurstMesh->SetRelativeScale3D(FVector(0.12f));
	RootComponent = BurstMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		BurstMesh->SetStaticMesh(SphereMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterialFinder(CombatVisuals::BaseMaterialPath);
	if (BaseMaterialFinder.Succeeded())
	{
		BaseMaterial = BaseMaterialFinder.Object;
	}
}

void ACombatImpactBurst::StartBurst(const FVector& WorldLocation, const FLinearColor& Color)
{
	DurationSeconds = CombatConfig::Tuning().ImpactBurstSeconds;
	ElapsedSeconds = 0.0f;

	SetActorLocation(WorldLocation);

	if (BaseMaterial)
	{
		BurstMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		CombatVisuals::SetColor(BurstMaterial, Color);
		BurstMesh->SetMaterial(0, BurstMaterial);
	}

	SetLifeSpan(DurationSeconds);
}

void ACombatImpactBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedSeconds += DeltaSeconds;

	const float Alpha = (DurationSeconds > 0.0f) ? FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f) : 1.0f;

	// 0.12 → 0.45 로 커지면서 사라진다(기본 구 100uu 기준 스케일).
	const float Scale = FMath::Lerp(0.12f, 0.45f, Alpha);
	BurstMesh->SetRelativeScale3D(FVector(Scale));

	if (BurstMaterial)
	{
		FLinearColor Color = FLinearColor(1.0f, 0.9f, 0.4f, 1.0f);
		Color.A = 1.0f - Alpha;
		CombatVisuals::SetColor(BurstMaterial, Color);
	}
}
