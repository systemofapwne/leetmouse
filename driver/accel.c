// SPDX-License-Identifier: GPL-2.0-or-later

#include "accel.h"
#include "util.h"
#include "config.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/string.h>   //strlen
#include <linux/init.h>
#include "fixedptc.h"

//Needed for kernel_fpu_begin/end
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
    //Pre Kernel 5.0.0
    #include <asm/i387.h>
#else
    #include <asm/fpu/api.h>
#endif

MODULE_AUTHOR("Christopher Williams <chilliams (at) gmail (dot) com>"); //Original idea of this module
MODULE_AUTHOR("Klaus Zipfel <klaus (at) zipfel (dot) family>");         //Current maintainer

//Converts a preprocessor define's value in "config.h" to a string - Suspect this to change in future version without a "config.h"
#define _s(x) #x
#define s(x) _s(x)

//Convenient helper for fixed point parameters, which are passed via a string to this module (must be individually parsed via atof() - available in util.c)
//TODO: Add parameter default value to string conversion
#define PARAM_F(param, default, desc)              \
    fixedpt g_##param = default;                                \
    static char* g_param_##param = s(default);                  \
    module_param_named(param, g_param_##param, charp, 0644);    \
    MODULE_PARM_DESC(param, desc);

#define PARAM(param, default, desc)                             \
    static char g_##param = default;                            \
    module_param_named(param, g_##param, byte, 0644);           \
    MODULE_PARM_DESC(param, desc);

// ########## Kernel module parameters

// Simple module parameters (instant update)
PARAM(no_bind,          0,              "This will disable binding to this driver via 'leetmouse_bind' by udev.");
PARAM(update,           0,              "Triggers an update of the acceleration parameters below");

//PARAM(AccelMode,        MODE,           "Acceleration method: 0 power law, 1: saturation, 2: log"); //Not yet implemented

// Acceleration parameters (type pchar. Converted to float via "updata_params" triggered by /sys/module/leetmouse/parameters/update)
PARAM_F(PreScaleX,      PRE_SCALE_X,    "Prescale X-Axis before applying acceleration.");
PARAM_F(PreScaleY,      PRE_SCALE_Y,    "Prescale Y-Axis before applying acceleration.");
PARAM_F(SpeedCap,       SPEED_CAP,      "Limit the maximum pointer speed before applying acceleration.");
PARAM_F(Sensitivity,    SENSITIVITY,    "Mouse base sensitivity.");
PARAM_F(Acceleration,   ACCELERATION,   "Mouse acceleration sensitivity.");
PARAM_F(SensitivityCap, SENS_CAP,       "Cap maximum sensitivity.");
PARAM_F(Offset,         OFFSET,         "Mouse base sensitivity.");
//PARAM_F(Power,          XXX,            "");           //Not yet implemented
PARAM_F(PostScaleX,     POST_SCALE_X,   "Postscale X-Axis after applying acceleration.");
PARAM_F(PostScaleY,     POST_SCALE_Y,   "Postscale Y-Axis after applying acceleration.");
//PARAM_F(AngleAdjustment,XXX,            "");           //Not yet implemented. Doubtful, if I will ever add it - Not very useful and needs me to implement trigonometric functions from scratch in C.
//PARAM_F(AngleSnapping,  XXX,            "");           //Not yet implemented. Doubtful, if I will ever add it - Not very useful and needs me to implement trigonometric functions from scratch in C.
PARAM_F(ScrollsPerTick, SCROLLS_PER_TICK, "Amount of lines to scroll per scroll-wheel tick.");

void update_param(const char *str, fixedpt *result) {
    //TODO: Add parameter updating here
    *result = *result;
}

// Updates the acceleration parameters. This is purposely done with a delay!
// First, to not hammer too much the logic in "accelerate()", which is called VERY OFTEN!
// Second, to fight possible cheating. However, this can be OFC changed, since we are OSS...
#define PARAM_UPDATE(param) update_param(g_param_##param, &g_##param);

static ktime_t g_next_update = 0;
INLINE void update_params(ktime_t now)
{
    if(!g_update) return;
    if(now < g_next_update) return;
    g_update = 0;
    g_next_update = now + 1000000000ll;    //Next update is allowed after 1s of delay

    PARAM_UPDATE(PreScaleX);
    PARAM_UPDATE(PreScaleY);
    PARAM_UPDATE(SpeedCap);
    PARAM_UPDATE(Sensitivity);
    PARAM_UPDATE(Acceleration);
    PARAM_UPDATE(SensitivityCap);
    PARAM_UPDATE(Offset);
    PARAM_UPDATE(PostScaleX);
    PARAM_UPDATE(PostScaleY);
    PARAM_UPDATE(ScrollsPerTick);
}

// ########## Acceleration code

// Acceleration happens here
void accelerate(int *x, int *y, int *wheel)
{
    fixedpt delta_x, delta_y, delta_whl, ms, rate, accel_sens;
    static fixedpt carry_x = fixedpt_rconst(0.0);
    static fixedpt carry_y = fixedpt_rconst(0.0);
    static fixedpt carry_whl = fixedpt_rconst(0.0);
    static fixedpt last_ms = fixedpt_rconst(1.0);
    static ktime_t last;
    ktime_t now;

    accel_sens = g_Sensitivity;

    delta_x = fixedpt_fromint(*x);
    delta_y = fixedpt_fromint(*y);
    delta_whl = fixedpt_fromint(*wheel);

    //Calculate frametime
    now = ktime_get();
    ms = fixedpt_div(fixedpt_fromint(now - last), fixedpt_fromint(1000*1000));
    last = now;
    if(ms < fixedpt_rconst(1.0)) ms = last_ms;        //Sometimes, urbs appear bunched -> Beyond µs resolution so the timing reading is plain wrong. Fallback to last known valid frametime
    if(ms > fixedpt_rconst(100.0)) ms = fixedpt_rconst(100.0);    //Original InterAccel has 200 here. RawAccel rounds to 100. So do we.
    last_ms = ms;

    //Update acceleration parameters periodically
    update_params(now);

    //Prescale
    delta_x = fixedpt_mul(delta_x, g_PreScaleX);
    delta_y = fixedpt_mul(delta_y, g_PreScaleY);

    //Calculate velocity (one step before rate, which divides rate by the last frametime)
    rate = fixedpt_add(fixedpt_mul(delta_x, delta_x), fixedpt_mul(delta_y, delta_y));
    rate = fixedpt_sqrt(rate);

    //Apply speedcap
    if(g_SpeedCap != fixedpt_rconst(0.0)) {
        if (rate >= g_SpeedCap) {
            delta_x = fixedpt_mul(delta_x, fixedpt_div(g_SpeedCap, rate));
            delta_y = fixedpt_mul(delta_y, fixedpt_div(g_SpeedCap, rate));
            rate = g_SpeedCap;
        }
    }

    //Calculate rate from travelled overall distance and add possible rate offsets
    rate = fixedpt_div(rate, ms);
    rate = fixedpt_sub(rate, g_Offset);

    //TODO: Add different acceleration styles
    //Apply linear acceleration on the sensitivity if applicable and limit maximum value
    if(rate > fixedpt_rconst(0.0)){
        rate = fixedpt_mul(rate, g_Acceleration);
        accel_sens = fixedpt_add(accel_sens, rate);
    }
    if(g_SensitivityCap > fixedpt_rconst(0.0) && accel_sens >= g_SensitivityCap){
        accel_sens = g_SensitivityCap;
    }

    //Actually apply accelerated sensitivity, allow post-scaling and apply carry from previous round
    accel_sens = fixedpt_div(accel_sens, g_Sensitivity);
    delta_x = fixedpt_mul(delta_x, accel_sens);
    delta_y = fixedpt_mul(delta_y, accel_sens);
    delta_x = fixedpt_mul(delta_x, g_PostScaleX);
    delta_y = fixedpt_mul(delta_y, g_PostScaleY);
    delta_whl = fixedpt_mul(delta_whl, fixedpt_div(g_ScrollsPerTick, fixedpt_rconst(3.0)));
    delta_x = fixedpt_add(delta_x, carry_x); 
    delta_y = fixedpt_add(delta_y, carry_y); 
    if((delta_whl < 0 && carry_whl < 0) || (delta_whl > 0 && carry_whl > 0)) //Only apply carry to the wheel, if it shares the same sign
        delta_whl += carry_whl;

    *x = fixedpt_toint(delta_x);
    *y = fixedpt_toint(delta_y);
    *wheel = fixedpt_toint(delta_whl);

    //Save carry for next round
    carry_x = fixedpt_sub(delta_x, fixedpt_fromint(*x));
    carry_y = fixedpt_sub(delta_y, fixedpt_fromint(*y));
    carry_whl = fixedpt_sub(delta_whl, fixedpt_fromint(*wheel));
}
