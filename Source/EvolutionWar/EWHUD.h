// Canvas HUD: biomass, unit selection, orbital boons (cost + cooldown) and evolution stats.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EWHUD.generated.h"

class AEWGameMode;
class AEWCameraPawn;

UCLASS()
class EVOLUTIONWAR_API AEWHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	/** Draw a filled rounded-ish panel (plain rect) with a subtle border. */
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill);
	/** Draw a framed value bar (dark track + colored fill), optionally right-anchored. */
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, bool bRightToLeft = false);
	/** Draw bright L-shaped accent ticks in a panel's corners for a sci-fi frame look. */
	void DrawCorners(float X, float Y, float W, float H, const FLinearColor& Col, float Len = 12.f);
	/** Draw a labeled "chip" (hotkey + name + sub text), highlighted when active. */
	float DrawChip(float X, float Y, float W, const FString& Hotkey, const FString& Name, const FString& Sub, bool bActive, const FLinearColor& Accent, float Fraction);
	void DrawTextShadowed(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float ScaleX = 1.f, float ScaleY = 1.f);
	/** Full-screen how-to-play guide (toggled with H). */
	void DrawHelpOverlay(float VW, float VH);
	/** Strategic lane minimap: whole arena with ground/air unit blips + bases. */
	void DrawMinimap(AEWGameMode* GM, float VW, float VH);
};
