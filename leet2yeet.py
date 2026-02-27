#!/usr/bin/env python3
import os
import re
import argparse

from math import sqrt

def parse_leet_cfg(path="./driver/config.h"):
    ''' Read in LEETMOUSE config.h '''
    assert os.path.exists(path), f"Config '{path}' does not exist!"

    with open(path, 'r') as file:
        data = file.readlines()
        data = "\n".join(data)
    
    entries = re.findall(r'#define\s+([A-Z_]+)\s+([\d+\.]+)', data)
    entries = {name.lower(): float(value) for name, value in entries}
    
    return entries

def build_yeet_cfg(pre_scale, input_cap, offset, sensitivity, acceleration, sensitivity_y, output_cap, smoothing=False):
    return f"""// Acceleration Mode
#define ACCELERATION_MODE AccelMode_Linear

// Global Parameters
#define SENSITIVITY {sensitivity:.4g} // For compatibility this is named SENSITIVITY, but it really refers just to the X axis
#define SENSITIVITY_Y {sensitivity_y:.4g} // Ratio Y/X
#define OUTPUT_CAP {output_cap:.4g}
#define INPUT_CAP {input_cap:.4g}
#define OFFSET {offset:.4g}
#define PRESCALE {pre_scale:.4g}

// Angle Snapping (in radians)
#define ANGLE_SNAPPING_THRESHOLD 0 // 0 deg. in rad.
#define ANGLE_SNAPPING_ANGLE 0 // 1.5708 - 90 deg. in rad.

// Rotation (in radians)
#define ROTATION_ANGLE 0

// LUT settings
#define LUT_SIZE 0
#define LUT_DATA 0

// Mode-specific parameters
#define ACCELERATION {acceleration:.4g}
#define MIDPOINT 1.3
#define MOTIVITY 1.5
#define EXPONENT 1.8
#define USE_SMOOTHING {1 if smoothing else 0} // 1 - True, 0 - False

// Custom Curve (Not used on the driver side)
#define CC_DATA_AGGREGATE
"""

def convert(
    pre_scale_x, pre_scale_y, offset, speed_cap,
    sensitivity, acceleration, sens_cap,   
    post_scale_x, post_scale_y, **kwargs):
    ''' Convert LEETMOUSE parameters to YeetMouse equivalent, if possible '''
    # LEET curve: xout = x*pre_scale*post_scale*   [1 + (pre_scale*sqrt(x²+y²)/ms + offset)*(accel/sensitivity)]
    # YEET curve: xout = x*pre_scale*sensitivity*  [1 + (pre_scale*sqrt(x²+y²)/ms + offset)*accel]

    assert pre_scale_x == pre_scale_y, "Pre scaler must match X and Y axis"

    accel_yeet = acceleration/sensitivity
    sense_yeet = post_scale_x
    output_cap = (sens_cap/sensitivity)*post_scale_x
    sensitivity_y = post_scale_y/post_scale_x

    return {
        "pre_scale":pre_scale_x,
        "input_cap": speed_cap,
        "offset": offset,
        "sensitivity":sense_yeet,
        "acceleration": accel_yeet, 
        "sensitivity_y": sensitivity_y,
        "output_cap": output_cap
    }

def accel_leet(dx, dy, 
    pre_scale_x=1, pre_scale_y=1, offset=0, speed_cap=0.0,                              # Pre accel
    sensitivity=0.85, acceleration=0.26, sens_cap=4.0, post_scale_x=1, post_scale_y=1): # Post accel
    ''' Takes in 'dx'/'dy' mickeys and scales them according to LEETMOUSE '''
    dx *= pre_scale_x
    dy *= pre_scale_y

    accel_sense = sensitivity

    rate = sqrt(dx**2 + dy**2)

    # Apply speedcap
    if(speed_cap > 0 and rate >= speed_cap):
        dx *= speed_cap / rate
        dy *= speed_cap / rate
        rate = speed_cap
    
    #rate /= ms     # Lets assume constant rate
    rate -= offset

    if rate > 0:
        rate *= acceleration
        accel_sense += rate
    if sens_cap > 0 and accel_sense >= sens_cap:
        accel_sense = sens_cap
    
    accel_sense /= sensitivity

    dx *= accel_sense
    dy *= accel_sense
    dx *= post_scale_x
    dy *= post_scale_y

    return dx, dy

def accel_yeet(dx, dy, 
        pre_scale=1, input_cap=0, offset=0,                                       # Pre accel
        sensitivity=0.85, acceleration=0.26, sensitivity_y=1, output_cap = 4):    # Post accel
    ''' Takes in 'dx'/'dy' mickeys and scales them according to YeetMouse '''

    dx *= pre_scale
    dy *= pre_scale

    speed = sqrt(dx**2 + dy**2)
    #speed /= ms # Lets assume constant rate

    if input_cap > 0 and speed > input_cap:
        speed = input_cap
    
    speed -= offset

    # Apply acceleration: Only simulate linear mode here as LEETMOUSE only has that mode.
    if speed > 0:
        speed = 1 + speed*acceleration
    else:
        speed = 1

    speed *= sensitivity
    speed_y = speed*sensitivity_y

    if output_cap > 0:
        if speed > output_cap:
            speed = output_cap
        if speed_y > output_cap:
            speed_y = output_cap
    
    dx *= speed
    dy *= speed

    return dx, dy

def test(do_plot = False):
    ''' Test conversion of LEETMOUSE paramters to YeetMouse'''
    # Test parameters, that almost (no input speed_cap) covers the full spectrum of LEETMOUSE. We will try to convert them from LEETMOUSE to YeetMouse
    params = {
        "pre_scale_x": 0.1,
        "pre_scale_y": 0.1,
        "offset": 10, 
        "speed_cap": 0.0,
        "sensitivity": 0.85,
        "acceleration": 0.26,
        "sens_cap": 4.0,
        "post_scale_x": 2,
        "post_scale_y": 2,
    }
    
    params_yeet = convert(**params)

    leet = lambda x: accel_leet(x, 0, **params)[0]
    yeet = lambda x: accel_yeet(x, 0, **params_yeet)[0]

    # Check for perfect linear dependency over "big range"
    for n in range(1,10000):
        ratio = round(yeet(n)/leet(n), ndigits=3)
        if ratio == 1.0: continue
        print(f"CONVERSION ERROR FOUND! {n=} : {ratio=}")
        do_plot = True
        break
    
    # Plot?
    if not do_plot: return
    
    import matplotlib.pyplot as plt
    import numpy as np

    x = np.linspace(1, 1000, 5000)
    y_leet = list(map(leet, x))/x
    y_yeet = list(map(yeet, x))/x

    plt.plot(x, y_leet)
    plt.plot(x, y_yeet)
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='LEETMOUSE to YeetMouse config converter')
    parser.add_argument('--config','-c', metavar='CFG', type=str,
                        default='./driver/config.h',
                        help="Path to LEETMOUSE config file. Default: %(default)s")
    parser.add_argument('--params','-p', action='store_true',
                        help='Only return converted parameters instead of generating YeetMouse config.h')
    parser.add_argument('--test','-t', action='store_true',
                        help='Perform a test of the conversion function')

    # Parse args
    args = parser.parse_args()

    if args.test:   # Test curve
        test(do_plot=True)
        exit(0)

    try:
        params = parse_leet_cfg(args.config)
    except Exception as e:
        print(f"Could not parse config: {e}")
        exit(1)
    
    params_yeet = convert(**params)

    if args.params: # Print params
        print(params_yeet)
        exit(0)
    
    # Convert to YeetMouse config.h
    print(build_yeet_cfg(**params_yeet))