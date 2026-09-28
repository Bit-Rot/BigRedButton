"""The single table behind every sound in the game.

Two scripts read it:

  AI/tools/prep_audio.py   (offline, plain Python)
      Source recordings -> Assets/Audio/**.wav, cut, cleaned, resampled and
      loudness-matched, plus Assets/Audio/CREDITS.md.

  AI/build_audio_assets.py (inside the running editor, via AI/tools/ue_remote.py)
      Assets/Audio/**.wav -> SoundWaves, the mix assets (SoundClasses,
      attenuation, concurrency), the UPartySoundEvent assets gameplay plays,
      and the PM_* physical materials that give level art a surface type.

Tuning a sound = edit this file, re-run both. Nothing here is hand-edited in
the editor, because both scripts overwrite what they own.

Levels:
  Every file is normalised to the same loudness by preset (see PRESETS), so a
  variant's level never depends on how hot the original recording was. The
  mix lives in two places only: each event's `volume_db`, and the SoundClass
  tree. Intensity shaping (vol_min/vol_max/pitch_*/lowpass_min) is how a sound
  reacts to how hard something happened — see UPartySoundEvent.
"""

SOURCE_ROOT = "D:/Dropbox/My Dropbox/Resources/Audio/Packs"

# Where prepped WAVs go, relative to the repo root, and where each group
# imports to. Shared groups live under /Game/Audio so every game can use them.
GROUPS = {
    "Octo":     ("Assets/Audio/SFX/Octo",     "/Game/OctoOdyssey/Audio/SFX"),
    "Surfaces": ("Assets/Audio/SFX/Surfaces", "/Game/Audio/SFX/Surfaces"),
}

EVENT_PATHS = {
    "Octo":   "/Game/OctoOdyssey/Audio/Events",
    "Shared": "/Game/Audio/Events",
}

MIX_PATH = "/Game/Audio/Mix"
PHYSMAT_PATH = "/Game/Audio/PhysicalMaterials"

# --------------------------------------------------------------------------
# Offline processing presets (prep_audio.py)
#
#   target_db   loudness target. One-shots: RMS of the loudest 50 ms window,
#               i.e. "how loud is the hit". Loops: RMS over the whole loop.
#   ceiling_db  true-peak-ish ceiling; gain is reduced to respect it.
#   highpass    rumble/DC cleanup. Low for things meant to have weight.
#   fade_ms     (in, out). The out fade is where a one-shot's tail is cut.
#   max_len     one-shots longer than this are faded out here (seconds).
#   loop_xfade  loops: seconds of equal-power crossfade folded into the seam.
# --------------------------------------------------------------------------
PRESETS = {
    "impact": dict(mono=True, target_db=-14.0, ceiling_db=-1.0, highpass=35.0, fade_ms=(1.5, 40.0), max_len=0.9),
    "short":  dict(mono=True, target_db=-16.0, ceiling_db=-1.0, highpass=60.0, fade_ms=(1.0, 25.0), max_len=0.5),
    "thump":  dict(mono=True, target_db=-14.0, ceiling_db=-1.0, highpass=25.0, fade_ms=(1.5, 120.0), max_len=0.8),
    "loop":   dict(mono=True, target_db=-20.0, ceiling_db=-3.0, highpass=40.0, loop_xfade=0.4),
}


def snd(group, name, src, slice=None, preset="impact", pitch=0.0):
    """One prepped file. `slice` is (start, end) seconds in the source — the
    numbers AI/tools/audio_scan.py prints. `pitch` is a varispeed shift in
    semitones, baked in (length changes with it, as with tape)."""
    return dict(group=group, name=name, src=src, slice=slice, preset=preset, pitch=pitch)


_GDC = "Miscellaneous & Uncategorized/GameAudioGDC/Sonniss.com - GDC - Game Audio Bundle"
_GDC17 = "Miscellaneous & Uncategorized/GameAudioGDC/Sonniss.com - GDC 2017 - Game Audio Bundle"
_GDC18 = "Miscellaneous & Uncategorized/GameAudioGDC/Sonniss.com - GDC 2018 - Game Audio Bundle"
_GDC19 = "Miscellaneous & Uncategorized/GameAudioGDC/Sonniss.com - GDC 2019 - Game Audio Bundle"
_GDC20 = "Miscellaneous & Uncategorized/GameAudioGDC/Sonniss.com - GDC 2020 - Game Audio Bundle"
_PMSFX = "Miscellaneous & Uncategorized/PMSFX Sampler/PMSFX SAMPLER JUNE 2020 Part1/PM LETHAL BLOW/SOURCE CONSTRUCTION-KIT"
_GOOP = "SFX/GameDevMarket/MMO Magic & Fantasy SFX/MMO_Game_Magic/Source/Goop/MMO_Game_Magic_Source_Goop_%02d_Mud_Silly_Putty_Wet_Gore.wav"
_BORAX = _GDC + "/Mattia Cellotto - The Borax Experiment/"
_CORK = "SFX/EpicStockMedia/fantasygameaudiopack/Fantasy_Game_24bit_Updated/Fantasy_Game_24bit/UI/Fantasy_Game_UI_Craft_Air_Bubble_Suction_Pop_Cork_%d_Water.wav"
_DRAIN = _GDC19 + "/The Sound Pack Tree - This Library Sucks/253203 - Drain Suction 03.wav"
_WOOD = "SFX/Misc/Crafting & RTS Sounds/Wood/Wood Impact %s.wav"
_RPG = "SFX/GameDevMarket/RPG SFX Bundle/RPG Sound Effects Bundle/"
_BODYFALL = "SFX/GameDevMarket/Medieval Combat SFX/Medieval Combat Sounds/Punch and Melee/Body Fall %d.wav"
_ZOMBIE_BODYFALL = "SFX/Misc/ZombieHorrorPackageLight/OGG/BodyFall/Foley_BodyFall_%03d.ogg"
_RL = _GDC19 + "/Red Libraries - Bodyfall/"
_SAND = "SFX/Misc/Footstep Sounds/Sand/Footstep Sand Running 1_%02d.wav"
_BAREFOOT = _GDC + "/Timothy McHugh -  Barefoot on Metal/FOOTSTEP - Metal %s Barefoot Male - %d.wav"

SOUNDS = [
    # ---- Octopus flesh: the body's own layer on every impact ------------------
    # Light: small wet drops and putty slaps.
    snd("Octo", "Octo_ImpactFlesh_Light_01", _GOOP % 9,  (0.015, 0.46)),
    snd("Octo", "Octo_ImpactFlesh_Light_02", _GOOP % 13, (0.127, 0.60)),
    snd("Octo", "Octo_ImpactFlesh_Light_03", _GOOP % 18, (0.234, 0.60)),
    snd("Octo", "Octo_ImpactFlesh_Light_04", _GDC20 + "/PMSFX - Lethal Blow/PM_LB_SOURCE_FLESH_DROPS_ORANGES_MKH8060_6.wav", (0.018, 0.30)),
    snd("Octo", "Octo_ImpactFlesh_Light_05", _PMSFX + "/PM_LB_SOURCE_FLESH_DROPS_ORANGES_MKH8060_10.wav", (0.033, 0.33)),
    # Medium: meat and lettuce smacks — the classic wet-slap foley.
    snd("Octo", "Octo_ImpactFlesh_Medium_01", _PMSFX + "/PM_LB_SOURCE_PUNCH_MEAT_BEEF_MKH8060_6.wav", (0.019, 0.40)),
    snd("Octo", "Octo_ImpactFlesh_Medium_02", _PMSFX + "/PM_LB_SOURCE_PUNCH_SMACK_LETTUCE_TRANSIENT_MKH8060_7.wav", (0.078, 0.33)),
    snd("Octo", "Octo_ImpactFlesh_Medium_03", _GDC20 + "/PMSFX - Lethal Blow/PM_LB_SOURCE_PUNCH_SMACK_LETTUCE_WITHCONTACTMIC_ENHANCED_MKH8040_16.wav", (0.061, 0.35)),
    snd("Octo", "Octo_ImpactFlesh_Medium_04", _GDC + "/Mechanical Wave - Vegetable Massacres/Juicy Burst_VM 03.wav", (0.168, 0.45)),
    snd("Octo", "Octo_ImpactFlesh_Medium_05", _GDC18 + "/Ancora Audio - VEGECIDE!/Squishes_Orange_3.wav", (0.107, 0.70), pitch=-2.0),
    # Heavy: slime splats, pitched down for body weight.
    snd("Octo", "Octo_ImpactFlesh_Heavy_01", _BORAX + "Borax,Impact,Slime,Gore,Various16.wav", (0.033, 0.50), pitch=-3.0),
    snd("Octo", "Octo_ImpactFlesh_Heavy_02", _BORAX + "Jelly,Movement,Gore,Liquid,Splat,Impact,Slime,Various09.wav", (0.029, 0.48), pitch=-3.0),
    snd("Octo", "Octo_ImpactFlesh_Heavy_03", _BORAX + "Borax,Impact,Slime,Gore,Various22.wav", (1.082, 1.70), pitch=-3.0),
    snd("Octo", "Octo_ImpactFlesh_Heavy_04", _BORAX + "Borax,Impact,Slime,Gore,Various63.wav", (0.188, 0.50), pitch=-3.0),
    snd("Octo", "Octo_ImpactFlesh_Heavy_05", _GDC + "/Coll Anderson - Deer/EFX INT Meat drops 03 B.wav", (5.860, 6.30)),

    # ---- Arm plant: soft sticky contact --------------------------------------
    snd("Octo", "Octo_ArmPlant_01", _GOOP % 4,  (0.297, 0.56), preset="short"),
    snd("Octo", "Octo_ArmPlant_02", _GOOP % 7,  (0.142, 0.34), preset="short"),
    snd("Octo", "Octo_ArmPlant_03", _GOOP % 11, (0.026, 0.30), preset="short"),
    snd("Octo", "Octo_ArmPlant_04", _GOOP % 14, (0.099, 0.41), preset="short"),
    snd("Octo", "Octo_ArmPlant_05", _GOOP % 15, (0.150, 0.36), preset="short"),
    snd("Octo", "Octo_ArmPlant_06", _GOOP % 16, (0.065, 0.28), preset="short"),

    # ---- Arm unstick: suction-cup release pops -------------------------------
    # The drain-plunger takes are the real thing: short, low, air-driven.
    snd("Octo", "Octo_ArmUnstick_01", _DRAIN, (0.185, 0.33), preset="short"),
    snd("Octo", "Octo_ArmUnstick_02", _DRAIN, (0.796, 0.94), preset="short"),
    snd("Octo", "Octo_ArmUnstick_03", _DRAIN, (0.995, 1.14), preset="short"),
    snd("Octo", "Octo_ArmUnstick_04", _DRAIN, (1.309, 1.50), preset="short"),
    snd("Octo", "Octo_ArmUnstick_05", _DRAIN, (1.929, 2.08), preset="short"),
    snd("Octo", "Octo_ArmUnstick_06", _DRAIN, (7.392, 7.52), preset="short"),
    snd("Octo", "Octo_ArmUnstick_07", _DRAIN, (7.655, 7.78), preset="short"),
    snd("Octo", "Octo_ArmUnstick_08", _CORK % 4, (0.017, 0.20), preset="short"),
    snd("Octo", "Octo_ArmUnstick_09", _CORK % 6, (0.001, 0.33), preset="short"),

    # ---- Push-off: low body "whumpf" under a strong launch --------------------
    snd("Octo", "Octo_PushOff_01", _GDC20 + "/Shapeforms - Hit & Punch/IMPACT_LOW_THUD_10.wav", (0.0, 0.20), preset="thump", pitch=-2.0),
    snd("Octo", "Octo_PushOff_02", _GDC19 + "/Baxter Audio - IMPACT/10 Hit Deep Thud.wav", (0.055, 0.75), preset="thump"),
    snd("Octo", "Octo_PushOff_03", _ZOMBIE_BODYFALL % 7, (0.0, 0.55), preset="thump", pitch=-4.0),
    snd("Octo", "Octo_PushOff_04", _PMSFX + "/PM_LB_SOURCE_SWISH_WHOOSH_FABRIC_CLOTH_MOVEMENT_MKH8060_9.wav", None, preset="thump", pitch=-5.0),

    # ---- Loops -----------------------------------------------------------------
    # Rolling: wet body turning over on itself. Slime movement, slowed for mass.
    snd("Octo", "Octo_RollLoop_01", _BORAX + "Gore,Slime,Creature,Meat,Blood,Long,Movement,Various19.wav", (0.09, 2.55), preset="loop", pitch=-4.0),
    snd("Octo", "Octo_RollLoop_02", _BORAX + "Gore,Slime,Creature,Meat,Blood,Medium,Movement,Various09.wav", (0.05, 1.58), preset="loop", pitch=-4.0),
    snd("Octo", "Octo_RollLoop_03", _GDC + "/Mechanical Wave - Vegetable Massacres/Drenched Manipulation_VM 03.wav", (0.07, 2.10), preset="loop", pitch=-5.0),
    # Air rush: wind, already seamless loops.
    snd("Octo", "Octo_AirRush_01", "Ambience/Ambiences_Sounds/WAV/Ambience_Wind_Intensity_Medium_Loop.wav", None, preset="loop"),
    snd("Octo", "Octo_AirRush_02", "Ambience/Ambiences_Sounds/WAV/Ambience_Wind_Intensity_High_Loop.wav", None, preset="loop"),

    # ---- Surfaces: what the body hit (shared by every game) --------------------
    snd("Surfaces", "Surface_Wood_Light_01", _WOOD % "1_1", (0.060, 0.40)),
    snd("Surfaces", "Surface_Wood_Light_02", _WOOD % "1_2", (0.010, 0.40)),
    snd("Surfaces", "Surface_Wood_Light_03", _WOOD % "1_3", (0.008, 0.40)),
    snd("Surfaces", "Surface_Wood_Light_04", _WOOD % "2_1", (0.045, 0.40)),
    snd("Surfaces", "Surface_Wood_Light_05", _WOOD % "2_3", (0.009, 0.40)),
    snd("Surfaces", "Surface_Wood_Heavy_01", _RPG + "Thud Wood.wav", (0.0, 0.50)),
    snd("Surfaces", "Surface_Wood_Heavy_02", _RPG + "Thud Wood 2.wav", (0.0, 0.60)),
    snd("Surfaces", "Surface_Wood_Heavy_03", _RPG + "Thud Wood Hollow.wav", (0.0, 0.40)),
    snd("Surfaces", "Surface_Wood_Heavy_04", _GDC17 + "/Double Trouble Audio - Wood Impacts and Debris/Drop Soft - Single Plank, Drop 02.wav", (0.0, 0.45)),
    snd("Surfaces", "Surface_Wood_Heavy_05", _RL + "RL_bodyfall_Wood_Rustic_M2_Distant_Mono_Soft_Impact_07.wav", (0.0, 0.70)),

    snd("Surfaces", "Surface_Stone_Light_01", _BODYFALL % 1, (0.0, 0.40)),
    snd("Surfaces", "Surface_Stone_Light_02", _BODYFALL % 2, (0.0, 0.30)),
    snd("Surfaces", "Surface_Stone_Light_03", _BODYFALL % 3, (0.025, 0.40)),
    snd("Surfaces", "Surface_Stone_Light_04", _BODYFALL % 4, (0.025, 0.40)),
    snd("Surfaces", "Surface_Stone_Light_05", _BODYFALL % 5, (0.0, 0.42)),
    snd("Surfaces", "Surface_Stone_Heavy_01", _RL + "RL_bodyfall_Concrete_Generic_Feet_Mid_Mono_Med_Impact_02.wav", (0.09, 0.80)),
    snd("Surfaces", "Surface_Stone_Heavy_02", _ZOMBIE_BODYFALL % 2, (0.015, 0.80)),
    snd("Surfaces", "Surface_Stone_Heavy_03", _ZOMBIE_BODYFALL % 5, (0.005, 0.86)),

    snd("Surfaces", "Surface_Sand_Light_01", _SAND % 1, (0.030, 0.28)),
    snd("Surfaces", "Surface_Sand_Light_02", _SAND % 2, (0.000, 0.22)),
    snd("Surfaces", "Surface_Sand_Light_03", _SAND % 3, (0.025, 0.27)),
    snd("Surfaces", "Surface_Sand_Light_04", _SAND % 4, (0.015, 0.26)),
    snd("Surfaces", "Surface_Sand_Light_05", _SAND % 6, (0.030, 0.25)),
    snd("Surfaces", "Surface_Sand_Heavy_01", _RL + "RL_bodyfall_Dirt_M4_Close_Stereo_Hard_Impact_10.wav", (0.055, 0.60)),
    snd("Surfaces", "Surface_Sand_Heavy_02", _GDC19 + "/Matt Script - You Me & Debris/impact_gritty_grit_sandy_dirt_dig_shovel_shingle_roof_02.wav", (0.0, 0.65)),

    snd("Surfaces", "Surface_Metal_Light_01", _BAREFOOT % ("Plate Jump", 17), (0.04, 0.64)),
    snd("Surfaces", "Surface_Metal_Light_02", _BAREFOOT % ("Board Jump", 11), (0.08, 0.61)),
    snd("Surfaces", "Surface_Metal_Light_03", _BAREFOOT % ("Grill Jump", 3), (0.07, 0.58)),
    snd("Surfaces", "Surface_Metal_Heavy_01", _BAREFOOT % ("Plate Land", 1), (0.05, 0.59)),
    snd("Surfaces", "Surface_Metal_Heavy_02", _BAREFOOT % ("Grill Land", 5), (0.035, 0.49)),
    snd("Surfaces", "Surface_Metal_Heavy_03", _BAREFOOT % ("Board Land", 1), (0.055, 0.49)),
    snd("Surfaces", "Surface_Metal_Heavy_04", _RL + "RL_bodyfall_Metal_Grid_M3_Close_Stereo_Hard_Impact_07.wav", (0.02, 0.64)),
]

# --------------------------------------------------------------------------
# Mix assets (build_audio_assets.py)
# --------------------------------------------------------------------------

# (name, parent, volume). Volumes are the coarse mix; per-event volume_db is the fine one.
SOUND_CLASSES = [
    ("SC_Master",   None,        1.0),
    ("SC_Music",    "SC_Master", 1.0),
    ("SC_SFX",      "SC_Master", 1.0),
    ("SC_Impacts",  "SC_SFX",    1.0),
    ("SC_Movement", "SC_SFX",    1.0),
    ("SC_UI",       "SC_SFX",    1.0),
    ("SC_Ambience", "SC_SFX",    1.0),
    ("SC_Stingers", "SC_SFX",    1.0),
]

# Distances in cm. The listener's DISTANCE is measured from the octopus (see
# UPartyAudioSubsystem::SetAttenuationFocus), so these describe how far from the
# octopus a sound can be heard — the octopus's own sounds are always at ~0.
# lpf: air absorption, the high end rolling off with distance.
ATTENUATIONS = {
    "ATT_Small":       dict(inner=300.0,  falloff=2700.0, lpf=(500.0, 3000.0, 20000.0, 4000.0)),
    "ATT_Medium":      dict(inner=600.0,  falloff=5000.0, lpf=(1000.0, 6000.0, 20000.0, 3000.0)),
    "ATT_AmbientWide": dict(inner=1500.0, falloff=8000.0, lpf=(2000.0, 9000.0, 20000.0, 2500.0)),
}

# How many of each may sound at once, and who loses when the budget is full.
CONCURRENCY = {
    "CON_Impacts":  dict(max_count=6, rule="STOP_QUIETEST"),
    "CON_Movement": dict(max_count=4, rule="STOP_QUIETEST"),
    "CON_UI":       dict(max_count=2, rule="STOP_OLDEST"),
}

# --------------------------------------------------------------------------
# Sound events (UPartySoundEvent assets)
#
# variants: (sound name, min intensity, max intensity). Light/medium/heavy bands
# overlap a little so the switch between recordings is never audible as a step.
# --------------------------------------------------------------------------

def _band(prefix, count, lo, hi, start=1):
    return [("%s_%02d" % (prefix, i), lo, hi) for i in range(start, start + count)]


EVENTS = {
    "SE_Octo_ImpactFlesh": dict(
        where="Octo", sound_class="SC_Impacts", attenuation="ATT_Small", concurrency="CON_Impacts",
        variants=_band("Octo_ImpactFlesh_Light", 5, 0.0, 0.42)
               + _band("Octo_ImpactFlesh_Medium", 5, 0.32, 0.78)
               + _band("Octo_ImpactFlesh_Heavy", 5, 0.68, 1.0),
        volume_db=-2.0, vol_min=0.18, vol_max=1.0, vol_exp=1.6,
        pitch_min=1.0, pitch_max=-1.0, lowpass_min=2500.0,
        vol_jitter=1.5, pitch_jitter=0.8, retrigger=0.06),

    "SE_Octo_ArmPlant": dict(
        where="Octo", sound_class="SC_Movement", attenuation="ATT_Small", concurrency="CON_Movement",
        variants=_band("Octo_ArmPlant", 6, 0.0, 1.0),
        volume_db=-7.0, vol_min=0.35, vol_max=1.0, vol_exp=1.0,
        pitch_min=1.5, pitch_max=-0.5, lowpass_min=3000.0,
        vol_jitter=2.0, pitch_jitter=1.5, retrigger=0.12),

    "SE_Octo_ArmUnstick": dict(
        where="Octo", sound_class="SC_Movement", attenuation="ATT_Small", concurrency="CON_Movement",
        variants=_band("Octo_ArmUnstick", 9, 0.0, 1.0),
        volume_db=-9.0, vol_min=0.4, vol_max=1.0, vol_exp=1.0,
        pitch_min=2.0, pitch_max=0.0, lowpass_min=4000.0,
        vol_jitter=2.0, pitch_jitter=2.0, retrigger=0.12),

    "SE_Octo_PushOff": dict(
        where="Octo", sound_class="SC_Movement", attenuation="ATT_Small", concurrency="CON_Movement",
        variants=_band("Octo_PushOff", 4, 0.0, 1.0),
        volume_db=-6.0, vol_min=0.3, vol_max=1.0, vol_exp=1.3,
        pitch_min=1.0, pitch_max=-1.0, lowpass_min=900.0,
        vol_jitter=1.0, pitch_jitter=1.0, retrigger=0.15),

    # Loops: intensity is the drive. Faster roll = louder, brighter, a touch higher.
    "SE_Octo_RollLoop": dict(
        where="Octo", sound_class="SC_Movement", attenuation="ATT_Small", concurrency="CON_Movement",
        variants=_band("Octo_RollLoop", 3, 0.0, 1.0), loop=True,
        volume_db=-8.0, vol_min=0.15, vol_max=1.0, vol_exp=1.2,
        pitch_min=-2.0, pitch_max=2.0, lowpass_min=1200.0,
        vol_jitter=1.0, pitch_jitter=1.0, retrigger=0.0, random_start=True),

    "SE_Octo_AirRush": dict(
        where="Octo", sound_class="SC_Movement", attenuation="ATT_Small", concurrency="CON_Movement",
        variants=_band("Octo_AirRush", 2, 0.0, 1.0), loop=True,
        volume_db=-10.0, vol_min=0.0, vol_max=1.0, vol_exp=1.8,
        pitch_min=-3.0, pitch_max=2.0, lowpass_min=700.0,
        vol_jitter=0.5, pitch_jitter=0.5, retrigger=0.0, random_start=True),

    # Surface layers: lower than the flesh layer — the body is soft, so what it
    # lands on is heard mostly as weight, not as a clean knock.
    "SE_Surface_Wood": dict(
        where="Shared", sound_class="SC_Impacts", attenuation="ATT_Small", concurrency="CON_Impacts",
        variants=_band("Surface_Wood_Light", 5, 0.0, 0.5) + _band("Surface_Wood_Heavy", 5, 0.4, 1.0),
        volume_db=-6.0, vol_min=0.12, vol_max=1.0, vol_exp=1.8,
        pitch_min=1.0, pitch_max=-1.0, lowpass_min=1500.0,
        vol_jitter=1.5, pitch_jitter=1.0, retrigger=0.06),

    "SE_Surface_Stone": dict(
        where="Shared", sound_class="SC_Impacts", attenuation="ATT_Small", concurrency="CON_Impacts",
        variants=_band("Surface_Stone_Light", 5, 0.0, 0.55) + _band("Surface_Stone_Heavy", 3, 0.45, 1.0),
        volume_db=-6.0, vol_min=0.12, vol_max=1.0, vol_exp=1.8,
        pitch_min=1.0, pitch_max=-1.5, lowpass_min=1200.0,
        vol_jitter=1.5, pitch_jitter=1.0, retrigger=0.06),

    "SE_Surface_Sand": dict(
        where="Shared", sound_class="SC_Impacts", attenuation="ATT_Small", concurrency="CON_Impacts",
        variants=_band("Surface_Sand_Light", 5, 0.0, 0.6) + _band("Surface_Sand_Heavy", 2, 0.5, 1.0),
        volume_db=-5.0, vol_min=0.15, vol_max=1.0, vol_exp=1.5,
        pitch_min=1.0, pitch_max=-1.0, lowpass_min=2000.0,
        vol_jitter=2.0, pitch_jitter=1.5, retrigger=0.06),

    "SE_Surface_Metal": dict(
        where="Shared", sound_class="SC_Impacts", attenuation="ATT_Small", concurrency="CON_Impacts",
        variants=_band("Surface_Metal_Light", 3, 0.0, 0.5) + _band("Surface_Metal_Heavy", 4, 0.4, 1.0),
        volume_db=-7.0, vol_min=0.12, vol_max=1.0, vol_exp=1.8,
        pitch_min=0.5, pitch_max=-1.0, lowpass_min=2000.0,
        vol_jitter=1.5, pitch_jitter=0.7, retrigger=0.06),
}

# --------------------------------------------------------------------------
# Surfaces (build_audio_assets.py)
#
# (asset, SurfaceType index, keywords). Indices match DefaultEngine.ini [/Script/PhysicsCore.PhysicsSettings].
# Each PM_* is assigned to every material used in L_OctoOdyssey whose path
# matches one of its keywords (first match wins, case-insensitive). The script
# prints what it matched and what it couldn't, so gaps are visible.
# --------------------------------------------------------------------------
SURFACES = [
    ("PM_Wood",  1, ["wood", "plank", "bark", "log", "crate", "dock", "pier"]),
    ("PM_Sand",  3, ["sand", "beach", "dune"]),
    ("PM_Metal", 4, ["metal", "iron", "steel", "rust"]),
    ("PM_Stone", 2, ["stone", "rock", "cliff", "boulder", "granite", "concrete", "brick", "pebble", "terrain", "ground", "dirt"]),
]

# The level whose materials get surfaces.
SURFACE_LEVEL = "/Game/Maps/L_OctoOdyssey"
