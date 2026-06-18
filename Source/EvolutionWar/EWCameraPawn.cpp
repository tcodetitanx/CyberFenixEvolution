#include "EWCameraPawn.h"
#include "EWGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

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
	// across the X-Z lane plane so the army fills the lower-centre of the frame.
	Camera->SetRelativeLocation(FVector(0.f, -3000.f, 430.f));
	Camera->SetRelativeRotation(FRotator(-3.f, 90.f, 0.f));
	Camera->SetFieldOfView(62.f);

	// Lock exposure so the scene stops washing out via auto eye-adaptation, tame
	// bloom, and add a touch of contrast/saturation + vignette so the neon pops.
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = 1.0f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 1.0f;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.0f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.45f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.38f;
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(1.14f, 1.14f, 1.14f, 1.0f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);
}

void AEWCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
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
	float WorldX;
	if (!MouseToLaneX(WorldX))
	{
		return;
	}
	AEWGameMode* GM = GetGM();
	if (!GM)
	{
		return;
	}
	if (ArmedOrbital >= 0)
	{
		GM->TriggerOrbital((EEWOrbital)ArmedOrbital, WorldX);
	}
	else
	{
		GM->DeployPlayerUnit(SelectedClass, WorldX);
	}
}

void AEWCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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

	// Mouse edge-scroll for convenience.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float MX, MY;
		int32 VX, VY;
		if (PC->GetMousePosition(MX, MY))
		{
			PC->GetViewportSize(VX, VY);
			if (VX > 0)
			{
				if (MX <= EdgeScrollMargin) { Move -= 1.f; }
				else if (MX >= VX - EdgeScrollMargin) { Move += 1.f; }
			}
		}
	}

	if (!FMath::IsNearlyZero(Move))
	{
		// Manual pan: player takes control and auto-follow pauses briefly.
		FVector Loc = GetActorLocation();
		Loc.X = FMath::Clamp(Loc.X + Move * PanSpeed * DeltaSeconds, MinX, MaxX);
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
