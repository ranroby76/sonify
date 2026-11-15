// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\exciter.dsp
import("stdfaust.lib");
freq = hslider("Freq",3000,800,10000,1) : si.smoo;
drive = hslider("Drive",12,0,30,0.1) : ba.db2linear : si.smoo;
amt = hslider("Amount",30,0,100,0.1)/100 : si.smoo;
lvl = hslider("Level",0,-18,12,0.1) : ba.db2linear : si.smoo;
ex = _ <: _, (fi.highpass(2, freq) : *(drive) : ma.tanh : fi.highpass(1, freq*0.5) : *(amt * 0.5)) :> *(lvl);
process = ex, ex;
