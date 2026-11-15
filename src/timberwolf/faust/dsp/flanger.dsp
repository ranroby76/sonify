// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\flanger.dsp
import("stdfaust.lib");
rate = hslider("Rate",0.3,0.02,10,0.01);
depth = hslider("Depth",60,0,100,0.1)/100 : si.smoo;
man = hslider("Manual",1.5,0.1,10,0.01) : si.smoo;
fb = hslider("Feedback",50,-95,95,0.1)/100 : si.smoo;
mix = hslider("Mix",100,0,100,0.1)/100 : si.smoo;
spread = hslider("Spread",90,0,180,1)/360;
inv = hslider("Invert",0,0,1,1);
ms = ma.SR/1000;
d1 = (man + depth*4*(1 + os.oscp(rate, 0))/2) * ms;
d2 = (man + depth*4*(1 + os.oscp(rate, spread*2*ma.PI))/2) * ms;
process = pf.flanger_stereo(2048, d1, d2, mix, fb, inv);
