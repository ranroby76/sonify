// D:\workspace\Fanan_web_hub\src\faust\greyhole.dsp
// Space Lifter 4's Greyhole voicing: Julian Parker's greyhole (reverbs.lib, MIT), wet only,
// its controls plain (no smoothing: SonifySpace smooths what it sets).
import("stdfaust.lib");
dt   = hslider("Delay", 0.2, 0.001, 1.45, 0.001);
damp = hslider("Damp", 0, 0, 99, 0.1) / 100;
size = hslider("Size", 1, 0.5, 3, 0.01);
diff = hslider("Diffusion", 50, 0, 99, 0.1) / 100;
fb   = hslider("Feedback", 90, 0, 100, 0.1) / 100;
mdep = hslider("Mod Depth", 10, 0, 100, 0.1) / 100;
mfrq = hslider("Mod Rate", 2, 0, 10, 0.01);
process = re.greyhole(dt, damp, size, diff, fb, mdep, mfrq);
