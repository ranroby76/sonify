// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\dimension.dsp
import("stdfaust.lib");
process = ef.xferDimensionExpander(hslider("Size",50,0,100,0.1)/100 : si.smoo, hslider("Mix",50,0,100,0.1)/100 : si.smoo);
