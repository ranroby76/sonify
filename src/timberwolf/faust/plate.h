// D:\workspace\Fanan_web_hub\src\timberwolf\faust\plate.h
// Written by the Faust compiler (2.70.3) from plate.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "plate"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Plate -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Plate_H__
#define  __Faust_Plate_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Plate
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif


class Faust_Plate : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec5[2];
	FAUSTFLOAT fHslider2;
	float fVec2[2];
	float fRec6[2];
	FAUSTFLOAT fHslider3;
	float fVec3[2];
	float fRec8[2];
	int IOTA0;
	float fVec4[1024];
	float fRec9[2];
	float fVec5[8192];
	float fRec7[2];
	float fVec6[4096];
	float fRec3[2];
	float fVec7[4096];
	float fVec8[65536];
	FAUSTFLOAT fHslider4;
	float fVec9[2];
	float fConst2;
	float fRec20[2];
	float fVec10[65536];
	FAUSTFLOAT fHslider5;
	float fVec11[2];
	float fRec21[2];
	float fRec19[3];
	float fVec12[256];
	float fRec17[2];
	float fVec13[128];
	float fRec15[2];
	float fVec14[512];
	float fRec13[2];
	float fVec15[512];
	float fRec11[2];
	float fRec1[2];
	float fVec16[1024];
	float fRec25[2];
	float fVec17[8192];
	float fRec24[2];
	float fVec18[2048];
	float fRec22[2];
	float fVec19[2048];
	float fRec2[2];
	
 public:
	Faust_Plate() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Plate -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "plate.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "plate");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("reverbs.lib/dattorro_rev:author", "Jakob Zerbian");
		m->declare("reverbs.lib/dattorro_rev:licence", "LicenseRef-STK-4.3");
		m->declare("reverbs.lib/name", "Faust Reverb Library");
		m->declare("reverbs.lib/version", "1.6.2");
		m->declare("routes.lib/name", "Faust Signal Routing Library");
		m->declare("routes.lib/version", "1.4.1");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.8.2");
	}

	virtual int getNumInputs() {
		return 2;
	}
	virtual int getNumOutputs() {
		return 2;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		float fConst0 = std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
		fConst1 = 1.0f - 44.1f / fConst0;
		fConst2 = 0.001f * fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(3e+01f);
		fHslider1 = FAUSTFLOAT(7e+01f);
		fHslider2 = FAUSTFLOAT(6e+01f);
		fHslider3 = FAUSTFLOAT(3e+01f);
		fHslider4 = FAUSTFLOAT(1e+01f);
		fHslider5 = FAUSTFLOAT(8e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec0[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fVec1[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec5[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec6[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec8[l7] = 0.0f;
		}
		IOTA0 = 0;
		for (int l8 = 0; l8 < 1024; l8 = l8 + 1) {
			fVec4[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec9[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 8192; l10 = l10 + 1) {
			fVec5[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec7[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 4096; l12 = l12 + 1) {
			fVec6[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec3[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 4096; l14 = l14 + 1) {
			fVec7[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 65536; l15 = l15 + 1) {
			fVec8[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fVec9[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec20[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 65536; l18 = l18 + 1) {
			fVec10[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fVec11[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec21[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 3; l21 = l21 + 1) {
			fRec19[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 256; l22 = l22 + 1) {
			fVec12[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec17[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 128; l24 = l24 + 1) {
			fVec13[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec15[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 512; l26 = l26 + 1) {
			fVec14[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec13[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 512; l28 = l28 + 1) {
			fVec15[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec11[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2; l30 = l30 + 1) {
			fRec1[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 1024; l31 = l31 + 1) {
			fVec16[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fRec25[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 8192; l33 = l33 + 1) {
			fVec17[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			fRec24[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 2048; l35 = l35 + 1) {
			fVec18[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			fRec22[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2048; l37 = l37 + 1) {
			fVec19[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec2[l38] = 0.0f;
		}
	}
	
	virtual void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual Faust_Plate* clone() {
		return new Faust_Plate();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("plate");
		ui_interface->addHorizontalSlider("Bandwidth", &fHslider5, FAUSTFLOAT(8e+01f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Damping", &fHslider3, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Decay", &fHslider2, FAUSTFLOAT(6e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(99.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Diffusion", &fHslider1, FAUSTFLOAT(7e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mix", &fHslider0, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Pre-delay", &fHslider4, FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2e+02f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.01f * fSlow0;
		float fSlow2 = float(fHslider1);
		float fSlow3 = 0.01f * fSlow2;
		float fSlow4 = float(fHslider2);
		float fSlow5 = 0.01f * fSlow4;
		float fSlow6 = float(fHslider3);
		float fSlow7 = 0.01f * fSlow6;
		float fSlow8 = float(fHslider4);
		float fSlow9 = fConst2 * fSlow8;
		float fSlow10 = float(fHslider5);
		float fSlow11 = 0.01f * fSlow10;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec0[1]))));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow1 + fRec0[0];
			fVec1[0] = fSlow2;
			float fTemp2 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec1[1]))) + fRec5[1]);
			fRec5[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = fSlow3 + fRec5[0];
			fVec2[0] = fSlow4;
			float fTemp4 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow4 - fVec2[1]))) + fRec6[1]);
			fRec6[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = fSlow5 + fRec6[0];
			fVec3[0] = fSlow6;
			float fTemp6 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow6 - fVec3[1]))) + fRec8[1]);
			fRec8[0] = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = 1.0f - fRec8[0] - fSlow7;
			float fTemp8 = 0.7f * fTemp3 * fRec9[1] + fRec2[1];
			fVec4[IOTA0 & 1023] = fTemp8;
			float fTemp9 = fVec4[(IOTA0 - 908) & 1023];
			fRec9[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			float fTemp10 = 0.7f * fTemp3 * fTemp8;
			float fRec10 = ((std::fabs(-fTemp10) > 1.1754944e-38f) ? -fTemp10 : 0.0f);
			fVec5[IOTA0 & 8191] = fRec10 + fRec9[1];
			float fTemp11 = fSlow7 + fRec8[0];
			float fTemp12 = fRec7[1] * fTemp11 + fVec5[(IOTA0 - 4217) & 8191] * fTemp7;
			fRec7[0] = ((std::fabs(fTemp12) > 1.1754944e-38f) ? fTemp12 : 0.0f);
			float fTemp13 = fRec7[0] * fTemp5 - 0.5f * fRec3[1] * fTemp3;
			fVec6[IOTA0 & 4095] = fTemp13;
			float fTemp14 = fVec6[(IOTA0 - 2656) & 4095];
			fRec3[0] = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			float fTemp15 = 0.5f * fTemp3 * fTemp13;
			float fRec4 = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			fVec7[IOTA0 & 4095] = fRec4 + fRec3[1];
			float fTemp16 = float(input1[i0]);
			fVec8[IOTA0 & 65535] = fTemp16;
			fVec9[0] = fSlow8;
			float fTemp17 = fConst1 * (fRec20[1] + std::min<float>(3.4028235e+38f, -(fConst2 * (fSlow8 - fVec9[1]))));
			fRec20[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fSlow9 + fRec20[0];
			int iTemp19 = int(std::min<float>(38405.0f, std::max<float>(-1.0f, fTemp18)));
			int iTemp20 = std::min<int>(38405, std::max<int>(0, iTemp19 + 1));
			float fTemp21 = float(input0[i0]);
			fVec10[IOTA0 & 65535] = fTemp21;
			float fTemp22 = std::floor(fTemp18);
			int iTemp23 = std::min<int>(38405, std::max<int>(0, iTemp19));
			fVec11[0] = fSlow10;
			float fTemp24 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow10 - fVec11[1]))) + fRec21[1]);
			fRec21[0] = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			float fTemp25 = fRec19[2] * (1.0f - fRec21[0] - fSlow11) + 0.5f * (fSlow11 + fRec21[0]) * ((fTemp22 + (1.0f - fRec20[0]) - fSlow9) * (fVec10[(IOTA0 - iTemp23) & 65535] + fVec8[(IOTA0 - iTemp23) & 65535]) + (fSlow9 + (fRec20[0] - fTemp22)) * (fVec10[(IOTA0 - iTemp20) & 65535] + fVec8[(IOTA0 - iTemp20) & 65535]));
			fRec19[0] = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			float fTemp26 = fRec19[0] - 0.75f * fTemp3 * fRec17[1];
			fVec12[IOTA0 & 255] = fTemp26;
			float fTemp27 = fVec12[(IOTA0 - 142) & 255];
			fRec17[0] = ((std::fabs(fTemp27) > 1.1754944e-38f) ? fTemp27 : 0.0f);
			float fTemp28 = 0.75f * fTemp3 * fTemp26;
			float fRec18 = ((std::fabs(fTemp28) > 1.1754944e-38f) ? fTemp28 : 0.0f);
			float fTemp29 = fRec18 + fRec17[1] - 0.75f * fTemp3 * fRec15[1];
			fVec13[IOTA0 & 127] = fTemp29;
			float fTemp30 = fVec13[(IOTA0 - 107) & 127];
			fRec15[0] = ((std::fabs(fTemp30) > 1.1754944e-38f) ? fTemp30 : 0.0f);
			float fTemp31 = 0.75f * fTemp3 * fTemp29;
			float fRec16 = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp32 = fRec16 + fRec15[1] - 0.625f * fTemp3 * fRec13[1];
			fVec14[IOTA0 & 511] = fTemp32;
			float fTemp33 = fVec14[(IOTA0 - 379) & 511];
			fRec13[0] = ((std::fabs(fTemp33) > 1.1754944e-38f) ? fTemp33 : 0.0f);
			float fTemp34 = 0.625f * fTemp3 * fTemp32;
			float fRec14 = ((std::fabs(fTemp34) > 1.1754944e-38f) ? fTemp34 : 0.0f);
			float fTemp35 = fRec14 + fRec13[1] - 0.625f * fTemp3 * fRec11[1];
			fVec15[IOTA0 & 511] = fTemp35;
			float fTemp36 = fVec15[(IOTA0 - 277) & 511];
			fRec11[0] = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fTemp37 = 0.625f * fTemp3 * fTemp35;
			float fRec12 = ((std::fabs(fTemp37) > 1.1754944e-38f) ? fTemp37 : 0.0f);
			float fTemp38 = fRec11[1] + fRec12 + fTemp5 * fVec7[(IOTA0 - 2656) & 4095];
			fRec1[0] = ((std::fabs(fTemp38) > 1.1754944e-38f) ? fTemp38 : 0.0f);
			float fTemp39 = 0.7f * fTemp3 * fRec25[1] + fRec1[1];
			fVec16[IOTA0 & 1023] = fTemp39;
			float fTemp40 = fVec16[(IOTA0 - 672) & 1023];
			fRec25[0] = ((std::fabs(fTemp40) > 1.1754944e-38f) ? fTemp40 : 0.0f);
			float fTemp41 = 0.7f * fTemp3 * fTemp39;
			float fRec26 = ((std::fabs(-fTemp41) > 1.1754944e-38f) ? -fTemp41 : 0.0f);
			fVec17[IOTA0 & 8191] = fRec26 + fRec25[1];
			float fTemp42 = fTemp11 * fRec24[1] + fTemp7 * fVec17[(IOTA0 - 4453) & 8191];
			fRec24[0] = ((std::fabs(fTemp42) > 1.1754944e-38f) ? fTemp42 : 0.0f);
			float fTemp43 = fRec24[0] * fTemp5 - 0.5f * fTemp3 * fRec22[1];
			fVec18[IOTA0 & 2047] = fTemp43;
			float fTemp44 = fVec18[(IOTA0 - 1800) & 2047];
			fRec22[0] = ((std::fabs(fTemp44) > 1.1754944e-38f) ? fTemp44 : 0.0f);
			float fTemp45 = 0.5f * fTemp3 * fTemp43;
			float fRec23 = ((std::fabs(fTemp45) > 1.1754944e-38f) ? fTemp45 : 0.0f);
			fVec19[IOTA0 & 2047] = fRec23 + fRec22[1];
			float fTemp46 = fRec12 + fRec11[1] + fTemp5 * fVec19[(IOTA0 - 1800) & 2047];
			fRec2[0] = ((std::fabs(fTemp46) > 1.1754944e-38f) ? fTemp46 : 0.0f);
			float fTemp47 = 1.0f - fRec0[0] - fSlow1;
			output0[i0] = FAUSTFLOAT(fTemp21 * fTemp47 + fRec1[0] * fTemp1);
			output1[i0] = FAUSTFLOAT(fTemp16 * fTemp47 + fRec2[0] * fTemp1);
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec5[1] = fRec5[0];
			fVec2[1] = fVec2[0];
			fRec6[1] = fRec6[0];
			fVec3[1] = fVec3[0];
			fRec8[1] = fRec8[0];
			IOTA0 = IOTA0 + 1;
			fRec9[1] = fRec9[0];
			fRec7[1] = fRec7[0];
			fRec3[1] = fRec3[0];
			fVec9[1] = fVec9[0];
			fRec20[1] = fRec20[0];
			fVec11[1] = fVec11[0];
			fRec21[1] = fRec21[0];
			fRec19[2] = fRec19[1];
			fRec19[1] = fRec19[0];
			fRec17[1] = fRec17[0];
			fRec15[1] = fRec15[0];
			fRec13[1] = fRec13[0];
			fRec11[1] = fRec11[0];
			fRec1[1] = fRec1[0];
			fRec25[1] = fRec25[0];
			fRec24[1] = fRec24[0];
			fRec22[1] = fRec22[0];
			fRec2[1] = fRec2[0];
		}
	}

};

#endif
