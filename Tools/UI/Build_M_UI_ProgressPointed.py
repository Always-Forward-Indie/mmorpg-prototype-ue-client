"""Build M_UI_ProgressPointed master material (pointed <=====> bar, SDF, no textures).

Run inside Unreal Editor (Tools -> Execute Python Script) or headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/UI/Build_M_UI_ProgressPointed.py"

Always rebuilds from scratch: existing asset is deleted first (GUI editor),
then a fresh material is created and wired. Safe to re-run. Result: /Game/UI/Materials/M_UI_ProgressPointed.

Graph mirrors FPointedProgressCore 1:1, material adds only SmoothStep AA:
  minEnd -> edgeRamp -> maxHalf, dy -> shapeDist -> shapeMask -> Opacity
  U-Progress / U-Ghost -> fill/ghost masks, border inner hull, 3x Lerp -> Final Color

API notes (UE 5.8, verified by probe): connect_material_expressions takes the
SOURCE output as "" (default pin) and the DEST input by name ("A"/"B"/"Input"/
"Min"/"Max"/"Value"/"Alpha"). Named source pins ("UV", "R", ...) return False.
"""
import unreal

PKG = "/Game/UI/Materials"
NAME = "M_UI_ProgressPointed"
PATH = "{}/{}".format(PKG, NAME)

PARAMS_SCALAR = {
    "Progress": 0.5, "Ghost": 0.5, "Tip": 0.12, "Border": 0.03, "AA": 0.005,
}
PARAMS_VECTOR = {
    "FillColor": (0.7, 0.1, 0.1, 1.0),
    "GhostColor": (1.0, 0.9, 0.6, 1.0),
    "BgColor": (0.08, 0.08, 0.1, 1.0),
    "BorderColor": (0.0, 0.0, 0.0, 1.0),
}

LIB = unreal.MaterialEditingLibrary
ALIB = unreal.EditorAssetLibrary


def step(msg):
    unreal.log("[PointedBar] {}".format(msg))


def fresh_material():
    if ALIB.does_asset_exist(PATH):
        step("deleting existing {}".format(PATH))
        if not ALIB.delete_asset(PATH):
            raise RuntimeError("delete_asset failed for {}".format(PATH))
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("create_asset returned None for {}".format(PATH))
    step("created {}".format(PATH))
    return mat


def make_expr(mat, cls, x, y):
    node = LIB.create_material_expression(mat, cls, x, y)
    if node is None:
        raise RuntimeError("create_material_expression failed for {}".format(cls))
    return node


def set_scalar(node, name, value):
    node.set_editor_property("ParameterName", name)
    node.set_editor_property("DefaultValue", value)


def set_vector(node, name, rgba):
    node.set_editor_property("ParameterName", name)
    node.set_editor_property("DefaultValue", unreal.LinearColor(*rgba))


def conn(a, b, in_b, what):
    # Source pin is always "" (default output); dest pin by name.
    if not LIB.connect_material_expressions(a, "", b, in_b):
        raise RuntimeError("connect failed: {}".format(what))


def build():
    mat = fresh_material()

    # Master settings: UI domain, unlit, translucent.
    mat.set_editor_property("MaterialDomain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("BlendMode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("ShadingModel", unreal.MaterialShadingModel.MSM_UNLIT)
    step("domain/blend/shading set")

    tex = make_expr(mat, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    mask_r = make_expr(mat, unreal.MaterialExpressionComponentMask, -440, -80)
    mask_r.set_editor_property("R", True)
    mask_g = make_expr(mat, unreal.MaterialExpressionComponentMask, -440, 80)
    mask_g.set_editor_property("G", True)
    conn(tex, mask_r, "", "tex->mask_r")
    conn(tex, mask_g, "", "tex->mask_g")
    step("uv split ok")

    ps, px = {}, -600
    for key, val in PARAMS_SCALAR.items():
        n = make_expr(mat, unreal.MaterialExpressionScalarParameter, px, 320)
        set_scalar(n, key, val)
        ps[key] = n
        px += 160
    pv = {}
    for key, val in PARAMS_VECTOR.items():
        n = make_expr(mat, unreal.MaterialExpressionVectorParameter, px, 420)
        set_vector(n, key, val)
        pv[key] = n
        px += 160
    step("params ok")

    const_half = make_expr(mat, unreal.MaterialExpressionConstant, -280, 200)
    const_half.set_editor_property("R", 0.5)
    const_one = make_expr(mat, unreal.MaterialExpressionConstant, -280, 260)
    const_one.set_editor_property("R", 1.0)

    # minEnd = min(U, 1-U)
    one_minus_u = make_expr(mat, unreal.MaterialExpressionSubtract, -280, -80)
    conn(const_one, one_minus_u, "A", "1->1-U.A")
    conn(mask_r, one_minus_u, "B", "U->1-U.B")
    min_end = make_expr(mat, unreal.MaterialExpressionMin, -120, -80)
    conn(mask_r, min_end, "A", "U->min.A")
    conn(one_minus_u, min_end, "B", "1-U->min.B")
    step("minEnd ok")

    # edgeRamp = saturate(minEnd / Tip); maxHalf = edgeRamp * 0.5
    div_tip = make_expr(mat, unreal.MaterialExpressionDivide, 40, -80)
    conn(min_end, div_tip, "A", "min->div.A")
    conn(ps["Tip"], div_tip, "B", "Tip->div.B")
    ramp = make_expr(mat, unreal.MaterialExpressionSaturate, 200, -80)
    conn(div_tip, ramp, "", "div->sat")
    max_half = make_expr(mat, unreal.MaterialExpressionMultiply, 360, -80)
    conn(ramp, max_half, "A", "ramp->half.A")
    conn(const_half, max_half, "B", "0.5->half.B")
    step("edgeRamp ok")

    # dy = |V - 0.5|; shapeDist = dy - maxHalf
    v_sub = make_expr(mat, unreal.MaterialExpressionSubtract, -120, 80)
    conn(mask_g, v_sub, "A", "V->vsub.A")
    conn(const_half, v_sub, "B", "0.5->vsub.B")
    dy = make_expr(mat, unreal.MaterialExpressionAbs, 40, 80)
    conn(v_sub, dy, "", "vsub->abs")
    shape_dist = make_expr(mat, unreal.MaterialExpressionSubtract, 520, 0)
    conn(dy, shape_dist, "A", "dy->shape.A")
    conn(max_half, shape_dist, "B", "half->shape.B")
    step("shapeDist ok")

    # shapeMask = 1 - smoothstep(-AA, +AA, shapeDist)
    m1 = make_expr(mat, unreal.MaterialExpressionConstant, 360, 160)
    m1.set_editor_property("R", -1.0)
    neg_aa = make_expr(mat, unreal.MaterialExpressionMultiply, 520, 160)
    conn(m1, neg_aa, "A", "-1->negAA.A")
    conn(ps["AA"], neg_aa, "B", "AA->negAA.B")
    smooth = make_expr(mat, unreal.MaterialExpressionSmoothStep, 680, 0)
    conn(neg_aa, smooth, "Min", "negAA->smooth.Min")
    conn(ps["AA"], smooth, "Max", "AA->smooth.Max")
    conn(shape_dist, smooth, "Value", "dist->smooth.Value")
    shape_mask = make_expr(mat, unreal.MaterialExpressionOneMinus, 840, 0)
    conn(smooth, shape_mask, "", "smooth->shapeMask")
    step("shapeMask ok")

    # fill/ghost masks = 1 - saturate((U - P) / AA)
    def wipe(param_name, y, tag):
        sub = make_expr(mat, unreal.MaterialExpressionSubtract, 520, y)
        conn(mask_r, sub, "A", "{} U->sub.A".format(tag))
        conn(ps[param_name], sub, "B", "{} P->sub.B".format(tag))
        div = make_expr(mat, unreal.MaterialExpressionDivide, 680, y)
        conn(sub, div, "A", "{} sub->div.A".format(tag))
        conn(ps["AA"], div, "B", "{} AA->div.B".format(tag))
        sat = make_expr(mat, unreal.MaterialExpressionSaturate, 840, y)
        conn(div, sat, "", "{} div->sat".format(tag))
        out = make_expr(mat, unreal.MaterialExpressionOneMinus, 1000, y)
        conn(sat, out, "", "{} sat->mask".format(tag))
        return out

    fill_mask = wipe("Progress", -160, "fill")
    ghost_mask = wipe("Ghost", -280, "ghost")
    step("fill/ghost ok")

    # Border inner hull: innerRamp = saturate((minEnd-Border)/(Tip-Border))
    tip_sub = make_expr(mat, unreal.MaterialExpressionSubtract, 40, 240)
    conn(ps["Tip"], tip_sub, "A", "Tip->tipsub.A")
    conn(ps["Border"], tip_sub, "B", "Border->tipsub.B")
    min_sub = make_expr(mat, unreal.MaterialExpressionSubtract, 200, 240)
    conn(min_end, min_sub, "A", "min->minsub.A")
    conn(ps["Border"], min_sub, "B", "Border->minsub.B")
    inner_div = make_expr(mat, unreal.MaterialExpressionDivide, 360, 240)
    conn(min_sub, inner_div, "A", "minsub->innerdiv.A")
    conn(tip_sub, inner_div, "B", "tipsub->innerdiv.B")
    inner_ramp = make_expr(mat, unreal.MaterialExpressionSaturate, 520, 240)
    conn(inner_div, inner_ramp, "", "innerdiv->innerramp")
    inner_half_a = make_expr(mat, unreal.MaterialExpressionMultiply, 680, 240)
    conn(inner_ramp, inner_half_a, "A", "ramp->ih.A")
    conn(const_half, inner_half_a, "B", "0.5->ih.B")
    border_half = make_expr(mat, unreal.MaterialExpressionMultiply, 680, 320)
    conn(ps["Border"], border_half, "A", "Border->bh.A")
    conn(const_half, border_half, "B", "0.5->bh.B")
    inner_half = make_expr(mat, unreal.MaterialExpressionSubtract, 840, 280)
    conn(inner_half_a, inner_half, "A", "ih->inner.A")
    conn(border_half, inner_half, "B", "bh->inner.B")
    inner_dist = make_expr(mat, unreal.MaterialExpressionSubtract, 1000, 280)
    conn(dy, inner_dist, "A", "dy->idist.A")
    conn(inner_half, inner_dist, "B", "inner->idist.B")
    inner_smooth = make_expr(mat, unreal.MaterialExpressionSmoothStep, 1160, 280)
    conn(neg_aa, inner_smooth, "Min", "negAA->ismooth.Min")
    conn(ps["AA"], inner_smooth, "Max", "AA->ismooth.Max")
    conn(inner_dist, inner_smooth, "Value", "idist->ismooth.Value")
    inner_one = make_expr(mat, unreal.MaterialExpressionOneMinus, 1320, 280)
    conn(inner_smooth, inner_one, "", "ismooth->ione")
    inner_mask = make_expr(mat, unreal.MaterialExpressionMultiply, 1480, 280)
    conn(inner_one, inner_mask, "A", "ione->imask.A")
    conn(shape_mask, inner_mask, "B", "shape->imask.B")
    step("inner hull ok")

    # Color chain: Bg -> Ghost -> Fill -> Border.
    def masked(mask_a, mask_b, y, tag):
        m = make_expr(mat, unreal.MaterialExpressionMultiply, 1160, y)
        conn(mask_a, m, "A", "{} m.A".format(tag))
        conn(mask_b, m, "B", "{} m.B".format(tag))
        return m

    w_ghost = masked(ghost_mask, inner_mask, -160, "wghost")
    w_fill = masked(fill_mask, inner_mask, -80, "wfill")
    ring_one = make_expr(mat, unreal.MaterialExpressionOneMinus, 1480, 160)
    conn(inner_mask, ring_one, "", "inner->ringone")
    w_ring = masked(shape_mask, ring_one, 160, "wring")

    lerp1 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1480, -120)
    conn(pv["BgColor"], lerp1, "A", "bg->lerp1.A")
    conn(pv["GhostColor"], lerp1, "B", "ghost->lerp1.B")
    conn(w_ghost, lerp1, "Alpha", "wghost->lerp1.Alpha")
    lerp2 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1640, -40)
    conn(lerp1, lerp2, "A", "lerp1->lerp2.A")
    conn(pv["FillColor"], lerp2, "B", "fill->lerp2.B")
    conn(w_fill, lerp2, "Alpha", "wfill->lerp2.Alpha")
    lerp3 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 1800, 40)
    conn(lerp2, lerp3, "A", "lerp2->lerp3.A")
    conn(pv["BorderColor"], lerp3, "B", "border->lerp3.B")
    conn(w_ring, lerp3, "Alpha", "wring->lerp3.Alpha")

    if not LIB.connect_material_property(lerp3, "", unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("connect MP_BASE_COLOR failed")
    if not LIB.connect_material_property(shape_mask, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("connect MP_OPACITY failed")
    step("outputs ok")

    if not ALIB.save_asset(PATH):
        raise RuntimeError("save_asset failed for {}".format(PATH))
    step("saved {}".format(PATH))
    unreal.log("[PointedBar] BUILT {}".format(NAME))


try:
    build()
except Exception as e:
    unreal.log_error("[PointedBar] BUILD FAILED: {}".format(e))
    raise
