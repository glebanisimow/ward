// O2 concentrator (ref fs346967): motor hum ~58 Hz, compressor pulse at
// motor/4, housing tones 490/604/949/1207 Hz, sieve-bed valves twice per
// 8.7 s pressure-swing cycle (the second bed vents weaker).
declare name "concentrator";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

motorHz = dv.dpar(hslider("[0]motor [unit:Hz]", 58, 30, 120, 0.01)) : ct.lag(1);
cycle   = dv.dpar(hslider("[1]cycle [unit:s]", 8.7, 1, 30, 0.01));
tonesLvl = dv.dpar(hslider("[2]tones", 1, 0, 3, 0.001)) : ct.lag(0.3);
chugLvl = dv.dpar(hslider("[3]chug", 1, 0, 3, 0.001)) : ct.lag(0.3);
valveLvl = dv.dpar(hslider("[4]valves", 1, 0, 3, 0.001)) : ct.lag(0.3);

concentrator = motor + tones + chug + valves
with {
    cy   = dv.clk(1.0 / cycle);
    hc   = ma.frac(2.0 * cy);
    load = hc * hc;
    fm   = motorHz * (1.0 - 0.012 * load);
    phM  = dv.ton(fm);
    phC  = dv.clk(0.25 * fm);                 // compressor stroke: rhythm, device time
    pulse = 0.5 + 0.5 * ct.sin2pi(phC);
    am   = 0.8 + 0.2 * pulse * (0.7 + 0.3 * load);
    motor = 0.5 * am * (dv.harm(phM, 1) + 0.09 * dv.harm(phM, 2) + 0.08 * dv.harm(phM, 3));
    tone(f, a) = a * ct.sin2pi(dv.ton(f * (1.0 - 0.006 * load))) * dv.hgate(f, 1);
    tones = tonesLvl * 0.5 * am * (tone(490.0, 0.13) + tone(604.0, 0.1) + tone(949.0, 0.07) + tone(1207.0, 0.03));
    chug = chugLvl * 0.12 * (0.3 + 0.7 * pulse * pulse) * ct.noise(dv.nkey(6)) : fi.lowpass(2, dv.hz(700.0));
    t    = dv.since(dv.wrapped(hc));
    nV   = ct.noise(dv.nkey(7));
    bed  = 1.0 - 0.4 * float(cy >= 0.5);
    valves = valveLvl * (bed * exp(0.0 - t / 0.35) * min(1.0, t / 0.02) * 0.5 * dv.bpf(1800.0, 0.9, nV)
           + exp(0.0 - t / 0.005) * min(1.0, t / dv.T * ma.SR / 24.0) * 1.2 * dv.bpf(3200.0, 1.5, nV));
};

process = concentrator * 0.5;
