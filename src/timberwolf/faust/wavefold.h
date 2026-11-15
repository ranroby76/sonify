// D:\workspace\Fanan_web_hub\src\timberwolf\faust\wavefold.h
// Written by the Faust compiler (2.70.3) from wavefold.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "wavefold"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Wavefold -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Wavefold_H__
#define  __Faust_Wavefold_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Wavefold
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


class Faust_Wavefold : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec1[2];
	FAUSTFLOAT fHslider2;
	float fVec2[2];
	float fRec3[2];
	FAUSTFLOAT fHslider3;
	float fVec3[2];
	float fRec4[2];
	float fVec4[2];
	float fConst2;
	float fRec2[2];
	
 public:
	Faust_Wavefold() {}

	void metadata(Meta* m) { 
		m->declare("aanl.lib/clip:deprecated", "internal helper, renamed _clip");
		m->declare("aanl.lib/name", "Faust Antialiased Nonlinearities");
		m->declare("aanl.lib/version", "1.6.2");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Wavefold -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "wavefold.dsp");
		m->declare("filters.lib/dcblocker:author", "Julius O. Smith III");
		m->declare("filters.lib/dcblocker:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/dcblocker:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/pole:author", "Julius O. Smith III");
		m->declare("filters.lib/pole:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/pole:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("filters.lib/zero:author", "Julius O. Smith III");
		m->declare("filters.lib/zero:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/zero:license", "LicenseRef-STK-4.3");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/version", "2.7.4");
		m->declare("misceffects.lib/wavefold:author", "David Braun");
		m->declare("name", "wavefold");
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
		float fConst0 = std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
		fConst1 = 1.0f - 44.1f / fConst0;
		fConst2 = std::pow(0.995f, 4.41e+04f / fConst0);
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(-6.0f);
		fHslider1 = FAUSTFLOAT(1e+02f);
		fHslider2 = FAUSTFLOAT(12.0f);
		fHslider3 = FAUSTFLOAT(5e+01f);
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
			fRec1[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec3[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec4[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fVec4[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec2[l9] = 0.0f;
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
	
	virtual Faust_Wavefold* clone() {
		return new Faust_Wavefold();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("wavefold");
		ui_interface->addHorizontalSlider("Drive", &fHslider2, FAUSTFLOAT(12.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(36.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Level", &fHslider0, FAUSTFLOAT(-6.0f), FAUSTFLOAT(-24.0f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mix", &fHslider1, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Width", &fHslider3, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = std::pow(1e+01f, 0.05f * float(fHslider0));
		float fSlow1 = float(fHslider1);
		float fSlow2 = 0.01f * fSlow1;
		float fSlow3 = std::pow(1e+01f, 0.05f * float(fHslider2));
		float fSlow4 = float(fHslider3);
		float fSlow5 = 0.01f * fSlow4;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			fVec1[0] = fSlow1;
			float fTemp1 = fConst1 * (fRec1[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow1 - fVec1[1]))));
			fRec1[0] = ((std::fabs(fTemp1) > 1.1754944e-38f) ? fTemp1 : 0.0f);
			fVec2[0] = fSlow3;
			float fTemp2 = fConst1 * (fRec3[1] + std::min<float>(3.4028235e+38f, fVec2[1] - fSlow3));
			fRec3[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = float(input0[i0]);
			float fTemp4 = fTemp3 * (fSlow3 + fRec3[0]);
			int iTemp5 = fTemp4 < 0.0f;
			fVec3[0] = fSlow4;
			float fTemp6 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow4 - fVec3[1]))) + fRec4[1]);
			fRec4[0] = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = std::max<float>(0.0f, std::min<float>(1.0f, fSlow5 + fRec4[0]));
			float fTemp8 = 0.8f * fTemp7;
			float fTemp9 = 1.0f - fTemp8;
			float fTemp10 = ((iTemp5) ? -fTemp4 : fTemp4);
			float fTemp11 = (fTemp10 + fTemp8 + -1.0f) / std::max<float>(1.1920929e-07f, fTemp8);
			float fTemp12 = ((fTemp10 > fTemp9) ? fTemp7 * (0.4f * std::fabs(fTemp11 + (-0.5f - std::floor(fTemp11))) + -1.0f) + 1.0f : fTemp10) / fTemp9;
			float fTemp13 = ((iTemp5) ? -fTemp12 : fTemp12);
			fVec4[0] = fTemp13;
			float fTemp14 = fConst2 * fRec2[1] + fTemp13 - fVec4[1];
			fRec2[0] = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			output0[i0] = FAUSTFLOAT(fTemp3 * (1.0f - fRec1[0] - fSlow2) + fRec2[0] * (fSlow2 + fRec1[0]) * (fSlow0 + fRec0[0]));
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec1[1] = fRec1[0];
			fVec2[1] = fVec2[0];
			fRec3[1] = fRec3[0];
			fVec3[1] = fVec3[0];
			fRec4[1] = fRec4[0];
			fVec4[1] = fVec4[0];
			fRec2[1] = fRec2[0];
		}
	}

};

#endif
