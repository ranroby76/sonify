// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\tilt.dsp
import("stdfaust.lib");
a = hslider("Tilt",0,-100,100,0.1)/100 * 0.6 : si.smoo;
lvl = hslider("Level",0,-18,12,0.1) : ba.db2linear : si.smoo;
t = fi.spectral_tilt(5, 50, 12000, a) : *(lvl);
process = t, t;
