// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\phaser.dsp
import("stdfaust.lib");
rate = hslider("Rate",0.5,0.02,10,0.01) : si.smoo;
depth = hslider("Depth",100,0,100,0.1)/100 : si.smoo;
fb = hslider("Feedback",50,0,95,0.1)/100 : si.smoo;
lo = hslider("Low",200,50,2000,1) : si.smoo;
hi = hslider("High",3000,800,10000,1) : si.smoo;
width = hslider("Width",1000,50,3000,1) : si.smoo;
inv = hslider("Invert",0,0,1,1);
process = pf.phaser2_stereo(4, width, lo, 1.5, hi, rate, depth, fb, inv);
