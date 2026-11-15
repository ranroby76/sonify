// D:\workspace\Fanan_web_hub\src\timberwolf\faust\echo.h
// Written by the Faust compiler (2.70.3) from echo.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "echo"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Echo -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Echo_H__
#define  __Faust_Echo_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Echo
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


class Faust_Echo : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec2[2];
	FAUSTFLOAT fHslider2;
	float fVec2[2];
	float fRec5[2];
	float fConst2;
	float fRec3[2];
	int IOTA0;
	float fVec3[2097152];
	FAUSTFLOAT fHslider3;
	float fVec4[2];
	float fConst3;
	float fRec6[2];
	float fRec1[2];
	
 public:
	Faust_Echo() {}

	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Echo -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "echo.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass0_highpass1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass0_highpass1:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/tf1s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "echo");
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
		fConst2 = 3.1415927f / fConst0;
		fConst3 = 0.001f * fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(3e+01f);
		fHslider1 = FAUSTFLOAT(35.0f);
		fHslider2 = FAUSTFLOAT(6e+03f);
		fHslider3 = FAUSTFLOAT(3.8e+02f);
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
			fRec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec5[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec3[l6] = 0.0f;
		}
		IOTA0 = 0;
		for (int l7 = 0; l7 < 2097152; l7 = l7 + 1) {
			fVec3[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fVec4[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec6[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec1[l10] = 0.0f;
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
	
	virtual Faust_Echo* clone() {
		return new Faust_Echo();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("echo");
		ui_interface->addHorizontalSlider("Feedback", &fHslider1, FAUSTFLOAT(35.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(98.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mix", &fHslider0, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Time", &fHslider3, FAUSTFLOAT(3.8e+02f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(2.5e+03f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Tone", &fHslider2, FAUSTFLOAT(6e+03f), FAUSTFLOAT(5e+02f), FAUSTFLOAT(1.6e+04f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.01f * fSlow0;
		float fSlow2 = float(fHslider1);
		float fSlow3 = 0.01f * fSlow2;
		float fSlow4 = float(fHslider2);
		float fSlow5 = float(fHslider3);
		float fSlow6 = fConst3 * fSlow5;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec0[1]))));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow1 + fRec0[0];
			fVec1[0] = fSlow2;
			float fTemp2 = fConst1 * (fRec2[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec1[1]))));
			fRec2[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			fVec2[0] = fSlow4;
			float fTemp3 = fConst1 * (std::min<float>(3.4028235e+38f, fVec2[1] - fSlow4) + fRec5[1]);
			fRec5[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = std::tan(std::max<float>(0.0f, std::min<float>(fConst2 * (fSlow4 + fRec5[0]), 1.5676547f)));
			float fTemp5 = fTemp4 * (fRec1[1] - fRec3[1]) * std::min<float>(3.4028235e+38f, 1.0f / (fTemp4 + 1.0f));
			float fTemp6 = fRec3[1] + 2.0f * fTemp5;
			fRec3[0] = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = fRec3[1] + fTemp5;
			float fRec4 = ((std::fabs(fTemp7) > 1.1754944e-38f) ? fTemp7 : 0.0f);
			float fTemp8 = float(input0[i0]);
			float fTemp9 = fTemp8 + fRec4 * (fSlow3 + fRec2[0]);
			fVec3[IOTA0 & 2097151] = fTemp9;
			fVec4[0] = fSlow5;
			float fTemp10 = fConst1 * (fRec6[1] + std::min<float>(3.4028235e+38f, -(fConst3 * (fSlow5 - fVec4[1]))));
			fRec6[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = fSlow6 + fRec6[0];
			int iTemp12 = int(std::min<float>(1152001.0f, std::max<float>(-1.0f, fTemp11)));
			float fTemp13 = std::floor(fTemp11);
			float fTemp14 = fVec3[(IOTA0 - std::min<int>(1152001, std::max<int>(0, iTemp12))) & 2097151] * (fTemp13 + (1.0f - fRec6[0]) - fSlow6) + (fSlow6 + (fRec6[0] - fTemp13)) * fVec3[(IOTA0 - std::min<int>(1152001, std::max<int>(0, iTemp12 + 1))) & 2097151];
			fRec1[0] = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			output0[i0] = FAUSTFLOAT(fTemp8 * (1.0f - 0.5f * fTemp1) + fRec1[0] * fTemp1);
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec2[1] = fRec2[0];
			fVec2[1] = fVec2[0];
			fRec5[1] = fRec5[0];
			fRec3[1] = fRec3[0];
			IOTA0 = IOTA0 + 1;
			fVec4[1] = fVec4[0];
			fRec6[1] = fRec6[0];
			fRec1[1] = fRec1[0];
		}
	}

};

#endif
