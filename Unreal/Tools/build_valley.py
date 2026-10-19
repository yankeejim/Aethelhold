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


def plant_mesh(name, code, var):
    """A Megascans plant mesh by pack name, code and variant letter."""
    return "/Game/Fab/Megascans/Plants/%s_%s/High/%s_tier_1/StaticMeshes/SM_%s_Var%s" % (name, code, code, code, var)


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


def grass_variety(mesh_path, density, cull_start_cm, cull_end_cm, scale, shadows):
    """One kind of plant the landscape scatters: how dense (per 10 m2), how far it shows."""
    v = unreal.GrassVariety()
    v.set_editor_property("grass_mesh", unreal.load_asset(mesh_path))
    d = unreal.PerPlatformFloat()
    d.set_editor_property("default", density)
    v.set_editor_property("grass_density", d)
    start = unreal.PerPlatformInt()
    start.set_editor_property("default", cull_start_cm)
    end = unreal.PerPlatformInt()
    end.set_editor_property("default", cull_end_cm)
    v.set_editor_property("start_cull_distance", start)
    v.set_editor_property("end_cull_distance", end)
    v.set_editor_property("scale_x", unreal.FloatInterval(*scale))
    v.set_editor_property("random_rotation", True)
    v.set_editor_property("align_to_surface", True)
    v.set_editor_property("cast_dynamic_shadow", shadows)
    return v


def reset_meadow_plants(codes):
    """Put Fab's meadow plant materials back as they came. Fab's distance-fade and tint
    switches both turn the grass near-black, so the grass's far edge is hidden by the
    ground colour instead (see ground_material)."""
    for name, code in codes:
        path = "/Game/Fab/Megascans/Plants/%s_%s/High/%s_tier_1/Materials/MI_%s" % (name, code, code, code)
        mi = unreal.load_asset(path)
        if not mi:
            continue
        mel.set_material_instance_static_switch_parameter_value(mi, "Use Distance Based Opacity", False)
        mel.set_material_instance_static_switch_parameter_value(mi, "Tint Base Color", False)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)


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
    # A real meadow is a mix: several grasses, clover-like sorrel low down, poppies here and
    # there. Densities are plants per 10 m2.
    near = [("Wild_Grass", "vlkhcbxia", "ABCDEFGH", 22.0, (1.3, 2.4)),
            ("Wild_Grass", "vczndjqja", "ABCDEFGH", 22.0, (1.0, 1.8)),
            ("Wheat_Grass", "tdcrdbur", "ABCDEF", 12.0, (0.9, 1.4)),
            ("Ribbon_Grass", "tbdpec3r", "ABCDEF", 6.0, (0.8, 1.3)),
            ("Wood_Sorrel", "wcwmchzja", "ABCDEFGH", 10.0, (1.0, 1.6)),
            ("Field_Poppy", "vmcobd0ja", "ABCDEFGH", 0.8, (0.9, 1.2))]
    reset_meadow_plants([(n, c) for n, c, _l, _d, _s in near])
    for name, code, letters, density, scale in near:
        for var in letters:
            varieties.append(grass_variety(plant_mesh(name, code, var), density / len(letters) * 8,
                                           6000, 9000, scale, True))
    grass.set_editor_property("grass_varieties", varieties)
    unreal.EditorAssetLibrary.save_loaded_asset(grass)
    return grass


def river_material(flow_map):
    """The river: Unreal's single-layer water (deep water darkens, shallows show the bed) with
    ripples that drift downstream along the baked flow map, quickest mid-channel."""
    mat = new_material("M_River")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    g = Graph(mat)
    ripples = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n")
    kind = unreal.MaterialSamplerType

    xy = g.mask(g.node(unreal.MaterialExpressionWorldPosition), "rg")
    flow = g.sample(flow_map, g.div(xy, float(SIZE_CM)), kind.SAMPLERTYPE_LINEAR_COLOR)
    direction = g.sub(g.mul(g.mask(flow, "rg", "RGB"), 2.0), 1.0)
    velocity = g.mul(direction, g.mask(flow, "b", "RGB"))

    # Two copies of the ripples slide along the flow half a cycle apart; each fades out as it
    # resets, so the water keeps moving without ever visibly jumping back.
    time = g.mul(g.node(unreal.MaterialExpressionTime), 0.08)
    phase_a = g.node(unreal.MaterialExpressionFrac)
    link(time, "", phase_a, "")
    phase_b = g.node(unreal.MaterialExpressionFrac)
    link(g.add(time, 0.5), "", phase_b, "")
    fade = g.node(unreal.MaterialExpressionAbs)
    link(g.sub(g.mul(phase_a, 2.0), 1.0), "", fade, "")

    def flowing(size_cm, reach):
        uv = g.div(xy, float(size_cm))
        a_uv = g.sub(uv, g.mul(velocity, g.mul(phase_a, reach)))
        b_uv = g.add(g.sub(uv, g.mul(velocity, g.mul(phase_b, reach))), 0.37)
        a_n = g.mask(g.sample(ripples, a_uv, kind.SAMPLERTYPE_NORMAL), "rgb", "RGB")
        b_n = g.mask(g.sample(ripples, b_uv, kind.SAMPLERTYPE_NORMAL), "rgb", "RGB")
        return g.lerp(a_n, b_n, fade)

    both = g.add(flowing(900, 1.2), flowing(300, 2.6))
    calm = g.lerp(g.color((0.0, 0.0, 1.0)), both, g.const(0.4))
    normal = g.node(unreal.MaterialExpressionNormalize)
    link(calm, "", normal, "")
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(g.const(0.04), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.color((0.02, 0.03, 0.025)), "", unreal.MaterialProperty.MP_BASE_COLOR)

    out = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
    link(g.color((0.04, 0.06, 0.04)), "", out, "ScatteringCoefficients")
    link(g.color((0.3, 0.1, 0.12)), "", out, "AbsorptionCoefficients")
    link(g.const(0.2), "", out, "PhaseG")
    mel.recompile_material(mat)
    return mat


def build_river(material):
    """One flat sheet of water at the river's level across the whole map. The banks hide it
    everywhere but the channel, so the shoreline is wherever the ground meets the water."""
    water = spawn(unreal.StaticMeshActor, (SIZE_CM / 2, SIZE_CM / 2, 0), label="River")
    mesh = water.static_mesh_component
    mesh.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    mesh.set_material(0, material)
    mesh.set_collision_profile_name("NoCollision")
    mesh.set_editor_property("cast_shadow", False)
    water.set_actor_scale3d(unreal.Vector(SIZE_CM / 100, SIZE_CM / 100, 1))


def ground_material(grass, forest_mask):
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
    meadow["colour"] = g.mul(meadow["colour"], g.lerp(g.color((0.5, 0.75, 0.33)), g.color((0.4, 0.68, 0.26)), lush))
    # Seen from afar a meadow is a soft sward, not a photo of dry stalks repeated every few
    # meters: past the grass clumps, fade the texture into the colour of the grass itself,
    # varied only in broad patches.
    eye = g.node(unreal.MaterialExpressionCameraPositionWS)
    to_eye = g.sub(pos, eye)
    dist = g.node(unreal.MaterialExpressionLength)
    link(to_eye, "", dist, "")
    far = g.ramp(dist, 5000, 8500)
    patches = g.noise(pos, 0.00012, 3)
    tufts = g.noise(pos, 0.0007, 2)
    sward = g.lerp(g.color((0.12, 0.13, 0.045)), g.color((0.2, 0.19, 0.075)),
                   g.lerp(g.lerp(patches, lush, g.const(0.4)), tufts, g.const(0.35)))
    # Keep a little of the photo's light and dark so it isn't a flat colour.
    grain = g.mul(g.mask(g.sample(unreal.load_asset(
        "/Game/Fab/Megascans/Surfaces/Dried_Grass_xbrgaba/High/xbrgaba_tier_1/Textures/T_xbrgaba_4K_B"),
        g.div(xy, 900.0), unreal.MaterialSamplerType.SAMPLERTYPE_COLOR), "g", "RGB"), 2.2)
    sward = g.mul(sward, g.lerp(g.const(1.0), grain, g.const(0.35)))
    meadow["colour"] = g.lerp(meadow["colour"], sward, far)
    meadow["normal"] = g.lerp(meadow["normal"], g.color((0.0, 0.0, 1.0)), g.mul(far, 0.8))
    forest = surface(g, xy, "forest", 300)
    # Darker, browner leaf litter in the shade of the woods.
    forest["colour"] = g.mul(forest["colour"], g.color((0.62, 0.62, 0.5)))
    mud = surface(g, xy, "mud", 280)
    pebbles = surface(g, xy, "pebbles", 180)
    rock = surface(g, xy, "rock", 700)

    # Forest floor on the higher ground and in noisy patches on the plain; mud along the
    # water's edge; pebbles below the water line; rock steeper than about 35 degrees.
    # Forest floor where the woods stand (the same map the trees are planted from).
    mask_uv = g.div(xy, float(SIZE_CM))
    woods = g.mask(g.sample(forest_mask, mask_uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE), "r", "RGB")
    forest_a = g.ramp(woods, 0.12, 0.4)
    shore_a = g.ramp(height, 90, 30)
    # Gravel from just above the waterline down: the beaches on the inside of bends.
    bed_a = g.ramp(height, 25, -15)
    rock_a = g.ramp(up, 0.86, 0.74)

    out = blend(g, meadow, forest, forest_a)
    out = blend(g, out, mud, shore_a)
    out = blend(g, out, pebbles, bed_a)
    out = blend(g, out, rock, rock_a)
    # The founders' camp: ground trampled bare around the fire, grass returning toward the edge.
    # The fire stands 15 m back from the landing spot (AColony::FoundTheColony).
    start = unreal.ValleyBuilder.start_spot(SEED)
    fire = g.color((start.x + 1500.0, start.y, 0.0))
    from_fire = g.node(unreal.MaterialExpressionDistance)
    link(xy, "", from_fire, "A")
    link(g.mask(fire, "rg"), "", from_fire, "B")
    camp_a = g.sat(g.add(g.ramp(from_fire, 2200, 900), g.mul(g.sub(mid, 0.5), 0.6)))
    out = blend(g, out, mud, g.mul(camp_a, 0.85))
    # Ground the river keeps wet: darker and shinier, fading out a little above the water.
    wet = g.ramp(height, 30, 0)
    colour = g.mul(out["colour"], g.lerp(g.const(1.0), g.const(0.7), wet))
    rough = g.lerp(out["rough"], g.const(0.3), wet)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(out["normal"], "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Grass grows on open meadow only, thinning toward forest, mud and rock.
    open_ground = g.mul(g.mul(g.mul(g.sub(1.0, forest_a), g.sub(1.0, shore_a)), g.sub(1.0, rock_a)), g.sub(1.0, bed_a))
    open_ground = g.mul(open_ground, g.sub(1.0, camp_a))
    grass_out = g.node(unreal.MaterialExpressionLandscapeGrassOutput)
    meadow_input = unreal.GrassInput()
    meadow_input.set_editor_property("name", "Meadow")
    meadow_input.set_editor_property("grass_type", grass)
    grass_out.set_editor_property("grass_types", [meadow_input])
    link(g.ramp(open_ground, 0.25, 0.75), "", grass_out, "Meadow")

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
    # Don't let the eye adjust all the way to darkness: nights should look like night.
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", -1.0)
    post.set_editor_property("settings", settings)


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
    # Left over from trying the Water plugin's river.
    if unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/MI_River"):
        unreal.EditorAssetLibrary.delete_asset("/Game/Materials/MI_River")

    log("materials")
    ground = ground_material(grass_type(), unreal.ValleyBuilder.build_forest_mask(SEED))

    log("landscape")
    unreal.ValleyBuilder.build_landscape(ground, SEED)
    build_river(river_material(unreal.ValleyBuilder.build_flow_map(SEED)))
    # Trees are planted by the game itself when it starts (AForest), so founders can fell them.
    unreal.ValleyBuilder.build_rocks(SEED)
    build_sky()

    start = unreal.ValleyBuilder.start_spot(SEED)
    spawn(unreal.PlayerStart, (start.x, start.y, start.z + 200), (0, 0, 180), "Founders arrive")

    # Megascans plants sway only when this actor is in the level (it drives their wind).
    wind = unreal.load_asset("/Game/Fab/Megascans/Plants/BP_GlobalFoliageActor")
    if wind:
        actors.spawn_actor_from_object(wind, unreal.Vector(0, 0, 0)).set_actor_label("Wind")

    for y in range(100, 2000, 150):
        log("river at y=%d m: x=%.0f m" % (y, unreal.ValleyBuilder.river_center_x(y * 100.0, SEED) / 100))
    log("saved" if levels.save_current_level() else "SAVE FAILED")


# -ExecutePythonScript closes the editor as soon as this script returns.
build()
unreal.EditorAssetLibrary.save_directory("/Game/Materials")
log("done")
