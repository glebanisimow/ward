// Ventilator: decelerating inspiratory flow, passive exhale, blower tones
// (harmonics 2,3,4,6,7 of ~111 Hz; ref 218/338/445/664/770 Hz), valve clicks.
declare name "ventilator";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

rr      = dv.dpar(hslider("[0]rate [unit:/min]", 16, 2, 40, 0.1));
ie      = dv.dpar(hslider("[1]ieRatio", 2, 1, 4, 0.01)) : ct.lag(2);
turbHz  = dv.dpar(hslider("[2]blower [unit:Hz] [scale:log]", 111, 40, 300, 0.1)) : ct.lag(1);
blowLvl = dv.dpar(hslider("[3]blowerLevel", 1, 0, 2, 0.001)) : ct.lag(0.3);
airLvl  = dv.dpar(hslider("[4]air", 1, 0, 2, 0.001)) : ct.lag(0.3);
clickLvl = dv.dpar(hslider("[5]valves", 0.5, 0, 1, 0.001)) : ct.lag(0.3);

ventilator = air + blower + valves
with {
    r  = rr / 60.0;
    bp = dv.clk(r);
    ti = 1.0 / (1.0 + ie);
    insp = bp < ti;
    tIn = ti / r;
    tEx = (1.0 - ti) / r;
    xi = bp / ti;
    xe = (bp - ti) / (1.0 - ti);
    fi = float(insp) * exp(-2.2 * xi) * min(1.0, xi * tIn / 0.05) : ct.lag(0.02);
    fe = (1.0 - float(insp)) * 0.75 * exp(-3.5 * xe) * min(1.0, xe * tEx / 0.05) : ct.lag(0.02);
    trI = dv.wrapped(bp);
    trE = insp' & (1 - insp);

    air = airLvl * (3.6 * fi * dv.bpf(1100.0, 1.2, ct.noise(dv.nkey(1)))
                    + 2.8 * fe * dv.bpf(450.0, 1.2, ct.noise(dv.nkey(2))));
    phB = dv.ton(turbHz * (1.0 + 0.1 * fi));
    blower = blowLvl * (0.35 + 0.65 * fi) * 0.4
           * (0.7 * dv.harm(phB, 2) + 0.5 * dv.harm(phB, 3) + 0.56 * dv.harm(phB, 4)
              + dv.harm(phB, 6) * dv.hgate(turbHz, 6) + 0.56 * dv.harm(phB, 7) * dv.hgate(turbHz, 7));
    t  = dv.since(trI | trE);
    nV = ct.noise(dv.nkey(3));
    vc = exp(0.0 - t / 0.006) * 2.0 * dv.bpf(2500.0, 1.4, nV)
       + exp(0.0 - t / 0.03) * 1.5 * ct.sin2pi(ma.frac(90.0 * t * dv.P / dv.T));
    valves = clickLvl * vc * min(1.0, t / dv.T * ma.SR / 24.0);   // 0.5 ms attack
};

process = ventilator * 0.25;
