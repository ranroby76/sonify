// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\phase90.dsp
import("stdfaust.lib");
process = ve.mxrPhase90(hslider("Rate",1.5,0.1,10,0.01) : si.smoo);
