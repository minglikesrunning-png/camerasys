// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/CameraState_Free.h"
#include "MyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/KismetMathLibrary.h"

UCameraState_Free::UCameraState_Free()
{
	// 设置自由相机状态的基本属性
	StateName = TEXT("FreeCamera");
	Priority = 0; // 最低优先级
	bCanBeInterrupted = true; // 可以被任何状态中断

	// 初始化配置参数
	DefaultArmLength = 300.0f;
	FieldOfView = 90.0f;
	LookSensitivity = 1.0f;
	bEnableSmoothRotation = false;
	SmoothRotationSpeed = 5.0f;
}

FCameraStateOutput UCameraState_Free::CalculateState_Implementation(float DeltaTime, AMyCharacter* OwnerCharacter, USpringArmComponent* SpringArm, UCameraComponent* Camera)
{
	FCameraStateOutput Output;

	// 验证输入参数
	if (!OwnerCharacter || !SpringArm || !Camera)
	{
		UE_LOG(LogTemp, Warning, TEXT("UCameraState_Free: Invalid parameters in CalculateState"));
		return Output;
	}

	// 获取当前SpringArm的位置和旋转
	FVector CurrentArmLocation = SpringArm->GetComponentLocation();
	FRotator CurrentArmRotation = SpringArm->GetComponentRotation();

	// 如果启用平滑旋转，使用插值
	FRotator TargetRotation = CurrentArmRotation;
	if (bEnableSmoothRotation)
	{
		// 获取控制器旋转作为目标
		AController* Controller = OwnerCharacter->GetController();
		if (Controller)
		{
			FRotator ControlRotation = Controller->GetControlRotation();
			TargetRotation = UKismetMathLibrary::RInterpTo(CurrentArmRotation, ControlRotation, DeltaTime, SmoothRotationSpeed);
		}
	}
	else
	{
		// 直接使用控制器旋转
		AController* Controller = OwnerCharacter->GetController();
		if (Controller)
		{
			TargetRotation = Controller->GetControlRotation();
		}
	}

	// 填充输出结构
	Output.TargetPosition = CurrentArmLocation; // 位置由SpringArm自动处理
	Output.TargetRotation = TargetRotation;
	Output.ArmLength = DefaultArmLength;
	Output.FieldOfView = FieldOfView;

	// ==================== ✅ 新增：设置旋转控制标志 ====================
	// 自由状态：不强制设置旋转，让bUsePawnControlRotation自动工作
	// 这样玩家的Turn/LookUp输入可以正常修改ControlRotation，SpringArm会自动跟随
	Output.bForceSpringArmRotation = false;
	Output.bForceControllerRotation = false;

	return Output;
}

void UCameraState_Free::OnEnterState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* PreviousState)
{
	Super::OnEnterState_Implementation(OwnerCharacter, PreviousState);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 
			2.0f, 
			FColor::Cyan, 
			FString::Printf(TEXT("Camera State: Entered Free Camera Mode"))
		);
	}

	UE_LOG(LogTemp, Log, TEXT("UCameraState_Free: Entered state from %s"), 
		PreviousState ? *PreviousState->GetStateName() : TEXT("None"));
}

void UCameraState_Free::OnExitState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* NextState)
{
	Super::OnExitState_Implementation(OwnerCharacter, NextState);

	UE_LOG(LogTemp, Log, TEXT("UCameraState_Free: Exiting to %s"), 
		NextState ? *NextState->GetStateName() : TEXT("None"));
}
