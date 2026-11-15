// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\hall.dsp
import("stdfaust.lib");
pre = hslider("Pre-delay",40,0,200,1);
f1 = hslider("Low X",200,50,1000,1);
f2 = hslider("Damp",6000,1500,23000,1);
lo = hslider("Low Time",3,1,8,0.01);
mid = hslider("Mid Time",2,1,8,0.01);
mix = hslider("Mix",30,0,100,0.1)/100 : si.smoo;
rev = re.zita_rev1_stereo(pre, f1, f2, lo, mid, 192000);
process = _,_ <: (*(1-mix), *(1-mix)), (rev : *(mix), *(mix)) :> _,_;
