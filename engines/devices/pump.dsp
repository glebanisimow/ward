// Infusion pump: stepper motor (harmonics of 175 Hz) in doses every ~6 s,
// optional alarm: two 1.09 s tones per 4.9 s, 4900 -> 4403 -> 3917 Hz.
declare name "pump";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

doseInt  = dv.dpar(hslider("[0]doseInterval [unit:s]", 6, 0.5, 30, 0.01));
doseLen  = dv.dpar(hslider("[1]doseLength [unit:s]", 0.45, 0.05, 5, 0.001));
motorHz  = dv.dpar(hslider("[2]motor [unit:Hz] [scale:log]", 175, 40, 800, 0.1)) : ct.lag(0.3);
motorLvl = dv.dpar(hslider("[3]motorLevel", 1, 0, 2, 0.001)) : ct.lag(0.3);
alarmOn  = dv.dpar(nentry("[4]alarm", 0, 0, 1, 1));
alarmLvl = dv.dpar(hslider("[5]alarmLevel", 1, 0, 2, 0.001)) : ct.lag(0.3);

pump = motor + alarm
with {
    dp  = dv.clk(1.0 / doseInt);
    td  = dp * doseInt;
    phS = dv.ton(motorHz);
    motor = motorLvl * 0.5 * dv.gate(td, 0.0, min(doseLen, 0.95 * doseInt), 0.02)
          * sum(h, 6, dv.harm(phS, h + 1) * dv.hgate(motorHz, h + 1) / float(h + 1));
    cyc = dv.clk(1.0 / 4.9);
    on  = ba.sAndH(dv.wrapped(cyc) | ct.isFirst, alarmOn);
    ta  = cyc * 4.9;
    env = (dv.gate(ta, 0.0, 1.09, 0.01) + dv.gate(ta, 1.35, 1.09, 0.01)) * on;
    tt  = ta - 1.35 * float(ta >= 1.35);
    fA  = select2(tt < 0.36, select2(tt < 0.72, 3917.0, 4403.0), 4900.0);
    phA = dv.ton(fA);
    alarm = alarmLvl * 0.4 * env * dv.hgate(fA, 1) * (dv.harm(phA, 1) + 0.2 * dv.harm(phA, 2) * dv.hgate(fA, 2));
};

process = pump * 0.6;
