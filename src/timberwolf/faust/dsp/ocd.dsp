// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\ocd.dsp
import("stdfaust.lib");
process = ve.fulltoneOCD(hslider("Drive",50,0,100,0.1)/100 : si.smoo, hslider("Tone",50,0,100,0.1)/100 : si.smoo, hslider("Volume",50,0,100,0.1)/100 : si.smoo, hslider("Peak",1,0,1,1) : si.smoo);
