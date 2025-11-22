// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/CameraStateBase.h"
#include "MyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

UCameraStateBase::UCameraStateBase()
{
	// 默认优先级
	Priority = 0;
	bCanBeInterrupted = true;
	StateName = TEXT("BaseState");
}

FCameraStateOutput UCameraStateBase::CalculateState_Implementation(float DeltaTime, AMyCharacter* OwnerCharacter, USpringArmComponent* SpringArm, UCameraComponent* Camera)
{
	// 基类默认实现：返回当前相机的状态
	FCameraStateOutput Output;

	if (SpringArm && Camera)
	{
		Output.TargetPosition = SpringArm->GetComponentLocation();
		Output.TargetRotation = SpringArm->GetComponentRotation();
		Output.ArmLength = SpringArm->TargetArmLength;
		Output.FieldOfView = Camera->FieldOfView;
	}

	return Output;
}

void UCameraStateBase::OnEnterState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* PreviousState)
{
	// 基类默认实现：记录日志
	if (GEngine && OwnerCharacter)
	{
		FString PreviousStateName = PreviousState ? PreviousState->StateName : TEXT("None");
		GEngine->AddOnScreenDebugMessage(
			-1, 
			2.0f, 
			FColor::Green, 
			FString::Printf(TEXT("Camera State: %s -> %s"), *PreviousStateName, *StateName)
		);
	}
}

void UCameraStateBase::OnExitState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* NextState)
{
	// 基类默认实现：记录日志
	if (GEngine && OwnerCharacter)
	{
		FString NextStateName = NextState ? NextState->StateName : TEXT("None");
		GEngine->AddOnScreenDebugMessage(
			-1, 
			2.0f, 
			FColor::Yellow, 
			FString::Printf(TEXT("Camera State Exiting: %s -> %s"), *StateName, *NextStateName)
		);
	}
}
