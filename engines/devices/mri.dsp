// MRI gradient noise: the gradient waveform repeating every TR = a comb at
// 1/TR shaped by coil / bore resonances, gated by slices and blocks.
// Sequences fitted to Freesound recordings (docs/medical_refs.md):
//   0 3D GRE "Bravo"  1 T2 TSE  2 EPI (fMRI)  3 Utrecht 1  4 Utrecht 2
//   5 Utrecht 4  6 GE Signa. exam = 1: a seeded sequence every seqLength s
// with 5 s of coldhead between. Coldhead is not from a reference.
declare name "mri";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

seqP   = dv.dpar(nentry("[0]sequence", 0, 0, 6, 1));
examOn = dv.dpar(nentry("[1]exam", 0, 0, 1, 1));
seqLen = dv.dpar(hslider("[2]seqLength [unit:s]", 40, 5, 300, 1));
gradLvl = dv.dpar(hslider("[3]gradient", 1, 0, 2, 0.001)) : ct.lag(0.3);
coldLvl = dv.dpar(hslider("[4]coldhead", 0.3, 0, 2, 0.001)) : ct.lag(0.3);
reso   = dv.dpar(hslider("[5]resonance", 1, 0.2, 4, 0.001)) : ct.lag(0.3);
dist   = dv.dpar(hslider("[6]distance", 0, 0, 1, 0.001)) : ct.lag(0.5);

mri = (gradient + cold) * ba.db2linear(-18.0 * dist) : fi.lowpass(2, 9000.0 * pow(2.0, -4.0 * dist))
with {
    F0  = (63.6, 88.0, 196.0, 302.0, 322.0, 437.0, 102.0);
    FR1 = (1016.0, 790.0, 980.0, 600.0, 970.0, 1310.0, 620.0);
    FR2 = (640.0, 170.0, 590.0, 910.0, 1292.0, 1745.0, 1350.0);
    G2  = (0.3, 0.25, 0.3, 0.5, 0.3, 0.5, 0.9);
    SP  = (1.0, 0.27, 0.07, 0.56, 1.0, 0.57, 0.24);
    SD  = (1.0, 0.5, 0.57, 0.72, 1.0, 0.63, 0.67);
    BP  = (1.74, 2.43, 2.0, 1.0, 1.0, 1.0, 1.0);
    BD  = (0.94, 0.8, 0.95, 1.0, 1.0, 1.0, 1.0);

    eLen = seqLen + 5.0;
    ep   = dv.clk(1.0 / eLen);
    eN   = (+(float(dv.wrapped(ep)))) ~ _;
    eIdx = (ct.hash(dv.key + 7907 * int(eN)) & 65535) % 7;
    exam = examOn > 0.5;
    idx  = select2(exam, int(seqP), eIdx);
    col(L) = sum(k, 7, float(idx == k) * ba.take(k + 1, L));
    f0 = col(F0); fr1 = col(FR1); fr2 = col(FR2); g2 = col(G2);
    sP = col(SP); sD = col(SD); bP = col(BP); bD = col(BD);

    full(t, Pd, D, r) = select2(D > 0.999, dv.gate(t, 0.0, D * Pd, r), 1.0);
    g1  = full(dv.clk(1.0 / sP) * sP, sP, sD, 0.002);
    bt  = dv.clk(1.0 / bP) * bP;
    g2b = full(bt, bP, bD, 0.01);
    eOn = select2(exam, 1.0, min(1.0, max(0.0, (ep * eLen - 5.0) / 0.3)));
    sw  = min(1.0, dv.since(idx != idx') / 0.3);
    gt  = g1 * g2b * eOn * sw;
    pe  = 0.75 + 0.25 * abs(cos(ma.PI * bt / bP));

    // band-limited saw at the comb spacing (polyBLEP)
    ph  = dv.ton(f0);
    dt  = f0 * dv.P / ma.SR;
    u   = ph / dt;
    v   = (ph - 1.0) / dt;
    blep = select2(ph < dt, select2(ph > 1.0 - dt, 0.0, v * v + 2.0 * v + 1.0), 2.0 * u - u * u - 1.0);
    saw = 2.0 * ph - 1.0 - blep;
    k1  = 0.25 / reso;
    k2  = 0.35 / reso;
    grad = k1 * dv.bpf(fr1, k1, saw) + g2 * k2 * dv.bpf(fr2, k2, saw);
    gradient = gradLvl * 2.2 * gt * pe * grad;

    tc  = dv.clk(1.0 / 1.2) * 1.2;
    nC  = ct.noise(dv.nkey(10));
    chirp = exp(0.0 - tc / 0.35) * min(1.0, tc / 0.04)
          * (dv.bpf(240.0, 0.8, nC) + 0.4 * ct.sin2pi(dv.ton(250.0 + 150.0 * exp(0.0 - tc / 0.08))));
    phH = dv.ton(100.0);
    cold = coldLvl * (0.5 * chirp + 0.08 * (dv.harm(phH, 1) + 0.5 * dv.harm(phH, 2)));
};

process = mri * 0.6;
