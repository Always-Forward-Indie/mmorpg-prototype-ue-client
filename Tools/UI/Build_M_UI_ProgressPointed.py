"""Build M_UI_ProgressPointed master material (pointed <=====> bar, SDF, no textures).

Run inside Unreal Editor (Tools -> Execute Python Script) or headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/UI/Build_M_UI_ProgressPointed.py"

Always rebuilds from scratch: existing asset is deleted first (GUI editor),
then a fresh material is created and wired. Safe to re-run. Result: /Game/UI/Materials/M_UI_ProgressPointed.

Shape (mirrors FPointedProgressCore 1:1, material adds only SmoothStep AA):
  Bar space is height-normalized: Y in 0..1, X scaled by Aspect = W/H so one
  unit is square. Tips use h = 0.5*t*(1.4-0.4t): a defined (crisp but
  kink-free) corner at the tip junction plus a smooth nose, never needle-sharp.
  Border ring is a uniform px inset: same hull with (edgeH - b) and (h - b).

API notes (UE 5.8, verified by probe): connect_material_expressions takes the
SOURCE output as "" (default pin) and the DEST input by name ("A"/"B"/"Input"/
"Min"/"Max"/"Value"/"Alpha"). Named source pins ("UV", "R", ...) return False.
Single-input pins (Mask/Saturate/Abs/OneMinus) take "".
"""
import unreal

PKG = "/Game/UI/Materials"
NAME = "M_UI_ProgressPointed"
PATH = "{}/{}".format(PKG, NAME)

PARAMS_SCALAR = {
    "Progress": 1.0, "Ghost": 1.0,
    "TipLenPx": 18.0, "BorderPx": 2.0,
    "BarWidthPx": 400.0, "BarHeightPx": 50.0,
    "BorderAlpha": 0.45,
    "AA": 0.02,
}
# RGBA 0..1 (sRGB-ish authoring values; linear conversion happens in C++ via FColor).
PARAMS_VECTOR = {
    # Track (background) gradient: #080E16 -> #1D2935.
    "TrackTop": (0x08 / 255.0, 0x0E / 255.0, 0x16 / 255.0, 1.0),
    "TrackBottom": (0x1D / 255.0, 0x29 / 255.0, 0x35 / 255.0, 1.0),
    # HP fill gradient: #C96060 -> #782C37.
    "FillTop": (0xC9 / 255.0, 0x60 / 255.0, 0x60 / 255.0, 1.0),
    "FillBottom": (0x78 / 255.0, 0x2C / 255.0, 0x37 / 255.0, 1.0),
    # HP ghost (damage trail) gradient: light red family.
    "GhostTop": (0xE6 / 255.0, 0xA9 / 255.0, 0xA9 / 255.0, 1.0),
    "GhostBottom": (0x93 / 255.0, 0x42 / 255.0, 0x4B / 255.0, 1.0),
    # Track outline: #526272 at 45%.
    "BorderColor": (0x52 / 255.0, 0x62 / 255.0, 0x72 / 255.0, 0.45),
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
    # NOTE: set ALL channel flags explicitly. Fresh nodes default to every
    # channel enabled, and set_editor_property only flips the named one;
    # leaving G/B/A on silently breaks the U/V split (rectangular bars).
    mask_r.set_editor_property("R", True)
    mask_r.set_editor_property("G", False)
    mask_r.set_editor_property("B", False)
    mask_r.set_editor_property("A", False)
    mask_g = make_expr(mat, unreal.MaterialExpressionComponentMask, -440, 80)
    mask_g.set_editor_property("R", False)
    mask_g.set_editor_property("G", True)
    mask_g.set_editor_property("B", False)
    mask_g.set_editor_property("A", False)
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
    const_zero = make_expr(mat, unreal.MaterialExpressionConstant, -280, 320)
    const_zero.set_editor_property("R", 0.0)

    # Aspect = W / H (height units are square).
    aspect = make_expr(mat, unreal.MaterialExpressionDivide, -280, -160)
    conn(ps["BarWidthPx"], aspect, "A", "W->aspect.A")
    conn(ps["BarHeightPx"], aspect, "B", "H->aspect.B")

    # ex = min(U, 1-U); edgeH = ex * Aspect.
    one_minus_u = make_expr(mat, unreal.MaterialExpressionSubtract, -280, -80)
    conn(const_one, one_minus_u, "A", "1->1-U.A")
    conn(mask_r, one_minus_u, "B", "U->1-U.B")
    min_end = make_expr(mat, unreal.MaterialExpressionMin, -120, -80)
    conn(mask_r, min_end, "A", "U->min.A")
    conn(one_minus_u, min_end, "B", "1-U->min.B")
    edge_h = make_expr(mat, unreal.MaterialExpressionMultiply, 40, -80)
    conn(min_end, edge_h, "A", "min->edge.A")
    conn(aspect, edge_h, "B", "aspect->edge.B")
    step("edgeH ok")

    # tipLenH = TipLenPx / H; t = saturate(edgeH / tipLenH).
    tip_len_h = make_expr(mat, unreal.MaterialExpressionDivide, 40, 40)
    conn(ps["TipLenPx"], tip_len_h, "A", "tipPx->tipH.A")
    conn(ps["BarHeightPx"], tip_len_h, "B", "H->tipH.B")
    t_param = make_expr(mat, unreal.MaterialExpressionDivide, 200, -80)
    conn(edge_h, t_param, "A", "edge->t.A")
    conn(tip_len_h, t_param, "B", "tipH->t.B")
    t_sat = make_expr(mat, unreal.MaterialExpressionSaturate, 360, -80)
    conn(t_param, t_sat, "", "t->tsat")

    # h = 0.5 * t * (1.4 - 0.4*t): defined (crisp but kink-free) corner at the
    # tip junction, smooth nose. Uses only Subtract/Multiply.
    const_one4 = make_expr(mat, unreal.MaterialExpressionConstant, 360, 140)
    const_one4.set_editor_property("R", 1.4)
    const_zero4 = make_expr(mat, unreal.MaterialExpressionConstant, 360, 200)
    const_zero4.set_editor_property("R", 0.4)
    mt04 = make_expr(mat, unreal.MaterialExpressionMultiply, 520, 200)
    conn(const_zero4, mt04, "A", "0.4->mt04.A")
    conn(t_sat, mt04, "B", "t->mt04.B")
    two_minus_t = make_expr(mat, unreal.MaterialExpressionSubtract, 680, 140)
    conn(const_one4, two_minus_t, "A", "1.4->tmt.A")
    conn(mt04, two_minus_t, "B", "mt04->tmt.B")
    tt = make_expr(mat, unreal.MaterialExpressionMultiply, 840, 140)
    conn(t_sat, tt, "A", "t->tt.A")
    conn(two_minus_t, tt, "B", "tmt->tt.B")
    half_h = make_expr(mat, unreal.MaterialExpressionMultiply, 1000, 140)
    conn(tt, half_h, "A", "tt->half.A")
    conn(const_half, half_h, "B", "0.5->half.B")
    step("hull ok")

    # dy = |V - 0.5|; d_outer = dy - h.
    v_sub = make_expr(mat, unreal.MaterialExpressionSubtract, -120, 160)
    conn(mask_g, v_sub, "A", "V->vsub.A")
    conn(const_half, v_sub, "B", "0.5->vsub.B")
    dy = make_expr(mat, unreal.MaterialExpressionAbs, 40, 160)
    conn(v_sub, dy, "", "vsub->abs")
    d_outer = make_expr(mat, unreal.MaterialExpressionSubtract, 1320, 0)
    conn(dy, d_outer, "A", "dy->dout.A")
    conn(half_h, d_outer, "B", "h->dout.B")
    outer = make_expr(mat, unreal.MaterialExpressionSmoothStep, 1480, 0)
    m1 = make_expr(mat, unreal.MaterialExpressionConstant, 1320, 160)
    m1.set_editor_property("R", -1.0)
    neg_aa = make_expr(mat, unreal.MaterialExpressionMultiply, 1320, 240)
    conn(m1, neg_aa, "A", "-1->negAA.A")
    conn(ps["AA"], neg_aa, "B", "AA->negAA.B")
    conn(neg_aa, outer, "Min", "negAA->outer.Min")
    conn(ps["AA"], outer, "Max", "AA->outer.Max")
    conn(d_outer, outer, "Value", "dout->outer.Value")
    shape_mask = make_expr(mat, unreal.MaterialExpressionOneMinus, 1640, 0)
    conn(outer, shape_mask, "", "outer->shapeMask")
    step("shapeMask ok")

    # Border inset in height units: b = BorderPx / H.
    b_h = make_expr(mat, unreal.MaterialExpressionDivide, 200, 240)
    conn(ps["BorderPx"], b_h, "A", "bpx->bh.A")
    conn(ps["BarHeightPx"], b_h, "B", "H->bh.B")
    # edgeI = max(edgeH - b, 0).
    edge_sub = make_expr(mat, unreal.MaterialExpressionSubtract, 360, 240)
    conn(edge_h, edge_sub, "A", "edge->esub.A")
    conn(b_h, edge_sub, "B", "b->esub.B")
    edge_i = make_expr(mat, unreal.MaterialExpressionMax, 520, 240)
    conn(edge_sub, edge_i, "A", "esub->ei.A")
    conn(const_zero, edge_i, "B", "0->ei.B")
    # tipLenI = max(tipLenH - b, eps).
    tip_sub = make_expr(mat, unreal.MaterialExpressionSubtract, 520, 320)
    conn(tip_len_h, tip_sub, "A", "tipH->tsub.A")
    conn(b_h, tip_sub, "B", "b->tsub.B")
    tip_i = make_expr(mat, unreal.MaterialExpressionMax, 680, 320)
    conn(tip_sub, tip_i, "A", "tsub->ti.A")
    eps = make_expr(mat, unreal.MaterialExpressionConstant, 520, 380)
    eps.set_editor_property("R", 0.001)
    conn(eps, tip_i, "B", "eps->ti.B")
    # Inner tip with the same corner profile, then inset by b.
    t_i = make_expr(mat, unreal.MaterialExpressionDivide, 840, 240)
    conn(edge_i, t_i, "A", "ei->ti2.A")
    conn(tip_i, t_i, "B", "tipI->ti2.B")
    t_i_sat = make_expr(mat, unreal.MaterialExpressionSaturate, 1000, 240)
    conn(t_i, t_i_sat, "", "ti2->tisat")
    one4_i = make_expr(mat, unreal.MaterialExpressionConstant, 1160, 180)
    one4_i.set_editor_property("R", 1.4)
    zero4_i = make_expr(mat, unreal.MaterialExpressionConstant, 1160, 300)
    zero4_i.set_editor_property("R", 0.4)
    mtm_i = make_expr(mat, unreal.MaterialExpressionMultiply, 1320, 300)
    conn(zero4_i, mtm_i, "A", "0.4->mtmi.A")
    conn(t_i_sat, mtm_i, "B", "tisat->mtmi.B")
    tmt_i = make_expr(mat, unreal.MaterialExpressionSubtract, 1480, 240)
    conn(one4_i, tmt_i, "A", "1.4->tmti.A")
    conn(mtm_i, tmt_i, "B", "mtmi->tmti.B")
    tt_i = make_expr(mat, unreal.MaterialExpressionMultiply, 1320, 240)
    conn(t_i_sat, tt_i, "A", "tisat->tti.A")
    conn(tmt_i, tt_i, "B", "tmti->tti.B")
    half_i = make_expr(mat, unreal.MaterialExpressionMultiply, 1480, 240)
    conn(tt_i, half_i, "A", "tti->halfi.A")
    conn(const_half, half_i, "B", "0.5->halfi.B")
    inner_sub = make_expr(mat, unreal.MaterialExpressionSubtract, 2120, 320)
    conn(half_i, inner_sub, "A", "halfi->isub.A")
    conn(b_h, inner_sub, "B", "b->isub.B")
    inner_h = make_expr(mat, unreal.MaterialExpressionMax, 2280, 320)
    conn(inner_sub, inner_h, "A", "isub->ih.A")
    conn(const_zero, inner_h, "B", "0->ih.B")
    d_inner = make_expr(mat, unreal.MaterialExpressionSubtract, 2280, 160)
    conn(dy, d_inner, "A", "dy->din.A")
    conn(inner_h, d_inner, "B", "ih->din.B")
    smooth_i = make_expr(mat, unreal.MaterialExpressionSmoothStep, 2440, 160)
    conn(neg_aa, smooth_i, "Min", "negAA->si.Min")
    conn(ps["AA"], smooth_i, "Max", "AA->si.Max")
    conn(d_inner, smooth_i, "Value", "din->si.Value")
    inner_one = make_expr(mat, unreal.MaterialExpressionOneMinus, 2600, 160)
    conn(smooth_i, inner_one, "", "si->ione")
    inner_mask = make_expr(mat, unreal.MaterialExpressionMultiply, 2760, 160)
    conn(inner_one, inner_mask, "A", "ione->imask.A")
    conn(shape_mask, inner_mask, "B", "shape->imask.B")
    step("inner hull ok")

    # Fill/ghost vertical wipe. AAX = 1.5px in U units.
    aa_x = make_expr(mat, unreal.MaterialExpressionDivide, 1960, 0)
    c15 = make_expr(mat, unreal.MaterialExpressionConstant, 1800, 60)
    c15.set_editor_property("R", 1.5)
    conn(c15, aa_x, "A", "1.5->aax.A")
    conn(ps["BarWidthPx"], aa_x, "B", "W->aax.B")

    def wipe(param_name, y, tag):
        sub = make_expr(mat, unreal.MaterialExpressionSubtract, 2120, y)
        conn(mask_r, sub, "A", "{} U->sub.A".format(tag))
        conn(ps[param_name], sub, "B", "{} P->sub.B".format(tag))
        div = make_expr(mat, unreal.MaterialExpressionDivide, 2280, y)
        conn(sub, div, "A", "{} sub->div.A".format(tag))
        conn(aa_x, div, "B", "{} AAX->div.B".format(tag))
        sat = make_expr(mat, unreal.MaterialExpressionSaturate, 2440, y)
        conn(div, sat, "", "{} div->sat".format(tag))
        out = make_expr(mat, unreal.MaterialExpressionOneMinus, 2600, y)
        conn(sat, out, "", "{} sat->mask".format(tag))
        return out

    fill_mask = wipe("Progress", -160, "fill")
    ghost_mask = wipe("Ghost", -280, "ghost")
    step("fill/ghost ok")

    # Vertical gradients by V.
    def vgrad(top_param, bot_param, y, tag):
        g = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 2600, y)
        conn(pv[top_param], g, "A", "{} top->g.A".format(tag))
        conn(pv[bot_param], g, "B", "{} bot->g.B".format(tag))
        conn(mask_g, g, "Alpha", "{} V->g.Alpha".format(tag))
        return g

    track_col = vgrad("TrackTop", "TrackBottom", -80, "track")
    ghost_col = vgrad("GhostTop", "GhostBottom", -160, "ghostC")
    fill_col = vgrad("FillTop", "FillBottom", -240, "fillC")
    step("gradients ok")

    # Color chain: track -> ghost -> fill -> border.
    def masked(mask_a, mask_b, y, tag):
        m = make_expr(mat, unreal.MaterialExpressionMultiply, 2760, y)
        conn(mask_a, m, "A", "{} m.A".format(tag))
        conn(mask_b, m, "B", "{} m.B".format(tag))
        return m

    w_ghost = masked(ghost_mask, inner_mask, -160, "wghost")
    w_fill = masked(fill_mask, inner_mask, -80, "wfill")
    ring_one = make_expr(mat, unreal.MaterialExpressionOneMinus, 2920, 160)
    conn(inner_mask, ring_one, "", "inner->ringone")
    w_ring = masked(shape_mask, ring_one, 160, "wring")

    lerp1 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 2920, -120)
    conn(track_col, lerp1, "A", "track->lerp1.A")
    conn(ghost_col, lerp1, "B", "ghost->lerp1.B")
    conn(w_ghost, lerp1, "Alpha", "wghost->lerp1.Alpha")
    lerp2 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 3080, -40)
    conn(lerp1, lerp2, "A", "lerp1->lerp2.A")
    conn(fill_col, lerp2, "B", "fill->lerp2.B")
    conn(w_fill, lerp2, "Alpha", "wfill->lerp2.Alpha")
    # Border RGB only; alpha handled per-pixel via BorderColor.A below.
    lerp3 = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 3240, 40)
    conn(lerp2, lerp3, "A", "lerp2->lerp3.A")
    conn(pv["BorderColor"], lerp3, "B", "border->lerp3.B")
    conn(w_ring, lerp3, "Alpha", "wring->lerp3.Alpha")

    # Per-pixel opacity: inner -> 1, ring -> BorderAlpha, outside -> 0.
    op_lerp = make_expr(mat, unreal.MaterialExpressionLinearInterpolate, 3240, 220)
    conn(const_one, op_lerp, "A", "1->op.A")
    conn(ps["BorderAlpha"], op_lerp, "B", "bA->op.B")
    conn(w_ring, op_lerp, "Alpha", "wring->op.Alpha")
    final_alpha = make_expr(mat, unreal.MaterialExpressionMultiply, 3400, 220)
    conn(shape_mask, final_alpha, "A", "shape->fa.A")
    conn(op_lerp, final_alpha, "B", "op->fa.B")

    if not LIB.connect_material_property(lerp3, "", unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("connect MP_BASE_COLOR failed")
    if not LIB.connect_material_property(final_alpha, "", unreal.MaterialProperty.MP_OPACITY):
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
