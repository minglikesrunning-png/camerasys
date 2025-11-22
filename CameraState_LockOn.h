// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraStateBase.h"
#include "CameraState_LockOn.generated.h"

/**
 * 锁定相机状态
 * 让相机看向锁定目标，使用角色的GetOptimalLockPosition()获取目标位置
 */
UCLASS(Blueprintable, BlueprintType)
class SOUL_API UCameraState_LockOn : public UCameraStateBase
{
	GENERATED_BODY()

public:
	/** 构造函数 */
	UCameraState_LockOn();

	/** 
	 * 计算锁定相机状态
	 * 计算从角色到锁定目标的相机朝向
	 */
	virtual FCameraStateOutput CalculateState_Implementation(float DeltaTime, AMyCharacter* OwnerCharacter, USpringArmComponent* SpringArm, UCameraComponent* Camera) override;

	/**
	 * 进入锁定相机状态
	 */
	virtual void OnEnterState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* PreviousState) override;

	/**
	 * 退出锁定相机状态
	 */
	virtual void OnExitState_Implementation(AMyCharacter* OwnerCharacter, UCameraStateBase* NextState) override;

protected:
	// ==================== 锁定相机配置参数 ====================
	
	/** 锁定状态下的弹簧臂长度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Configuration", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
	float LockOnArmLength = 350.0f;

	/** 目标位置偏移（用于调整锁定点高度等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Configuration")
	FVector TargetOffset = FVector(0.0f, 0.0f, 0.0f);

	/** 相机跟踪目标的速度（插值速度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Configuration", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float TrackingSpeed = 5.0f;

	/** 锁定状态下的视野角度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Configuration", meta = (ClampMin = "60.0", ClampMax = "120.0"))
	float LockOnFieldOfView = 90.0f;

	/** 是否启用平滑跟踪（暂时设为false，直接返回计算结果） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Configuration")
	bool bEnableSmoothTracking = false;

	/** 相机偏移角度（可用于调整相机在目标上方/下方的角度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Configuration", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float CameraPitchOffset = 0.0f;

	// ==================== ✅ 新增：距离检测配置 ====================
	
	/** 是否启用距离自动解锁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Distance Detection", 
		meta = (DisplayName = "Enable Distance Auto Unlock"))
	bool bEnableDistanceAutoUnlock = true;

	/** 最大锁定距离（超过此距离自动解锁） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Distance Detection", 
		meta = (ClampMin = "500.0", ClampMax = "5000.0", EditCondition = "bEnableDistanceAutoUnlock"))
	float MaxLockOnDistance = 2000.0f;

	/** 距离检测间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn Camera|Distance Detection", 
		meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bEnableDistanceAutoUnlock"))
	float DistanceCheckInterval = 0.2f;

private:
	/** 上一帧的相机旋转（用于平滑插值） */
	FRotator LastCameraRotation = FRotator::ZeroRotator;

	/** 是否已经初始化上一帧旋转 */
	bool bLastRotationInitialized = false;

	// ==================== ✅ 新增：距离检测状态 ====================
	
	/** 上次距离检测时间 */
	float LastDistanceCheckTime = 0.0f;
};
