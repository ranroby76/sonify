// D:\workspace\Fanan_web_hub\src\timberwolf\faust\dimension.h
// Written by the Faust compiler (2.70.3) from dimension.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "dimension"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Dimension -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Dimension_H__
#define  __Faust_Dimension_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Dimension
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

static float Faust_Dimension_faustpower2_f(float value) {
	return value * value;
}

class Faust_Dimension : public FaustDsp {
	
 private:
	
	int iVec0[2];
	FAUSTFLOAT fHslider0;
	float fVec1[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	float fConst2;
	int IOTA0;
	float fVec2[65536];
	float fConst3;
	int iConst7;
	int iConst8;
	int iRec1[2];
	int iRec2[2];
	float fConst9;
	float fConst10;
	FAUSTFLOAT fHslider1;
	float fVec3[2];
	float fRec3[2];
	
 public:
	Faust_Dimension() {}

	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Dimension -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "dimension.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/version", "2.7.4");
		m->declare("misceffects.lib/xferDimensionExpander:author", "David Braun");
		m->declare("misceffects.lib/xferDimensionExpander:license", "MIT");
		m->declare("name", "dimension");
		m->declare("oscillators.lib/lf_sawpos:author", "Bart Brouns, revised by Stéphane Letz");
		m->declare("oscillators.lib/lf_sawpos:licence", "LicenseRef-STK-4.3");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.9.6");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
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
		fConst2 = 0.0177f * fConst0;
		fConst3 = 0.02464548f * fConst0;
		float fConst4 = 0.45486f / fConst0;
		float fConst5 = 1.0737418e+09f * (fConst4 - std::rint(fConst4));
		float fConst6 = std::floor(fConst5);
		iConst7 = int(16777216.0f * (fConst5 - fConst6));
		iConst8 = int(fConst6);
		fConst9 = 0.020886f * fConst0;
		fConst10 = 0.015f * fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(5e+01f);
		fHslider1 = FAUSTFLOAT(5e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fVec1[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec0[l2] = 0.0f;
		}
		IOTA0 = 0;
		for (int l3 = 0; l3 < 65536; l3 = l3 + 1) {
			fVec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			iRec1[l4] = 0;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			iRec2[l5] = 0;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec3[l7] = 0.0f;
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
	
	virtual Faust_Dimension* clone() {
		return new Faust_Dimension();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("dimension");
		ui_interface->addHorizontalSlider("Mix", &fHslider1, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Size", &fHslider0, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
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
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			fVec1[0] = fSlow0;
			float fTemp0 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec1[1]))) + fRec0[1]);
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = 6.6666665f * std::max<float>(0.0f, std::min<float>(1.0f, fSlow1 + fRec0[0])) + 1.0f;
			float fTemp2 = fConst2 * fTemp1;
			float fTemp3 = std::floor(fTemp2);
			float fTemp4 = float(input1[i0]);
			float fTemp5 = float(input0[i0]);
			float fTemp6 = fTemp5 + fTemp4;
			fVec2[IOTA0 & 65535] = fTemp6;
			int iTemp7 = int(fTemp3);
			float fTemp8 = fTemp2 - fTemp3;
			float fTemp9 = Faust_Dimension_faustpower2_f(fTemp8);
			float fTemp10 = fConst3 * fTemp1;
			float fTemp11 = std::floor(fTemp10);
			int iTemp12 = int(fTemp11);
			float fTemp13 = fTemp10 - fTemp11;
			float fTemp14 = Faust_Dimension_faustpower2_f(fTemp13);
			float fTemp15 = 1.5f * fTemp8;
			float fTemp16 = 1.5f * fTemp13;
			int iTemp17 = 1 - iVec0[1];
			int iTemp18 = iConst7 + iRec2[1];
			iRec1[0] = ((iTemp17) ? 0 : (iConst8 + iRec1[1] + (iTemp18 >> 24)) & 1073741823);
			iRec2[0] = ((iTemp17) ? 0 : iTemp18 & 16777215);
			float fTemp19 = 3.7450704e-07f * float(iRec1[0] >> 6);
			float fTemp20 = fConst9 * fTemp1;
			float fTemp21 = std::floor(fTemp20);
			int iTemp22 = int(fTemp21);
			float fTemp23 = fTemp20 - fTemp21;
			float fTemp24 = Faust_Dimension_faustpower2_f(fTemp23);
			float fTemp25 = fConst10 * fTemp1;
			float fTemp26 = std::floor(fTemp25);
			int iTemp27 = int(fTemp26);
			float fTemp28 = fTemp25 - fTemp26;
			float fTemp29 = Faust_Dimension_faustpower2_f(fTemp28);
			float fTemp30 = 1.5f * fTemp23;
			float fTemp31 = 1.5f * fTemp28;
			fVec3[0] = fSlow2;
			float fTemp32 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec3[1]))) + fRec3[1]);
			fRec3[0] = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fTemp33 = 0.5f * (fSlow3 + fRec3[0]) * (std::sin(fTemp19) * (0.5f * ((fTemp29 * (fTemp31 + -2.5f) + 1.0f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp27)))) & 65535] + fTemp28 * ((fTemp28 * (1.0f - 0.5f * fTemp28) + -0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp27 + -1)))) & 65535] + (fTemp28 * (2.0f - fTemp31) + 0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp27 + 1)))) & 65535]) - ((fTemp24 * (fTemp30 + -2.5f) + 1.0f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp22)))) & 65535] + fTemp23 * ((fTemp23 * (1.0f - 0.5f * fTemp23) + -0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp22 + -1)))) & 65535] + (fTemp23 * (2.0f - fTemp30) + 0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp22 + 1)))) & 65535]))) - 0.25f * (fTemp29 * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp27 + 2)))) & 65535] * (fTemp26 + (1.0f - fTemp25)) - fTemp24 * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp22 + 2)))) & 65535] * (fTemp21 + (1.0f - fTemp20)))) + std::cos(fTemp19) * (0.5f * ((fTemp14 * (fTemp16 + -2.5f) + 1.0f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp12)))) & 65535] + fTemp13 * ((fTemp13 * (1.0f - 0.5f * fTemp13) + -0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp12 + -1)))) & 65535] + (fTemp13 * (2.0f - fTemp16) + 0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp12 + 1)))) & 65535]) - ((fTemp9 * (fTemp15 + -2.5f) + 1.0f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp7)))) & 65535] + fTemp8 * ((fTemp8 * (1.0f - 0.5f * fTemp8) + -0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp7 + -1)))) & 65535] + (fTemp8 * (2.0f - fTemp15) + 0.5f) * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp7 + 1)))) & 65535]))) - 0.25f * (fTemp14 * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp12 + 2)))) & 65535] * (fTemp11 + (1.0f - fTemp10)) - fTemp9 * fVec2[(IOTA0 - std::min<int>(65536, std::max<int>(0, std::max<int>(0, iTemp7 + 2)))) & 65535] * (fTemp3 + (1.0f - fTemp2)))));
			output0[i0] = FAUSTFLOAT(fTemp5 + fTemp33);
			output1[i0] = FAUSTFLOAT(fTemp4 - fTemp33);
			iVec0[1] = iVec0[0];
			fVec1[1] = fVec1[0];
			fRec0[1] = fRec0[0];
			IOTA0 = IOTA0 + 1;
			iRec1[1] = iRec1[0];
			iRec2[1] = iRec2[0];
			fVec3[1] = fVec3[0];
			fRec3[1] = fRec3[0];
		}
	}

};

#endif
