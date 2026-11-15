// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\room.dsp
import("stdfaust.lib");
size = hslider("Size",60,0,100,0.1)/100 * 0.28 + 0.7 : si.smoo;
damp = hslider("Damp",40,0,100,0.1)/100 : si.smoo;
mix = hslider("Mix",25,0,100,0.1)/100 : si.smoo;
rev = re.stereo_freeverb(size, 0.5, damp, 23) : *(0.12), *(0.12);
process = _,_ <: (*(1-mix), *(1-mix)), (rev : *(mix), *(mix)) :> _,_;
