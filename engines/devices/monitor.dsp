// Patient monitor beep: one beep per heart beat, square-like (odd harmonics),
// pitch falls with SpO2 (refs 880 / 1192 Hz). PVC beats are quieter.
declare name "monitor";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

hr     = dv.dpar(hslider("[0]heartRate [unit:bpm]", 72, 20, 200, 0.1));
hrv    = dv.dpar(hslider("[1]variability", 0.5, 0, 1, 0.001)) : ct.lag(1);
ectopy = dv.dpar(hslider("[2]ectopy", 0.2, 0, 1, 0.001)) : ct.lag(1);
beepHz = dv.dpar(hslider("[3]beepPitch [unit:Hz] [scale:log]", 880, 200, 3000, 0.1));
beepLen = dv.dpar(hslider("[4]beepLength [unit:s] [scale:log]", 0.12, 0.02, 2, 0.001));
spo2   = dv.dpar(hslider("[5]spo2 [unit:%]", 98, 60, 100, 0.1));
drop   = dv.dpar(hslider("[6]pitchPerPct [unit:semitone]", 0.4, 0, 1, 0.001));
bright = dv.dpar(hslider("[7]brightness", 0.33, 0, 1, 0.001)) : ct.lag(0.2);

monitor = env * (dv.harm(ph, 1) + bright * dv.harm(ph, 3) * dv.hgate(f, 3)
                 + 0.6 * bright * dv.harm(ph, 5) * dv.hgate(f, 5))
with {
    bt  = dv.beats(dv.hrDrift(hr, hrv) / 60.0, ectopy * 0.15, hrv);
    w   = bt : _, !;
    pvc = bt : !, _;
    f   = ba.sAndH(w | ct.isFirst, beepHz * ct.exp2(0.0 - (100.0 - spo2) * drop / 12.0));
    ph  = dv.ton(f);
    env = dv.gate(dv.since(w), 0.0, beepLen, 0.004) * select2(pvc > 0.5, 1.0, 0.45);
};

process = monitor * 0.5;
