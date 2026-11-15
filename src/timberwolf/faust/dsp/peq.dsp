// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\peq.dsp
import("stdfaust.lib");
lf = hslider("Low Freq",120,30,600,1);
lg = hslider("Low Gain",0,-15,15,0.1) : si.smoo;
mf = hslider("Mid Freq",800,150,6000,1) : si.smoo;
mg = hslider("Mid Gain",0,-15,15,0.1) : si.smoo;
mq = hslider("Mid Q",1,0.2,8,0.01) : si.smoo;
hf = hslider("High Freq",4000,1500,15000,1);
hg = hslider("High Gain",0,-15,15,0.1) : si.smoo;
lvl = hslider("Level",0,-18,12,0.1) : ba.db2linear : si.smoo;
eq = fi.low_shelf(lg, lf) : fi.peak_eq(mg, mf, mf/mq) : fi.high_shelf(hg, hf) : *(lvl);
process = eq, eq;
