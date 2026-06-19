#include "EWCameraPawn.h"
#include "EWGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// Class index of the unit being placed while the mouse is held (-1 = none). The HUD
// reads this to draw a ghost preview at the cursor; the actual deploy happens on release.
int32 GGhostClass = -1;

namespace
{
	/** Cinematic colour grade for the spectator camera. Applied every frame (and in
	 *  the constructor) so tweaks here take effect via Live Coding without a restart. */
	void ApplyCameraGrade(UCameraComponent* Cam)
	{
		if (!Cam) { return; }
		FPostProcessSettings& PP = Cam->PostProcessSettings;
		// Lock exposure so the scene stops washing out via auto eye-adaptation.
		PP.bOverride_AutoExposureMinBrightness = true;
		PP.AutoExposureMinBrightness = 1.0f;
		PP.bOverride_AutoExposureMaxBrightness = true;
		PP.AutoExposureMaxBrightness = 1.0f;
		// Lift exposure so the otherwise very dark background reads lighter.
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = 0.75f;
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.40f;
		// Light vignette so the frame edges aren't crushed to black.
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.12f;
		// Neutral saturation/contrast so colours match the source art rather than
		// over-saturating the warm insect sprites into looking red.
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.02f, 1.02f, 1.02f, 1.0f);
	}
}

AEWCameraPawn::AEWCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	// Pull back along -Y and sit near unit eye-level, looking almost level toward +Y
	// across the X-Z lane plane so the army fills the lower-centre of the frame. The
	// world's X is mirrored at runtime (see AEWGameMode) so the cyborgs read on the
	// screen-left while the camera keeps its original, background-friendly side.
	Camera->SetRelativeLocation(FVector(0.f, -3000.f, 900.f));
	// Tilt down more so the three depth lanes separate into distinct rows on screen.
	Camera->SetRelativeRotation(FRotator(-16.f, 90.f, 0.f));
	Camera->SetFieldOfView(62.f);

	ApplyCameraGrade(Camera);

	// Faster manual panning (3x the original 2600).
	PanSpeed = 7800.f;

	// Camera stays where the player leaves it; no auto-drift toward the clash.
	// (Still toggleable live with F.)
	bAutoFollow = false;

	// Vertical view panning (Up/Down keys + top/bottom screen edge), 5x faster.
	HeightStep = 450.f;       // was 90
	EdgeHeightSpeed = 1600.f; // was 320

	// Elevated eye-line to match the steeper tilt (see the rotation above).
	BaseCamZ = 900.f;         // was 430

	// Vertical pan range: don't allow panning below the resting framing (that showed
	// empty space under the battlefield), and give a lot more room to pan upward.
	MinHeight = 0.f;          // was -150
	MaxHeight = 1100.f;       // was 600
}

void AEWCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;

		// Confine the cursor to the play window so it can't leave the screen and so
		// edge-panning stays reliable, while keeping the cursor visible for clicks.
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
}

void AEWCameraPawn::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);
	if (!InInputComponent)
	{
		return;
	}
	InInputComponent->BindAxis(TEXT("EWPanX"), this, &AEWCameraPawn::PanX);

	// Direct key bindings (no project input mappings needed).
	// Unit class selection 1..5.
	InInputComponent->BindKey(EKeys::One,   IE_Pressed, this, &AEWCameraPawn::OnSelect1);
	InInputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &AEWCameraPawn::OnSelect2);
	InInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AEWCameraPawn::OnSelect3);
	InInputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &AEWCameraPawn::OnSelect4);
	InInputComponent->BindKey(EKeys::Five,  IE_Pressed, this, &AEWCameraPawn::OnSelect5);

	// Orbital arming Q/W/E/R -> Cannon/Bomb/Laser/Shield.
	InInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AEWCameraPawn::OnArmCannon);
	InInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AEWCameraPawn::OnArmBomb);
	InInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AEWCameraPawn::OnArmLaser);
	InInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AEWCameraPawn::OnArmShield);

	// Left-click: deploy a unit, or fire the armed orbital, at the lane point.
	InInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AEWCameraPawn::OnPrimaryClick);

	// Camera: zoom (mouse wheel or +/-), raise/lower (Up/Down), help (H), follow toggle (F).
	InInputComponent->BindKey(EKeys::MouseScrollUp,   IE_Pressed, this, &AEWCameraPawn::OnZoomIn);
	InInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AEWCameraPawn::OnZoomOut);
	InInputComponent->BindKey(EKeys::Equals,          IE_Pressed, this, &AEWCameraPawn::OnZoomIn);
	InInputComponent->BindKey(EKeys::Add,             IE_Pressed, this, &AEWCameraPawn::OnZoomIn);
	InInputComponent->BindKey(EKeys::Hyphen,          IE_Pressed, this, &AEWCameraPawn::OnZoomOut);
	InInputComponent->BindKey(EKeys::Subtract,        IE_Pressed, this, &AEWCameraPawn::OnZoomOut);
	InInputComponent->BindKey(EKeys::Up,              IE_Pressed, this, &AEWCameraPawn::OnRaise);
	InInputComponent->BindKey(EKeys::Down,            IE_Pressed, this, &AEWCameraPawn::OnLower);
	InInputComponent->BindKey(EKeys::H,               IE_Pressed, this, &AEWCameraPawn::OnToggleHelp);
	InInputComponent->BindKey(EKeys::F,               IE_Pressed, this, &AEWCameraPawn::OnToggleFollow);
}

void AEWCameraPawn::PanX(float Value)
{
	PanInput = Value;
}

void AEWCameraPawn::SelectClass(int32 Index)
{
	SelectedClass = Index;
	ArmedOrbital = -1; // selecting a unit disarms any orbital
}

void AEWCameraPawn::ArmOrbital(int32 OrbitalIndex)
{
	// Toggle: pressing the same key again disarms.
	ArmedOrbital = (ArmedOrbital == OrbitalIndex) ? -1 : OrbitalIndex;
}

AEWGameMode* AEWCameraPawn::GetGM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AEWGameMode>() : nullptr;
}

bool AEWCameraPawn::MouseToLaneX(float& OutX) const
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}
	FVector Origin, Dir;
	if (!PC->DeprojectMousePositionToWorld(Origin, Dir))
	{
		return false;
	}

	AEWGameMode* GM = GetGM();
	const float LaneY = GM ? GM->LaneY : 0.f;
	if (FMath::IsNearlyZero(Dir.Y))
	{
		return false;
	}
	// Intersect the view ray with the vertical lane plane Y = LaneY.
	const float T = (LaneY - Origin.Y) / Dir.Y;
	if (T < 0.f)
	{
		return false;
	}
	OutX = Origin.X + Dir.X * T;
	return true;
}

void AEWCameraPawn::OnPrimaryClick()
{
	AEWGameMode* GM = GetGM();
	if (!GM)
	{
		return;
	}

	if (ArmedOrbital >= 0)
	{
		// Orbitals still fire immediately on click.
		float WorldX;
		if (MouseToLaneX(WorldX))
		{
			GM->TriggerOrbital((EEWOrbital)ArmedOrbital, WorldX);
		}
		return;
	}

	// Unit deploy: hold to preview a ghost at the cursor; the unit is placed on release
	// (handled in Tick). Start the ghost now.
	GGhostClass = SelectedClass;
}

void AEWCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Re-apply the colour grade each frame so Live Coding tweaks show without a
	// Play restart (post-process lives on the camera component, set cheaply here).
	ApplyCameraGrade(Camera);

	// Live-tunable vertical framing (set here so Live Coding tweaks apply without a
	// Play restart). A higher eye + shallow upward tilt pushes the empty foreground off
	// the bottom and opens up more at the top; the tall MaxHeight gives lots of pan room.
	BaseCamZ = 950.f;
	MinHeight = 0.f;
	MaxHeight = 1200.f;
	TargetHeight = FMath::Clamp(TargetHeight, MinHeight, MaxHeight);
	if (Camera) { Camera->SetRelativeRotation(FRotator(-4.f, 90.f, 0.f)); }

	// Vertical (top/bottom) panning 5x faster; side-to-side (PanSpeed) left unchanged.
	EdgeHeightSpeed = 8000.f; // was 1600
	HeightStep = 2250.f;      // was 450

	// Ghost deploy: while the mouse is held after starting a placement, the HUD shows a
	// ghost at the cursor; on release we drop the real unit at the cursor's lane point.
	if (GGhostClass >= 0)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		const bool bHeld = PC && PC->IsInputKeyDown(EKeys::LeftMouseButton);
		if (!bHeld)
		{
			float WorldX;
			if (MouseToLaneX(WorldX))
			{
				if (AEWGameMode* GM = GetGM())
				{
					GM->DeployPlayerUnit(GGhostClass, WorldX);
				}
			}
			GGhostClass = -1;
		}
	}

	// Smoothly ease the camera toward the desired zoom (depth) and look-height.
	CurrentZoom = FMath::FInterpTo(CurrentZoom, TargetZoom, DeltaSeconds, ZoomInterpSpeed);
	CurrentHeight = FMath::FInterpTo(CurrentHeight, TargetHeight, DeltaSeconds, ZoomInterpSpeed);
	if (Camera)
	{
		FVector Rel = Camera->GetRelativeLocation();
		Rel.Y = BaseCamY * CurrentZoom;        // closer when zoomed in, farther when out
		Rel.Z = BaseCamZ + CurrentHeight;      // raise/lower the eye line
		Camera->SetRelativeLocation(Rel);
	}

	float Move = PanInput;

	// League-of-Legends-style edge panning: shove the mouse against any screen edge
	// to drive the camera. Left/right pan along the lane; top/bottom raise/lower the
	// view. Only acts when the cursor is actually inside the viewport.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float MX, MY;
		int32 VX, VY;
		if (PC->GetMousePosition(MX, MY))
		{
			PC->GetViewportSize(VX, VY);
			if (VX > 0 && VY > 0)
			{
				// Work off a clamped cursor reading so a momentary overshoot at the
				// locked border doesn't toggle the pan off (that caused the stutter).
				const float CX = FMath::Clamp(MX, 0.f, (float)VX);
				const float CY = FMath::Clamp(MY, 0.f, (float)VY);

				// Keep the cursor this far inside the border so it "stops a little
				// before the edge" instead of grinding against the hard lock.
				const float Inset = 8.f;
				float NX = CX, NY = CY;
				bool bReposition = false;

				if (CX <= EdgeScrollMargin)        { Move -= 1.f; if (CX < Inset)       { NX = Inset;      bReposition = true; } }
				else if (CX >= VX - EdgeScrollMargin) { Move += 1.f; if (CX > VX - Inset) { NX = VX - Inset;  bReposition = true; } }

				if (CY <= EdgeScrollMargin)
				{
					TargetHeight = FMath::Clamp(TargetHeight + EdgeHeightSpeed * DeltaSeconds, MinHeight, MaxHeight);
					if (CY < Inset) { NY = Inset; bReposition = true; }
				}
				else if (CY >= VY - EdgeScrollMargin)
				{
					TargetHeight = FMath::Clamp(TargetHeight - EdgeHeightSpeed * DeltaSeconds, MinHeight, MaxHeight);
					if (CY > VY - Inset) { NY = VY - Inset; bReposition = true; }
				}

				if (bReposition)
				{
					PC->SetMouseLocation(FMath::RoundToInt(NX), FMath::RoundToInt(NY));
				}
			}
		}
	}

	if (!FMath::IsNearlyZero(Move))
	{
		// Manual pan: player takes control and auto-follow pauses briefly. The camera's
		// right vector is -X (it films from -Y), so a screen-right input (+Move) must
		// move the camera toward -X to pan the view right.
		FVector Loc = GetActorLocation();
		Loc.X = FMath::Clamp(Loc.X - Move * PanSpeed * DeltaSeconds, MinX, MaxX);
		SetActorLocation(Loc);
		AutoFollowHold = 2.5f;
	}
	else if (bAutoFollow)
	{
		// Idle: ease the camera toward where the two armies are clashing.
		AutoFollowHold = FMath::Max(0.f, AutoFollowHold - DeltaSeconds);
		if (AutoFollowHold <= 0.f)
		{
			if (AEWGameMode* GM = GetGM())
			{
				const float TargetX = FMath::Clamp(GM->GetCombatCenterX(), MinX, MaxX);
				FVector Loc = GetActorLocation();
				Loc.X = FMath::FInterpTo(Loc.X, TargetX, DeltaSeconds, AutoFollowSpeed);
				SetActorLocation(Loc);
			}
		}
	}
	PanInput = 0.f;
}
