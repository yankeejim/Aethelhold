"""Builds the founding map, /Game/Maps/Valley: terrain, river, sky, light and fog.

Run it through the editor (it saves and quits when done):
    Tools\\build_valley.bat
Running it again rebuilds the map from scratch. Never hand-edit the map: change this script.
"""

import unreal

SEED = 1
MAP = "/Game/Maps/Valley"
SIZE_CM = 2016 * 100

mel = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(text):
    unreal.log("[build_valley] " + text)


# --- materials ---------------------------------------------------------------

def new_material(name, folder="/Game/Materials"):
    path = folder + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return assets.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def node(mat, cls, x, y, **props):
    expr = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


def link(a, a_out, b, b_in):
    mel.connect_material_expressions(a, a_out, b, b_in)


def color(mat, rgb, x, y):
    return node(mat, unreal.MaterialExpressionConstant3Vector, x, y,
                constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def const(mat, value, x, y):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=value)


def lerp(mat, a, b, alpha, x, y):
    out = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, "", out, "A")
    link(b, "", out, "B")
    link(alpha, "", out, "Alpha")
    return out


def noise(mat, position, scale, levels, lo, hi, x, y):
    out = node(mat, unreal.MaterialExpressionNoise, x, y, scale=scale, levels=levels,
               output_min=lo, output_max=hi, turbulence=False)
    link(position, "", out, "Position")
    return out


class Graph:
    """Small helper for building material graphs in Python without placing every node by hand."""

    def __init__(self, mat):
        self.mat = mat
        self.n = 0

    def node(self, cls, **props):
        self.n += 1
        return node(self.mat, cls, -2400 + (self.n % 12) * 200, (self.n // 12) * 150 - 1200, **props)

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def color(self, rgb):
        return self.node(unreal.MaterialExpressionConstant3Vector,
                         constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def op(self, cls, a, b):
        out = self.node(cls)
        link(self.const(a) if isinstance(a, float) else a, "", out, "A")
        link(self.const(b) if isinstance(b, float) else b, "", out, "B")
        return out

    def mul(self, a, b):
        return self.op(unreal.MaterialExpressionMultiply, a, b)

    def add(self, a, b):
        return self.op(unreal.MaterialExpressionAdd, a, b)

    def sub(self, a, b):
        return self.op(unreal.MaterialExpressionSubtract, a, b)

    def div(self, a, b):
        return self.op(unreal.MaterialExpressionDivide, a, b)

    def sat(self, a):
        out = self.node(unreal.MaterialExpressionSaturate)
        link(a, "", out, "")
        return out

    def mask(self, a, channels, out_name=""):
        out = self.node(unreal.MaterialExpressionComponentMask, r="r" in channels, g="g" in channels,
                        b="b" in channels, a="a" in channels)
        link(a, out_name, out, "")
        return out

    def lerp(self, a, b, alpha):
        out = self.node(unreal.MaterialExpressionLinearInterpolate)
        link(a, "", out, "A")
        link(b, "", out, "B")
        link(alpha, "", out, "Alpha")
        return out

    def ramp(self, value, lo, hi):
        """0 at lo, 1 at hi, clamped (works in either direction)."""
        return self.sat(self.div(self.sub(value, float(lo)), float(hi - lo)))

    def noise(self, position, scale, levels):
        out = self.node(unreal.MaterialExpressionNoise, scale=scale, levels=levels,
                        output_min=0.0, output_max=1.0, turbulence=False)
        link(position, "", out, "Position")
        return out

    def sample(self, texture, uv, kind):
        out = self.node(unreal.MaterialExpressionTextureSample, texture=texture, sampler_type=kind,
                        sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        link(uv, "", out, "UVs")
        return out


# Megascans surfaces from Fab (docs/art.md): folder name and code.
SURFACES = {
    "meadow": ("Dried_Grass", "xbrgaba"),
    "forest": ("Forest_Floor", "vktfeilaw"),
    "mud": ("Soil_Mud", "seymaixa"),
    "pebbles": ("Small_Pebbles_Ground", "xbliajz"),
    "rock": ("Rock_Cliff", "xccibbi"),
}
GRASS_MESHES = "/Game/Fab/Megascans/Plants/Wild_Grass_vlkhcbxia/High/vlkhcbxia_tier_1/StaticMeshes/"


def surface(g, xy, name, tile_cm):
    """One ground layer: colour, normal, roughness and height, projected from above.
    The colour mixes in a much larger copy of itself so the repeat is hard to see."""
    folder, code = SURFACES[name]
    base = "/Game/Fab/Megascans/Surfaces/%s_%s/High/%s_tier_1/Textures/T_%s_4K_" % (folder, code, code, code)
    tex = {k: unreal.load_asset(base + k) for k in ("B", "N", "ORM", "H")}
    kind = unreal.MaterialSamplerType
    uv = g.div(xy, float(tile_cm))
    near = g.sample(tex["B"], uv, kind.SAMPLERTYPE_COLOR)
    far = g.sample(tex["B"], g.div(xy, float(tile_cm * 4.3)), kind.SAMPLERTYPE_COLOR)
    return {
        "colour": g.lerp(g.mask(near, "rgb", "RGB"), g.mask(far, "rgb", "RGB"), g.const(0.4)),
        "normal": g.mask(g.sample(tex["N"], uv, kind.SAMPLERTYPE_NORMAL), "rgb", "RGB"),
        "rough": g.mask(g.sample(tex["ORM"], uv, kind.SAMPLERTYPE_MASKS), "g", "RGB"),
        "height": g.mask(g.sample(tex["H"], uv, kind.SAMPLERTYPE_ALPHA), "r", "RGB"),
    }


def blend(g, a, b, alpha):
    """Lay b over a where alpha is high. b's own height decides the edge, so stones and
    tufts poke through first instead of a soft smear."""
    edge = g.sat(g.add(g.mul(g.add(g.sub(alpha, 0.5), g.mul(g.sub(b["height"], 0.5), 0.8)), 5.0), 0.5))
    return {k: g.lerp(a[k], b[k], edge) for k in ("colour", "normal", "rough", "height")}


def grass_type():
    """Real grass clumps (Megascans Wild Grass) that the landscape grows on the meadows."""
    # Fab's foliage materials aren't flagged for instanced meshes, which landscape grass is.
    for parent in ("/Game/Fab/Materials/Standard/M_MS_Foliage", "/Game/Fab/Materials/Standard/M_MS_Billboard"):
        m = unreal.load_asset(parent)
        if not m.get_editor_property("used_with_instanced_static_meshes"):
            m.set_editor_property("used_with_instanced_static_meshes", True)
            mel.recompile_material(m)
            unreal.EditorAssetLibrary.save_loaded_asset(m)

    path = "/Game/Materials/LGT_Meadow"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        grass = unreal.load_asset(path)
    else:
        grass = assets.create_asset("LGT_Meadow", "/Game/Materials", unreal.LandscapeGrassType, None)
    varieties = []
    for var in "ABCDEFGH":
        v = unreal.GrassVariety()
        v.set_editor_property("grass_mesh", unreal.load_asset(GRASS_MESHES + "SM_vlkhcbxia_Var" + var))
        density = unreal.PerPlatformFloat()
        density.set_editor_property("default", 35.0)
        v.set_editor_property("grass_density", density)
        start = unreal.PerPlatformInt()
        start.set_editor_property("default", 4500)
        end = unreal.PerPlatformInt()
        end.set_editor_property("default", 7500)
        v.set_editor_property("start_cull_distance", start)
        v.set_editor_property("end_cull_distance", end)
        v.set_editor_property("scale_x", unreal.FloatInterval(1.4, 2.6))
        v.set_editor_property("random_rotation", True)
        v.set_editor_property("align_to_surface", True)
        v.set_editor_property("cast_dynamic_shadow", True)
        varieties.append(v)
    grass.set_editor_property("grass_varieties", varieties)
    unreal.EditorAssetLibrary.save_loaded_asset(grass)
    return grass


def ground_material(grass):
    """The valley floor from Megascans surfaces: meadow, forest floor on the hills, mud along
    the river, pebbles on the river bed and rock on steep slopes. Each one's place comes from
    height, slope and noise, so the map needs no hand painting."""
    mat = new_material("M_ValleyGround")
    g = Graph(mat)
    pos = g.node(unreal.MaterialExpressionWorldPosition)
    xy = g.mask(pos, "rg")
    height = g.mask(pos, "b")
    up = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS), "b")
    big = g.noise(pos, 0.00022, 4)
    mid = g.noise(pos, 0.0016, 3)

    # Meadow: Megascans dried grass, greened where the noise says the grass is lusher.
    lush = g.ramp(g.mul(big, mid), 0.12, 0.4)
    meadow = surface(g, xy, "meadow", 260)
    meadow["colour"] = g.mul(meadow["colour"], g.lerp(g.color((0.55, 0.78, 0.36)), g.color((0.36, 0.66, 0.22)), lush))
    forest = surface(g, xy, "forest", 300)
    mud = surface(g, xy, "mud", 280)
    pebbles = surface(g, xy, "pebbles", 180)
    rock = surface(g, xy, "rock", 700)

    # Forest floor on the higher ground and in noisy patches on the plain; mud along the
    # water's edge; pebbles below the water line; rock steeper than about 35 degrees.
    forest_a = g.sat(g.add(g.ramp(height, 1200, 2600), g.mul(g.sub(big, 0.74), 3.0)))
    shore_a = g.ramp(height, 150, 50)
    bed_a = g.ramp(height, -20, -90)
    rock_a = g.ramp(up, 0.86, 0.74)

    out = blend(g, meadow, forest, forest_a)
    out = blend(g, out, mud, shore_a)
    out = blend(g, out, pebbles, bed_a)
    out = blend(g, out, rock, rock_a)
    mel.connect_material_property(out["colour"], "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(out["normal"], "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(out["rough"], "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Grass grows on open meadow only, thinning toward forest, mud and rock.
    open_ground = g.mul(g.mul(g.sub(1.0, forest_a), g.sub(1.0, shore_a)), g.sub(1.0, rock_a))
    grass_out = g.node(unreal.MaterialExpressionLandscapeGrassOutput)
    meadow_input = unreal.GrassInput()
    meadow_input.set_editor_property("name", "Meadow")
    meadow_input.set_editor_property("grass_type", grass)
    grass_out.set_editor_property("grass_types", [meadow_input])
    link(g.ramp(open_ground, 0.25, 0.75), "", grass_out, "Meadow")

    mel.recompile_material(mat)
    return mat


def water_material():
    """A slow river: Unreal's single-layer water, with two drifting ripple layers."""
    mat = new_material("M_River")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    ripple_tex = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n")

    pos = node(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    xy = node(mat, unreal.MaterialExpressionComponentMask, -1250, 0, r=True, g=True, b=False, a=False)
    link(pos, "", xy, "")

    def ripples(size, sx, sy, y):
        uv = node(mat, unreal.MaterialExpressionDivide, -1100, y)
        link(xy, "", uv, "A")
        link(const(mat, size, -1200, y + 80), "", uv, "B")
        pan = node(mat, unreal.MaterialExpressionPanner, -950, y, speed_x=sx, speed_y=sy)
        link(uv, "", pan, "Coordinate")
        sample = node(mat, unreal.MaterialExpressionTextureSample, -750, y, texture=ripple_tex,
                      sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        link(pan, "", sample, "UVs")
        return sample

    a = ripples(900.0, 0.010, 0.022, -200)
    b = ripples(340.0, -0.017, 0.031, 100)
    both = node(mat, unreal.MaterialExpressionAdd, -500, -50)
    link(a, "RGB", both, "A")
    link(b, "RGB", both, "B")
    flat = node(mat, unreal.MaterialExpressionLinearInterpolate, -350, -50)
    link(color(mat, (0.0, 0.0, 1.0), -500, 80), "", flat, "A")
    link(both, "", flat, "B")
    link(const(mat, 0.35, -500, 160), "", flat, "Alpha")
    normal = node(mat, unreal.MaterialExpressionNormalize, -200, -50)
    link(flat, "", normal, "")
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(const(mat, 0.04, -200, 150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(color(mat, (0.02, 0.03, 0.025), -200, 250), "",
                                  unreal.MaterialProperty.MP_BASE_COLOR)

    out = node(mat, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 200, 300)
    link(color(mat, (0.035, 0.065, 0.05), 0, 250), "", out, "ScatteringCoefficients")
    link(color(mat, (0.45, 0.16, 0.10), 0, 350), "", out, "AbsorptionCoefficients")
    link(const(mat, 0.2, 0, 450), "", out, "PhaseG")
    mel.recompile_material(mat)
    return mat


# --- the level ---------------------------------------------------------------

def spawn(cls, location=(0, 0, 0), rotation=(0, 0, 0), label=None):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(*rotation))
    if label:
        actor.set_actor_label(label)
    return actor


def build_sky():
    # Rotator here is (roll, pitch, yaw). The light shines along its yaw, so the sun stands
    # opposite: low in the west-south-west, ahead of the arriving founders, late afternoon.
    sun = spawn(unreal.DirectionalLight, (0, 0, 10000), (0, -24, 320), "Sun")
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    light.set_editor_property("intensity", 10.0)
    light.set_editor_property("atmosphere_sun_light", True)
    light.set_editor_property("cast_cloud_shadows", True)
    light.set_editor_property("light_source_angle", 0.6)

    spawn(unreal.SkyAtmosphere, label="Sky")

    sky = spawn(unreal.SkyLight, (0, 0, 10000), label="Sky Light")
    sky_light = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_light.set_editor_property("real_time_capture", True)

    spawn(unreal.VolumetricCloud, label="Clouds")

    fog = spawn(unreal.ExponentialHeightFog, (SIZE_CM / 2, SIZE_CM / 2, -500), label="Haze")
    fog_c = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_c.set_editor_property("fog_density", 0.012)
    fog_c.set_editor_property("fog_height_falloff", 0.05)
    fog_c.set_editor_property("enable_volumetric_fog", True)
    fog_c.set_editor_property("volumetric_fog_scattering_distribution", 0.6)

    post = spawn(unreal.PostProcessVolume, label="Look")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.35)
    settings.set_editor_property("override_bloom_intensity", True)
    settings.set_editor_property("bloom_intensity", 0.5)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", -0.3)
    post.set_editor_property("settings", settings)


def build_river(material):
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    water = spawn(unreal.StaticMeshActor, (SIZE_CM / 2, SIZE_CM / 2, 0), label="River")
    mesh = water.static_mesh_component
    mesh.set_static_mesh(plane)
    mesh.set_material(0, material)
    mesh.set_collision_profile_name("NoCollision")
    mesh.set_editor_property("cast_shadow", False)
    water.set_actor_scale3d(unreal.Vector(SIZE_CM / 100, SIZE_CM / 100, 1))


def build():
    log("new level")
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        # Rebuild in place: open the map and clear it out.
        levels.load_level(MAP)
        for actor in actors.get_all_level_actors():
            if not isinstance(actor, unreal.WorldSettings):
                actors.destroy_actor(actor)
    elif not levels.new_level(MAP, False):
        raise RuntimeError("could not create " + MAP)

    log("materials")
    ground = ground_material(grass_type())
    river = water_material()

    log("landscape")
    unreal.ValleyBuilder.build_landscape(ground, SEED)
    build_river(river)
    build_sky()

    start = unreal.ValleyBuilder.start_spot(SEED)
    spawn(unreal.PlayerStart, (start.x, start.y, start.z + 200), (0, 0, 180), "Founders arrive")

    log("saved" if levels.save_current_level() else "SAVE FAILED")


# -ExecutePythonScript closes the editor as soon as this script returns.
build()
unreal.EditorAssetLibrary.save_directory("/Game/Materials")
log("done")
