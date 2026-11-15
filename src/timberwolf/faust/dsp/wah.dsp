// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\wah.dsp
import("stdfaust.lib");
p = hslider("Pedal",50,0,100,0.1)/100 : si.smoo;
lo = hslider("Low",350,200,800,1);
hi = hslider("High",2200,1000,4000,1);
process = ve.wah4(lo * pow(hi/lo, p)) : *(hslider("Level",0,-18,12,0.1) : ba.db2linear : si.smoo);
