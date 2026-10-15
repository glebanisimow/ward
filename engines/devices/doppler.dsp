// Carotid CW Doppler (ref09): band-limited noise whose upper edge follows the
// flow velocity of each beat: diastole ~0.45 of peak, 90 ms rise to the
// systolic peak (ref 1540 Hz), decay tau 0.15 s. Fixed half-octave band bank
// with weights from fmax: a cutoff driven by the beat clock compiles ~100x
// slower in Faust.
declare name "doppler";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

hr     = dv.dpar(hslider("[0]heartRate [unit:bpm]", 82, 20, 200, 0.1));
hrv    = dv.dpar(hslider("[1]variability", 0.5, 0, 1, 0.001)) : ct.lag(1);
ectopy = dv.dpar(hslider("[2]ectopy", 0.1, 0, 1, 0.001)) : ct.lag(1);
peak   = dv.dpar(hslider("[3]peak [unit:Hz] [scale:log]", 1540, 300, 4000, 0.1)) : ct.lag(0.5);
dia    = dv.dpar(hslider("[4]diastole", 0.45, 0, 0.9, 0.001)) : ct.lag(0.5);
rise   = dv.dpar(hslider("[5]rise [unit:s]", 0.09, 0.02, 0.3, 0.001));
decay  = dv.dpar(hslider("[6]decay [unit:s]", 0.15, 0.03, 0.6, 0.001));

doppler = amp * band
with {
    bt  = dv.beats(dv.hrDrift(hr, hrv) / 60.0, ectopy * 0.15, hrv);
    w   = bt : _, !;
    pvc = bt : !, _;
    tB  = dv.since(w);
    x   = min(1.0, tB / rise);
    s   = min(x * x * (3.0 - 2.0 * x), exp(0.0 - (tB - rise) / decay))
        * ba.sAndH(w | ct.isFirst, select2(pvc > 0.5, 1.0, 0.5));
    fmax = peak * (dia + (1.0 - dia) * s);            // before pitch
    amp  = 0.35 + 0.65 * s;
    n    = ct.noise(dv.nkey(8));
    fc(i) = 200.0 * pow(2.0, 0.5 * float(i));
    wt(i) = max(0.0, min(1.0, 2.0 * ma.log2(fmax / fc(i)) - 0.5));
    bp(i) = dv.bpf(fc(i), 1.0, dv.bpf(fc(i), 1.0, n));
    band = 1.13 * sum(i, 10, wt(i) * bp(i))
         : fi.highpass(2, dv.hz(180.0)) : fi.lowpass(4, dv.hz(1.3 * peak));
};

process = doppler * 0.6;
