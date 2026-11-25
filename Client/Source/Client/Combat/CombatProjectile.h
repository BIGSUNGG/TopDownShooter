#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;

/**
 * 피스톨 탄환.
 *
 * 속도 8000/s 에서 한 프레임 이동 거리가 벽 두께보다 클 수 있으므로 ProjectileMovementComponent 의
 * 스윕 충돌(bSweepCollision)로 이전 위치 → 현재 위치를 훑어 벽·캐릭터를 건너뛰지 않게 한다.
 * 명중하면 ICombatDamageable::ApplyCombatDamage 로 피해를 넘기고 스스로 사라진다.
 */
UCLASS()
class CLIENT_API ACombatProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACombatProjectile();

	/** 발사 직후 호출. 방향과 속도, 수명(사거리/속도), 피해량을 설정한다. */
	void InitProjectile(const FVector& Direction, AActor* InCauser);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

	/** 명중 지점에 기본 도형 기반 임팩트를 남기고 액터를 정리한다. */
	void FinishImpact(const FVector& ImpactLocation, AActor* OtherActor);

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** 동적 인스턴스를 만들 때 쓰는 부모 머티리얼(기본 도형 머티리얼). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** 사운드 훅. 에셋이 없으면 조용히 넘어간다. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TObjectPtr<USoundBase> ImpactSound;

	/** 이 탄환의 피해량. CombatConfig 에서 채운다. */
	UPROPERTY(VisibleInstanceOnly, Category = "Combat")
	float DamageAmount = 0.0f;

	/** 발사한 액터. 자기 자신·소유자에게는 피해를 주지 않는다. */
	UPROPERTY(VisibleInstanceOnly, Category = "Combat")
	TObjectPtr<AActor> CauserActor;

	/** OnComponentHit 과 OnProjectileStop 이 둘 다 들어올 수 있어 임팩트를 한 번만 처리한다. */
	bool bImpactHandled = false;
};

/**
 * 임팩트 연출용 1회성 액터(0.3초). 전용 파티클 에셋 없이 기본 도형을 키웠다 지우는 방식.
 */
UCLASS()
class CLIENT_API ACombatImpactBurst : public AActor
{
	GENERATED_BODY()

public:
	ACombatImpactBurst();

	void StartBurst(const FVector& WorldLocation, const FLinearColor& Color);

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UStaticMeshComponent> BurstMesh;

	/** 생성자에서 한 번 찾아 둔다(ConstructorHelpers 는 생성자 밖에서 쓸 수 없다). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BurstMaterial;

	float ElapsedSeconds = 0.0f;
	float DurationSeconds = 0.3f;
};
