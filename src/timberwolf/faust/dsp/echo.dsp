// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dsp\echo.dsp
import("stdfaust.lib");
t = hslider("Time",380,10,2500,0.1) * ma.SR / 1000 : si.smoo;
fb = hslider("Feedback",35,0,98,0.1)/100 : si.smoo;
tone = hslider("Tone",6000,500,16000,1) : si.smoo;
mix = hslider("Mix",30,0,100,0.1)/100 : si.smoo;
loop = (+ : de.fdelay(2*192000*3, t)) ~ (fi.lowpass(1, tone) : *(fb));
process = _ <: *(1-mix*0.5), (loop : *(mix)) :> _;
