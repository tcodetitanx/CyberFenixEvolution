#!/usr/bin/env python3
"""Batch sprite generation for EvolutionWar. Skips assets that already exist.
Edit MANIFEST and run:  python Tools/gen_batch.py
"""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "Tools", "gen_sprite.py")
REF = os.path.join(ROOT, "Content", "Reference", "vibe.png")
OUT = os.path.join(ROOT, "Content", "Art", "Source")
STYLE = ("Painterly high-detail sci-fi concept-art style, dramatic rim lighting, "
         "clean readable silhouette, consistent with a dark cyber-evolution warfare aesthetic.")

# name: (prompt, size, transparent)
MANIFEST = {
 "cyborg_base": ("A massive futuristic CYBORG FORTRESS base building, side view facing right. Heavily armored sci-fi bunker citadel with glowing cyan energy reactors, antenna arrays, defensive turret emplacements, layered dark metal plating with electric-blue neon trim. "+STYLE+" Isolated, centered, FULLY TRANSPARENT background, no ground shadow, no text, no UI.", "1024x1024", True),
 "generation_depot": ("A futuristic CYBORG GENERATION DEPOT factory building, side view. Mechanical assembly structure with glowing cyan cloning vats and tubes growing cyborg soldiers, holographic readouts, robotic arms, dark metal with cyan neon. "+STYLE+" Isolated, centered, FULLY TRANSPARENT background, no ground shadow, no text, no UI.", "1024x1024", True),
 "insect_hive": ("A giant alien INSECT HIVE base structure, side view. Organic chitinous mound fortress with glowing amber-orange egg sacs, dripping resin, bony spires, pulsing bio-luminescence. Menacing. "+STYLE+" Isolated, centered, FULLY TRANSPARENT background, no ground shadow, no text, no UI.", "1024x1024", True),
 "insect_grunt": ("A single alien INSECT WARRIOR creature as a 2D game unit sprite. Full body side profile facing LEFT, aggressive charging pose. Chitinous armored carapace, sharp mandibles, multiple legs, glowing amber-orange bio-luminescent veins. "+STYLE+" Isolated, centered, FULLY TRANSPARENT background, no ground shadow, no text, no UI.", "1024x1024", True),
 "insect_flyer": ("A flying alien WASP-like INSECT creature as a 2D game unit sprite. Side profile facing LEFT, translucent buzzing wings, barbed stinger, segmented body with amber-orange glow. "+STYLE+" Isolated, centered, FULLY TRANSPARENT background, no ground shadow, no text, no UI.", "1024x1024", True),
 "cyborg_heavy": ("A heavy CYBORG MECH WARRIOR as a 2D game unit sprite. Full body side profile facing RIGHT, bulky bipedal battle-mech with shoulder cannons, reinforced armor plates, glowing cyan power core and visor. "+STYLE+" Isolated, centered, FULLY TRANSPARENT background, no ground shadow, no text, no UI.", "1024x1024", True),
 "battlefield_bg": ("A wide panoramic DARK SCI-FI BATTLEFIELD background for a side-scrolling game. War-torn alien planet surface, cracked ground, distant ruined megastructures, glowing energy storms and embers on the horizon, moody atmospheric lighting in deep blues and ambers. Horizontal parallax background plate. No characters, no creatures, no UI, no text.", "1536x1024", False),
}

def main():
    os.makedirs(OUT, exist_ok=True)
    only = set(sys.argv[1:])
    for name,(prompt,size,transparent) in MANIFEST.items():
        if only and name not in only:
            continue
        out = os.path.join(OUT, name+".png")
        if os.path.exists(out):
            print("SKIP exists", name); continue
        cmd = [sys.executable, GEN, "--prompt", prompt, "--out", out, "--size", size, "--quality", "high"]
        if transparent: cmd.append("--transparent")
        if os.path.exists(REF): cmd += ["--ref", REF]
        print("=== GENERATING", name, "===", flush=True)
        r = subprocess.run(cmd)
        print("   ->", "OK" if r.returncode==0 else "FAIL("+str(r.returncode)+")", name, flush=True)
    print("BATCH DONE")

if __name__=="__main__":
    main()
