// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\crybaby.dsp
import("stdfaust.lib");
process = ve.crybaby(hslider("Pedal",50,0,100,0.1)/100 : si.smoo) : *(hslider("Level",0,-18,12,0.1) : ba.db2linear : si.smoo);
