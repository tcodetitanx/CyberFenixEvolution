#include "EWHUD.h"
#include "EWGameMode.h"
#include "EWCameraPawn.h"
#include "EWUnit.h"
#include "EWBuilding.h"
#include "EWTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
	// Cyborg-team neon palette.
	const FLinearColor PanelBG(0.02f, 0.03f, 0.05f, 0.72f);
	const FLinearColor Cyan(0.10f, 0.85f, 1.0f, 1.f);
	const FLinearColor Amber(1.0f, 0.72f, 0.18f, 1.f);
	const FLinearColor Magenta(1.0f, 0.25f, 0.7f, 1.f);
	const FLinearColor Green(0.35f, 1.0f, 0.45f, 1.f);
	const FLinearColor Dim(0.55f, 0.6f, 0.68f, 1.f);

	const TCHAR* RoleName(EEWRole R)
	{
		switch (R)
		{
		case EEWRole::Melee:   return TEXT("MELEE");
		case EEWRole::Ranged:  return TEXT("RANGED");
		case EEWRole::Support: return TEXT("SUPPORT");
		case EEWRole::Flanker: return TEXT("FLANKER");
		case EEWRole::Aerial:  return TEXT("AERIAL");
		case EEWRole::Tank:    return TEXT("TANK");
		default:               return TEXT("UNIT");
		}
	}
}

void AEWHUD::DrawTextShadowed(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float ScaleX, float ScaleY)
{
	if (!Font) { return; }
	FCanvasTextItem Shadow(FVector2D(X + 1.f, Y + 1.f), FText::FromString(Text), Font, FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Shadow.Scale = FVector2D(ScaleX, ScaleY);
	Canvas->DrawItem(Shadow);
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(ScaleX, ScaleY);
	Canvas->DrawItem(Item);
}

void AEWHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Fill)
{
	DrawRect(Fill, X, Y, W, H);
	// Thin top accent line.
	DrawRect(FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.5f), X, Y, W, 2.f);
}

void AEWHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, bool bRightToLeft)
{
	const float F = FMath::Clamp(Fraction, 0.f, 1.f);
	// Dark recessed track + 1px frame.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), X - 1.f, Y - 1.f, W + 2.f, H + 2.f);
	DrawRect(FLinearColor(0.05f, 0.07f, 0.10f, 0.95f), X, Y, W, H);
	const float FillW = W * F;
	if (bRightToLeft)
	{
		DrawRect(Fill, X + W - FillW, Y, FillW, H);
	}
	else
	{
		DrawRect(Fill, X, Y, FillW, H);
	}
	// Bright leading edge highlight.
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), bRightToLeft ? X + W - FillW : X, Y, 2.f, H);
}

void AEWHUD::DrawCorners(float X, float Y, float W, float H, const FLinearColor& Col, float Len)
{
	const float T = 2.f;
	// Top-left
	DrawRect(Col, X, Y, Len, T);
	DrawRect(Col, X, Y, T, Len);
	// Top-right
	DrawRect(Col, X + W - Len, Y, Len, T);
	DrawRect(Col, X + W - T, Y, T, Len);
	// Bottom-left
	DrawRect(Col, X, Y + H - T, Len, T);
	DrawRect(Col, X, Y + H - Len, T, Len);
	// Bottom-right
	DrawRect(Col, X + W - Len, Y + H - T, Len, T);
	DrawRect(Col, X + W - T, Y + H - Len, T, Len);
}

float AEWHUD::DrawChip(float X, float Y, float W, const FString& Hotkey, const FString& Name, const FString& Sub, bool bActive, const FLinearColor& Accent, float Fraction)
{
	const float H = 46.f;
	const FLinearColor Fill = bActive ? FLinearColor(Accent.R * 0.35f, Accent.G * 0.35f, Accent.B * 0.35f, 0.92f)
		: FLinearColor(0.06f, 0.08f, 0.11f, 0.85f);
	DrawRect(Fill, X, Y, W, H);

	// Cooldown / readiness fill bar along the bottom (Fraction 0..1 of readiness).
	const float Clamped = FMath::Clamp(Fraction, 0.f, 1.f);
	DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.85f), X, Y + H - 4.f, W * Clamped, 4.f);

	// Border when active.
	if (bActive)
	{
		DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 1.f), X, Y, W, 2.f);
		DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 1.f), X, Y + H - 2.f, W, 2.f);
	}

	UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;
	UFont* Med = GEngine ? GEngine->GetMediumFont() : nullptr;
	DrawTextShadowed(Hotkey, Accent, X + 6.f, Y + 5.f, Med);
	DrawTextShadowed(Name, bActive ? FLinearColor::White : Dim, X + 28.f, Y + 6.f, Small);
	if (!Sub.IsEmpty())
	{
		DrawTextShadowed(Sub, Dim, X + 28.f, Y + 24.f, Small);
	}
	return W;
}

void AEWHUD::DrawHelpOverlay(float VW, float VH)
{
	UFont* Large = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* Med = GEngine ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;

	// Dim the whole screen, then a centered glass panel.
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.78f), 0.f, 0.f, VW, VH);
	const float PW = 720.f, PH = 470.f;
	const float PX = VW * 0.5f - PW * 0.5f;
	const float PY = VH * 0.5f - PH * 0.5f;
	DrawRect(FLinearColor(0.03f, 0.05f, 0.08f, 0.95f), PX, PY, PW, PH);
	DrawRect(FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.9f), PX, PY, PW, 3.f);
	DrawRect(FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.9f), PX, PY + PH - 3.f, PW, 3.f);

	float X = PX + 36.f;
	float Y = PY + 26.f;
	DrawTextShadowed(TEXT("HOW TO PLAY  -  CYBER EVOLUTION"), Cyan, X, Y, Large, 1.2f, 1.2f);
	Y += 46.f;
	DrawTextShadowed(TEXT("Destroy the INSECT HIVE (right) before your CYBORG BASE (left) falls."), FLinearColor::White, X, Y, Small);
	Y += 18.f;
	DrawTextShadowed(TEXT("Your army auto-spawns, fights and EVOLVES. Winners become champions that breed stronger strains."), Dim, X, Y, Small);
	Y += 30.f;

	DrawTextShadowed(TEXT("CONTROLS"), Amber, X, Y, Med);
	Y += 26.f;
	struct FRow { const TCHAR* Key; const TCHAR* Desc; };
	static const FRow Rows[] = {
		{ TEXT("A / D  or  Arrows  or  screen edge"), TEXT("Pan the camera across the arena") },
		{ TEXT("Mouse Wheel  or  + / -"),             TEXT("Zoom in / out") },
		{ TEXT("Up / Down"),                          TEXT("Raise / lower the view") },
		{ TEXT("F"),                                  TEXT("Toggle auto-follow the battle front") },
		{ TEXT("1 - 5,  then Left-Click a lane"),     TEXT("Deploy the selected unit class (costs Biomass)") },
		{ TEXT("Q / W / E / R,  then Left-Click"),    TEXT("Fire Cannon / Bomb / Laser / Shield orbital") },
		{ TEXT("H"),                                  TEXT("Toggle this help screen") },
	};
	for (const FRow& R : Rows)
	{
		DrawTextShadowed(R.Key, Green, X, Y, Small);
		DrawTextShadowed(R.Desc, FLinearColor::White, X + 330.f, Y, Small);
		Y += 22.f;
	}
	Y += 12.f;
	DrawTextShadowed(TEXT("TIP: Biomass regenerates over time - spend it on units and orbital strikes."), Amber, X, Y, Small);
	Y += 18.f;
	DrawTextShadowed(TEXT("TIP: Hold your front line; champions evolve to counter the enemy each generation."), Amber, X, Y, Small);

	DrawTextShadowed(TEXT("Press  H  to close"), Cyan, VW * 0.5f - 60.f, PY + PH - 30.f, Small);
}

void AEWHUD::DrawMinimap(AEWGameMode* GM, float VW, float VH)
{
	if (!GM) { return; }

	// A wide strategic strip along the top: the entire arena compressed to one bar,
	// so the player can read the whole front at a glance even though the camera only
	// frames a slice of the long horizontal battlefield. Air units ride an upper
	// sub-row, ground units a lower one.
	const float MW = FMath::Min(VW * 0.5f, 560.f);
	const float MH = 46.f;
	const float MX = VW * 0.5f - MW * 0.5f;
	const float MY = 70.f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.78f), MX, MY, MW, MH);
	DrawCorners(MX, MY, MW, MH, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.5f), 8.f);
	DrawTextShadowed(TEXT("TACTICAL MAP"), Dim, MX + 6.f, MY - 14.f, GEngine ? GEngine->GetSmallFont() : nullptr);

	const float MinX = GM->PlayerBaseX;
	const float MaxX = GM->EnemyBaseX;
	const float Span = FMath::Max(1.f, MaxX - MinX);
	const float AirY = MY + 8.f;     // upper sub-row centre
	const float GndY = MY + 30.f;    // lower sub-row centre

	// Faint divider between air and ground rows.
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f), MX + 4.f, MY + 22.f, MW - 8.f, 1.f);

	auto ToMapX = [&](float WorldX) -> float
	{
		const float F = FMath::Clamp((WorldX - MinX) / Span, 0.f, 1.f);
		return MX + 6.f + F * (MW - 12.f);
	};

	int32 CyAir = 0, CyGnd = 0, InAir = 0, InGnd = 0;
	auto PlotTeam = [&](EEWTeam UTeam, const FLinearColor& Col, int32& Air, int32& Gnd)
	{
		for (const TObjectPtr<AEWUnit>& U : GM->GetTeamUnits(UTeam))
		{
			if (!U || !U->IsAlive()) { continue; }
			const bool bAir = U->IsAerial();
			const float PX = ToMapX(U->GetActorLocation().X);
			const float PY = bAir ? AirY : GndY;
			DrawRect(Col, PX - 1.f, PY - 2.f, 3.f, 4.f);
			if (bAir) { ++Air; } else { ++Gnd; }
		}
	};
	PlotTeam(EEWTeam::Cyborg, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.95f), CyAir, CyGnd);
	PlotTeam(EEWTeam::Insect, FLinearColor(Amber.R, Amber.G, Amber.B, 0.95f), InAir, InGnd);

	// Base markers at each end.
	DrawRect(Cyan, ToMapX(MinX) - 2.f, MY + 4.f, 4.f, MH - 8.f);
	DrawRect(Amber, ToMapX(MaxX) - 2.f, MY + 4.f, 4.f, MH - 8.f);

	// Combat front marker.
	const float FrontX = ToMapX(GM->GetCombatCenterX());
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.85f), FrontX - 1.f, MY + 2.f, 2.f, MH - 4.f);

	UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;
	DrawTextShadowed(FString::Printf(TEXT("AIR %d"), CyAir), Cyan, MX + 6.f, MY + MH + 2.f, Small);
	DrawTextShadowed(FString::Printf(TEXT("GND %d"), CyGnd), Cyan, MX + 70.f, MY + MH + 2.f, Small);
	DrawTextShadowed(FString::Printf(TEXT("AIR %d"), InAir), Amber, MX + MW - 130.f, MY + MH + 2.f, Small);
	DrawTextShadowed(FString::Printf(TEXT("GND %d"), InGnd), Amber, MX + MW - 64.f, MY + MH + 2.f, Small);
}

void AEWHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) { return; }

	AEWGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AEWGameMode>() : nullptr;
	AEWCameraPawn* Pawn = GetOwningPawn() ? Cast<AEWCameraPawn>(GetOwningPawn()) : nullptr;
	if (!GM) { return; }

	UFont* Large = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* Med = GEngine ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;

	const float VW = Canvas->ClipX;
	const float VH = Canvas->ClipY;

	// ---- World-space overlays (drawn first, UI panels sit on top) -----------
	const float GroundZ = 0.f;
	const float LaneY = GM->LaneY;

	// Floating health bars above every living unit.
	auto DrawUnitBars = [&](EEWTeam UTeam, const FLinearColor& TeamCol)
	{
		for (const TObjectPtr<AEWUnit>& U : GM->GetTeamUnits(UTeam))
		{
			if (!U || !U->IsAlive()) { continue; }
			const float Frac = U->GetHealthFraction();
			if (Frac >= 0.999f) { continue; } // hide full-health bars to reduce clutter
			const float TopZ = U->GetActorLocation().Z + U->GetSpriteWorldHeight() * U->GetGenome().Scale * 0.5f + 30.f;
			const FVector Top(U->GetActorLocation().X, U->GetActorLocation().Y, TopZ);
			const FVector S = Project(Top);
			if (S.Z <= 0.f || S.X < -50.f || S.X > VW + 50.f || S.Y < -50.f || S.Y > VH + 50.f) { continue; }
			const float BW = 40.f, BH = 5.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), S.X - BW * 0.5f - 1.f, S.Y - 1.f, BW + 2.f, BH + 2.f);
			const FLinearColor Fill = FMath::Lerp(FLinearColor(0.9f, 0.15f, 0.12f, 1.f), FLinearColor(0.3f, 0.95f, 0.35f, 1.f), Frac);
			DrawRect(Fill, S.X - BW * 0.5f, S.Y, BW * Frac, BH);
			DrawRect(FLinearColor(TeamCol.R, TeamCol.G, TeamCol.B, 0.9f), S.X - BW * 0.5f, S.Y, 2.f, BH);
		}
	};
	DrawUnitBars(EEWTeam::Cyborg, Cyan);
	DrawUnitBars(EEWTeam::Insect, Amber);

	// Expanding orbital impact rings.
	for (const FEWStrike& Strike : GM->GetRecentStrikes())
	{
		const FVector Center = Project(FVector(Strike.X, LaneY, GroundZ));
		const FVector Edge = Project(FVector(Strike.X + Strike.Radius, LaneY, GroundZ));
		if (Center.Z <= 0.f) { continue; }
		const float PixR = FMath::Abs(Edge.X - Center.X);
		const float A = FMath::Clamp(Strike.Age / Strike.Life, 0.f, 1.f);
		const float R = PixR * FMath::Lerp(0.25f, 1.f, A);
		const float Alpha = 1.f - A;
		const FLinearColor Col(Strike.Color.R, Strike.Color.G, Strike.Color.B, Alpha);
		const int32 Segs = 28;
		for (int32 i = 0; i < Segs; ++i)
		{
			const float A0 = (2.f * PI * i) / Segs;
			const float A1 = (2.f * PI * (i + 1)) / Segs;
			DrawLine(Center.X + FMath::Cos(A0) * R, Center.Y + FMath::Sin(A0) * R * 0.45f,
				Center.X + FMath::Cos(A1) * R, Center.Y + FMath::Sin(A1) * R * 0.45f, Col, 2.f);
		}
	}

	// Count living units per side (dying units are excluded by IsAlive()).
	auto CountAlive = [&](EEWTeam UTeam) -> int32
	{
		int32 N = 0;
		for (const TObjectPtr<AEWUnit>& U : GM->GetTeamUnits(UTeam))
		{
			if (U && U->IsAlive()) { ++N; }
		}
		return N;
	};

	// ---- Top-center: base health bars + army census -------------------------
	{
		const float BarW = 280.f, BarH = 16.f;
		const float CyFrac = GM->GetBaseHealthFraction(EEWTeam::Cyborg);
		const float InFrac = GM->GetBaseHealthFraction(EEWTeam::Insect);
		const float CX = VW * 0.5f;
		const float BY = 18.f;

		// Framed bars: cyborg fills right-to-left toward the center "VS".
		DrawBar(CX - BarW - 40.f, BY, BarW, BarH, CyFrac, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.95f), true);
		DrawBar(CX + 40.f, BY, BarW, BarH, InFrac, FLinearColor(Amber.R, Amber.G, Amber.B, 0.95f), false);

		// Labels + live HP percentages.
		DrawTextShadowed(TEXT("CYBORG BASE"), Cyan, CX - BarW - 40.f, BY + 20.f, Small);
		DrawTextShadowed(FString::Printf(TEXT("%.0f%%"), CyFrac * 100.f), FLinearColor::White, CX - 84.f, BY + 20.f, Small);
		DrawTextShadowed(TEXT("INSECT HIVE"), Amber, CX + 40.f, BY + 20.f, Small);
		DrawTextShadowed(FString::Printf(TEXT("%.0f%%"), InFrac * 100.f), FLinearColor::White, CX + BarW - 4.f, BY + 20.f, Small);

		// Center VS medallion with the two army populations flanking it.
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), CX - 30.f, BY - 4.f, 60.f, BarH + 8.f);
		DrawCorners(CX - 30.f, BY - 4.f, 60.f, BarH + 8.f, FLinearColor(1.f, 1.f, 1.f, 0.8f), 8.f);
		DrawTextShadowed(TEXT("VS"), FLinearColor::White, CX - 11.f, BY, Med);
		DrawTextShadowed(FString::Printf(TEXT("x%d"), CountAlive(EEWTeam::Cyborg)), Cyan, CX - 38.f, BY + 20.f, Small);
		DrawTextShadowed(FString::Printf(TEXT("x%d"), CountAlive(EEWTeam::Insect)), Amber, CX + 16.f, BY + 20.f, Small);
	}

	// ---- Strategic lane minimap (whole-arena overview) ----------------------
	DrawMinimap(GM, VW, VH);

	// ---- Top-left: biomass + evolution telemetry ----------------------------
	DrawPanel(16.f, 16.f, 360.f, 100.f, PanelBG);
	DrawCorners(16.f, 16.f, 360.f, 100.f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.6f));
	DrawTextShadowed(TEXT("BIOMASS"), Cyan, 28.f, 24.f, Small);
	DrawTextShadowed(FString::Printf(TEXT("%.0f"), GM->GetBiomass()), Green, 28.f, 38.f, Large, 1.4f, 1.4f);
	DrawTextShadowed(FString::Printf(TEXT("+%.0f/s"), GM->BiomassPerSecond), Dim, 30.f, 76.f, Small);
	// Reserve meter (relative to a soft 500 ceiling) for an at-a-glance fuel read.
	DrawBar(28.f, 94.f, 110.f, 6.f, GM->GetBiomass() / 500.f, FLinearColor(Green.R, Green.G, Green.B, 0.95f));

	DrawTextShadowed(
		FString::Printf(TEXT("CYBORG  gen %d   champ %.0f"), GM->GetGeneration(EEWTeam::Cyborg), GM->GetChampionFitness(EEWTeam::Cyborg)),
		Cyan, 160.f, 36.f, Small);
	DrawTextShadowed(
		FString::Printf(TEXT("INSECT  gen %d   champ %.0f"), GM->GetGeneration(EEWTeam::Insect), GM->GetChampionFitness(EEWTeam::Insect)),
		Magenta, 160.f, 54.f, Small);
	DrawTextShadowed(
		FString::Printf(TEXT("ROSTER  %d cyborg / %d insect classes"), GM->CyborgClasses.Num(), GM->InsectClasses.Num()),
		Dim, 160.f, 78.f, Small);

	// ---- Bottom: unit roster (left) and orbital boons (right) ---------------
	const int32 ArmedOrbital = Pawn ? Pawn->GetArmedOrbital() : -1;
	const int32 SelectedClass = Pawn ? Pawn->GetSelectedClass() : 0;

	// Unit roster.
	const TArray<FUnitClassDef>& Roster = GM->CyborgClasses;
	const int32 NumShown = FMath::Min(Roster.Num(), 5);
	const float ChipW = 150.f;
	const float Pad = 8.f;
	const float RosterW = NumShown * (ChipW + Pad) + Pad;
	float RX = 16.f;
	const float RY = VH - 70.f;
	const float RPanelW = FMath::Max(RosterW, 160.f);
	DrawPanel(RX, RY - 24.f, RPanelW, 92.f, PanelBG);
	DrawCorners(RX, RY - 24.f, RPanelW, 92.f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.5f));
	DrawTextShadowed(TEXT("UNITS  [1-5]  click lane to deploy"), Dim, RX + 12.f, RY - 20.f, Small);

	// Selected-class stat readout, so the player can compare unit archetypes.
	if (Roster.IsValidIndex(SelectedClass))
	{
		const FUnitGenome& G = Roster[SelectedClass].BaseGenome;
		const float Dps = G.Damage / FMath::Max(0.1f, G.AttackInterval);
		const FString Stats = FString::Printf(
			TEXT("%s  [%s]   HP %.0f   DPS %.0f   SPD %.0f   RNG %.0f   ARM %.0f"),
			*Roster[SelectedClass].ClassName.ToString(), RoleName(G.Role), G.MaxHealth, Dps, G.MoveSpeed, G.AttackRange, G.Armor);
		const float IPH = 26.f;
		DrawRect(FLinearColor(0.04f, 0.06f, 0.09f, 0.92f), RX, RY - 24.f - IPH - 4.f, RPanelW, IPH);
		DrawRect(FLinearColor(Green.R, Green.G, Green.B, 0.8f), RX, RY - 24.f - IPH - 4.f, 3.f, IPH);
		DrawTextShadowed(Stats, FLinearColor::White, RX + 12.f, RY - 24.f - IPH + 2.f, Small);
	}
	float CX = RX + Pad;
	for (int32 i = 0; i < NumShown; ++i)
	{
		const FUnitClassDef& Def = Roster[i];
		const bool bSel = (ArmedOrbital < 0) && (i == SelectedClass);
		DrawChip(CX, RY, ChipW,
			FString::Printf(TEXT("%d"), i + 1),
			Def.ClassName.ToString(),
			FString::Printf(TEXT("%.0f bio"), Def.DeployCost),
			bSel, Cyan, bSel ? 1.f : 0.f);
		CX += ChipW + Pad;
	}

	// Orbital boons (right side).
	static const TCHAR* OrbHotkeys[4] = { TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R") };
	static const TCHAR* OrbNames[4] = { TEXT("Cannon"), TEXT("Bomb"), TEXT("Laser"), TEXT("Shield") };
	static const FLinearColor OrbColors[4] = { Amber, Magenta, Cyan, Green };
	const float OChipW = 132.f;
	const float OrbW = 4 * (OChipW + Pad) + Pad;
	const float OX = VW - OrbW - 16.f;
	const float OY = VH - 70.f;
	DrawPanel(OX, OY - 24.f, OrbW, 92.f, PanelBG);
	DrawCorners(OX, OY - 24.f, OrbW, 92.f, FLinearColor(Amber.R, Amber.G, Amber.B, 0.5f));
	DrawTextShadowed(TEXT("ORBITAL BOONS  [Q W E R]  click lane to fire"), Dim, OX + 12.f, OY - 20.f, Small);
	float OCX = OX + Pad;
	for (int32 i = 0; i < 4; ++i)
	{
		const EEWOrbital Orb = (EEWOrbital)i;
		const float Cd = GM->GetOrbitalCooldownRemaining(Orb);
		const float Cost = GM->GetOrbitalCost(Orb);
		const bool bArmed = (ArmedOrbital == i);
		const bool bReady = (Cd <= 0.f) && (GM->GetBiomass() >= Cost);
		FString Sub = (Cd > 0.f) ? FString::Printf(TEXT("CD %.1fs"), Cd) : FString::Printf(TEXT("%.0f bio"), Cost);
		DrawChip(OCX, OY, OChipW, OrbHotkeys[i], OrbNames[i], Sub, bArmed, OrbColors[i], bArmed ? 1.f : (bReady ? 0.6f : 0.f));
		OCX += OChipW + Pad;
	}

	// ---- Center crosshair when an orbital is armed --------------------------
	if (ArmedOrbital >= 0 && ArmedOrbital < 4)
	{
		const FLinearColor C = OrbColors[ArmedOrbital];
		float MX = 0.f, MY = 0.f;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->GetMousePosition(MX, MY);
		}
		DrawRect(FLinearColor(C.R, C.G, C.B, 0.9f), MX - 14.f, MY - 1.f, 28.f, 2.f);
		DrawRect(FLinearColor(C.R, C.G, C.B, 0.9f), MX - 1.f, MY - 14.f, 2.f, 28.f);
		DrawTextShadowed(FString::Printf(TEXT("%s ARMED"), OrbNames[ArmedOrbital]), C, MX + 18.f, MY - 8.f, Small);
	}

	// ---- Persistent help hint + toggleable how-to-play overlay --------------
	{
		const bool bHelp = Pawn ? Pawn->IsHelpVisible() : false;
		DrawTextShadowed(TEXT("[H] How to play"), Dim, VW * 0.5f - 48.f, VH - 92.f, Small);
		if (bHelp)
		{
			DrawHelpOverlay(VW, VH);
		}
	}

	// ---- Match-over banner --------------------------------------------------
	if (GM->IsMatchOver())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, VW, VH);
		const bool bWon = GM->DidPlayerWin();
		const FString Banner = bWon ? TEXT("VICTORY") : TEXT("DEFEAT");
		const FLinearColor BannerCol = bWon ? Green : FLinearColor(1.0f, 0.25f, 0.2f, 1.f);
		float TW = 0.f, TH = 0.f;
		if (Large) { Canvas->TextSize(Large, Banner, TW, TH); }
		DrawTextShadowed(Banner, BannerCol, VW * 0.5f - TW * 2.0f, VH * 0.4f, Large, 4.f, 4.f);
		const FString Sub = bWon ? TEXT("The hive has fallen.") : TEXT("Your base was overrun.");
		float SW = 0.f, SH = 0.f;
		if (Med) { Canvas->TextSize(Med, Sub, SW, SH); }
		DrawTextShadowed(Sub, FLinearColor::White, VW * 0.5f - SW * 0.5f, VH * 0.4f + 80.f, Med);
	}
}
