// D:\workspace\Fanan_web_hub\src\timberwolf\faust\phase90.h
// Written by the Faust compiler (2.70.3) from phase90.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "phase90"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Phase90 -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Phase90_H__
#define  __Faust_Phase90_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Phase90
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

static float Faust_Phase90_faustpower2_f(float value) {
	return value * value;
}

class Faust_Phase90 : public FaustDsp {
	
 private:
	
	int iVec0[2];
	FAUSTFLOAT fHslider0;
	float fVec1[2];
	int fSampleRate;
	float fConst1;
	float fRec7[2];
	float fConst2;
	int iRec5[2];
	int iRec6[2];
	float fConst3;
	float fConst6;
	float fRec17[2];
	float fConst7;
	float fRec14[2];
	float fRec15[2];
	float fRec11[2];
	float fRec12[2];
	float fRec8[2];
	float fRec9[2];
	float fRec2[2];
	float fRec3[2];
	float fConst9;
	float fConst10;
	float fRec0[2];
	
 public:
	Faust_Phase90() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Phase90 -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "phase90.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/highpass:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/highpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass0_highpass1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass0_highpass1:license", "LicenseRef-STK-4.3");
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
		m->declare("name", "phase90");
		m->declare("oscillators.lib/lf_sawpos:author", "Bart Brouns, revised by Stéphane Letz");
		m->declare("oscillators.lib/lf_sawpos:licence", "LicenseRef-STK-4.3");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.9.6");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.8.2");
		m->declare("vaeffects.lib/mxrPhase90:author", "David Braun");
		m->declare("vaeffects.lib/mxrPhase90:license", "MIT");
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
		fConst2 = 1.0f / fConst0;
		fConst3 = 10638298.0f / fConst0;
		float fConst4 = std::tan(std::max<float>(0.0f, std::min<float>(104.166664f / fConst0, 1.5676547f)));
		float fConst5 = std::min<float>(3.4028235e+38f, 1.0f / (fConst4 + 1.0f));
		fConst6 = 2.0f * fConst4 * fConst5;
		fConst7 = 0.9791667f * fConst5;
		float fConst8 = std::tan(std::max<float>(0.0f, std::min<float>(70.92199f / fConst0, 1.5676547f)));
		fConst9 = std::min<float>(3.4028235e+38f, 1.0f / (fConst8 + 1.0f));
		fConst10 = 2.0f * fConst8 * fConst9;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(1.5f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fVec1[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec7[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			iRec5[l3] = 0;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			iRec6[l4] = 0;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec17[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec14[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec15[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec11[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec12[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec8[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec9[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec2[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec3[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec0[l14] = 0.0f;
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
	
	virtual Faust_Phase90* clone() {
		return new Faust_Phase90();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("phase90");
		ui_interface->addHorizontalSlider("Rate", &fHslider0, FAUSTFLOAT(1.5f), FAUSTFLOAT(0.1f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = float(fHslider0);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			int iTemp0 = 1 - iVec0[1];
			fVec1[0] = fSlow0;
			float fTemp1 = fConst1 * (std::min<float>(3.4028235e+38f, fVec1[1] - fSlow0) + fRec7[1]);
			fRec7[0] = ((std::fabs(fTemp1) > 1.1754944e-38f) ? fTemp1 : 0.0f);
			float fTemp2 = fConst2 * (fSlow0 + fRec7[0]);
			float fTemp3 = 1.0737418e+09f * (fTemp2 - std::rint(fTemp2));
			float fTemp4 = std::floor(fTemp3);
			int iTemp5 = iRec6[1] + int(16777216.0f * (fTemp3 - fTemp4));
			iRec5[0] = ((iTemp0) ? 0 : (iRec5[1] + int(fTemp4) + (iTemp5 >> 24)) & 1073741823);
			iRec6[0] = ((iTemp0) ? 0 : iTemp5 & 16777215);
			float fTemp6 = float(iRec5[0] >> 6);
			float fTemp7 = 5.9604645e-08f * fTemp6;
			float fTemp8 = 0.15f * ((fTemp7 < 0.65f) ? 1.8339891e-07f * fTemp6 + -1.0f : 1.0f - 5.714286f * (fTemp7 + -0.65f));
			float fTemp9 = fTemp8 + 0.171f;
			int iTemp10 = fTemp9 < 0.0f;
			float fTemp11 = Faust_Phase90_faustpower2_f(0.49480456f * (fTemp8 + -1.85f) + 1.0f);
			float fTemp12 = std::tan(std::min<float>(fConst3 * (((iTemp10) ? 0.0f : ((fRec3[1] <= fTemp9) ? 0.0026280219f * (fTemp8 + (0.171f - 0.5f * fRec3[1])) : 0.005367f * (fTemp11 * (0.004f * fRec3[1] + 1.0f) / std::max<float>(fRec3[1], 1e-09f)))) + 4.1666666e-05f), 0.7853982f));
			float fTemp13 = std::tan(std::min<float>(fConst3 * (((iTemp10) ? 0.0f : ((fRec9[1] <= fTemp9) ? 0.0026280219f * (fTemp8 + (0.171f - 0.5f * fRec9[1])) : 0.005367f * (fTemp11 * (0.004f * fRec9[1] + 1.0f) / std::max<float>(fRec9[1], 1e-09f)))) + 4.1666666e-05f), 0.7853982f));
			float fTemp14 = std::tan(std::min<float>(fConst3 * (((iTemp10) ? 0.0f : ((fRec12[1] <= fTemp9) ? 0.0026280219f * (fTemp8 + (0.171f - 0.5f * fRec12[1])) : 0.005367f * (fTemp11 * (0.004f * fRec12[1] + 1.0f) / std::max<float>(fRec12[1], 1e-09f)))) + 4.1666666e-05f), 0.7853982f));
			float fTemp15 = std::tan(std::min<float>(fConst3 * (((iTemp10) ? 0.0f : ((fRec15[1] <= fTemp9) ? 0.0026280219f * (fTemp8 + (0.171f - 0.5f * fRec15[1])) : 0.005367f * (fTemp11 * (0.004f * fRec15[1] + 1.0f) / std::max<float>(fRec15[1], 1e-09f)))) + 4.1666666e-05f), 0.7853982f));
			float fTemp16 = float(input0[i0]) - fRec17[1];
			float fTemp17 = fRec17[1] + fConst6 * fTemp16;
			fRec17[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fConst7 * fTemp16;
			float fRec18 = ((std::fabs(fTemp18) > 1.1754944e-38f) ? fTemp18 : 0.0f);
			float fTemp19 = (fRec18 * fTemp15 + fRec14[1]) / (fTemp15 + 1.0f);
			float fTemp20 = 2.0f * fTemp19;
			float fTemp21 = fTemp20 - fRec14[1];
			fRec14[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fRec18 - fTemp19;
			fRec15[0] = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			float fTemp23 = fRec18 - fTemp20;
			float fRec16 = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fTemp24 = (fRec16 * fTemp14 + fRec11[1]) / (fTemp14 + 1.0f);
			float fTemp25 = 2.0f * fTemp24;
			float fTemp26 = fTemp25 - fRec11[1];
			fRec11[0] = ((std::fabs(fTemp26) > 1.1754944e-38f) ? fTemp26 : 0.0f);
			float fTemp27 = fRec16 - fTemp24;
			fRec12[0] = ((std::fabs(fTemp27) > 1.1754944e-38f) ? fTemp27 : 0.0f);
			float fTemp28 = fRec16 - fTemp25;
			float fRec13 = ((std::fabs(fTemp28) > 1.1754944e-38f) ? fTemp28 : 0.0f);
			float fTemp29 = (fRec13 * fTemp13 + fRec8[1]) / (fTemp13 + 1.0f);
			float fTemp30 = 2.0f * fTemp29;
			float fTemp31 = fTemp30 - fRec8[1];
			fRec8[0] = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp32 = fRec13 - fTemp29;
			fRec9[0] = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fTemp33 = fRec13 - fTemp30;
			float fRec10 = ((std::fabs(fTemp33) > 1.1754944e-38f) ? fTemp33 : 0.0f);
			float fTemp34 = (fRec10 * fTemp12 + fRec2[1]) / (fTemp12 + 1.0f);
			float fTemp35 = 2.0f * fTemp34;
			float fTemp36 = fTemp35 - fRec2[1];
			fRec2[0] = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fTemp37 = fRec10 - fTemp34;
			fRec3[0] = ((std::fabs(fTemp37) > 1.1754944e-38f) ? fTemp37 : 0.0f);
			float fTemp38 = fRec10 - fTemp35;
			float fRec4 = ((std::fabs(fTemp38) > 1.1754944e-38f) ? fTemp38 : 0.0f);
			float fTemp39 = 0.5f * (fRec18 + fRec4) - fRec0[1];
			float fTemp40 = fRec0[1] + fConst10 * fTemp39;
			fRec0[0] = ((std::fabs(fTemp40) > 1.1754944e-38f) ? fTemp40 : 0.0f);
			float fTemp41 = fConst9 * fTemp39;
			float fRec1 = ((std::fabs(fTemp41) > 1.1754944e-38f) ? fTemp41 : 0.0f);
			output0[i0] = FAUSTFLOAT(-1.788f * fRec1);
			iVec0[1] = iVec0[0];
			fVec1[1] = fVec1[0];
			fRec7[1] = fRec7[0];
			iRec5[1] = iRec5[0];
			iRec6[1] = iRec6[0];
			fRec17[1] = fRec17[0];
			fRec14[1] = fRec14[0];
			fRec15[1] = fRec15[0];
			fRec11[1] = fRec11[0];
			fRec12[1] = fRec12[0];
			fRec8[1] = fRec8[0];
			fRec9[1] = fRec9[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
