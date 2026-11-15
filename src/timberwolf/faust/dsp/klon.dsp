// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\klon.dsp
import("stdfaust.lib");
process = ve.klonCentaur(hslider("Gain",50,0,100,0.1)/100 : si.smoo, hslider("Treble",50,0,100,0.1)/100 : si.smoo, hslider("Level",50,0,100,0.1)/100 : si.smoo);
