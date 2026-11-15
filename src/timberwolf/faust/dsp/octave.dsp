// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\octave.dsp
import("stdfaust.lib");
dry = hslider("Dry",0,-60,6,0.1) : ba.db2linear : si.smoo;
d1 = hslider("Down 1",-6,-60,6,0.1) : ba.db2linear : si.smoo;
d2 = hslider("Down 2",-60,-60,6,0.1) : ba.db2linear : si.smoo;
u1 = hslider("Up 1",-60,-60,6,0.1) : ba.db2linear : si.smoo;
w = 2048; x = 512;
process = _ <: *(dry), (ef.transpose(w, x, -12) : *(d1)), (ef.transpose(w, x, -24) : *(d2)), (ef.transpose(w, x, 12) : *(u1)) :> _;
