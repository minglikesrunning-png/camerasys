// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/CameraPipeline.h"
#include "MyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

UCameraPipeline::UCameraPipeline()
{
	// 设置组件每帧Tick
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // 在物理和动画之后更新

	// 初始化默认混合配置
	DefaultBlendInfo = FCameraBlendInfo(0.5f, 2.0f, false);

	// 默认状态
	CurrentState = ECameraPipelineState::Free;
	PreviousState = ECameraPipelineState::None;

	// 混合状态初始化
	bIsBlending = false;
	StateBlendTimer = 0.0f;
	StateBlendDuration = 0.5f;
	CurrentBlendExponent = 2.0f;

	// 后处理配置
	bEnableCollisionDetection = true;
	CollisionSafetyFactor = 0.9f;
	MinimumArmLength = 50.0f;

	// 调试设置
	bEnableDebugLogs = false;
	bShowDebugInfo = false;
	bShowCollisionDebug = false;
}

void UCameraPipeline::BeginPlay()
{
	Super::BeginPlay();

	// 缓存组件引用
	CacheComponents();

	// 初始化所有状态实例
	InitializeStates();

	// 激活初始状态
	if (StateInstances.Contains(CurrentState))
	{
		UCameraStateBase* InitialState = StateInstances[CurrentState];
		if (InitialState)
		{
			InitialState->OnEnterState(OwnerCharacter, nullptr);
		}
	}

	if (bEnableDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraPipeline: Initialized with %d states"), StateInstances.Num());
	}
}

void UCameraPipeline::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 验证组件引用
	if (!OwnerCharacter || !CachedSpringArm || !CachedCamera)
	{
		if (bEnableDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraPipeline: Missing component references"));
		}
		return;
	}

	// 获取当前状态实例
	UCameraStateBase* CurrentStateInstance = StateInstances.FindRef(CurrentState);
	if (!CurrentStateInstance)
	{
		if (bEnableDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraPipeline: No state instance for current state"));
		}
		return;
	}

	// 计算当前状态输出
	FCameraStateOutput NewOutput = CurrentStateInstance->CalculateState(DeltaTime, OwnerCharacter, CachedSpringArm, CachedCamera);
	
	// 如果正在切换状态，进行混合
	if (bIsBlending && StateBlendTimer < StateBlendDuration && PreviousState != ECameraPipelineState::None)
	{
		StateBlendTimer += DeltaTime;
		float Alpha = FMath::Clamp(StateBlendTimer / StateBlendDuration, 0.0f, 1.0f);
		
		// 使用缓动函数实现平滑过渡
		Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, CurrentBlendExponent);
		
		// 混合两个状态的输出
		CurrentStateOutput = BlendStates(PreviousStateOutput, NewOutput, Alpha);
		
		// 检查混合是否完成
		if (StateBlendTimer >= StateBlendDuration)
		{
			bIsBlending = false;
			if (bEnableDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("CameraPipeline: Blend completed"));
			}
		}
	}
	else
	{
		// 不混合时直接使用当前状态输出
		CurrentStateOutput = NewOutput;
		bIsBlending = false;
	}
	
	// 应用后处理（碰撞检测等）
	FinalOutput = CurrentStateOutput;
	ApplyPostProcessing(FinalOutput);

	// 应用相机输出到组件
	ApplyCameraOutput();

	// 调试显示
	if (bShowDebugInfo && GEngine)
	{
		FString StateText = UEnum::GetValueAsString(CurrentState);
		GEngine->AddOnScreenDebugMessage(
			1, 
			0.0f, 
			FColor::Cyan, 
			FString::Printf(TEXT("Camera State: %s %s"), *StateText, bIsBlending ? TEXT("[BLENDING]") : TEXT(""))
		);
		GEngine->AddOnScreenDebugMessage(
			2, 
			0.0f, 
			FColor::White, 
			FString::Printf(TEXT("Arm Length: %.1f | FOV: %.1f"), FinalOutput.ArmLength, FinalOutput.FieldOfView)
		);
		if (bIsBlending)
		{
			float BlendProgress = (StateBlendTimer / StateBlendDuration) * 100.0f;
			GEngine->AddOnScreenDebugMessage(
				3, 
				0.0f, 
				FColor::Yellow, 
				FString::Printf(TEXT("Blend Progress: %.1f%%"), BlendProgress)
			);
		}
	}
}

bool UCameraPipeline::SetCameraState(ECameraPipelineState NewState, bool bForceChange)
{
	// 如果已经是目标状态，不需要切换
	if (CurrentState == NewState && !bForceChange)
	{
		if (bEnableDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("CameraPipeline: Already in state %s"), *UEnum::GetValueAsString(NewState));
		}
		return true;
	}

	// 检查状态实例是否存在
	if (!StateInstances.Contains(NewState))
	{
		if (bEnableDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraPipeline: State %s not found in StateInstances"), *UEnum::GetValueAsString(NewState));
		}
		return false;
	}

	// 优先级检查（如果不是强制切换）
	if (!bForceChange)
	{
		UCameraStateBase* CurrentStateInstance = StateInstances.FindRef(CurrentState);
		UCameraStateBase* NewStateInstance = StateInstances.FindRef(NewState);

		if (CurrentStateInstance && NewStateInstance)
		{
			// 如果当前状态不可被中断，且新状态优先级不高于当前状态，则拒绝切换
			if (!CurrentStateInstance->GetCanBeInterrupted() && 
				NewStateInstance->GetPriority() <= CurrentStateInstance->GetPriority())
			{
				if (bEnableDebugLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("CameraPipeline: State switch rejected - current state cannot be interrupted"));
				}
				return false;
			}
		}
	}

	// 执行状态切换
	return SwitchToState(NewState);
}

UCameraStateBase* UCameraPipeline::GetStateInstance(ECameraPipelineState State) const
{
	return StateInstances.FindRef(State);
}

void UCameraPipeline::ApplyCameraOutput()
{
	if (!CachedSpringArm || !CachedCamera)
	{
		return;
	}

	// 应用弹簧臂长度
	CachedSpringArm->TargetArmLength = FinalOutput.ArmLength;

	// 应用相机旋转
	CachedSpringArm->SetWorldRotation(FinalOutput.TargetRotation);

	// 应用视野角度
	CachedCamera->SetFieldOfView(FinalOutput.FieldOfView);

	// 注意：位置通常由SpringArm自动处理，除非有特殊需求
	// 如果需要直接设置位置，可以使用：
	// CachedSpringArm->SetWorldLocation(FinalOutput.TargetPosition);
}

FCameraStateOutput UCameraPipeline::BlendStates(const FCameraStateOutput& From, const FCameraStateOutput& To, float Alpha) const
{
	FCameraStateOutput Result;
	
	// 混合位置
	Result.TargetPosition = FMath::Lerp(From.TargetPosition, To.TargetPosition, Alpha);
	
	// 混合旋转（使用四元数插值以避免万向节锁）
	FQuat FromQuat = From.TargetRotation.Quaternion();
	FQuat ToQuat = To.TargetRotation.Quaternion();
	FQuat BlendedQuat = FQuat::Slerp(FromQuat, ToQuat, Alpha);
	Result.TargetRotation = BlendedQuat.Rotator();
	
	// 混合臂长
	Result.ArmLength = FMath::Lerp(From.ArmLength, To.ArmLength, Alpha);
	
	// 混合视野角度
	Result.FieldOfView = FMath::Lerp(From.FieldOfView, To.FieldOfView, Alpha);
	
	return Result;
}

void UCameraPipeline::ApplyPostProcessing(FCameraStateOutput& Output)
{
	if (!bEnableCollisionDetection || !OwnerCharacter)
	{
		return;
	}

	// 射线检测防穿墙
	FVector Start = OwnerCharacter->GetActorLocation();
	FVector End = Start - (Output.TargetRotation.Vector() * Output.ArmLength);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerCharacter);
	Params.bTraceComplex = false; // 使用简单碰撞以提高性能

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Camera, Params))
	{
		// 调整臂长避免穿墙
		float HitDistance = FVector::Dist(Start, Hit.Location);
		float NewLength = HitDistance * CollisionSafetyFactor;
		Output.ArmLength = FMath::Max(MinimumArmLength, NewLength);

		if (bEnableDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("CameraPipeline: Collision detected, adjusting arm length from %.1f to %.1f"), 
				Output.ArmLength, NewLength);
		}

		// 调试绘制
		if (bShowCollisionDebug)
		{
			DrawDebugLine(GetWorld(), Start, Hit.Location, FColor::Red, false, 0.0f, 0, 2.0f);
			DrawDebugSphere(GetWorld(), Hit.Location, 10.0f, 8, FColor::Red, false, 0.0f);
		}
	}
	else if (bShowCollisionDebug)
	{
		// 无碰撞时绘制绿线
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 0.0f, 0, 1.0f);
	}
}

void UCameraPipeline::InitializeStates()
{
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("CameraPipeline: OwnerCharacter is null, cannot initialize states"));
		return;
	}

	// 清空现有实例
	StateInstances.Empty();

	// 遍历配置的状态类，创建实例
	for (const auto& StateClassPair : StateClasses)
	{
		ECameraPipelineState StateType = StateClassPair.Key;
		TSubclassOf<UCameraStateBase> StateClass = StateClassPair.Value;

		if (StateClass)
		{
			// 创建状态实例
			UCameraStateBase* NewStateInstance = NewObject<UCameraStateBase>(this, StateClass);
			if (NewStateInstance)
			{
				StateInstances.Add(StateType, NewStateInstance);

				if (bEnableDebugLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("CameraPipeline: Created state instance for %s"), *UEnum::GetValueAsString(StateType));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("CameraPipeline: Failed to create state instance for %s"), *UEnum::GetValueAsString(StateType));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraPipeline: No class specified for state %s"), *UEnum::GetValueAsString(StateType));
		}
	}

	// 确保至少有一个Free状态
	if (!StateInstances.Contains(ECameraPipelineState::Free))
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraPipeline: No Free state configured, creating default"));
		UCameraStateBase* DefaultState = NewObject<UCameraStateBase>(this, UCameraStateBase::StaticClass());
		if (DefaultState)
		{
			DefaultState->SetStateName(TEXT("DefaultFree"));
			StateInstances.Add(ECameraPipelineState::Free, DefaultState);
		}
	}
}

bool UCameraPipeline::SwitchToState(ECameraPipelineState NewState)
{
	UCameraStateBase* CurrentStateInstance = StateInstances.FindRef(CurrentState);
	UCameraStateBase* NewStateInstance = StateInstances.FindRef(NewState);

	if (!NewStateInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("CameraPipeline: Cannot switch to state %s - instance not found"), *UEnum::GetValueAsString(NewState));
		return false;
	}

	// 保存切换前的输出作为混合起点
	PreviousStateOutput = CurrentStateOutput;
	
	// 调用当前状态的OnExitState
	if (CurrentStateInstance)
	{
		CurrentStateInstance->OnExitState(OwnerCharacter, NewStateInstance);
	}

	// 保存上一个状态
	PreviousState = CurrentState;
	
	// 更新状态
	CurrentState = NewState;

	// 获取混合配置
	FCameraBlendInfo* BlendInfo = BlendSettings.Find(NewState);
	if (!BlendInfo)
	{
		BlendInfo = &DefaultBlendInfo;
	}

	// 设置混合参数
	StateBlendDuration = BlendInfo->BlendTime;
	CurrentBlendExponent = BlendInfo->BlendExponent;
	StateBlendTimer = 0.0f;
	
	// 只有当混合时间大于0时才启用混合
	bIsBlending = (StateBlendDuration > 0.0f && PreviousState != ECameraPipelineState::None);

	// 调用新状态的OnEnterState
	NewStateInstance->OnEnterState(OwnerCharacter, CurrentStateInstance);

	if (bEnableDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraPipeline: Switched from %s to %s (Blending: %s, Duration: %.2f)"), 
			*UEnum::GetValueAsString(PreviousState), 
			*UEnum::GetValueAsString(CurrentState),
			bIsBlending ? TEXT("Yes") : TEXT("No"),
			StateBlendDuration);
	}

	return true;
}

void UCameraPipeline::CacheComponents()
{
	// 获取拥有者角色
	OwnerCharacter = Cast<AMyCharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("CameraPipeline: Owner is not AMyCharacter"));
		return;
	}

	// 查找SpringArm组件
	CachedSpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
	if (!CachedSpringArm)
	{
		UE_LOG(LogTemp, Error, TEXT("CameraPipeline: SpringArm component not found on owner"));
	}

	// 查找Camera组件
	CachedCamera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
	if (!CachedCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("CameraPipeline: Camera component not found on owner"));
	}

	if (bEnableDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraPipeline: Components cached - SpringArm: %s, Camera: %s"), 
			CachedSpringArm ? TEXT("Valid") : TEXT("NULL"),
			CachedCamera ? TEXT("Valid") : TEXT("NULL"));
	}
}
