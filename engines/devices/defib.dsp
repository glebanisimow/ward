// Defibrillator (ref fs243638, Lifepak 12), cycling: charge = linear sweep
// 410 -> 1107 Hz (+3rd harmonic) over chargeTime, 1000 Hz beep, 440 Hz ready
// tone (2nd, 5th harmonics) for readyHold, then a shock (thump + crack) or a
// silent disarm; repeats every interval (device time).
declare name "defib";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

interval = dv.dpar(hslider("[0]interval [unit:s]", 16, 2, 120, 0.01));
tc       = dv.dpar(hslider("[1]chargeTime [unit:s]", 7, 0.5, 12, 0.01));
th       = dv.dpar(hslider("[2]readyHold [unit:s]", 3, 0.2, 30, 0.01));
shockOn  = dv.dpar(nentry("[3]shock", 1, 0, 1, 1));
shockLvl = dv.dpar(hslider("[4]shockLevel", 1, 0, 2, 0.001)) : ct.lag(0.3);
toneLvl  = dv.dpar(hslider("[5]toneLevel", 1, 0, 2, 0.001)) : ct.lag(0.3);

defib = toneLvl * 0.45 * (sweep + beep + ready) + shockLvl * on * (1.6 * thump + 1.2 * crack + rattle)
with {
    cyc = dv.clk(1.0 / interval);
    t   = cyc * interval;                     // device seconds into the cycle
    cT  = ba.sAndH(dv.wrapped(cyc) | ct.isFirst, tc);
    hT  = ba.sAndH(dv.wrapped(cyc) | ct.isFirst, th);
    ts  = cT + 0.5 + hT;                      // shock time
    shk = ba.sAndH(dv.wrapped(cyc) | ct.isFirst, shockOn) > 0.5;

    fS  = 410.0 + 697.0 * min(1.0, t / cT);
    phS = dv.ton(fS);
    sweep = dv.gate(t, 0.0, cT, 0.01) * (dv.harm(phS, 1) + 0.25 * dv.harm(phS, 3) * dv.hgate(fS, 3));
    phB = dv.ton(1000.0);
    beep = dv.gate(t, cT, 0.45, 0.005) * (dv.harm(phB, 1) + 0.2 * dv.harm(phB, 3) * dv.hgate(1000.0, 3));
    phR = dv.ton(440.0);
    ready = dv.gate(t, cT + 0.5, hT, 0.005)
          * (dv.harm(phR, 1) + 0.3 * dv.harm(phR, 2) + 0.2 * dv.harm(phR, 5) * dv.hgate(440.0, 5));

    u  = max(0.0, t - ts);                    // clamped: exp(-u/tau) stays finite
    on = float(shk & (t >= ts));
    nT = ct.noise(dv.nkey(9));
    uR = u / dv.T;                            // real seconds, for the thump pitch
    thump = exp(0.0 - u / 0.12) * ct.sin2pi(dv.osc((38.0 + 40.0 * exp(0.0 - u / 0.03)) * dv.P))
          * min(1.0, uR * ma.SR / 48.0);
    crack = exp(0.0 - u / 0.004) * dv.bpf(2600.0, 0.7, nT) * min(1.0, uR * ma.SR / 8.0);
    rattle = exp(0.0 - u / 0.08) * 0.4 * dv.bpf(160.0, 0.5, nT);
};

process = defib * 0.4;
