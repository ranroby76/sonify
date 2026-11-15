// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\granular.dsp
import("stdfaust.lib");
dur = hslider("Grain",80,10,400,1)/1000;
pitch = hslider("Pitch",0,-24,24,0.1) : si.smoo;
pos = hslider("Position",120,10,1500,1)/1000 : si.smoo;
jit = hslider("Jitter",30,0,500,1)/1000;
mix = hslider("Mix",50,0,100,0.1)/100 : si.smoo;
gr = ef.granular(8, dur, pow(2, pitch/12), pos, jit);
process = _ <: *(1-mix), (gr : *(mix)) :> _;
