// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\plate.dsp
import("stdfaust.lib");
pre = hslider("Pre-delay",10,0,200,1) * ma.SR / 1000 : si.smoo;
bw = hslider("Bandwidth",80,10,100,0.1)/100 : si.smoo;
decay = hslider("Decay",60,0,99,0.1)/100 : si.smoo;
damp = hslider("Damping",30,0,100,0.1)/100 : si.smoo;
diff = hslider("Diffusion",70,0,100,0.1)/100 : si.smoo;
mix = hslider("Mix",30,0,100,0.1)/100 : si.smoo;
pd = de.fdelay(192000/5 + 4, pre);
rev = par(i,2,pd) : re.dattorro_rev(0, bw, diff*0.75, diff*0.625, decay, diff*0.7, diff*0.5, damp);
process = _,_ <: (*(1-mix), *(1-mix)), (rev : *(mix), *(mix)) :> _,_;
