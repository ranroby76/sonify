// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\univibe.dsp
import("stdfaust.lib");
rate = hslider("Speed",3,0.1,12,0.01) : si.smoo;
mode = hslider("Mode",0,0,1,1) : si.smoo;
intens = hslider("Intensity",70,0,100,0.1)/100 : si.smoo;
fb = hslider("Feedback",20,0,90,0.1)/100 : si.smoo;
lvl = hslider("Level",0,-18,12,0.1) : ba.db2linear : si.smoo;
vib = pf.vibrato2_mono(4, 1, fb, 800, 120 + (1-intens)*600, 1.8, 900 + intens*3000, rate);
process = _ <: (vib : *(1 - mode*0.5)), *(mode*0.5) :> *(lvl);
