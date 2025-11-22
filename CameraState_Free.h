// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraStateBase.h"
#include "CameraState_Free.generated.h"

/**
 * 自由相机状态
 * 保持SpringArm的默认行为，跟随角色朝向
 */
UCLASS(Blueprintable, BlueprintType)
class SOUL_API UCameraState_Free : public UCameraStateBase
{
	GENERATED_BODY()

public:
	/** 构造函数 */
	UCameraState_Free();

	/** 
	 * 计算自由相机状态
	 * 使用角色当前朝向，保持默认SpringArm行为
	 */
	virtual FCameraStateOutput CalculateState_Implementation(float DeltaTime, AMyCharacter* OwnerCharacter, USpringArmComponent* SpringArm, UCameraComponent* Camera) override;

	/**
	 * 进入自由相机状态
	 */
	virtual void OnEnterState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* PreviousState) override;

	/**
	 * 退出自由相机状态
	 */
	virtual void OnExitState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* NextState) override;

protected:
	// ==================== 自由相机配置参数 ====================
	
	/** 默认弹簧臂长度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Free Camera|Configuration", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
	float DefaultArmLength = 300.0f;

	/** 相机视野角度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Free Camera|Configuration", meta = (ClampMin = "60.0", ClampMax = "120.0"))
	float FieldOfView = 90.0f;

	/** 相机灵敏度（控制旋转响应速度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Free Camera|Configuration", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float LookSensitivity = 1.0f;

	/** 是否启用相机平滑插值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Free Camera|Configuration")
	bool bEnableSmoothRotation = false;

	/** 相机旋转平滑插值速度（仅在bEnableSmoothRotation为true时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Free Camera|Configuration", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float SmoothRotationSpeed = 5.0f;
};
