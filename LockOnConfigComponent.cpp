// Fill out your copyright notice in the Description page of Project Settings.

#include "LockOnConfigComponent.h"

// Sets default values for this component's properties
ULockOnConfigComponent::ULockOnConfigComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false; // 此组件不需要Tick，节省性能

	// ==================== 默认值设置 ====================
	bOverrideOffset = false;
	CustomOffset = FVector(0, 0, -50.0f);
	bPreferSocket = true;
	CustomSocketName = TEXT("LockOnSocket");
}

// Called when the game starts
void ULockOnConfigComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// ==================== 初始化验证 ====================
	if (bOverrideOffset)
	{
		UE_LOG(LogTemp, Log, TEXT("LockOnConfigComponent: Custom offset enabled for %s with offset %s"), 
			*GetOwner()->GetName(), *CustomOffset.ToString());
		
		if (bPreferSocket)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Preferred Socket: %s"), *CustomSocketName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("  - Using Capsule-based lock position"));
		}
	}
}

// Called every frame
void ULockOnConfigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// NOTE: Tick已禁用以优化性能（PrimaryComponentTick.bCanEverTick = false）
	// 如果需要运行时动态调整，可以重新启用Tick
}

// ==================== 辅助函数实现 ====================

bool ULockOnConfigComponent::IsConfigValid() const
{
	// 基础验证：确保Owner存在
	if (!GetOwner())
	{
		return false;
	}
	
	// 如果启用覆盖，验证自定义Socket名称是否有效
	if (bOverrideOffset && bPreferSocket)
	{
		// 检查Socket名称是否为空
		if (CustomSocketName.IsNone() || CustomSocketName == NAME_None)
		{
			UE_LOG(LogTemp, Warning, TEXT("LockOnConfigComponent: Invalid CustomSocketName for %s"), 
				*GetOwner()->GetName());
			return false;
		}
	}
	
	return true;
}

FVector ULockOnConfigComponent::GetEffectiveOffset() const
{
	if (bOverrideOffset)
	{
		return CustomOffset;
	}
	
	// 未启用覆盖时返回零向量（让调用者使用默认偏移）
	return FVector::ZeroVector;
}

FName ULockOnConfigComponent::GetEffectiveSocketName() const
{
	if (bOverrideOffset && bPreferSocket)
	{
		return CustomSocketName;
	}
	
	// 未启用覆盖时返回None（让调用者使用默认Socket）
	return NAME_None;
}
