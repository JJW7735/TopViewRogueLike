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
	// Sets default values for this character's properties
	ARLCharacter();
	// 1. 외부(히트박스)에서 이 캐릭터를 때렸을 때 실행되는 피격 수신 함수
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ReceiveCustomDamage(const FRLDamageData& DamageData, const FHitResult& HitInfo);
	// 2. 무적 상태(회피/대시) On/Off 함수
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetInvulnerable(bool bNewState) { bIsInvulnerable = bNewState; }
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInvulnerable() const { return bIsInvulnerable; }
	// 3. 직사각형(Box) 히트박스 생성 함수
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CreateBoxHitbox(FVector BoxExtent, float ForwardOffset, const FRLDamageData& DamageData, bool bDrawDebug = true);
	// 4. 테스트용 공격 실행 함수
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TestAttack();
	UFUNCTION(BlueprintPure, Category = "Components")
	URLSkillComponent* GetSkillComponent() const { return SkillComp; }
	// 블루프린트에서 쉽게 테스트할 수 있도록 노드로 노출합니다.
	UFUNCTION(BlueprintCallable, Category = "Action")
	void Dodge(FVector CursorLocation);
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* TopDownCamera;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat")
	bool bIsInvulnerable = false;
	// 체력 컴포넌트 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class URLHealthComponent* HealthComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	URLSkillComponent* SkillComp;

	// 슬롯별 기본 장착 스킬 (캐릭터 블루프린트에서 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_Q;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_W;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_E;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Default")
	URLSkillData* DefaultSkill_R;

	UFUNCTION()
	virtual void HandleDeath();
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};