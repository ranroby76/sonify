// D:\workspace\Fanan_web_hub\src\timberwolf\faust\autowah.h
// Written by the Faust compiler (2.70.3) from autowah.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "autowah"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Autowah -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Autowah_H__
#define  __Faust_Autowah_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Autowah
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

static float Faust_Autowah_faustpower2_f(float value) {
	return value * value;
}

class Faust_Autowah : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fVec1[2];
	float fConst1;
	float fRec0[2];
	FAUSTFLOAT fHslider1;
	float fVec2[2];
	float fRec1[2];
	FAUSTFLOAT fHslider2;
	float fVec3[2];
	float fRec2[2];
	float fConst2;
	float fRec5[2];
	float fConst3;
	float fConst4;
	float fRec7[2];
	float fConst5;
	float fConst6;
	float fVec4[2];
	float fRec6[2];
	float fVec5[2];
	float fRec8[2];
	float fVec6[2];
	float fConst7;
	float fRec9[2];
	float fRec3[2];
	float fRec4[2];
	
 public:
	Faust_Autowah() {}

	void metadata(Meta* m) { 
		m->declare("analyzers.lib/name", "Faust Analyzer Library");
		m->declare("analyzers.lib/version", "1.6.3");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Autowah -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "autowah.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "autowah");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.8.2");
		m->declare("vaeffects.lib/name", "Faust Virtual Analog Filter Effect Library");
		m->declare("vaeffects.lib/version", "1.7.2");
	}

	virtual int getNumInputs() {
		return 1;
	}
	virtual int getNumOutputs() {
		return 1;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		float fConst0 = std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
		fConst1 = 1.0f - 44.1f / fConst0;
		fConst2 = std::pow(0.999f, 4.41e+04f / fConst0);
		fConst3 = std::exp(-(1e+01f / fConst0));
		fConst4 = 1.0f - fConst3;
		fConst5 = 1413.7167f / fConst0;
		fConst6 = 1998594.9f / Faust_Autowah_faustpower2_f(fConst0);
		fConst7 = 4.41e+03f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(0.0f);
		fHslider2 = FAUSTFLOAT(8e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fVec1[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec0[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fVec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec1[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fVec3[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec2[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec5[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec7[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fVec4[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec6[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fVec5[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec8[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fVec6[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec9[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec3[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec4[l16] = 0.0f;
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
	
	virtual Faust_Autowah* clone() {
		return new Faust_Autowah();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("autowah");
		ui_interface->addHorizontalSlider("Amount", &fHslider2, FAUSTFLOAT(8e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Level", &fHslider1, FAUSTFLOAT(0.0f), FAUSTFLOAT(-18.0f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Sensitivity", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-24.0f), FAUSTFLOAT(24.0f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = std::pow(1e+01f, 0.05f * float(fHslider0));
		float fSlow1 = std::pow(1e+01f, 0.05f * float(fHslider1));
		float fSlow2 = float(fHslider2);
		float fSlow3 = 0.01f * fSlow2;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			fVec1[0] = 1.0f;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow0 + fRec0[0];
			fVec2[0] = fSlow1;
			float fTemp2 = fConst1 * (fRec1[1] + std::min<float>(3.4028235e+38f, fVec2[1] - fSlow1));
			fRec1[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			fVec3[0] = fSlow2;
			float fTemp3 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec3[1]))) + fRec2[1]);
			fRec2[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = float(input0[i0]) * fTemp1;
			float fTemp5 = fConst2 * (fRec5[1] + (1.0f - fVec1[1]));
			fRec5[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			float fTemp6 = fRec5[0] * float(std::fabs(fRec5[0]) > 1.1754944e-38f);
			float fTemp7 = std::fabs(fTemp4);
			float fTemp8 = std::max<float>(fTemp7, fConst3 * fRec7[1] + fConst4 * fTemp7);
			fRec7[0] = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			float fTemp9 = std::max<float>(0.0f, std::min<float>(1.0f, fRec7[0]));
			float fTemp10 = std::pow(2.0f, 2.3f * fTemp9);
			float fTemp11 = std::pow(2.0f, 2.0f * (1.0f - fTemp9) + 1.0f);
			float fTemp12 = fConst5 * (fTemp10 / fTemp11);
			float fTemp13 = 4.0f * (1.0f - fTemp12) * Faust_Autowah_faustpower2_f(std::sin(fConst5 * fTemp10));
			float fTemp14 = fConst6 * (Faust_Autowah_faustpower2_f(fTemp10) / Faust_Autowah_faustpower2_f(fTemp11));
			float fTemp15 = fTemp14 + fTemp13;
			fVec4[0] = fTemp15;
			float fTemp16 = fConst2 * (fRec6[1] + std::min<float>(3.4028235e+38f, fVec4[1] - fTemp15));
			fRec6[0] = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			float fTemp17 = fTemp13 + fTemp14 + fRec6[0] + fTemp6;
			float fTemp18 = fTemp10 * (2.0f - fTemp12) / fTemp11;
			fVec5[0] = fTemp18;
			float fTemp19 = fConst2 * (fRec8[1] + std::min<float>(3.4028235e+38f, -(fConst5 * (fTemp18 - fVec5[1]))));
			fRec8[0] = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = std::pow(4.0f, fTemp9);
			fVec6[0] = fTemp20;
			float fTemp21 = fConst2 * (fRec9[1] + std::min<float>(3.4028235e+38f, -(fConst7 * (fTemp20 - fVec6[1]))));
			fRec9[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fTemp4 * (fRec9[0] + fConst7 * fTemp20) + fRec4[1] * (1.0f - (fConst5 * fTemp18 + fRec8[0] + fTemp6));
			float fTemp23 = fTemp22 + fRec3[1] * (1.0f - fTemp17);
			fRec3[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fTemp24 = fTemp22 - fRec3[1] * fTemp17;
			fRec4[0] = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			output0[i0] = FAUSTFLOAT((fRec4[0] * (fSlow3 + fRec2[0]) + fTemp4 * (1.0f - fRec2[0] - fSlow3)) * (fSlow1 + fRec1[0]) / fTemp1);
			fVec0[1] = fVec0[0];
			fVec1[1] = fVec1[0];
			fRec0[1] = fRec0[0];
			fVec2[1] = fVec2[0];
			fRec1[1] = fRec1[0];
			fVec3[1] = fVec3[0];
			fRec2[1] = fRec2[0];
			fRec5[1] = fRec5[0];
			fRec7[1] = fRec7[0];
			fVec4[1] = fVec4[0];
			fRec6[1] = fRec6[0];
			fVec5[1] = fVec5[0];
			fRec8[1] = fRec8[0];
			fVec6[1] = fVec6[0];
			fRec9[1] = fRec9[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
		}
	}

};

#endif
