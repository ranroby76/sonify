// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\autowah.dsp
import("stdfaust.lib");
sens = hslider("Sensitivity",0,-24,24,0.1) : ba.db2linear : si.smoo;
process = *(sens) : ve.autowah(hslider("Amount",80,0,100,0.1)/100 : si.smoo) : /(sens) : *(hslider("Level",0,-18,12,0.1) : ba.db2linear : si.smoo);
