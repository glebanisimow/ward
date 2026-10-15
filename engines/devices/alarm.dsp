// Monitor alarm melodies (IEC 60601-1-8 style): medium = C-E-G,
// high = C-E-G G-C twice; 4 harmonics; pattern repeats every cycle.
declare name "alarm";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

prio   = dv.dpar(nentry("[0]priority", 2, 1, 2, 1));
cycle  = dv.dpar(hslider("[1]cycle [unit:s]", 5, 2, 20, 0.01));
noteLen = dv.dpar(hslider("[2]noteLength", 1, 0.3, 3, 0.001));
bright = dv.dpar(hslider("[3]brightness", 0.6, 0, 1, 0.001)) : ct.lag(0.2);

alarm = env * (dv.harm(ph, 1) + bright * (dv.harm(ph, 2) * dv.hgate(f, 2)
               + 0.66 * dv.harm(ph, 3) * dv.hgate(f, 3) + 0.5 * dv.harm(ph, 4) * dv.hgate(f, 4)))
with {
    cyc = dv.clk(1.0 / cycle);
    ta  = cyc * cycle;
    hi  = ba.sAndH(dv.wrapped(cyc) | ct.isFirst, prio) > 1.5;
    C5 = 523.25; E5 = 659.26; G5 = 783.99; C6 = 1046.5;
    medT = (0.0, 0.25, 0.5, 99.0);
    medF = (C5, E5, G5);
    hiT  = (0.0, 0.13, 0.26, 0.52, 0.65, 1.4, 1.53, 1.66, 1.92, 2.05, 99.0);
    hiF  = (C5, E5, G5, G5, C6, C5, E5, G5, G5, C6);
    envOf(Ts, n, d) = sum(i, n, dv.gate(ta, ba.take(i + 1, Ts), d * noteLen, 0.012));
    frqOf(Ts, F, n) = sum(i, n, ba.take(i + 1, F)
                      * float((ta >= ba.take(i + 1, Ts)) & (ta < ba.take(i + 2, Ts))));
    env = select2(hi, envOf(medT, 3, 0.2), envOf(hiT, 10, 0.11)) : min(1.0);
    f   = select2(hi, frqOf(medT, medF, 3), frqOf(hiT, hiF, 10)) : max(100.0) : ba.sAndH(env > 0.0);
    ph  = dv.ton(f);
};

process = alarm * 0.35;
