// SPDX-License-Identifier: GPL-2.0-or-later

#include "accel.h"
#include "util.h"
#include "config.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/string.h>   //strlen
#include <linux/init.h>
#include "libfixmath/libfixmath/fixmath.h"

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

//Convenient helper for fixed point parameters, which are passed via a string module param and parsed using libfixmath's fix16_from_str
//TODO: Add proper fixed point to string conversion of defaults. Validity checking in update_param can be removed once this is done.
#define PARAM_F(param, default, desc)              \
    fix16_t g_##param = default;                                \
    static char* g_param_##param = s(default);                 \
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
//PARAM_F(AngleAdjustment,XXX,            "");           //Not yet implemented. Douptful, if I will ever add it - Not very useful and needs me to implement trigonometric functions from scratch in C.
//PARAM_F(AngleSnapping,  XXX,            "");           //Not yet implemented. Douptful, if I will ever add it - Not very useful and needs me to implement trigonometric functions from scratch in C.
PARAM_F(ScrollsPerTick, SCROLLS_PER_TICK, "Amount of lines to scroll per scroll-wheel tick.");

void update_param(const char *str, fix16_t *result) {
    fix16_t new_value = fix16_from_str(str);
    if (new_value != fix16_overflow)
        *result = new_value;
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
    fix16_t delta_x, delta_y, delta_whl, ms, rate, accel_sens;
    static fix16_t carry_x = F16(0.0);
    static fix16_t carry_y = F16(0.0);
    static fix16_t carry_whl = F16(0.0);
    static fix16_t last_ms = F16(1.0);
    static ktime_t last;
    ktime_t now;

    accel_sens = g_Sensitivity;

    delta_x = fix16_from_int(*x);
    delta_y = fix16_from_int(*y);
    delta_whl = fix16_from_int(*wheel);

    //Calculate frametime
    now = ktime_get();
    ms = fix16_div(fix16_from_int(now - last), fix16_from_int(1000*1000));
    last = now;
    if(ms < fix16_one) ms = last_ms;        //Sometimes, urbs appear bunched -> Beyond µs resolution so the timing reading is plain wrong. Fallback to last known valid frametime
    if(ms > F16(100.0)) ms = F16(100.0);    //Original InterAccel has 200 here. RawAccel rounds to 100. So do we.
    last_ms = ms;

    //Update acceleration parameters periodically
    update_params(now);

    //Prescale
    delta_x = fix16_mul(delta_x, g_PreScaleX);
    delta_y = fix16_mul(delta_y, g_PreScaleY);

    //Calculate velocity (one step before rate, which divides rate by the last frametime)
    rate = fix16_add(fix16_mul(delta_x, delta_x), fix16_mul(delta_y, delta_y));
    rate = fix16_sqrt(rate);

    //Apply speedcap
    if(g_SpeedCap != F16(0.0)) {
        if (rate >= g_SpeedCap) {
            delta_x = fix16_mul(delta_x, fix16_div(g_SpeedCap, rate));
            delta_y = fix16_mul(delta_y, fix16_div(g_SpeedCap, rate));
            rate = g_SpeedCap;
        }
    }

    //Calculate rate from travelled overall distance and add possible rate offsets
    rate = fix16_div(rate, ms);
    rate = fix16_sub(rate, g_Offset);

    //TODO: Add different acceleration styles
    //Apply linear acceleration on the sensitivity if applicable and limit maximum value
    if(rate > F16(0.0)){
        rate = fix16_mul(rate, g_Acceleration);
        accel_sens = fix16_add(accel_sens, rate);
    }
    if(g_SensitivityCap > F16(0.0) && accel_sens >= g_SensitivityCap){
        accel_sens = g_SensitivityCap;
    }

    //Actually apply accelerated sensitivity, allow post-scaling and apply carry from previous round
    accel_sens = fix16_div(accel_sens, g_Sensitivity);
    //Comments below are (dumb) examples of how to add debug code.
    //char delta_x_str[13];
    //char delta_x_result_str[13];
    //fix16_to_str(delta_x, delta_x_str, 5);
    delta_x = fix16_mul(delta_x, accel_sens);
    //fix16_to_str(delta_x, delta_x_result_str, 5);
    //printk("Before: %s, After: %s", delta_x_str, delta_x_result_str);
    delta_y = fix16_mul(delta_y, accel_sens);
    delta_x = fix16_mul(delta_x, g_PostScaleX);
    delta_y = fix16_mul(delta_y, g_PostScaleY);

    delta_whl = fix16_mul(delta_whl, fix16_div(g_ScrollsPerTick, F16(3.0)));
    delta_x = fix16_add(delta_x, carry_x);
    delta_y = fix16_add(delta_y, carry_y);
    if((delta_whl < 0 && carry_whl < 0) || (delta_whl > 0 && carry_whl > 0)) //Only apply carry to the wheel, if it shares the same sign
        delta_whl = fix16_add(delta_whl, carry_whl);

    if (delta_x == fix16_overflow || delta_y == fix16_overflow || delta_whl == fix16_overflow)
        printk("LEETMOUSE: Arithmetic overflow in acceleration math.");

    *x = fix16_to_int(delta_x);
    *y = fix16_to_int(delta_y);
    *wheel = fix16_to_int(delta_whl);

    //Save carry for next round
    carry_x = fix16_sub(delta_x, fix16_from_int(*x));
    carry_y = fix16_sub(delta_y, fix16_from_int(*y));
    carry_whl = fix16_sub(delta_whl, fix16_from_int(*wheel));
}
