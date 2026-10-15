// Room tone: mains hum (2nd..4th harmonics) and filtered air noise.
declare name "room";
declare version "0.1";
declare license "MIT";
import("stdfaust.lib");
ct = library("contract.lib");
dv = library("devices.lib");

mains  = dv.dpar(nentry("[0]mains [unit:Hz]", 50, 50, 60, 10)) : ct.lag(1);
humLvl = dv.dpar(hslider("[1]hum", 0.3, 0, 2, 0.001)) : ct.lag(0.3);
airLvl = dv.dpar(hslider("[2]air", 1, 0, 2, 0.001)) : ct.lag(0.3);
airTone = dv.dpar(hslider("[3]airTone [unit:Hz] [scale:log]", 350, 60, 4000, 0.1)) : ct.lag(0.3);
breathe = dv.dpar(hslider("[4]breathe", 0.2, 0, 1, 0.001)) : ct.lag(0.3);

room = hum + air
with {
    ph  = dv.ton(mains);
    hum = humLvl * (dv.harm(ph, 2) + 0.4 * dv.harm(ph, 3) + 0.25 * dv.harm(ph, 4));
    sw  = 1.0 - breathe * (0.5 + 0.5 * ct.sin2pi(dv.clk(1.0 / 11.0)));   // slow HVAC swell
    air = airLvl * sw * 3.0 * ct.noise(dv.nkey(4)) : fi.lowpass(2, dv.hz(airTone));
};

process = room * 0.25;
