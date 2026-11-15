// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\tapestop.dsp
import("stdfaust.lib");
stop = hslider("Stop",0,0,1,1);
t = hslider("Time",800,50,4000,1) * ma.SR / 1000;
curve = hslider("Curve",1,0.05,2,0.01);
fade = hslider("Fade",1,0.05,2,0.01);
process = ef.tapeStop(2, 3, 4*192000, 2048, fade, curve, t, stop);
