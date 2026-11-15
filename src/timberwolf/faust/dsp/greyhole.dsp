// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\greyhole.dsp
import("stdfaust.lib");
dt = hslider("Delay",0.2,0.001,1.45,0.001);
damp = hslider("Damp",0,0,99,0.1)/100 : si.smoo;
size = hslider("Size",1,0.5,3,0.01);
diff = hslider("Diffusion",50,0,99,0.1)/100 : si.smoo;
fb = hslider("Feedback",90,0,100,0.1)/100 : si.smoo;
mdep = hslider("Mod Depth",10,0,100,0.1)/100 : si.smoo;
mfrq = hslider("Mod Rate",2,0,10,0.01) : si.smoo;
mix = hslider("Mix",35,0,100,0.1)/100 : si.smoo;
rev = re.greyhole(dt, damp, size, diff, fb, mdep, mfrq);
process = _,_ <: (*(1-mix), *(1-mix)), (rev : *(mix), *(mix)) :> _,_;
