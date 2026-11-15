// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\moog.dsp
import("stdfaust.lib");
cut = hslider("Cutoff",1200,40,12000,1);
res = hslider("Resonance",30,0,100,0.1)/100;
rate = hslider("LFO Rate",0.5,0.02,10,0.01);
depth = hslider("LFO Depth",0,0,100,0.1)/100 * 3;
drive = hslider("Drive",0,0,24,0.1) : ba.db2linear : si.smoo;
mix = hslider("Mix",100,0,100,0.1)/100 : si.smoo;
f = cut * pow(2, depth * os.osc(rate)) : min(18000) : si.smoo;
nf = log10(max(f, 20) / 20) / 3 : min(1);
q = 0.707 + res * 9.3 : si.smoo;
fx = *(drive) : ve.moogLadder(nf, q) : /(sqrt(drive));
process = _ <: *(1-mix), (fx : *(mix)) :> _;
