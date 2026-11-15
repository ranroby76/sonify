// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\wavefold.dsp
import("stdfaust.lib");
drive = hslider("Drive",12,0,36,0.1) : ba.db2linear : si.smoo;
width = hslider("Width",50,0,100,0.1)/100 : si.smoo;
lvl = hslider("Level",-6,-24,12,0.1) : ba.db2linear : si.smoo;
mix = hslider("Mix",100,0,100,0.1)/100 : si.smoo;
fx = *(drive) : ef.wavefold(width) : fi.dcblocker : *(lvl);
process = _ <: *(1-mix), (fx : *(mix)) :> _;
