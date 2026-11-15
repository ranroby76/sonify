// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\ott.dsp
import("stdfaust.lib");
depth = hslider("Depth",100,0,100,0.1)/100 : si.smoo;
time = hslider("Time",100,0,1000,0.1)/100 : si.smoo;
ing = hslider("In Gain",0,-24,19,0.1) : si.smoo;
outg = hslider("Out Gain",0,-24,19,0.1) : si.smoo;
up = hslider("Upward",100,0,200,0.1)/100 : si.smoo;
dn = hslider("Downward",100,0,200,0.1)/100 : si.smoo;
process = co.xfer_ott(depth, time, ing, outg, up, dn, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
