"""Throwaway preview-level script for visually checking M_WoodBlock_Triplanar. Not part of the repo's real tooling."""
import unreal

MAP_PATH = "/Game/OctoOdyssey/Materials/_WoodPreview"
CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"


def make_rotator(pitch, yaw, roll):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAP_PATH)

    unreal.EditorLevelLibrary.new_level(MAP_PATH)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cube_mesh = unreal.load_asset(CUBE_MESH_PATH)

    materials = [
        unreal.load_asset("/Game/OctoOdyssey/Materials/MI_WoodBlock"),
        unreal.load_asset("/Game/OctoOdyssey/Materials/MI_WoodBlock_Dark"),
        unreal.load_asset("/Game/OctoOdyssey/Materials/MI_WoodBlock_Weathered"),
    ]

    # Three plain cubes side by side.
    for i, mat in enumerate(materials):
        actor = actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(i * 250.0, 0.0, 50.0), make_rotator(0, 0, 0))
        mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh_comp.set_static_mesh(cube_mesh)
        mesh_comp.set_material(0, mat)
        actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        actor.set_actor_label(f"Cube_{i}")

    # A non-uniformly scaled cube (checks tiling density per axis, not distortion).
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(750.0, 0.0, 100.0), make_rotator(0, 0, 0))
    mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh_comp.set_static_mesh(cube_mesh)
    mesh_comp.set_material(0, materials[0])
    actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 3.0))
    actor.set_actor_label("Cube_Tall")

    # A cube rotated on multiple axes — the whole point of object-space triplanar
    # is that this should still look like continuous wood grain, not a seam mess.
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(1050.0, 0.0, 50.0), make_rotator(25, 40, 15))
    mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh_comp.set_static_mesh(cube_mesh)
    mesh_comp.set_material(0, materials[0])
    actor.set_actor_label("Cube_Rotated")

    # Light + sky so it isn't rendered pitch black.
    light = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 500), make_rotator(-40, 200, 0))
    light_comp = light.get_component_by_class(unreal.DirectionalLightComponent)
    light_comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    light_comp.set_editor_property("intensity", 6.0)

    sky = actor_subsystem.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500), make_rotator(0, 0, 0))
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_comp.set_editor_property("intensity", 1.5)

    # PlayerStart so `-game` mode's DefaultPawn spawns framing the cubes.
    start = actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(500.0, -700.0, 250.0), make_rotator(-10, 60, 0))
    start.set_actor_label("PreviewStart")

    world_settings = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    world_settings.set_editor_property("default_game_mode", unreal.GameModeBase)

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log_warning(f"[preview] Saved {MAP_PATH}")


main()
