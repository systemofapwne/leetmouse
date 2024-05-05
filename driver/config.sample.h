#include "fixedptc.h"
// Maximum number of packets allowed to be sent from the mouse at once. Linux's default value is 8, which at
// least causes EOVERFLOW for my mouse (SteelSeries Rival 600). Increase this, if 'dmesg -w' tells you to!
#define BUFFER_SIZE 8

/*
 * This should be your desired acceleration. It needs to end with an f.
 * For example, setting this to "0.1f" should be equal to
 * cl_mouseaccel 0.1 in Quake.
 */

// Changes behaviour of the scroll-wheel. Default is 3.0f
#define SCROLLS_PER_TICK fixedpt_rconst(3.0)

#define SENSITIVITY fixedpt_rconst(1.0)
#define ACCELERATION fixedpt_rconst(0.04)
#define SENS_CAP fixedpt_rconst(3.0)
#define OFFSET fixedpt_rconst(0.0)
#define POST_SCALE_X fixedpt_rconst(1.0)
#define POST_SCALE_Y fixedpt_rconst(1.0)
// Sensor rotation correction in radians (current value rotates mouse movement about 7.2 degrees to the right)
#define ROTATION_ANGLE fixedpt_rconst(-0.125) 
#define SPEED_CAP fixedpt_rconst(0.0)

// Prescaler for different DPI values. 1.0f at 400 DPI. To adjust it for <your_DPI>, calculate 400/your_DPI

// Generic @ 400 DPI
#define PRE_SCALE_X fixedpt_rconst(1.0)
#define PRE_SCALE_Y fixedpt_rconst(1.0)

// Steelseries Rival 110 @ 7200 DPI
//#define PRE_SCALE_X 0.0555555f
//define PRE_SCALE_Y 0.0555555f

// Steelseries Rival 600/650 @ 12000 DPI
//#define PRE_SCALE_X 0.0333333f
//#define PRE_SCALE_Y 0.0333333f
