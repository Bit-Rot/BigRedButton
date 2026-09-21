"""
build_wood_material.py — headless Unreal Editor Python script.

Imports T_Wood_Tiling_01.png and builds M_WoodBlock_Triplanar: an
object-space triplanar material (so the projection is fixed relative to each
mesh and does not "swim" across the surface when the actor moves or rotates),
plus a few Material Instance variants that only vary Tint / ORM levels /
sharpness, since only one source texture exists today.

Projection is done in OBJECT space, not world space:
  - Absolute World Position -> Transform Position (World -> Local)
  - Vertex Normal WS        -> Transform (World -> Local)
Both transforms are relative to the mesh's own LocalToWorld, so the blend
weights and the UVs they drive travel with the object instead of the world.

Normal-map blending uses the "whiteout" technique (reorient each planar
tangent-space sample into local space via the local geometric normal, then
weighted-sum and renormalize) via a Custom HLSL node, then transforms the
result from Local back to World space because the material's
"Tangent Space Normal" flag is turned off (Normal input expects World space
when that flag is false). This is the standard technique for triplanar normal
mapping — see Ben Golus' "Normal Mapping for a Triplanar Shader" for the
formula this reproduces node-for-node.

Normal/ORM source: prefers the AI-generated maps at
AI/reference/rtx-remix-texture-gen.md's output location (see ORM_SOURCE /
NORMAL_SOURCE below) and falls back to flat/neutral placeholders — with
NormalIntensity defaulting to 0.0 in that fallback case — if they aren't
present. Regenerate them with AI/tools/generate_pbr_maps.py + pack_orm.py.

Each ORM channel (AO / Roughness / Metallic) gets its own Levels-style
remap — <Channel>InputMin/Max, <Channel>OutputMin/Max — exposed as Material
Instance scalar parameters. This is the standard "levels" technique (as in
Photoshop/Substance): InputMin/Max pick the black/white points *within the
source texture's own value range*, stretching whatever detail lives between
them across the full 0-1 span; OutputMin/Max then remap that stretched value
into the range you actually want. That combination is what lets you raise or
lower a channel's overall level without flattening the relative
light/dark detail baked into the texture — a plain multiply can't lift a
near-zero floor, and a plain add clips highlights.

The AI-generated roughness map for T_Wood_Tiling_01 measures range
[0.0, 0.49] (mean 0.16) — inspected directly with PIL, not assumed — hence
RoughnessInputMax defaults to 0.5: that's the real ceiling of the source
data, so the full input range maps its detail across the full 0-1 span
before RoughnessOutputMin/Max remaps it up into a matte-wood band.

Run via:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
  -unattended -nopause -NoSplash -stdout -log

Only unreal.log_warning / unreal.log_error reach the log under
-run=pythonscript (plain print()/unreal.log() are silently swallowed), so all
progress reporting below goes through log_warning on purpose.
"""

import os
import struct
import tempfile
import zlib

import unreal

TEXTURE_SOURCE = "C:/Users/BitRot/BigRedButton/Assets/Textures/T_Wood_Tiling_01.png"
TEXTURES_DIR = "/Game/OctoOdyssey/Textures"
MATERIALS_DIR = "/Game/OctoOdyssey/Materials"
BASE_COLOR_TEXTURE_NAME = "T_Wood_Tiling_01"
MATERIAL_NAME = "M_WoodBlock_Triplanar"

# AI-generated maps (see AI/reference/rtx-remix-texture-gen.md). Used if present; falls
# back to the neutral placeholders below otherwise.
ORM_SOURCE = "C:/Users/BitRot/BigRedButton/Assets/RTXRemix/T_Wood_Tiling_01/T_Wood_Tiling_01_ORM.png"
NORMAL_SOURCE = "C:/Users/BitRot/BigRedButton/Assets/RTXRemix/T_Wood_Tiling_01/T_Wood_Tiling_01_normal_dx.png"
ORM_TEXTURE_NAME = "T_Wood_Tiling_01_ORM"
NORMAL_TEXTURE_NAME = "T_Wood_Tiling_01_Normal"

# Neutral placeholders for the ORM/Normal slots: no real maps have been authored
# yet (only a base-color wood texture was supplied). A texture parameter whose
# SamplerType is Normal/Masks MUST point at a texture whose own Compression
# Settings match — otherwise the per-platform shader compile hard-fails and the
# whole material silently falls back to the checkerboard default in-game (it
# still "compiles" in the editor's own preview, which is why this only shows up
# once you launch with -game). Engine-provided textures like WhiteSquareTexture
# are Color-compressed, so they fail that check for Normal/Masks — hence
# generating tiny same-compression placeholders instead of reusing engine assets.
NEUTRAL_ORM_NAME = "T_WoodBlock_NeutralORM"      # R=AO(1), G=Roughness(0.75), B=Metallic(0)
FLAT_NORMAL_NAME = "T_WoodBlock_FlatNormal"      # (128,128,255) -> tangent-space (0,0,1)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning(f"[build_wood_material] {msg}")


def write_solid_rgb_png(path, width, height, rgb):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    signature = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit, truecolor (no alpha)
    row = bytes([0]) + bytes(rgb) * width  # filter byte 0 (None) + pixels
    raw = row * height
    idat = zlib.compress(raw, 9)
    with open(path, "wb") as f:
        f.write(signature + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b""))


def import_texture(source_file, destination_name):
    # NOTE: unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([...]) hard-crashes
    # here (EXCEPTION: Assertion failed: CurrentApplication.IsValid(), SlateApplication.h:321)
    # — under -run=pythonscript there is no Slate Application, and AssetTools' import path
    # touches ContentBrowser UI as a side effect of the import even with automated=True.
    # Going straight through InterchangeManager's scripted API skips that UI touch entirely.
    existing_path = f"{TEXTURES_DIR}/{destination_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(existing_path):
        log(f"{existing_path} already exists — reusing.")
        return unreal.load_asset(existing_path)

    unreal.EditorAssetLibrary.make_directory(TEXTURES_DIR)
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    source_data = mgr.create_source_data(source_file)
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("destination_name", destination_name)
    mgr.import_asset(TEXTURES_DIR, source_data, params)
    mgr.wait_until_all_tasks_done(False)

    if not unreal.EditorAssetLibrary.does_asset_exist(existing_path):
        log(f"ERROR: import did not produce {existing_path}")
        return None
    log(f"Imported {existing_path}")
    return unreal.load_asset(existing_path)


def import_base_color_texture():
    return import_texture(TEXTURE_SOURCE, BASE_COLOR_TEXTURE_NAME)


def import_placeholder_maps():
    tmp_dir = tempfile.gettempdir()
    orm_path = os.path.join(tmp_dir, f"{NEUTRAL_ORM_NAME}.png")
    normal_path = os.path.join(tmp_dir, f"{FLAT_NORMAL_NAME}.png")
    write_solid_rgb_png(orm_path, 4, 4, (255, 191, 0))
    write_solid_rgb_png(normal_path, 4, 4, (128, 128, 255))

    orm_tex = import_texture(orm_path, NEUTRAL_ORM_NAME)
    orm_tex.set_editor_property("CompressionSettings", unreal.TextureCompressionSettings.TC_MASKS)
    orm_tex.set_editor_property("SRGB", False)
    unreal.EditorAssetLibrary.save_asset(f"{TEXTURES_DIR}/{NEUTRAL_ORM_NAME}")

    normal_tex = import_texture(normal_path, FLAT_NORMAL_NAME)
    normal_tex.set_editor_property("CompressionSettings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    normal_tex.set_editor_property("SRGB", False)
    unreal.EditorAssetLibrary.save_asset(f"{TEXTURES_DIR}/{FLAT_NORMAL_NAME}")

    os.remove(orm_path)
    os.remove(normal_path)
    return orm_tex, normal_tex


def import_orm_and_normal():
    """
    Prefer the AI-generated Normal/ORM maps if present; otherwise fall back to the flat
    neutral placeholders.

    Returns:
        (orm_texture, normal_texture, default_normal_intensity)
    """
    if os.path.exists(ORM_SOURCE) and os.path.exists(NORMAL_SOURCE):
        orm_tex = import_texture(ORM_SOURCE, ORM_TEXTURE_NAME)
        orm_tex.set_editor_property("CompressionSettings", unreal.TextureCompressionSettings.TC_MASKS)
        orm_tex.set_editor_property("SRGB", False)
        unreal.EditorAssetLibrary.save_asset(f"{TEXTURES_DIR}/{ORM_TEXTURE_NAME}")

        normal_tex = import_texture(NORMAL_SOURCE, NORMAL_TEXTURE_NAME)
        normal_tex.set_editor_property("CompressionSettings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        normal_tex.set_editor_property("SRGB", False)
        unreal.EditorAssetLibrary.save_asset(f"{TEXTURES_DIR}/{NORMAL_TEXTURE_NAME}")

        log(f"Using generated maps: {ORM_SOURCE} / {NORMAL_SOURCE}")
        return orm_tex, normal_tex, 1.0

    log(
        "Generated ORM/Normal maps not found - falling back to neutral placeholders. "
        "Run AI/tools/generate_pbr_maps.py + pack_orm.py to produce real ones."
    )
    return import_placeholder_maps() + (0.0,)


def make_expr(material, expr_class, x, y):
    return mel.create_material_expression(material, expr_class, x, y)


def connect(frm, from_out, to, to_in):
    ok = mel.connect_material_expressions(frm, from_out, to, to_in)
    if not ok:
        log(f"ERROR: failed to connect {frm.get_name()}.{from_out!r} -> {to.get_name()}.{to_in!r}")
    return ok


def add_channel_levels(material, raw_expr, prefix, x, y, input_min, input_max, output_min, output_max):
    """
    Standard "Levels" remap for one packed ORM channel (Input Min/Max, Output Min/Max —
    same mental model as Photoshop/Substance's Levels tool): normalizes the raw [0,1]
    sample against [InputMin, InputMax] (the source texture's own black/white points),
    then remaps that normalized value into [OutputMin, OutputMax]. Exposed as four
    Material Instance scalar parameters: "{prefix}InputMin/Max", "{prefix}OutputMin/Max".
    """
    p_in_min = make_expr(material, unreal.MaterialExpressionScalarParameter, x - 300, y)
    p_in_min.set_editor_property("ParameterName", f"{prefix}InputMin")
    p_in_min.set_editor_property("DefaultValue", input_min)

    p_in_max = make_expr(material, unreal.MaterialExpressionScalarParameter, x - 300, y + 50)
    p_in_max.set_editor_property("ParameterName", f"{prefix}InputMax")
    p_in_max.set_editor_property("DefaultValue", input_max)

    p_out_min = make_expr(material, unreal.MaterialExpressionScalarParameter, x - 300, y + 100)
    p_out_min.set_editor_property("ParameterName", f"{prefix}OutputMin")
    p_out_min.set_editor_property("DefaultValue", output_min)

    p_out_max = make_expr(material, unreal.MaterialExpressionScalarParameter, x - 300, y + 150)
    p_out_max.set_editor_property("ParameterName", f"{prefix}OutputMax")
    p_out_max.set_editor_property("DefaultValue", output_max)

    custom = make_expr(material, unreal.MaterialExpressionCustom, x, y)
    custom.set_editor_property("Description", f"{prefix}Levels")
    custom.set_editor_property("OutputType", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom_inputs = []
    for n in ["Raw", "InputMin", "InputMax", "OutputMin", "OutputMax"]:
        ci = unreal.CustomInput()
        ci.set_editor_property("InputName", n)
        custom_inputs.append(ci)
    custom.set_editor_property("Inputs", custom_inputs)
    custom.set_editor_property("Code", """
float t = saturate((Raw - InputMin) / max(InputMax - InputMin, 0.0001));
return saturate(lerp(OutputMin, OutputMax, t));
""".strip())

    connect(raw_expr, "", custom, "Raw")
    connect(p_in_min, "", custom, "InputMin")
    connect(p_in_max, "", custom, "InputMax")
    connect(p_out_min, "", custom, "OutputMin")
    connect(p_out_max, "", custom, "OutputMax")
    return custom


def build_material(base_color_texture, orm_texture, normal_texture, normal_intensity_default):
    material_path = f"{MATERIALS_DIR}/{MATERIAL_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        unreal.EditorAssetLibrary.delete_asset(material_path)
        log(f"Deleted existing {material_path} for a clean rebuild.")

    unreal.EditorAssetLibrary.make_directory(MATERIALS_DIR)
    material = asset_tools.create_asset(MATERIAL_NAME, MATERIALS_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("bTangentSpaceNormal", False)  # Normal pin below is World space.

    # ---- Parameters -----------------------------------------------------
    p_base_color_tex = []
    p_orm_tex = []
    p_normal_tex = []
    for i in range(3):
        t = make_expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, -600 + i * 180)
        t.set_editor_property("ParameterName", "BaseColorTexture")
        t.set_editor_property("SamplerType", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        t.set_editor_property("Texture", base_color_texture)
        p_base_color_tex.append(t)

        o = make_expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, -100 + i * 180)
        o.set_editor_property("ParameterName", "ORMTexture")
        o.set_editor_property("SamplerType", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        o.set_editor_property("Texture", orm_texture)
        p_orm_tex.append(o)

        n = make_expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 500 + i * 180)
        n.set_editor_property("ParameterName", "NormalTexture")
        n.set_editor_property("SamplerType", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        n.set_editor_property("Texture", normal_texture)
        p_normal_tex.append(n)

    p_scale = make_expr(material, unreal.MaterialExpressionScalarParameter, -1400, -900)
    p_scale.set_editor_property("ParameterName", "TextureScale")
    p_scale.set_editor_property("DefaultValue", 0.01)  # 1 UV tile per meter on a 100uu cube.

    p_offset = make_expr(material, unreal.MaterialExpressionVectorParameter, -1400, -800)
    p_offset.set_editor_property("ParameterName", "TextureOffset")
    p_offset.set_editor_property("DefaultValue", unreal.LinearColor(0.0, 0.0, 0.0, 0.0))

    p_offset_rg = make_expr(material, unreal.MaterialExpressionComponentMask, -1200, -800)
    p_offset_rg.set_editor_property("R", True)
    p_offset_rg.set_editor_property("G", True)
    p_offset_rg.set_editor_property("B", False)
    p_offset_rg.set_editor_property("A", False)
    connect(p_offset, "", p_offset_rg, "")

    p_tint = make_expr(material, unreal.MaterialExpressionVectorParameter, 400, -900)
    p_tint.set_editor_property("ParameterName", "Tint")
    p_tint.set_editor_property("DefaultValue", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    p_sharpness = make_expr(material, unreal.MaterialExpressionScalarParameter, -1400, -1000)
    p_sharpness.set_editor_property("ParameterName", "TriplanarBlendSharpness")
    p_sharpness.set_editor_property("DefaultValue", 4.0)

    p_normal_intensity = make_expr(material, unreal.MaterialExpressionScalarParameter, 400, 700)
    p_normal_intensity.set_editor_property("ParameterName", "NormalIntensity")
    p_normal_intensity.set_editor_property("DefaultValue", normal_intensity_default)

    # ---- Object-space position / normal ----------------------------------
    world_pos = make_expr(material, unreal.MaterialExpressionWorldPosition, -1400, -400)

    local_pos = make_expr(material, unreal.MaterialExpressionTransformPosition, -1200, -400)
    local_pos.set_editor_property("TransformSourceType", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local_pos.set_editor_property("TransformType", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    connect(world_pos, "", local_pos, "")

    vertex_normal_ws = make_expr(material, unreal.MaterialExpressionVertexNormalWS, -1400, -300)

    local_normal = make_expr(material, unreal.MaterialExpressionTransform, -1200, -300)
    local_normal.set_editor_property("TransformSourceType", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    local_normal.set_editor_property("TransformType", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    connect(vertex_normal_ws, "", local_normal, "")

    # ---- Blend weights: abs(localNormal)^sharpness, normalized to sum 1 --
    abs_local_normal = make_expr(material, unreal.MaterialExpressionAbs, -1000, -300)
    connect(local_normal, "", abs_local_normal, "")

    sharpened = make_expr(material, unreal.MaterialExpressionPower, -900, -300)
    connect(abs_local_normal, "", sharpened, "Base")
    connect(p_sharpness, "", sharpened, "Exp")

    ones = make_expr(material, unreal.MaterialExpressionConstant3Vector, -900, -200)
    ones.set_editor_property("Constant", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    weight_sum = make_expr(material, unreal.MaterialExpressionDotProduct, -800, -250)
    connect(sharpened, "", weight_sum, "A")
    connect(ones, "", weight_sum, "B")

    blend_weights = make_expr(material, unreal.MaterialExpressionDivide, -700, -300)
    connect(sharpened, "", blend_weights, "A")
    connect(weight_sum, "", blend_weights, "B")

    def mask(expr, x, y, r, g, b, a=False):
        m = make_expr(material, unreal.MaterialExpressionComponentMask, x, y)
        m.set_editor_property("R", r)
        m.set_editor_property("G", g)
        m.set_editor_property("B", b)
        m.set_editor_property("A", a)
        connect(expr, "", m, "")
        return m

    weight_x = mask(blend_weights, -600, -380, True, False, False)
    weight_y = mask(blend_weights, -600, -300, False, True, False)
    weight_z = mask(blend_weights, -600, -220, False, False, True)

    # ---- Per-axis UVs: local position swizzled per plane, scaled+offset --
    uv_local_x = mask(local_pos, -1000, -450, False, True, True)   # YZ plane (X-facing)
    uv_local_y = mask(local_pos, -1000, -400, True, False, True)   # XZ plane (Y-facing)
    uv_local_z = mask(local_pos, -1000, -350, True, True, False)   # XY plane (Z-facing)

    def scaled_uv(local_uv, x, y):
        scaled = make_expr(material, unreal.MaterialExpressionMultiply, x, y)
        connect(local_uv, "", scaled, "A")
        connect(p_scale, "", scaled, "B")
        offset = make_expr(material, unreal.MaterialExpressionAdd, x + 150, y)
        connect(scaled, "", offset, "A")
        connect(p_offset_rg, "", offset, "B")
        return offset

    uv_x = scaled_uv(uv_local_x, -850, -450)
    uv_y = scaled_uv(uv_local_y, -850, -400)
    uv_z = scaled_uv(uv_local_z, -850, -350)
    uvs = [uv_x, uv_y, uv_z]

    for i, uv in enumerate(uvs):
        connect(uv, "", p_base_color_tex[i], "UVs")
        connect(uv, "", p_orm_tex[i], "UVs")
        connect(uv, "", p_normal_tex[i], "UVs")

    def weighted_sum3(sx, sy, sz, x, y, out_name=""):
        wx = make_expr(material, unreal.MaterialExpressionMultiply, x, y)
        connect(sx, out_name, wx, "A")
        connect(weight_x, "", wx, "B")
        wy = make_expr(material, unreal.MaterialExpressionMultiply, x, y + 60)
        connect(sy, out_name, wy, "A")
        connect(weight_y, "", wy, "B")
        wz = make_expr(material, unreal.MaterialExpressionMultiply, x, y + 120)
        connect(sz, out_name, wz, "A")
        connect(weight_z, "", wz, "B")
        add1 = make_expr(material, unreal.MaterialExpressionAdd, x + 180, y + 20)
        connect(wx, "", add1, "A")
        connect(wy, "", add1, "B")
        add2 = make_expr(material, unreal.MaterialExpressionAdd, x + 340, y + 40)
        connect(add1, "", add2, "A")
        connect(wz, "", add2, "B")
        return add2

    # ---- Base color: blend, then tint -------------------------------------
    blended_base_color = weighted_sum3(p_base_color_tex[0], p_base_color_tex[1], p_base_color_tex[2], -100, -900, "RGB")
    tint_rgb = mask(p_tint, 250, -950, True, True, True)
    final_base_color = make_expr(material, unreal.MaterialExpressionMultiply, 550, -900)
    connect(blended_base_color, "", final_base_color, "A")
    connect(tint_rgb, "", final_base_color, "B")
    mel.connect_material_property(final_base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # ---- ORM: blend, then split AO / Roughness / Metallic, each through its own
    # Levels remap (see add_channel_levels) ----------------------------------
    blended_orm = weighted_sum3(p_orm_tex[0], p_orm_tex[1], p_orm_tex[2], -100, -100, "RGB")

    ao_raw = mask(blended_orm, 250, -150, True, False, False)
    ao = add_channel_levels(
        material, ao_raw, "AO", 700, -150,
        input_min=0.0, input_max=1.0, output_min=0.0, output_max=1.0,  # identity: no known AO issue yet.
    )
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    roughness_raw = mask(blended_orm, 250, -50, False, True, False)
    roughness = add_channel_levels(
        material, roughness_raw, "Roughness", 700, -50,
        # The generated roughness map measures [0.0, 0.49] (mean 0.16) - InputMax=0.5
        # stretches that real ceiling across the full 0-1 range before OutputMin/Max
        # remaps it up into a matte-wood band, instead of just leaving it glossy.
        input_min=0.0, input_max=0.5, output_min=0.45, output_max=0.85,
    )
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metallic_raw = mask(blended_orm, 250, 50, False, False, True)
    metallic = add_channel_levels(
        material, metallic_raw, "Metallic", 700, 50,
        input_min=0.0, input_max=1.0, output_min=0.0, output_max=1.0,  # identity: ORM.B=0 (non-metal).
    )
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    # ---- Normal: whiteout triplanar blend in local space, then to world ---
    custom = make_expr(material, unreal.MaterialExpressionCustom, 100, 500)
    custom.set_editor_property("Description", "TriplanarWhiteoutNormalBlend")
    custom.set_editor_property("OutputType", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    input_names = ["NX", "NY", "NZ", "LocalNormal", "Wx", "Wy", "Wz"]
    custom_inputs = []
    for n in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("InputName", n)
        custom_inputs.append(ci)
    custom.set_editor_property("Inputs", custom_inputs)
    custom.set_editor_property("Code", """
float3 axisSign = sign(LocalNormal);

float3 tnormalX = NX;
tnormalX.xy += LocalNormal.zy;
tnormalX.z = abs(tnormalX.z) * axisSign.x;

float3 tnormalY = NY;
tnormalY.xy += LocalNormal.xz;
tnormalY.z = abs(tnormalY.z) * axisSign.y;

float3 tnormalZ = NZ;
tnormalZ.xy += LocalNormal.xy;
tnormalZ.z = abs(tnormalZ.z) * axisSign.z;

float3 blended =
    tnormalX.zyx * Wx +
    tnormalY.xzy * Wy +
    tnormalZ.xyz * Wz;

return normalize(blended);
""".strip())

    connect(p_normal_tex[0], "RGB", custom, "NX")
    connect(p_normal_tex[1], "RGB", custom, "NY")
    connect(p_normal_tex[2], "RGB", custom, "NZ")
    connect(local_normal, "", custom, "LocalNormal")
    connect(weight_x, "", custom, "Wx")
    connect(weight_y, "", custom, "Wy")
    connect(weight_z, "", custom, "Wz")

    world_normal = make_expr(material, unreal.MaterialExpressionTransform, 350, 500)
    world_normal.set_editor_property("TransformSourceType", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    world_normal.set_editor_property("TransformType", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    connect(custom, "", world_normal, "")

    final_normal = make_expr(material, unreal.MaterialExpressionLinearInterpolate, 550, 600)
    connect(vertex_normal_ws, "", final_normal, "A")
    connect(world_normal, "", final_normal, "B")
    connect(p_normal_intensity, "", final_normal, "Alpha")
    mel.connect_material_property(final_normal, "", unreal.MaterialProperty.MP_NORMAL)

    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(material_path)
    log(f"Built and saved {material_path}")
    return material


def make_instance(parent, name, vector_overrides, scalar_overrides):
    path = f"{MATERIALS_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)

    factory = unreal.MaterialInstanceConstantFactoryNew()
    instance = asset_tools.create_asset(name, MATERIALS_DIR, unreal.MaterialInstanceConstant, factory)
    mel.set_material_instance_parent(instance, parent)

    for param_name, color in vector_overrides.items():
        mel.set_material_instance_vector_parameter_value(instance, param_name, color)
    for param_name, value in scalar_overrides.items():
        mel.set_material_instance_scalar_parameter_value(instance, param_name, value)

    unreal.EditorAssetLibrary.save_asset(path)
    log(f"Built and saved {path}")
    return instance


def main():
    base_color_texture = import_base_color_texture()
    if base_color_texture is None:
        log("ERROR: aborting, base color texture import failed.")
        return
    orm_texture, normal_texture, normal_intensity_default = import_orm_and_normal()

    material = build_material(base_color_texture, orm_texture, normal_texture, normal_intensity_default)

    make_instance(
        material, "MI_WoodBlock",
        vector_overrides={},
        scalar_overrides={},
    )
    make_instance(
        material, "MI_WoodBlock_Dark",
        vector_overrides={"Tint": unreal.LinearColor(0.45, 0.30, 0.18, 1.0)},
        scalar_overrides={"RoughnessOutputMin": 0.35, "RoughnessOutputMax": 0.75},
    )
    make_instance(
        material, "MI_WoodBlock_Weathered",
        vector_overrides={"Tint": unreal.LinearColor(0.55, 0.55, 0.52, 1.0)},
        scalar_overrides={"RoughnessOutputMin": 0.55, "RoughnessOutputMax": 0.95},
    )

    log("Done.")


main()
