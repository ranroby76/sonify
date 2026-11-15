// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\jpverb.dsp
import("stdfaust.lib");
t60 = hslider("Decay",3,0.1,30,0.01);
size = hslider("Size",1,0.5,3,0.01);
damp = hslider("Damp",0,0,99,0.1)/100 : si.smoo;
diff = hslider("Diffusion",70,0,99,0.1)/100 : si.smoo;
mdep = hslider("Mod Depth",10,0,100,0.1)/100 : si.smoo;
mfrq = hslider("Mod Rate",2,0,10,0.01) : si.smoo;
low = hslider("Low",1,0.1,2,0.01) : si.smoo;
high = hslider("High",1,0.1,2,0.01) : si.smoo;
mix = hslider("Mix",30,0,100,0.1)/100 : si.smoo;
rev = re.jpverb(t60, damp, size, diff, mdep, mfrq, low, 1, high, 500, 2000);
process = _,_ <: (*(1-mix), *(1-mix)), (rev : *(mix), *(mix)) :> _,_;
