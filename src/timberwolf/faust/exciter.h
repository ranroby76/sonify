// D:\workspace\Fanan_web_hub\src\timberwolf\faust\exciter.h
// Written by the Faust compiler (2.70.3) from exciter.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "exciter"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Exciter -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Exciter_H__
#define  __Faust_Exciter_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

/* link with : "" */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Exciter
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


class Faust_Exciter : public FaustDsp {
	
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
	float fRec4[2];
	float fConst2;
	FAUSTFLOAT fHslider3;
	float fVec3[2];
	float fRec5[2];
	float fConst3;
	float fRec6[2];
	float fRec7[2];
	float fRec2[2];
	float fRec12[2];
	float fRec13[2];
	float fRec10[2];
	
 public:
	Faust_Exciter() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Exciter -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "exciter.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/highpass:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/highpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass0_highpass1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass0_highpass1:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/svf:author", "Oleg Nesterov");
		m->declare("filters.lib/svf:copyright", "Copyright (C) 2020 Oleg Nesterov <oleg@redhat.com>");
		m->declare("filters.lib/svf:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf1s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "exciter");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
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
		fConst2 = 1.5707964f / fConst0;
		fConst3 = 3.1415927f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(3e+01f);
		fHslider2 = FAUSTFLOAT(3e+03f);
		fHslider3 = FAUSTFLOAT(12.0f);
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
			fRec4[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec5[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec6[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec7[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec2[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec12[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec13[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec10[l13] = 0.0f;
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
	
	virtual Faust_Exciter* clone() {
		return new Faust_Exciter();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("exciter");
		ui_interface->addHorizontalSlider("Amount", &fHslider1, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Drive", &fHslider3, FAUSTFLOAT(12.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Freq", &fHslider2, FAUSTFLOAT(3e+03f), FAUSTFLOAT(8e+02f), FAUSTFLOAT(1e+04f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Level", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-18.0f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = std::pow(1e+01f, 0.05f * float(fHslider0));
		float fSlow1 = float(fHslider1);
		float fSlow2 = 0.01f * fSlow1;
		float fSlow3 = float(fHslider2);
		float fSlow4 = std::pow(1e+01f, 0.05f * float(fHslider3));
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow0 + fRec0[0];
			fVec1[0] = fSlow1;
			float fTemp2 = fConst1 * (fRec1[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow1 - fVec1[1]))));
			fRec1[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = fSlow2 + fRec1[0];
			fVec2[0] = fSlow3;
			float fTemp4 = fConst1 * (std::min<float>(3.4028235e+38f, fVec2[1] - fSlow3) + fRec4[1]);
			fRec4[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = fSlow3 + fRec4[0];
			float fTemp6 = std::tan(std::max<float>(0.0f, std::min<float>(fConst2 * fTemp5, 1.5676547f)));
			float fTemp7 = std::min<float>(3.4028235e+38f, 1.0f / (fTemp6 + 1.0f));
			fVec3[0] = fSlow4;
			float fTemp8 = fConst1 * (fRec5[1] + std::min<float>(3.4028235e+38f, fVec3[1] - fSlow4));
			fRec5[0] = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			float fTemp9 = fSlow4 + fRec5[0];
			float fTemp10 = std::tan(std::max<float>(0.0f, std::min<float>(fConst3 * fTemp5, 1.5676547f)));
			float fTemp11 = fTemp10 + 1.4142135f;
			float fTemp12 = std::min<float>(3.4028235e+38f, 1.0f / (fTemp10 * fTemp11 + 1.0f));
			float fTemp13 = float(input0[i0]);
			float fTemp14 = fTemp10 * (fTemp13 - (fTemp11 * fRec6[1] + fRec7[1])) * fTemp12;
			float fTemp15 = fRec6[1] + 2.0f * fTemp14;
			fRec6[0] = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = fRec6[1] + fTemp14;
			float fTemp17 = fTemp10 * fTemp16;
			float fTemp18 = fRec7[1] + 2.0f * fTemp17;
			fRec7[0] = ((std::fabs(fTemp18) > 1.1754944e-38f) ? fTemp18 : 0.0f);
			float fRec8 = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			float fTemp19 = fRec7[1] + fTemp17;
			float fRec9 = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = tanhf((fTemp13 - (fRec9 + 1.4142135f * fRec8)) * fTemp9) - fRec2[1];
			float fTemp21 = fRec2[1] + 2.0f * fTemp6 * fTemp20 * fTemp7;
			fRec2[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fTemp20 * fTemp7;
			float fRec3 = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			output0[i0] = FAUSTFLOAT((fTemp13 + 0.5f * fRec3 * fTemp3) * fTemp1);
			float fTemp23 = float(input1[i0]);
			float fTemp24 = fTemp10 * fTemp12 * (fTemp23 - (fTemp11 * fRec12[1] + fRec13[1]));
			float fTemp25 = fRec12[1] + 2.0f * fTemp24;
			fRec12[0] = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			float fTemp26 = fRec12[1] + fTemp24;
			float fTemp27 = fTemp10 * fTemp26;
			float fTemp28 = fRec13[1] + 2.0f * fTemp27;
			fRec13[0] = ((std::fabs(fTemp28) > 1.1754944e-38f) ? fTemp28 : 0.0f);
			float fRec14 = ((std::fabs(fTemp26) > 1.1754944e-38f) ? fTemp26 : 0.0f);
			float fTemp29 = fRec13[1] + fTemp27;
			float fRec15 = ((std::fabs(fTemp29) > 1.1754944e-38f) ? fTemp29 : 0.0f);
			float fTemp30 = tanhf(fTemp9 * (fTemp23 - (fRec15 + 1.4142135f * fRec14))) - fRec10[1];
			float fTemp31 = fRec10[1] + 2.0f * fTemp6 * fTemp7 * fTemp30;
			fRec10[0] = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp32 = fTemp7 * fTemp30;
			float fRec11 = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			output1[i0] = FAUSTFLOAT(fTemp1 * (fTemp23 + 0.5f * fRec11 * fTemp3));
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec1[1] = fRec1[0];
			fVec2[1] = fVec2[0];
			fRec4[1] = fRec4[0];
			fVec3[1] = fVec3[0];
			fRec5[1] = fRec5[0];
			fRec6[1] = fRec6[0];
			fRec7[1] = fRec7[0];
			fRec2[1] = fRec2[0];
			fRec12[1] = fRec12[0];
			fRec13[1] = fRec13[0];
			fRec10[1] = fRec10[0];
		}
	}

};

#endif
