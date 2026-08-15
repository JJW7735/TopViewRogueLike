// Fill out your copyright notice in the Description page of Project Settings.


#include "RLCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RLHealthComponent.h"

// Sets default values
ARLCharacter::ARLCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // 캐릭터 회전과 카메라 회전 분리
	CameraBoom->TargetArmLength = 1300.f;       // 탑뷰 카메라 거리
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f)); // 쿼터뷰 각도
	CameraBoom->bDoCollisionTest = false;      // 벽 충돌 줌인 방지

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f); // 빠른 회전
	GetCharacterMovement()->bConstrainToPlane = true;

	GetCharacterMovement()->bRequestedMoveUseAcceleration = true;
	GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;

	bUseControllerRotationYaw = false;

	// 생성자 맨 아래에 체력 컴포넌트를 조립해 줍니다.
	HealthComp = CreateDefaultSubobject<URLHealthComponent>(TEXT("HealthComp"));
}

// Called when the game starts or when spawned
void ARLCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARLCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ARLCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

