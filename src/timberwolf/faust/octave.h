// D:\workspace\Fanan_web_hub\src\timberwolf\faust\octave.h
// Written by the Faust compiler (2.70.3) from octave.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "octave"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Octave -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Octave_H__
#define  __Faust_Octave_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Octave
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


class Faust_Octave : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst0;
	float fRec0[2];
	float fRec1[2];
	int IOTA0;
	float fVec1[131072];
	FAUSTFLOAT fHslider1;
	float fVec2[2];
	float fRec2[2];
	float fRec3[2];
	FAUSTFLOAT fHslider2;
	float fVec3[2];
	float fRec4[2];
	float fRec5[2];
	FAUSTFLOAT fHslider3;
	float fVec4[2];
	float fRec6[2];
	
 public:
	Faust_Octave() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Octave -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "octave.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/version", "2.7.4");
		m->declare("name", "octave");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.8.2");
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
		fConst0 = 1.0f - 44.1f / std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(-6e+01f);
		fHslider1 = FAUSTFLOAT(-6e+01f);
		fHslider2 = FAUSTFLOAT(-6.0f);
		fHslider3 = FAUSTFLOAT(0.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec0[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec1[l2] = 0.0f;
		}
		IOTA0 = 0;
		for (int l3 = 0; l3 < 131072; l3 = l3 + 1) {
			fVec1[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec2[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fVec3[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec4[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec5[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fVec4[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec6[l11] = 0.0f;
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
	
	virtual Faust_Octave* clone() {
		return new Faust_Octave();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("octave");
		ui_interface->addHorizontalSlider("Down 1", &fHslider2, FAUSTFLOAT(-6.0f), FAUSTFLOAT(-6e+01f), FAUSTFLOAT(6.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Down 2", &fHslider1, FAUSTFLOAT(-6e+01f), FAUSTFLOAT(-6e+01f), FAUSTFLOAT(6.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Dry", &fHslider3, FAUSTFLOAT(0.0f), FAUSTFLOAT(-6e+01f), FAUSTFLOAT(6.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Up 1", &fHslider0, FAUSTFLOAT(-6e+01f), FAUSTFLOAT(-6e+01f), FAUSTFLOAT(6.0f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = std::pow(1e+01f, 0.05f * float(fHslider0));
		float fSlow1 = std::pow(1e+01f, 0.05f * float(fHslider1));
		float fSlow2 = std::pow(1e+01f, 0.05f * float(fHslider2));
		float fSlow3 = std::pow(1e+01f, 0.05f * float(fHslider3));
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst0 * (fRec0[1] + std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = std::fmod(fRec1[1] + 2047.0f, 2048.0f);
			fRec1[0] = ((std::fabs(fTemp1) > 1.1754944e-38f) ? fTemp1 : 0.0f);
			float fTemp2 = std::min<float>(0.001953125f * fRec1[0], 1.0f);
			float fTemp3 = fRec1[0] + 2048.0f;
			float fTemp4 = std::floor(fTemp3);
			float fTemp5 = float(input0[i0]);
			fVec1[IOTA0 & 131071] = fTemp5;
			int iTemp6 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp3)));
			int iTemp7 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fRec1[0])));
			float fTemp8 = std::floor(fRec1[0]);
			fVec2[0] = fSlow1;
			float fTemp9 = fConst0 * (fRec2[1] + std::min<float>(3.4028235e+38f, fVec2[1] - fSlow1));
			fRec2[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			float fTemp10 = std::fmod(fRec3[1] + 2048.75f, 2048.0f);
			fRec3[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = std::min<float>(0.001953125f * fRec3[0], 1.0f);
			float fTemp12 = fRec3[0] + 2048.0f;
			float fTemp13 = std::floor(fTemp12);
			int iTemp14 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp12)));
			int iTemp15 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fRec3[0])));
			float fTemp16 = std::floor(fRec3[0]);
			fVec3[0] = fSlow2;
			float fTemp17 = fConst0 * (fRec4[1] + std::min<float>(3.4028235e+38f, fVec3[1] - fSlow2));
			fRec4[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = std::fmod(fRec5[1] + 2048.5f, 2048.0f);
			fRec5[0] = ((std::fabs(fTemp18) > 1.1754944e-38f) ? fTemp18 : 0.0f);
			float fTemp19 = std::min<float>(0.001953125f * fRec5[0], 1.0f);
			float fTemp20 = fRec5[0] + 2048.0f;
			float fTemp21 = std::floor(fTemp20);
			int iTemp22 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp20)));
			int iTemp23 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fRec5[0])));
			float fTemp24 = std::floor(fRec5[0]);
			fVec4[0] = fSlow3;
			float fTemp25 = fConst0 * (fRec6[1] + std::min<float>(3.4028235e+38f, fVec4[1] - fSlow3));
			fRec6[0] = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			output0[i0] = FAUSTFLOAT(fTemp5 * (fSlow3 + fRec6[0]) + ((fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp23))) & 131071] * (fTemp24 + (1.0f - fRec5[0])) + (fRec5[0] - fTemp24) * fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp23 + 1))) & 131071]) * fTemp19 + (fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp22))) & 131071] * (fTemp21 + (-2047.0f - fRec5[0])) + fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp22 + 1))) & 131071] * (fRec5[0] + (2048.0f - fTemp21))) * (1.0f - fTemp19)) * (fSlow2 + fRec4[0]) + ((fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp15))) & 131071] * (fTemp16 + (1.0f - fRec3[0])) + (fRec3[0] - fTemp16) * fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp15 + 1))) & 131071]) * fTemp11 + (fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp14))) & 131071] * (fTemp13 + (-2047.0f - fRec3[0])) + fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp14 + 1))) & 131071] * (fRec3[0] + (2048.0f - fTemp13))) * (1.0f - fTemp11)) * (fSlow1 + fRec2[0]) + ((fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp7))) & 131071] * (fTemp8 + (1.0f - fRec1[0])) + (fRec1[0] - fTemp8) * fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp7 + 1))) & 131071]) * fTemp2 + (fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp6))) & 131071] * (fTemp4 + (-2047.0f - fRec1[0])) + fVec1[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp6 + 1))) & 131071] * (fRec1[0] + (2048.0f - fTemp4))) * (1.0f - fTemp2)) * (fSlow0 + fRec0[0]));
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fRec1[1] = fRec1[0];
			IOTA0 = IOTA0 + 1;
			fVec2[1] = fVec2[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fVec3[1] = fVec3[0];
			fRec4[1] = fRec4[0];
			fRec5[1] = fRec5[0];
			fVec4[1] = fVec4[0];
			fRec6[1] = fRec6[0];
		}
	}

};

#endif
