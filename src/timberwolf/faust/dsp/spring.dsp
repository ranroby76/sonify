// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\spring.dsp
import("stdfaust.lib");
process = re.springreverb(hslider("Dwell",50,0,100,0.1)/100 : si.smoo, hslider("Mix",50,0,100,0.1)/100 : si.smoo, hslider("Tone",50,0,100,0.1)/100 : si.smoo, hslider("Tension",50,0,100,0.1)/100 : si.smoo, 1);
