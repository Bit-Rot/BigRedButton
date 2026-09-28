"""
build_audio_assets.py — runs INSIDE the open Unreal editor.

  python AI/tools/prep_audio.py                          # first: source -> Assets/Audio WAVs
  python AI/tools/ue_remote.py AI/build_audio_assets.py  # then: WAVs -> assets

Builds everything described in AI/audio_manifest.py:

  1. mix assets under /Game/Audio/Mix: the SoundClass tree, attenuation and
     concurrency settings;
  2. SoundWaves, imported from Assets/Audio (re-imported every run, so a
     re-prepped WAV is picked up), each routed to its event's SoundClass, and
     loops flagged looping;
  3. the UPartySoundEvent assets gameplay references;
  4. PM_* physical materials, assigned to the materials L_OctoOdyssey uses (by
     keyword) so impacts can tell wood from stone.

Idempotent: every asset is created if missing and otherwise overwritten in
place, so asset references (the octopus's soft paths, the level) never break.
Everything this script owns is rewritten from the manifest, so edit the
manifest, not the assets.

Step 4 only runs when L_OctoOdyssey is the open level (it reads the level's
meshes to find which materials are in use); it never switches levels.

Needs the PartyAudio plugin compiled into this editor (unreal.PartySoundEvent,
unreal.PartyAudioLibrary). Paths are absolute: ue_remote.py sends this file as
text, so __file__ is unset.
"""

import importlib
import os
import sys

import unreal

REPO_ROOT = "C:/Users/BitRot/BigRedButton"
sys.path.insert(0, f"{REPO_ROOT}/AI")
import audio_manifest  # noqa: E402

M = importlib.reload(audio_manifest)  # pick up edits between runs in one editor session

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    print(f"[build_audio_assets] {msg}")


def warn(msg):
    unreal.log_warning(f"[build_audio_assets] {msg}")


def get_or_create(folder, name, asset_class, factory):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        asset = eal.load_asset(path)
        if asset and isinstance(asset, asset_class):
            return asset
        raise RuntimeError(f"{path} exists but is not a {asset_class.__name__}")
    asset = asset_tools.create_asset(name, folder, asset_class, factory)
    if not asset:
        raise RuntimeError(f"could not create {path}")
    return asset


# --------------------------------------------------------------------------
# 1. Mix
# --------------------------------------------------------------------------

def build_sound_classes():
    classes = {}
    for name, parent, volume in M.SOUND_CLASSES:
        sc = get_or_create(M.MIX_PATH, name, unreal.SoundClass, unreal.SoundClassFactory())
        props = sc.get_editor_property("properties")
        props.set_editor_property("volume", volume)
        sc.set_editor_property("properties", props)
        classes[name] = sc

    # Parent and child lists are both stored; the editor keeps them in step, so do the same.
    for name, parent, _ in M.SOUND_CLASSES:
        sc = classes[name]
        sc.set_editor_property("parent_class", classes[parent] if parent else None)
        children = [classes[n] for n, p, _ in M.SOUND_CLASSES if p == name]
        sc.set_editor_property("child_classes", children)

    for sc in classes.values():
        eal.save_loaded_asset(sc, only_if_is_dirty=False)
    log(f"sound classes: {', '.join(classes)}")
    return classes


def build_attenuations():
    result = {}
    for name, spec in M.ATTENUATIONS.items():
        att = get_or_create(M.MIX_PATH, name, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
        s = att.get_editor_property("attenuation")
        s.set_editor_property("attenuate", True)
        s.set_editor_property("spatialize", True)
        s.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
        s.set_editor_property("attenuation_shape_extents", unreal.Vector(spec["inner"], 0.0, 0.0))
        s.set_editor_property("falloff_distance", spec["falloff"])
        # Natural-sound falloff: the real-world curve, not a linear ramp that
        # makes everything sound equally far until it suddenly isn't.
        s.set_editor_property("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND)
        lpf_min, lpf_max, freq_at_min, freq_at_max = spec["lpf"]
        s.set_editor_property("attenuate_with_lpf", True)
        s.set_editor_property("lpf_radius_min", lpf_min)
        s.set_editor_property("lpf_radius_max", lpf_max)
        s.set_editor_property("lpf_frequency_at_min", freq_at_min)
        s.set_editor_property("lpf_frequency_at_max", freq_at_max)
        att.set_editor_property("attenuation", s)
        eal.save_loaded_asset(att, only_if_is_dirty=False)
        result[name] = att
    log(f"attenuations: {', '.join(result)}")
    return result


def build_concurrency():
    result = {}
    for name, spec in M.CONCURRENCY.items():
        con = get_or_create(M.MIX_PATH, name, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
        s = con.get_editor_property("concurrency")
        s.set_editor_property("max_count", spec["max_count"])
        s.set_editor_property("resolution_rule", getattr(unreal.MaxConcurrentResolutionRule, spec["rule"]))
        con.set_editor_property("concurrency", s)
        eal.save_loaded_asset(con, only_if_is_dirty=False)
        result[name] = con
    log(f"concurrency: {', '.join(result)}")
    return result


# --------------------------------------------------------------------------
# 2. SoundWaves
# --------------------------------------------------------------------------

def sound_routing():
    """name -> (sound class name, looping), from the first event that uses the sound."""
    routing = {}
    for event in M.EVENTS.values():
        for name, _, _ in event["variants"]:
            routing.setdefault(name, (event["sound_class"], bool(event.get("loop"))))
    return routing


def import_sounds(classes):
    routing = sound_routing()
    tasks, specs = [], []
    for spec in M.SOUNDS:
        wav_dir, content_dir = M.GROUPS[spec["group"]]
        source = f"{REPO_ROOT}/{wav_dir}/{spec['name']}.wav"
        if not os.path.isfile(source):
            warn(f"missing {source} — run AI/tools/prep_audio.py")
            continue
        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = content_dir
        task.destination_name = spec["name"]
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
        specs.append(spec)

    asset_tools.import_asset_tasks(tasks)

    sounds = {}
    for spec in specs:
        path = f"{M.GROUPS[spec['group']][1]}/{spec['name']}"
        wave = eal.load_asset(path)
        if not wave:
            warn(f"import produced nothing at {path}")
            continue
        class_name, looping = routing.get(spec["name"], ("SC_SFX", False))
        wave.set_editor_property("looping", looping)
        wave.set_editor_property("sound_class_object", classes[class_name])
        eal.save_loaded_asset(wave, only_if_is_dirty=False)
        sounds[spec["name"]] = wave

    unused = sorted(set(sounds) - set(routing))
    if unused:
        warn(f"imported but used by no event: {unused}")
    log(f"sound waves: {len(sounds)} imported")
    return sounds


# --------------------------------------------------------------------------
# 3. Events
# --------------------------------------------------------------------------

def build_events(sounds, attenuations, concurrency):
    events = {}
    for name, spec in M.EVENTS.items():
        folder = M.EVENT_PATHS[spec["where"]]
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.PartySoundEvent)
        event = get_or_create(folder, name, unreal.PartySoundEvent, factory)

        variants = []
        for sound_name, lo, hi in spec["variants"]:
            wave = sounds.get(sound_name)
            if not wave:
                warn(f"{name}: no sound '{sound_name}'")
                continue
            v = unreal.PartySoundVariant()
            v.set_editor_property("sound", wave)
            v.set_editor_property("weight", 1.0)
            v.set_editor_property("min_intensity", lo)
            v.set_editor_property("max_intensity", hi)
            variants.append(v)

        event.set_editor_property("variants", variants)
        event.set_editor_property("selection", unreal.PartySoundSelection.SHUFFLE_BAG)
        event.set_editor_property("volume_db", spec["volume_db"])
        event.set_editor_property("volume_jitter_db", spec["vol_jitter"])
        event.set_editor_property("pitch_jitter_semitones", spec["pitch_jitter"])
        event.set_editor_property("volume_at_min_intensity", spec["vol_min"])
        event.set_editor_property("volume_at_max_intensity", spec["vol_max"])
        event.set_editor_property("volume_curve_exponent", spec["vol_exp"])
        event.set_editor_property("pitch_at_min_intensity_semitones", spec["pitch_min"])
        event.set_editor_property("pitch_at_max_intensity_semitones", spec["pitch_max"])
        event.set_editor_property("low_pass_at_min_intensity_hz", spec["lowpass_min"])
        event.set_editor_property("min_retrigger_seconds", spec["retrigger"])
        event.set_editor_property("spatialized", spec.get("spatial", True))
        event.set_editor_property("random_start_time", spec.get("random_start", False))
        event.set_editor_property("attenuation", attenuations.get(spec.get("attenuation")))
        event.set_editor_property("concurrency", concurrency.get(spec.get("concurrency")))
        eal.save_loaded_asset(event, only_if_is_dirty=False)
        events[name] = event
        log(f"event {folder}/{name}: {len(variants)} variants")
    return events


# --------------------------------------------------------------------------
# 4. Surfaces
# --------------------------------------------------------------------------

def build_physical_materials():
    pms = []
    for name, index, keywords in M.SURFACES:
        pm = get_or_create(M.PHYSMAT_PATH, name, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
        if not unreal.PartyAudioLibrary.set_physical_material_surface_index(pm, index):
            warn(f"could not set {name} to SurfaceType{index}")
        eal.save_loaded_asset(pm, only_if_is_dirty=False)
        pms.append((pm, [k.lower() for k in keywords]))
    log(f"physical materials: {', '.join(n for n, _, _ in M.SURFACES)}")
    return pms


def level_materials():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_outermost().get_name() != M.SURFACE_LEVEL:
        return None
    found = {}
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.MeshComponent):
            for mat in comp.get_materials():
                if mat:
                    found[mat.get_path_name()] = mat
    return found


def assign_surfaces(pms):
    materials = level_materials()
    if materials is None:
        warn(f"{M.SURFACE_LEVEL} is not the open level — physical materials built but NOT assigned. "
             "Open it and re-run to assign surfaces.")
        return

    assigned, unmatched, skipped = [], [], []
    for path, mat in sorted(materials.items()):
        if path.startswith("/Engine/"):
            skipped.append(path)  # engine content is read-only; shows up as Default (stone layer)
            continue
        lower = path.lower()
        match = next((pm for pm, keys in pms if any(k in lower for k in keys)), None)
        if not match:
            unmatched.append(path)
            continue
        # Walk up the instance chain too, while the parent still matches the same
        # keyword: sibling instances not placed yet (MI_WoodBlock_Dark, ...)
        # inherit the surface from M_WoodBlock_Triplanar instead of waiting to be used.
        keys = next(k for pm, k in pms if pm == match)
        chain = [mat]
        while isinstance(chain[-1], unreal.MaterialInstance):
            parent = chain[-1].get_editor_property("parent")
            if not parent or parent.get_path_name().startswith("/Engine/") \
                    or not any(k in parent.get_path_name().lower() for k in keys):
                break
            chain.append(parent)
        for m in chain:
            if m.get_editor_property("phys_material") != match:
                m.set_editor_property("phys_material", match)
                eal.save_loaded_asset(m, only_if_is_dirty=False)
            assigned.append(f"{m.get_path_name().split('.')[0]} -> {match.get_name()}")
    assigned = sorted(set(assigned))

    log(f"surfaces assigned ({len(assigned)}):")
    for line in assigned:
        log(f"  {line}")
    if unmatched:
        warn(f"no surface keyword matched (they play the default/stone layer): {unmatched}")
    if skipped:
        log(f"engine materials left alone: {skipped}")


# --------------------------------------------------------------------------

def verify(events):
    ok = True
    for name, event in events.items():
        variants = event.get_editor_property("variants")
        if not variants:
            warn(f"{name} has no variants")
            ok = False
        if not event.get_editor_property("attenuation") or not event.get_editor_property("concurrency"):
            warn(f"{name} is missing attenuation or concurrency")
            ok = False
    log("verify: " + ("all events playable" if ok else "PROBLEMS above"))


def main():
    if not hasattr(unreal, "PartySoundEvent"):
        unreal.log_error("[build_audio_assets] This editor has no PartyAudio plugin loaded (unreal.PartySoundEvent). "
                         "Build PartyButtonsEditor and restart the editor first.")
        return
    classes = build_sound_classes()
    attenuations = build_attenuations()
    concurrency = build_concurrency()
    sounds = import_sounds(classes)
    events = build_events(sounds, attenuations, concurrency)
    assign_surfaces(build_physical_materials())
    verify(events)


main()
