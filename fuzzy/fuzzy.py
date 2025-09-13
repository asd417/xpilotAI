import os
import cma
import json
import math
import pickle
import numpy as np

# ---------- integration helpers ----------

def integrateLinearX(a, b, minx, maxx):
    """Integral of (a*x^2 + b*x) dx from minx to maxx."""
    if minx > maxx:
        return 0.0
    return a * (maxx**3 - minx**3) / 3.0 + b * (maxx**2 - minx**2) / 2.0

def integrateLinearXWithY(a, b, maxy):
    """Integral of (a*x^2 + b*x) dx between the x where y goes 0→maxy."""
    if a == 0.0:
        return 0.0  # mirrors safe behavior; C would divide by zero
    x1 = (maxy - b) / a
    x2 = -b / a
    lo, hi = (x2, x1) if x1 > x2 else (x1, x2)
    return integrateLinearX(a, b, lo, hi)

def integrateLinear(a, b, minx, maxx):
    """Integral of (a*x + b) dx from minx to maxx."""
    if minx > maxx:
        return 0.0
    return a * (maxx**2 - minx**2) / 2.0 + b * (maxx - minx)

def integrateLinearWithY(a, b, maxy):
    """Integral of (a*x + b) dx between the x where y goes 0→maxy."""
    if a == 0.0:
        return 0.0
    x1 = (maxy - b) / a
    x2 = -b / a
    lo, hi = (x2, x1) if x1 > x2 else (x1, x2)
    return integrateLinear(a, b, lo, hi)

def integrateBox(y, minx, maxx):
    if minx > maxx:
        return 0.0
    return y * (maxx - minx)

def integrateBoxX(y, minx, maxx):
    if minx > maxx:
        return 0.0
    return y * (maxx**2 - minx**2) / 2.0

# ---------- composite integrals (two ramps + flat) ----------

def integrateDoubleLinear(maxy, f1a, f1b, f2a, f2b):
    # Keep x1/x4 for parity with the C (x4 uses /f1a there and is unused)
    p1 = (maxy - f1b) / f1a if f1a != 0.0 else float('nan')
    p2 = (maxy - f2b) / f2a if f2a != 0.0 else float('nan')
    # assume p2 > p1
    return (integrateLinearWithY(f1a, f1b, maxy)
            + integrateBox(maxy, p1, p2)
            + integrateLinearWithY(f2a, f2b, maxy))

def integrateDoubleLinearX(maxy, f1a, f1b, f2a, f2b):
    p1 = (maxy - f1b) / f1a if f1a != 0.0 else float('nan')
    p2 = (maxy - f2b) / f2a if f2a != 0.0 else float('nan')
    # assume p2 > p1
    return (integrateLinearXWithY(f1a, f1b, maxy)
            + integrateBoxX(maxy, p1, p2)
            + integrateLinearXWithY(f2a, f2b, maxy))

# ---------- angles & membership functions ----------

def tanAngle(x):
    """Convert degrees x to tan(x/2), clamp to [-50, 50], special-case x==180 -> +50."""
    if x == 180:
        return 50.0
    t = math.tan(math.radians(x / 2.0))
    return max(min(t, 50.0), -50.0)

# Shared between ClosestAngle and FurthestAngle
def mem_Angle_Front(x, a=5, b=1.0):
    tanx = tanAngle(x)
    if tanx <= 0.0:
        return min(max(a*tanx+b, 0.0),1)
    else:
        return min(max(-a*tanx+b, 0.0),1)

def mem_Angle_Left(x, a=2,b=-1):
    tanx = tanAngle(x)
    return min(max(a*tanx+b, 0.0), 1.0)

def mem_Angle_Right(x, a=-2, b=-1):
    tanx = tanAngle(x)
    return min(max(a*tanx+b, 0.0), 1.0)

# Distance to closest wall
def mem_Wall_Safe(x, a=0.015, b=-5):
    return min(max(a*x+b, 0.0), 1.0)

def mem_Wall_Close(x, a=0.008, b=-1.2, c=-0.009, d=3.6):
    if a * x + b < 1.0:
        return min(max(a*x+b, 0.0), 1.0)
    else:
        return min(max(c*x+d, 0.0), 1.0)

def mem_Wall_Danger(x, a=-0.006, b=1.6):
    return min(max(a*x+b, 0.0), 1.0)

# ---------- defuzzification ----------

def centroidTurn(turnLeft, noTurn, turnRight):
    denum = (integrateDoubleLinear(turnLeft, 2, 0,  -2, 2)
             + integrateDoubleLinear(noTurn, 3, -2, -3, 4)
             + integrateDoubleLinear(turnRight, 2, -2, -2, 4))
    if denum == 0.0:
        return 1.0
    num = (integrateDoubleLinearX(turnLeft, 2, 0,  -2, 2)
           + integrateDoubleLinearX(noTurn, 3, -2, -3, 4)
           + integrateDoubleLinearX(turnRight, 2, -2, -2, 4))
    return num / denum

# ---------- fuzzy logic ops (avoid Python keywords) ----------

def f_and(a, b):
    return a if a < b else b

def f_or(a, b):
    return a if a > b else b

# ---------- rules ----------

def r1(closest, closestAngle, wd1, wd2, al1, al2):
    # If ClosestWall Danger and closestAngle Left then turn right
    return f_and(mem_Wall_Danger(closest, wd1, wd2), mem_Angle_Left(closestAngle, al1, al2))

def r2(closest, closestAngle, wd1, wd2, as1, as2):
    # If ClosestWall Danger and closestAngle Right then turn left
    return f_and(mem_Wall_Danger(closest, wd1, wd2), mem_Angle_Right(closestAngle, as1, as2))

def r3(closest, closestAngle, wc1, wc2, wc3, wc4, al1, al2):
    # If ClosestWall Close and closestAngle Left then turn right
    return f_and(mem_Wall_Close(closest, wc1, wc2, wc3, wc4), mem_Angle_Left(closestAngle, al1, al2))

def r4(closest, closestAngle, wc1, wc2, wc3, wc4, ar1, ar2):
    # If ClosestWall Close and closestAngle Right then turn left
    return f_and(mem_Wall_Close(closest, wc1, wc2, wc3, wc4), mem_Angle_Right(closestAngle, ar1, ar2))

def r5(closest, ws1, ws2):
    # If ClosestWall Safe Noturn
    return mem_Wall_Safe(closest, ws1, ws2)

def r6(furthestAngle, ar1, ar2):
    # If FurthestAngle Right turn Right
    return mem_Angle_Right(furthestAngle, ar1, ar2)

def r7(furthestAngle, al1, al2):
    # If FurthestAngle Left turn Left
    return mem_Angle_Left(furthestAngle, al1, al2)

def r8(furthestAngle, af1, af2):
    # If FurthestAngle Forward dont turn
    return mem_Angle_Front(furthestAngle, af1, af2)

def turnRules(closest, closestAngle, furthestAngle, chromosome):
    wd1 = chromosome[0]
    wd2 = chromosome[1]
    ar1 = chromosome[2]
    ar2 = chromosome[3]
    wc1 = chromosome[4]
    wc2 = chromosome[5]
    wc3 = chromosome[6]
    wc4 = chromosome[7]
    ws1 = chromosome[8]
    ws2 = chromosome[9]
    af1 = chromosome[10]
    af2 = chromosome[11]
    # rule activations
    # angle left is symmetrical to angle right
    turnLeft  = r2(closest, closestAngle, wd1, wd2, ar1, ar2) + r4(closest, closestAngle, wc1, wc2, wc3, wc4, ar1, ar2) + r7(furthestAngle, -ar1, ar2)
    noTurn    = r5(closest, ws1, ws2) + r8(furthestAngle, af1, af2)
    turnRight = r1(closest, closestAngle, wd1, wd2, -ar1, ar2) + r3(closest, closestAngle, wc1, wc2, wc3, wc4, ar1, ar2) + r6(furthestAngle, ar1, ar2)
    # defuzzify to crisp turn
    return centroidTurn(turnLeft, noTurn, turnRight)


import libpyAI as ai

from pathlib import Path
CKPT = Path("es.ckpt")

def load_or_create_es(x0, sigma0, opts):
    if CKPT.is_file():
        with CKPT.open("rb") as fh:
            es = pickle.load(fh)
        # keep logging, append to existing files
        es.logger = cma.CMADataLogger().register(es, append=True)
        print("[resume] loaded checkpoint:", CKPT)
    else:
        es = cma.CMAEvolutionStrategy(x0, sigma0, opts)
        es.logger = cma.CMADataLogger().register(es)
        print("[fresh] created new CMA-ES")
    return es

def save_ckpt(es, path=CKPT):
    tmp = Path(str(path) + ".tmp")
    with tmp.open("wb") as f:
        f.write(es.pickle_dumps())          # bytes with full optimizer state
    os.replace(tmp, path)

def AI_loop():
    if not hasattr(ai, "cmaes"):
        x0 = np.array([-0.006, 1.6, -2, -1, 0.008, -1.2, -0.009, 3.6, 0.015, -5, 5, 1], dtype=float)
        CMA_stds = [0.0012, 0.32, 0.4, 0.2, 0.0016, 0.24, 0.0018, 0.72, 0.003, 1.0, 1.0, 0.2]
        lower = [-0.02, -6, -6, -6, -0.02, -6, -0.02, -6, -0.02, -6, -6, -6]
        upper = [ 0.02,  6,  6,  6,  0.02,  6,  0.02,  6,  0.02,  6,  6,  6]
        opts = {"CMA_stds": CMA_stds, "bounds": [lower, upper]}
        es = load_or_create_es(x0,1.0, opts)
        setattr(ai, "cmaes", es)
        setattr(ai, "cmaes_candidates", es.ask())
        setattr(ai, "cmaes_candidates_count", len(ai.cmaes_candidates))
        setattr(ai, "cmaes_current", 0)
        setattr(ai, "cmaes_current_eval", 0)
        setattr(ai, "cmaes_current_fit", 0)
        setattr(ai, "cmaes_candidates_fit", [])
        setattr(ai, "cmaes_agent_alive", True)
        setattr(ai, "fitness", 1000000)

    if ai.selfAlive() == 0 and ai.cmaes_agent_alive: # agent just died
        print(f"Agent {ai.cmaes_current} Fitness: {ai.fitness}")
        if ai.cmaes_current_eval < 5:
            ai.cmaes_current_eval += 1
            ai.cmaes_current_fit += ai.fitness
        else:
            print(f"Agent {ai.cmaes_current} Avg Fitness: {ai.cmaes_current_fit / 5} Saving...")
            ai.cmaes_candidates_fit.append(ai.cmaes_current_fit / 5)
            ai.cmaes_current_fit = 0
            ai.cmaes_current_eval = 0
            ai.cmaes_current += 1
        if ai.cmaes_current == ai.cmaes_candidates_count:
            print(f"Generation {ai.cmaes.countiter} Done")
            ai.cmaes.tell(ai.cmaes_candidates, ai.cmaes_candidates_fit)
            setattr(ai, "cmaes_candidates", ai.cmaes.ask())
            setattr(ai, "cmaes_candidates_count", len(ai.cmaes_candidates))
            setattr(ai, "cmaes_current", 0)
            setattr(ai, "cmaes_candidates_fit", [])
            xbest = np.asarray(ai.cmaes.result.xbest)
            fbest = float(ai.cmaes.result.fbest)

            payload = {"xbest": xbest.tolist(), "fbest": fbest}
            with open("cma_best.json", "w") as f:
                json.dump(payload, f, indent=2)

            print(f"Best Chromosome: {xbest} Achieved: {fbest}")
            save_ckpt(ai.cmaes)
            setattr(ai, "cmaes_saved", True)
        setattr(ai, "fitness", 1000000)
        setattr(ai, "cmaes_agent_alive", False)
        
    if ai.selfAlive() == 1 and not ai.cmaes_agent_alive:
        setattr(ai, "cmaes_agent_alive", True)
        #print("New Agent")

    ai.fitness -= 1
    ai.setTurnSpeedDeg(20)
    aimDir = ai.aimdir(0)

    # Set variables
    heading = int(ai.selfHeadingDeg())
    tracking = int(ai.selfTrackingDeg())
    trackWall = ai.wallFeeler(500, tracking)

    # clockwise rotation around the ship
    frontWall = ai.wallFeeler(500, heading)
    wall5  = ai.wallFeeler(500, heading + 150)
    backWall = ai.wallFeeler(500, heading + 180)
    wall7  = ai.wallFeeler(500, heading + 210)

    shouldThrust = 0  # 0/1
    turnDir = 0       # right = 1, left = -1, none = 0

    # scan 360 degrees relative to heading
    furthest = 0.0
    furthest_angle = 0
    closest = 600.0
    closest_angle = 0
    for i in range(360):
        dist = ai.wallFeeler(500, heading + i)
        if dist > furthest:
            furthest = dist
            furthest_angle = i
        if dist < closest:
            closest = dist
            closest_angle = i
    headingTrackingDiff = int(heading + 360 - tracking) % 360
    headingAimingDiff = int(heading + 360 - aimDir) % 360  # kept for parity, unused below

    shotDanger = ai.shotAlert(0)
    if (shotDanger > 0 and shotDanger < 200 and frontWall > 200 and trackWall > 80):
        # print(f"avoiding shot: {shotDanger}")
        shouldThrust = 1
    elif (furthest_angle == 0) and (ai.selfSpeed() < 7) and (frontWall > 200):
        # print("propulsion")
        shouldThrust = 1
    elif (110 < headingTrackingDiff < 250) and (trackWall < 300) and (frontWall > 250):
        # print("thrusters type 1")
        shouldThrust = 1
    elif (backWall < 70) and (frontWall > 250):
        # print("thrusters type 2")
        shouldThrust = 1
    elif (85 < headingTrackingDiff < 275) and (trackWall < 100) and (frontWall > 200):
        # print("thrusters type 3")
        shouldThrust = 1
    elif (wall5 < 70) and (frontWall > 250):
        # print("thrusters type 4")
        shouldThrust = 1
    elif (wall7 < 70) and (frontWall > 250):
        # print("thrusters type 5")
        shouldThrust = 1
    elif ai.selfSpeed() < 5:
        shouldThrust = 1

    turn = turnRules(closest, closest_angle, furthest_angle, ai.cmaes_candidates[ai.cmaes_current])
    # print(f"centroid: {turn}")
    if turn < 0.75:
        turnDir = -1
    elif turn < 1.25:
        turnDir = 0
    else:
        turnDir = 1

    ai.thrust(1 if shouldThrust else 0)

    if turnDir == 1:
        ai.turnRight(1)
        ai.turnLeft(0)
    elif turnDir == 0:
        ai.turnRight(0)
        ai.turnLeft(0)
    elif turnDir == -1:
        ai.turnRight(0)
        ai.turnLeft(1)

    return 0



ai.start(AI_loop,["-name","Fuzzy","-join","localhost", ])