#include "../api.h"
#include "../platforms/platforms.h"
#include "solus/val.h"

solu_call_ex smc_ctrl_held(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val btn = solu_get(s, 0);
    if (btn.tt != SOLU_TI64)
        return solu_err(s, "arg 'btn' expected i64 got %s", solu_typename(btn).c_str);

    return solu_ok((solu_val){SOLU_TBOOL,
        .boolean=smc_controller_held(&g->platform, (smc_controller)btn.i64)
    });
}

solu_call_ex smc_ctrl_pressed(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val btn = solu_get(s, 0);
    if (btn.tt != SOLU_TI64)
        return solu_err(s, "arg 'btn' expected i64 got %s", solu_typename(btn).c_str);

    return solu_ok((solu_val){SOLU_TBOOL,
        .boolean=smc_controller_pressed(&g->platform, (smc_controller)btn.i64)
    });
}

solu_call_ex smc_ctrl_released(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val btn = solu_get(s, 0);
    if (btn.tt != SOLU_TI64)
        return solu_err(s, "arg 'btn' expected i64 got %s", solu_typename(btn).c_str);

    return solu_ok((solu_val){SOLU_TBOOL,
        .boolean=smc_controller_released(&g->platform, (smc_controller)btn.i64)
    });
}

solu_call_ex smc_ctrl_axis(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val stick = solu_get(s, 0);
    if (stick.tt != SOLU_TI64)
        return solu_err(s, "arg 'stick' expected i64 got %s", solu_typename(stick).c_str);
    if (stick.i64 < 0 || stick.i64 > SMC_RIGHT_STICK)
        return solu_err(s, "arg 'stick' is not a valid analog stick");

    sf_vec2 axis = smc_controller_axis(&g->platform, (smc_analog)stick.i64);
    solu_val out = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(out.dyn, "x", (solu_val){SOLU_TF64, .f64=axis.x});
    solu_dobj_strset(out.dyn, "y", (solu_val){SOLU_TF64, .f64=axis.y});
    solu_valvec_push(&((solu_dobj *)out.dyn)->array, (solu_val){SOLU_TF64, .f64=axis.x});
    solu_valvec_push(&((solu_dobj *)out.dyn)->array, (solu_val){SOLU_TF64, .f64=axis.y});
    return solu_ok(out);
}

solu_val smc_ctrl(solu_state *s) {
    solu_val obj = solu_dnew(s, SOLU_DOBJ);

    solu_dobj_strset(obj.dyn, "a", (solu_val){SOLU_TI64, .i64=SMC_CTRL_A});
    solu_dobj_strset(obj.dyn, "cross", (solu_val){SOLU_TI64, .i64=SMC_CTRL_A});

    solu_dobj_strset(obj.dyn, "b", (solu_val){SOLU_TI64, .i64=SMC_CTRL_B});
    solu_dobj_strset(obj.dyn, "circle", (solu_val){SOLU_TI64, .i64=SMC_CTRL_B});

    solu_dobj_strset(obj.dyn, "x", (solu_val){SOLU_TI64, .i64=SMC_CTRL_X});
    solu_dobj_strset(obj.dyn, "square", (solu_val){SOLU_TI64, .i64=SMC_CTRL_X});

    solu_dobj_strset(obj.dyn, "y", (solu_val){SOLU_TI64, .i64=SMC_CTRL_Y});
    solu_dobj_strset(obj.dyn, "triangle", (solu_val){SOLU_TI64, .i64=SMC_CTRL_Y});

    solu_dobj_strset(obj.dyn, "lb", (solu_val){SOLU_TI64, .i64=SMC_CTRL_L1});
    solu_dobj_strset(obj.dyn, "l1", (solu_val){SOLU_TI64, .i64=SMC_CTRL_L1});

    solu_dobj_strset(obj.dyn, "rb", (solu_val){SOLU_TI64, .i64=SMC_CTRL_R1});
    solu_dobj_strset(obj.dyn, "r1", (solu_val){SOLU_TI64, .i64=SMC_CTRL_R1});

    solu_dobj_strset(obj.dyn, "select", (solu_val){SOLU_TI64, .i64=SMC_CTRL_SELECT});
    solu_dobj_strset(obj.dyn, "start", (solu_val){SOLU_TI64, .i64=SMC_CTRL_START});

    solu_dobj_strset(obj.dyn, "up", (solu_val){SOLU_TI64, .i64=SMC_CTRL_UP});
    solu_dobj_strset(obj.dyn, "down", (solu_val){SOLU_TI64, .i64=SMC_CTRL_DOWN});
    solu_dobj_strset(obj.dyn, "left", (solu_val){SOLU_TI64, .i64=SMC_CTRL_LEFT});
    solu_dobj_strset(obj.dyn, "right", (solu_val){SOLU_TI64, .i64=SMC_CTRL_RIGHT});

    solu_dobj_strset(obj.dyn, "lstick", (solu_val){SOLU_TI64, .i64=SMC_LEFT_STICK});
    solu_dobj_strset(obj.dyn, "rstick", (solu_val){SOLU_TI64, .i64=SMC_RIGHT_STICK});

    return obj;
}
