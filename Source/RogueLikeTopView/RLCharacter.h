// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RLTypes.h"
#include "RLCharacter.generated.h"
class USpringArmComponent;
class UCameraComponent;
class URLHealthComponent;
class URLSkillComponent;
class URLSkillData;
UCLASS()
class ROGUELIKETOPVIEW_API ARLCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ARLCharacter();

	UPROPERTY(BlueprintReadWrite, Category = "Skill")
	bool bIsSkillMoving = false;

	// ★ 부드러운 대시 이동 시작
	UFUNCTION(BlueprintCallable, Category = "Skill")
	void StartDashMovement(FVector TargetLocation, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo);
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetInvulnerable(bool bNewState) { bIsInvulnerable = bNewState; }
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInvulnerable() const { return bIsInvulnerable; }
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateBoxHitbox(FVector BoxExtent, float ForwardOffset, const FRLDamageData& DamageData, bool bDrawDebug = true);
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TestAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateLocationHitbox(FVector TargetLocation, float Radius, const FRLDamageData& DamageData, bool bDrawDebug = true);
	UFUNCTION(BlueprintCallable, Category = "Action")
	void Dodge(FVector CursorLocation);
	UFUNCTION(BlueprintPure, Category = "Skill")
	URLSkillComponent* GetSkillComponent() const { return SkillComp; }
protected:
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* TopDownCamera;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat")
	bool bIsInvulnerable = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class URLHealthComponent* HealthComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	URLSkillComponent* SkillComp;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_Q;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_W;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_E;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_R;

	// ★ 대시 이동 관련 변수
	bool bIsDashing = false;
	FVector DashStartLocation;
	FVector DashTargetLocation;
	float DashElapsedTime = 0.0f;
	float DashDuration = 0.0f;

	UFUNCTION()
	virtual void HandleDeath();
public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;
};