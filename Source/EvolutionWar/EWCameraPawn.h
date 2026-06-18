// Side-scroll spectator camera. Pans horizontally across the arena.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "EWTypes.h"
#include "EWCameraPawn.generated.h"

class UCameraComponent;
class AEWGameMode;

UCLASS()
class EVOLUTIONWAR_API AEWCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AEWCameraPawn();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Currently selected unit class index for manual deploy (left-click when no orbital armed). */
	UFUNCTION(BlueprintPure, Category = "EW|Player")
	int32 GetSelectedClass() const { return SelectedClass; }

	/** Currently armed orbital, or -1 if none (left-click deploys a unit instead). */
	UFUNCTION(BlueprintPure, Category = "EW|Player")
	int32 GetArmedOrbital() const { return ArmedOrbital; }

	/** True while the how-to-play overlay is toggled on (HUD reads this). */
	UFUNCTION(BlueprintPure, Category = "EW|Player")
	bool IsHelpVisible() const { return bShowHelp; }

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float PanSpeed = 2600.f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float MinX = -9500.f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float MaxX = 9500.f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float EdgeScrollMargin = 24.f;

	/** When idle, drift the camera to follow the clash between the two armies. */
	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	bool bAutoFollow = true;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float AutoFollowSpeed = 1.5f;

	// ---- Zoom ----------------------------------------------------------------
	/** Base camera depth (-Y) and height (Z); the rest pose before zoom/height offset. */
	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float BaseCamY = -3000.f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float BaseCamZ = 430.f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float MinZoom = 0.55f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float MaxZoom = 1.9f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float ZoomStep = 0.12f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float ZoomInterpSpeed = 9.f;

	/** Vertical look-height limits, added to BaseCamZ (Up/Down arrows). */
	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float MinHeight = -150.f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float MaxHeight = 600.f;

	UPROPERTY(EditAnywhere, Category = "EW|Camera")
	float HeightStep = 90.f;

protected:
	void PanX(float Value);

	// ---- Camera movement -----------------------------------------------------
	void OnZoomIn()  { TargetZoom = FMath::Clamp(TargetZoom - ZoomStep, MinZoom, MaxZoom); }
	void OnZoomOut() { TargetZoom = FMath::Clamp(TargetZoom + ZoomStep, MinZoom, MaxZoom); }
	void OnRaise()   { TargetHeight = FMath::Clamp(TargetHeight + HeightStep, MinHeight, MaxHeight); }
	void OnLower()   { TargetHeight = FMath::Clamp(TargetHeight - HeightStep, MinHeight, MaxHeight); }
	void OnToggleHelp()   { bShowHelp = !bShowHelp; }
	void OnToggleFollow() { bAutoFollow = !bAutoFollow; AutoFollowHold = 0.f; }

	// ---- Player command input ------------------------------------------------
	void SelectClass(int32 Index);          // number keys 1..5
	void ArmOrbital(int32 OrbitalIndex);    // Q/W/E/R
	void OnPrimaryClick();                  // left mouse: deploy unit or fire armed orbital

	// Zero-arg key handlers (UInputComponent::BindKey needs void() delegates).
	void OnSelect1() { SelectClass(0); }
	void OnSelect2() { SelectClass(1); }
	void OnSelect3() { SelectClass(2); }
	void OnSelect4() { SelectClass(3); }
	void OnSelect5() { SelectClass(4); }
	void OnArmCannon() { ArmOrbital(0); }
	void OnArmBomb()   { ArmOrbital(1); }
	void OnArmLaser()  { ArmOrbital(2); }
	void OnArmShield() { ArmOrbital(3); }

	/** Deproject the mouse to the lane plane (Y = LaneY) and return the world X. */
	bool MouseToLaneX(float& OutX) const;

	AEWGameMode* GetGM() const;

	UPROPERTY(VisibleAnywhere, Category = "EW|Camera")
	TObjectPtr<UCameraComponent> Camera;

	float PanInput = 0.f;
	float AutoFollowHold = 0.f; // seconds to suppress auto-follow after manual pan
	int32 SelectedClass = 0;
	int32 ArmedOrbital = -1;

	float TargetZoom = 1.f;
	float CurrentZoom = 1.f;
	float TargetHeight = 0.f;
	float CurrentHeight = 0.f;
	bool bShowHelp = false;
};
