#!/usr/bin/env python3
"""Batch-generate the EvolutionWar expansion sprite set (20 cyborg + 20 insect).

Resilient: skips outputs that already exist (>1KB), continues past failures,
prints STATUS lines so progress can be tailed from a log file.

Run from the project root:
    python Tools/gen_expansion.py
"""
import os, sys, time, traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_sprite

REF = "Content/Reference/vibe.png"
OUTDIR = "Content/Art/Source"
SIZE = "1024x1024"
QUALITY = "high"

CYBORG_STYLE = (
    "Full-body side-view 2D game sprite of {desc}. Cyan and electric-blue "
    "cybernetic armor plating with glowing neon energy accents and holographic "
    "rim lighting, sleek high-tech sci-fi cyborg. Detailed painterly concept-art "
    "style matching the reference's palette and finish. Character facing right, "
    "entire body inside the frame, centered, standing pose. Clean fully "
    "transparent background, no ground, no shadow, no text, no UI, single character only."
)
INSECT_STYLE = (
    "Full-body side-view 2D game sprite of {desc}. Chitinous amber-orange and "
    "deep-red alien exoskeleton with glowing bio-luminescent accents, menacing "
    "biomechanical insect. Detailed painterly concept-art style matching the "
    "reference's palette and finish. Creature facing left, entire body inside the "
    "frame, centered. Clean fully transparent background, no ground, no shadow, "
    "no text, no UI, single creature only."
)

CYBORGS = [
    ("cyborg_sentinel",   "a heavy guardian cyborg with a large energy tower shield and reinforced shoulders"),
    ("cyborg_ranger",     "a lightweight scout cyborg holding a compact energy carbine"),
    ("cyborg_lancer",     "an agile cyborg wielding a long glowing energy lance"),
    ("cyborg_artillery",  "a walker-mech cyborg with a huge shoulder-mounted plasma cannon"),
    ("cyborg_medic",      "a support cyborg with repair tools and a glowing green healing emitter"),
    ("cyborg_assassin",   "a slender stealth cyborg with twin glowing energy blades"),
    ("cyborg_juggernaut", "a massive heavily-armored battle mech cyborg, bulky tank-like frame"),
    ("cyborg_striker",    "a fast cyborg gunslinger dual-wielding glowing plasma pistols"),
    ("cyborg_engineer",   "a utility cyborg carrying a deployable turret pack and tools"),
    ("cyborg_vanguard",   "a frontline soldier cyborg with rifle and segmented combat armor"),
    ("cyborg_railgunner", "a tall sniper cyborg aiming a long glowing railgun rifle"),
    ("cyborg_pyro",       "a cyborg with a flamethrower projector and heat-resistant armor"),
    ("cyborg_orbdrone",   "a small hovering spherical combat drone with a glowing optic eye"),
    ("cyborg_warden",     "a riot-control cyborg with a wide energy barrier shield and baton"),
    ("cyborg_berserker",  "an aggressive cyborg with spinning chainblade arms, hunched forward"),
    ("cyborg_marksman",   "a precise cyborg with a long-barreled designated marksman rifle"),
    ("cyborg_titan",      "a colossal towering war-titan cyborg, immense armored legs"),
    ("cyborg_scout",      "a quick lean recon cyborg in a dynamic running pose"),
    ("cyborg_guardian",   "a defensive cyborg projecting a glowing hexagonal energy barrier"),
    ("cyborg_destroyer",  "a cyborg with a shoulder rocket-launcher battery and heavy armor"),
]

INSECTS = [
    ("insect_soldier",   "an armored warrior beetle with hardened plated carapace and mandibles"),
    ("insect_spitter",   "a bulbous acid-spitting insect with a swollen venom sac"),
    ("insect_burrower",  "a digging mole-insect with massive clawed forelimbs"),
    ("insect_queen",     "a large regal insect matriarch with an ornate crowned head"),
    ("insect_workerbug", "a small skittering worker drone insect, lean and quick"),
    ("insect_mantis",    "a tall mantis-like insect with long scything blade arms"),
    ("insect_scorpion",  "an armored scorpion-insect with a raised glowing stinger tail"),
    ("insect_wasp",      "a flying wasp-insect with translucent wings and a sharp stinger"),
    ("insect_carapace",  "a heavily armored tank insect with a thick domed shell"),
    ("insect_swarmer",   "a tiny fast swarm insect in an aggressive lunging pose"),
    ("insect_weaver",    "a spider-insect with spinnerets and bristled legs"),
    ("insect_charger",   "a horned rhinoceros-beetle insect lowered to charge"),
    ("insect_locust",    "a flying locust-insect with chewing mandibles and folded wings"),
    ("insect_centipede", "a long segmented centipede-insect rearing up, many legs"),
    ("insect_hornet",    "an aggressive armored hornet-insect mid-flight, wings spread"),
    ("insect_brute",     "a hulking heavy crusher insect with massive crushing claws"),
    ("insect_widow",     "a venomous black-and-red widow spider-insect, fangs bared"),
    ("insect_ravager",   "a predatory clawed insect beast, muscular and feral"),
    ("insect_moth",      "a large winged moth-insect with glowing patterned wings"),
    ("insect_behemoth",  "a gigantic towering boss insect, monstrous armored bulk"),
]


def run(items, style):
    for name, desc in items:
        out = f"{OUTDIR}/{name}.png"
        if os.path.exists(out) and os.path.getsize(out) > 1024:
            print(f"STATUS SKIP {name} (exists)", flush=True)
            continue
        prompt = style.format(desc=desc)
        print(f"STATUS START {name}", flush=True)
        t0 = time.time()
        try:
            gen_sprite.generate(prompt, out, SIZE, [REF], True, QUALITY)
            print(f"STATUS DONE {name} {time.time()-t0:.0f}s", flush=True)
        except SystemExit as e:
            print(f"STATUS FAIL {name}: {e}", flush=True)
        except Exception:
            print(f"STATUS FAIL {name}:\n{traceback.format_exc()}", flush=True)


if __name__ == "__main__":
    run(CYBORGS, CYBORG_STYLE)
    run(INSECTS, INSECT_STYLE)
    print("STATUS ALL_COMPLETE", flush=True)
