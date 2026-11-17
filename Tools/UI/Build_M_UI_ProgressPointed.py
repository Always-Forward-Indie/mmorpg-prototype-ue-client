"""Build M_UI_ProgressPointed master material (pointed <=====> bar, SDF, no textures).

Run inside Unreal Editor: Tools -> Execute Python Script -> pick this file.
Creates /Game/UI/Materials/M_UI_ProgressPointed (or rebuilds its graph in place).

Graph mirrors FPointedProgressCore 1:1, material adds only SmoothStep AA:
  minEnd -> edgeRamp -> maxHalf, dy -> shapeDist -> shapeMask -> Opacity
  U-Progress / U-Ghost -> fill/ghost masks, border inner hull, 3x Lerp -> Final Color
"""
import unreal

PKG = "/Game/UI/Materials"
NAME = "M_UI_ProgressPointed"

PARAMS_SCALAR = {
    "Progress": 0.5, "Ghost": 0.5, "Tip": 0.12, "Border": 0.03, "AA": 0.005,
}
PARAMS_VECTOR = {
    "FillColor": (0.7, 0.1, 0.1, 1.0),
    "GhostColor": (1.0, 0.9, 0.6, 1.0),
    "BgColor": (0.08, 0.08, 0.1, 1.0),
    "BorderColor": (0.0, 0.0, 0.0, 1.0),
}


def find_or_create_material():
    path = "{}/{}".format(PKG, NAME)
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if mat:
        return mat
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    mat = tools.create_asset(NAME, PKG, unreal.Material, factory)
    return mat


def make_expr(mat, cls, x, y):
    lib = unreal.MaterialEditingLibrary
    return lib.create_material_expression(mat, cls, x, y)


def set_scalar(node, name, value):
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)


def set_vector(node, name, rgba):
    node.set_editor_property("parameter_name", name)
    c = unreal.LinearColor(*rgba)
    node.set_editor_property("default_value", c)


def conn(a, out_a, b, in_b):
    unreal.MaterialEditingLibrary.connect_material_expressions(a, out_a, b, in_b)


def build():
    mat = find_or_create_material()
    lib = unreal.MaterialEditingLibrary

    # Master settings: UI domain, unlit, translucent.
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    # Wipe old graph on rebuild (keeps the same asset, safe to re-run).
    for expr in list(mat.get_editor_property("expressions") or []):
        try:
            lib.delete_material_expression(mat, expr)
        except Exception:
            pass

    tex = make_expr(mat, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    mask_r = make_expr(mat, unreal.MaterialExpressionComponentMask, -440, -80)
    mask_r.set_editor_property("r", True)
    mask_g = make_expr(mat, unreal.MaterialExpressionComponentMask, -440, 80)
    mask_g.set_editor_property("g", True)
    conn(tex, "", mask_r, "Input")
    conn(tex, "", mask_g, "Input")

    # Params.
    ps, px = {}, -600
    for key, val in PARAMS_SCALAR.items():
        n = make_expr(mat, unreal.MaterialExpressionScalarParameter, px, 320)
        set_scalar(n, key, val)
        ps[key] = n
        px += 40
    pv = {}
    for key, val in PARAMS_VECTOR.items():
        n = make_expr(mat, unreal.MaterialExpressionVectorParameter, px, 420)
        set_vector(n, key, val)
        pv[key] = n
        px += 40

    const_half = make_expr(mat, unreal.MaterialExpressionConstant, -280, 200)
    const_half.set_editor_property("r", 0.5)
    const_one = make_expr(mat, unreal.MaterialExpressionConstant, -280, 260)
    const_one.set_editor_property("r", 1.0)

    # minEnd = min(U, 1-U)
    one_minus_u = make_expr(mat, unreal.MaterialExpressionSubtract, -280, -80)
    conn(const_one, "", one_minus_u, "A")
    conn(mask_r, "R", one_minus_u, "B")
    min_end = make_expr(mat, unreal.MaterialExpressionMin, -120, -80)
    conn(mask_r, "R", min_end, "A")
    conn(one_minus_u, "", min_end, "B")

    # edgeRamp = saturate(minEnd / Tip); maxHalf = edgeRamp * 0.5
    div_tip = make_expr(mat, unreal.MaterialExpressionDivide, 40, -80)
    conn(min_end, "", div_tip, "A")
    conn(ps["Tip"], "", div_tip, "B")
    ramp = make_expr(mat, unreal.MaterialExpressionSaturate, 200, -80)
    conn(div_tip, "", ramp, "")
    max_half = make_expr(mat, unreal.MaterialExpressionMultiply, 360, -80)
    conn(ramp, "", max_half, "A")
    conn(const_half, "", max_half, "B")

    # dy = |V - 0.5|; shapeDist = dy - maxHalf
    v_sub = make_expr(mat, unreal.MaterialExpressionSubtract, -120, 80)
    conn(mask_g, "G", v_sub, "A")
    conn(const_half, "", v_sub, "B")
    dy = make_expr(mat, unreal.MaterialExpressionAbsolute, 40, 80)
    conn(v_sub, "", dy, "")
    shape_dist = make_expr(mat, unreal.MaterialExpressionSubtract, 520, 0)
    conn(dy, "", shape_dist, "A")
    conn(max_half, "", shape_dist, "B")

    # shapeMask = 1 - smoothstep(-AA, +AA, shapeDist)
    neg_aa = make_expr(mat, unreal.MaterialExpressionMultiply, 520, 160)
    m1 = make_expr(mat, unreal.MaterialExpressionConstant, 360, 160)
    m1.set_editor_property("r", -1.0)
    conn(m1, "", neg_aa, "A")
    conn(ps["AA"], "", neg_aa, "B")
    smooth = make_expr(mat, unreal.MaterialExpressionSmoothStep, 680, 0)
    conn(neg_aa, "", smooth, "Min")
    conn(ps["AA"], "", smooth, "Max")
    conn(shape_dist, "", smooth, "Value")
    shape_mask = make_expr(mat, unreal.MaterialExpressionOneMinus, 840, 0)
    conn(smooth, "", shape_mask, "")

    # fill/ghost masks = 1 - saturate((U - P) / AA)
    def wipe(param_name, y):
        sub = make_expr(mat, unreal.MaterialExpressionSubtract, 520, y)
        conn(mask_r, "R", sub, "A")
        conn(ps[param_name], "", sub, "B")
        div = make_expr(mat, unreal.MaterialExpressionDivide, 680, y)
        conn(sub, "", div, "A")
        conn(ps["AA"], "", div, "B")
        sat = make_expr(mat, unreal.MaterialExpressionSaturate, 840, y)
        conn(div, "", sat, "")
        out = make_expr(mat, unreal.MaterialExpressionOneMinus, 1000, y)
        conn(sat, "", out, "")
        return out

    fill_mask = wipe("Progress", -160)
    ghost_mask = wipe("Ghost", -280)

    # Border inner hull: innerRamp = saturate((minEnd-Border)/(Tip-Border))
    tip_sub = make_expr(mat, unreal.MaterialExpressionSubtract, 40, 240)
    conn(ps["Tip"], "", tip_sub, "A")
    conn(ps["Border"], "", tip_sub, "B")
    min_sub = make_expr(mat, unreal.MaterialExpressionSubtract, 200, 240)
    conn(min_end, "", min_sub, "A")
    conn(ps["Border"], "", min_sub, "B")
    inner_div = make_expr(mat, unreal.MaterialExpressionDivide, 360, 240)
    conn(min_sub, "", inner_div, "A")
    conn(tip_sub, "", inner_div, "B")
    inner_ramp = make_expr(mat, unreal.MaterialExpressionSaturate, 520, 240)
    conn(inner_div, "", inner_ramp, "")
    inner_half_a = make_expr(mat, unreal.MaterialExpressionMultiply, 680, 240)
    conn(inner_ramp, "", inner_half_a, "A")
    conn(const_half, "", inner_half_a, "B")
    border_half = make_expr(mat, unreal.MaterialExpressionMultiply, 680, 320)
    conn(ps["Border"], "", border_half, "A")
    conn(const_half, "", border_half, "B")
    inner_half = make_expr(mat, unreal.MaterialExpressionSubtract, 840, 280)
    conn(inner_half_a, "", inner_half, "A")
    conn(border_half, "", inner_half, "B")
    inner_dist = make_expr(mat, unreal.MaterialExpressionSubtract, 1000, 280)
    conn(dy, "", inner_dist, "A")
    conn(inner_half, "", inner_dist, "B")
    inner_smooth = make_expr(mat, unreal.MaterialExpressionSmoothStep, 1160, 280)
    conn(neg_aa, "", inner_smooth, "Min")
    conn(ps["AA"], "", inner_smooth, "Max")
    conn(inner_dist, "", inner_smooth, "Value")
    inner_one = make_expr(mat, unreal.MaterialExpressionOneMinus, 1320, 280)
    conn(inner_smooth, "", inner_one, "")
    inner_mask = make_expr(mat, unreal.MaterialExpressionMultiply, 1480, 280)
    conn(inner_one, "", inner_mask, "A")
    conn(shape_mask, "", inner_mask, "B")

    # Color chain: Bg -> Ghost -> Fill -> Border; weights masked by inner/outer.
    def masked(mask_a, mask_b, y):
        m = make_expr(mat, unreal.MaterialExpressionMultiply, 1160, y)
        conn(mask_a, "", m, "A")
        conn(mask_b, "", m, "B")
        return m

    w_ghost = masked(ghost_mask, inner_mask, -160)
    w_fill = masked(fill_mask, inner_mask, -80)
    ring_one = make_expr(mat, unreal.MaterialExpressionOneMinus, 1480, 160)
    conn(inner_mask, "", ring_one, "")
    w_ring = masked(shape_mask, ring_one, 160)

    lerp1 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1480, -120)
    conn(pv["BgColor"], "", lerp1, "A")
    conn(pv["GhostColor"], "", lerp1, "B")
    conn(w_ghost, "", lerp1, "Alpha")
    lerp2 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1640, -40)
    conn(lerp1, "", lerp2, "A")
    conn(pv["FillColor"], "", lerp2, "B")
    conn(w_fill, "", lerp2, "Alpha")
    lerp3 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1800, 40)
    conn(lerp2, "", lerp3, "A")
    conn(pv["BorderColor"], "", lerp3, "B")
    conn(w_ring, "", lerp3, "Alpha")

    lib.connect_material_property(lerp3, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(shape_mask, "", unreal.MaterialProperty.MP_OPACITY)

    unreal.EditorAssetLibrary.save_asset("{}/{}.{}".format(PKG, NAME, NAME))
    print("Built {}".format(NAME))


if __name__ == "__main__" if "__name__" in dir() else True:
    try:
        build()
    except Exception as e:
        print("Build failed: {}".format(e))
        raise
