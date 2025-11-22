// Fill out your copyright notice in the Description page of Project Settings.

#include "UIManagerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Sets default values for this component's properties
UUIManagerComponent::UUIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	// Initialize state
	LockOnWidgetInstance = nullptr;
	PreviousLockOnTarget = nullptr;
	CurrentLockOnTarget = nullptr;
	OwnerCharacter = nullptr;
	OwnerController = nullptr;
	CurrentUIScale = 1.0f;
	CurrentUIColor = FLinearColor::White;
	CurrentUIDisplayMode = EUIDisplayMode::ScreenSpace; // FIXED: Changed from Hybrid which doesn't exist in EUIDisplayMode
	bEnableUIDebugLogs = false;
	bEnableSizeAnalysisDebugLogs = false;
	
	UE_LOG(LogTemp, Log, TEXT("UIManagerComponent: Initialized"));
}

// Called when the game starts
void UUIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeUIManager();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManagerComponent: BeginPlay called"));
}

// Called every frame
void UUIManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// ==================== 关键修复：添加有效性检查，防止无效目标重新显示UI ====================
	// Update projection widget if active
	if (CurrentLockOnTarget && IsValid(CurrentLockOnTarget) && LockOnWidgetInstance)
	{
		UpdateProjectionWidget(CurrentLockOnTarget);
	}
	else if (LockOnWidgetInstance && LockOnWidgetInstance->IsInViewport())
	{
		// 如果没有有效目标但Widget仍在视口，强制移除
		if (bEnableUIDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("UIManagerComponent::Tick - No valid target, removing orphaned widget"));
		}
		
		LockOnWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
		LockOnWidgetInstance->RemoveFromViewport();
		LockOnWidgetInstance = nullptr;
		CurrentLockOnTarget = nullptr;
	}
}

// ==================== Core Public Interface ====================

void UUIManagerComponent::ShowLockOnWidget(AActor* Target)
{
	if (!IsValidTargetForUI(Target))
	{
		return;
	}
	
	HideAllLockOnWidgets();
	CurrentLockOnTarget = Target;
	
	switch (CurrentUIDisplayMode)
	{
		case EUIDisplayMode::SocketProjection:
		case EUIDisplayMode::ScreenSpace:
			ShowSocketProjectionWidget(Target);
			break;
			
		case EUIDisplayMode::SizeAdaptive:
		{
			EEnemySizeCategory SizeCategory = AnalyzeTargetSize(Target);
			ShowSizeAdaptiveWidget(Target, SizeCategory);
			break;
		}
			
		case EUIDisplayMode::Traditional3D:
		default:
			UActorComponent* WidgetComp = Target->GetComponentByClass(UWidgetComponent::StaticClass());
			if (WidgetComp)
			{
				UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>(WidgetComp);
				if (WidgetComponent)
				{
					WidgetComponent->SetVisibility(true);
					TargetsWithActiveWidgets.AddUnique(Target);
				}
			}
			break;
	}
	
	OnLockOnWidgetShown.Broadcast(Target);
}

void UUIManagerComponent::HideLockOnWidget()
{
	// ==================== 关键修复：先清空CurrentLockOnTarget，防止Tick重新显示UI ====================
	// 必须在最开始就清空，否则TickComponent会在下一帧重新显示Widget
	CurrentLockOnTarget = nullptr;
	
	// 隐藏所有传统3D Widget
	HideAllLockOnWidgets();
	
	// 确保ScreenSpace Widget被完全移除
	if (LockOnWidgetInstance)
	{
		if (LockOnWidgetInstance->IsInViewport())
		{
			// 先设置为隐藏（立即生效）
			LockOnWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
			// 然后从视口移除（彻底清理）
			LockOnWidgetInstance->RemoveFromViewport();
			
			if (bEnableUIDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("UIManagerComponent::HideLockOnWidget - Widget removed from viewport"));
			}
		}
		
		// 清空引用，确保下次重新创建
		LockOnWidgetInstance = nullptr;
	}
	
	// 广播隐藏事件
	OnLockOnWidgetHidden.Broadcast();
	
	if (bEnableUIDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("UIManagerComponent::HideLockOnWidget - All widgets hidden and CurrentLockOnTarget cleared"));
	}
}

void UUIManagerComponent::HideAllLockOnWidgets()
{
	// ==================== 关键修复：先清空CurrentLockOnTarget，防止Tick重新显示UI ====================
	CurrentLockOnTarget = nullptr;
	
	// 隐藏所有传统3D Widget组件
	for (AActor* Target : TargetsWithActiveWidgets)
	{
		if (IsValid(Target))
		{
			UActorComponent* WidgetComp = Target->GetComponentByClass(UWidgetComponent::StaticClass());
			if (WidgetComp)
			{
				UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>(WidgetComp);
				if (WidgetComponent && WidgetComponent->IsVisible())
				{
					WidgetComponent->SetVisibility(false);
				}
			}
		}
	}
	
	TargetsWithActiveWidgets.Empty();
	
	// 确保ScreenSpace Widget也被移除
	if (LockOnWidgetInstance)
	{
		if (LockOnWidgetInstance->IsInViewport())
		{
			// 先隐藏再移除
			LockOnWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
			LockOnWidgetInstance->RemoveFromViewport();
			
			if (bEnableUIDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("UIManagerComponent::HideAllLockOnWidgets - ScreenSpace widget removed and CurrentLockOnTarget cleared"));
			}
		}
		
		// 清空引用
		LockOnWidgetInstance = nullptr;
	}
}

void UUIManagerComponent::UpdateLockOnWidget(AActor* CurrentTarget, AActor* PreviousTarget)
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (LastUIUpdateTarget == CurrentTarget && (CurrentTime - LastUIUpdateTime) < UI_UPDATE_INTERVAL)
	{
		return;
	}
	
	LastUIUpdateTarget = CurrentTarget;
	LastUIUpdateTime = CurrentTime;
	
	if (IsValid(PreviousTarget) && PreviousTarget != CurrentTarget)
	{
		UActorComponent* PrevWidgetComp = PreviousTarget->GetComponentByClass(UWidgetComponent::StaticClass());
		if (PrevWidgetComp)
		{
			UWidgetComponent* PrevWidgetComponent = Cast<UWidgetComponent>(PrevWidgetComp);
			if (PrevWidgetComponent && PrevWidgetComponent->IsVisible())
			{
				PrevWidgetComponent->SetVisibility(false);
			}
		}
		
		TargetsWithActiveWidgets.Remove(PreviousTarget);
	}
	
	if (IsValid(CurrentTarget))
	{
		CurrentLockOnTarget = CurrentTarget;
		ShowLockOnWidget(CurrentTarget);
	}
	
	PreviousLockOnTarget = PreviousTarget;
}

// ==================== Socket Projection Interface ====================

FVector UUIManagerComponent::GetTargetProjectionLocation(AActor* Target) const
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}
	
	switch (HybridProjectionSettings.ProjectionMode)
	{
		case EProjectionMode::ActorCenter:
			return Target->GetActorLocation();
			
		case EProjectionMode::BoundsCenter:
		{
			FVector Origin, BoxExtent;
			Target->GetActorBounds(false, Origin, BoxExtent);
			return Origin;
		}
			
		case EProjectionMode::CustomOffset:
			return Target->GetActorLocation() + HybridProjectionSettings.CustomOffset;
			
		case EProjectionMode::Hybrid:
		default:
		{
			USkeletalMeshComponent* SkeletalMesh = Target->FindComponentByClass<USkeletalMeshComponent>();
			if (SkeletalMesh)
			{
				FName SocketName = TEXT("Spine2Socket");
				if (SkeletalMesh->DoesSocketExist(SocketName))
				{
					return SkeletalMesh->GetSocketLocation(SocketName) + HybridProjectionSettings.CustomOffset;
				}
			}
			
			FVector Origin, BoxExtent;
			Target->GetActorBounds(false, Origin, BoxExtent);
			FVector BoundsOffset = FVector(0, 0, BoxExtent.Z * HybridProjectionSettings.BoundsOffsetRatio);
			return Origin + BoundsOffset + HybridProjectionSettings.CustomOffset;
		}
	}
}

bool UUIManagerComponent::HasValidSocket(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}
	
	USkeletalMeshComponent* SkeletalMesh = Target->FindComponentByClass<USkeletalMeshComponent>();
	if (SkeletalMesh)
	{
		FName SocketName = TEXT("Spine2Socket");
		return SkeletalMesh->DoesSocketExist(SocketName);
	}
	
	return false;
}

FVector2D UUIManagerComponent::ProjectSocketToScreen(const FVector& SocketWorldLocation) const
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return FVector2D::ZeroVector;
	}
	
	FVector2D ScreenLocation;
	bool bProjected = PC->ProjectWorldLocationToScreen(SocketWorldLocation, ScreenLocation);
	
	return bProjected ? ScreenLocation : FVector2D::ZeroVector;
}

FVector2D UUIManagerComponent::ProjectToScreen(const FVector& WorldLocation) const
{
	return ProjectSocketToScreen(WorldLocation);
}

void UUIManagerComponent::ShowSocketProjectionWidget(AActor* Target)
{
	if (!Target || !LockOnWidgetClass)
	{
		return;
	}
	
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}
	
	if (LockOnWidgetInstance && LockOnWidgetInstance->IsInViewport())
	{
		LockOnWidgetInstance->RemoveFromViewport();
	}
	
	LockOnWidgetInstance = CreateWidget<UUserWidget>(PC, LockOnWidgetClass);
	if (LockOnWidgetInstance)
	{
		LockOnWidgetInstance->AddToViewport();
		CurrentLockOnTarget = Target;
		UpdateProjectionWidget(Target);
	}
}

void UUIManagerComponent::UpdateProjectionWidget(AActor* Target)
{
	if (!Target || !LockOnWidgetInstance || !LockOnWidgetInstance->IsInViewport())
	{
		return;
	}
	
	FVector WorldLocation = GetTargetProjectionLocation(Target);
	FVector2D ScreenPosition = ProjectToScreen(WorldLocation);
	
	if (ScreenPosition != FVector2D::ZeroVector)
	{
		UFunction* UpdateFunction = LockOnWidgetInstance->GetClass()->FindFunctionByName(FName(TEXT("UpdateLockOnPostition")));
		
		if (UpdateFunction)
		{
			struct FUpdateParams
			{
				FVector2D ScreenPos;
			};
			
			FUpdateParams Params;
			Params.ScreenPos = ScreenPosition;
			
			LockOnWidgetInstance->ProcessEvent(UpdateFunction, &Params);
		}
		
		if (!LockOnWidgetInstance->IsVisible())
		{
			LockOnWidgetInstance->SetVisibility(ESlateVisibility::Visible);
		}
		
		OnSocketProjectionUpdated.Broadcast(Target, ScreenPosition);
	}
	else
	{
		if (LockOnWidgetInstance->IsVisible())
		{
			LockOnWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UUIManagerComponent::UpdateSocketProjectionWidget(AActor* Target)
{
	UpdateProjectionWidget(Target);
}

void UUIManagerComponent::HideSocketProjectionWidget()
{
	if (LockOnWidgetInstance && LockOnWidgetInstance->IsInViewport())
	{
		LockOnWidgetInstance->RemoveFromViewport();
		LockOnWidgetInstance = nullptr;
	}
}

// ==================== Size Adaptive UI Interface ====================

void UUIManagerComponent::ShowSizeAdaptiveWidget(AActor* Target, EEnemySizeCategory SizeCategory)
{
	if (!bEnableSizeAdaptiveUI)
	{
		ShowLockOnWidget(Target);
		return;
	}
	
	CurrentUIScale = GetUIScaleForEnemySize(SizeCategory);
	CurrentUIColor = GetUIColorForEnemySize(SizeCategory);
	
	ShowSocketProjectionWidget(Target);
	
	if (LockOnWidgetInstance)
	{
		UFunction* SetScaleFunc = LockOnWidgetInstance->GetClass()->FindFunctionByName(FName(TEXT("SetUIScale")));
		if (SetScaleFunc)
		{
			float Scale = CurrentUIScale;
			LockOnWidgetInstance->ProcessEvent(SetScaleFunc, &Scale);
		}
		
		UFunction* SetColorFunc = LockOnWidgetInstance->GetClass()->FindFunctionByName(FName(TEXT("SetUIColor")));
		if (SetColorFunc)
		{
			FLinearColor Color = CurrentUIColor;
			LockOnWidgetInstance->ProcessEvent(SetColorFunc, &Color);
		}
	}
}

void UUIManagerComponent::UpdateWidgetForEnemySize(AActor* Target, EEnemySizeCategory SizeCategory)
{
	if (!bEnableSizeAdaptiveUI || !LockOnWidgetInstance)
	{
		return;
	}
	
	CurrentUIScale = GetUIScaleForEnemySize(SizeCategory);
	CurrentUIColor = GetUIColorForEnemySize(SizeCategory);
	
	ShowSizeAdaptiveWidget(Target, SizeCategory);
}

float UUIManagerComponent::GetUIScaleForEnemySize(EEnemySizeCategory SizeCategory) const
{
	switch (SizeCategory)
	{
		case EEnemySizeCategory::Small:
			return SmallEnemyUIScale;
		case EEnemySizeCategory::Medium:
			return MediumEnemyUIScale;
		case EEnemySizeCategory::Large:
			return LargeEnemyUIScale;
		default:
			return 1.0f;
	}
}

FLinearColor UUIManagerComponent::GetUIColorForEnemySize(EEnemySizeCategory SizeCategory) const
{
	switch (SizeCategory)
	{
		case EEnemySizeCategory::Small:
			return SmallEnemyUIColor;
		case EEnemySizeCategory::Medium:
			return MediumEnemyUIColor;
		case EEnemySizeCategory::Large:
			return LargeEnemyUIColor;
		default:
			return FLinearColor::White;
	}
}

// ==================== Debug Interface ====================

void UUIManagerComponent::DebugWidgetSetup()
{
	UE_LOG(LogTemp, Warning, TEXT("=== UI MANAGER DEBUG ==="));
	UE_LOG(LogTemp, Warning, TEXT("Widget Class: %s"), LockOnWidgetClass ? TEXT("SET") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("Display Mode: %d"), static_cast<int32>(CurrentUIDisplayMode));
	UE_LOG(LogTemp, Warning, TEXT("Current Target: %s"), CurrentLockOnTarget ? *CurrentLockOnTarget->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Warning, TEXT("Widget Instance: %s"), LockOnWidgetInstance ? TEXT("EXISTS") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("======================"));
}

void UUIManagerComponent::LogWidgetState(const FString& Context)
{
	if (bEnableUIDebugLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] UIManager State - Target: %s, Widget: %s, Mode: %d"), 
			*Context,
			CurrentLockOnTarget ? *CurrentLockOnTarget->GetName() : TEXT("None"),
			LockOnWidgetInstance ? TEXT("Active") : TEXT("Inactive"),
			static_cast<int32>(CurrentUIDisplayMode));
	}
}

bool UUIManagerComponent::ValidateWidgetClass() const
{
	return LockOnWidgetClass != nullptr;
}

// ==================== Configuration Accessors ====================

void UUIManagerComponent::SetUIDisplayMode(EUIDisplayMode NewMode)
{
	CurrentUIDisplayMode = NewMode;
}

void UUIManagerComponent::SetHybridProjectionSettings(const FHybridProjectionSettings& NewSettings)
{
	HybridProjectionSettings = NewSettings;
}

void UUIManagerComponent::SetAdvancedCameraSettings(const FAdvancedCameraSettings& NewSettings)
{
	AdvancedCameraSettings = NewSettings;
}

void UUIManagerComponent::SwitchTargetPart(ETargetBodyPart NewPart)
{
	MultiPartConfig.TargetPart = NewPart;
	
	if (MultiPartConfig.PartOffsets.Contains(NewPart))
	{
		HybridProjectionSettings.CustomOffset = MultiPartConfig.PartOffsets[NewPart];
	}
}

ETargetBodyPart UUIManagerComponent::GetBestTargetPart(AActor* Target) const
{
	if (!MultiPartConfig.bEnableSmartSelection)
	{
		return MultiPartConfig.TargetPart;
	}
	
	EEnemySizeCategory SizeCategory = AnalyzeTargetSize(Target);
	
	switch (SizeCategory)
	{
		case EEnemySizeCategory::Small:
			return ETargetBodyPart::Center;
		case EEnemySizeCategory::Large:
			return ETargetBodyPart::Chest;
		default:
			return ETargetBodyPart::Chest;
	}
}

// ==================== Internal Helper Functions ====================

void UUIManagerComponent::InitializeUIManager()
{
	UpdateOwnerReferences();
	
	if (!LockOnWidgetClass)
	{
		TryFindWidgetClassAtRuntime();
	}
}

void UUIManagerComponent::CleanupUIResources()
{
	HideAllLockOnWidgets();
	ClearWidgetComponentCache();
	CleanupSizeCache();
}

APlayerController* UUIManagerComponent::GetPlayerController() const
{
	if (OwnerController)
	{
		return OwnerController;
	}
	
	if (OwnerCharacter)
	{
		return Cast<APlayerController>(OwnerCharacter->GetController());
	}
	
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

bool UUIManagerComponent::IsValidTargetForUI(AActor* Target) const
{
	return Target && IsValid(Target) && !Target->IsPendingKill();
}

void UUIManagerComponent::ClearWidgetComponentCache()
{
	WidgetComponentCache.Empty();
}

void UUIManagerComponent::UpdateOwnerReferences()
{
	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
	}
}

// ==================== Size Analysis Helper Functions ====================

bool UUIManagerComponent::TryFindWidgetClassAtRuntime()
{
	return false;
}

float UUIManagerComponent::CalculateTargetBoundingBoxSize(AActor* Target) const
{
	if (!Target)
	{
		return 0.0f;
	}
	
	FVector Origin, BoxExtent;
	Target->GetActorBounds(false, Origin, BoxExtent);
	
	return FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z) * 2.0f;
}

EEnemySizeCategory UUIManagerComponent::AnalyzeTargetSize(AActor* Target) const
{
	if (!Target)
	{
		return EEnemySizeCategory::Medium;
	}
	
	if (EnemySizeCache.Contains(Target))
	{
		return EnemySizeCache[Target];
	}
	
	float BoundingBoxSize = CalculateTargetBoundingBoxSize(Target);
	
	if (BoundingBoxSize <= AdvancedCameraSettings.SmallEnemySizeThreshold)
	{
		return EEnemySizeCategory::Small;
	}
	else if (BoundingBoxSize <= AdvancedCameraSettings.LargeEnemySizeThreshold)
	{
		return EEnemySizeCategory::Medium;
	}
	else
	{
		return EEnemySizeCategory::Large;
	}
}

void UUIManagerComponent::UpdateTargetSizeCategory(AActor* Target)
{
	if (!Target)
	{
		return;
	}
	
	EEnemySizeCategory Category = AnalyzeTargetSize(Target);
	EnemySizeCache.FindOrAdd(Target) = Category;
}

void UUIManagerComponent::CleanupSizeCache()
{
	TArray<AActor*> InvalidActors;
	
	for (auto& Pair : EnemySizeCache)
	{
		if (!IsValid(Pair.Key))
		{
			InvalidActors.Add(Pair.Key);
		}
	}
	
	for (AActor* InvalidActor : InvalidActors)
	{
		EnemySizeCache.Remove(InvalidActor);
	}
}
