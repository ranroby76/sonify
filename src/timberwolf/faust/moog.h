// D:\workspace\Fanan_web_hub\src\timberwolf\faust\moog.h
// Written by the Faust compiler (2.70.3) from moog.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "moog"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Moog -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Moog_H__
#define  __Faust_Moog_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Moog
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

class Faust_MoogSIG0 {
	
  private:
	
	int iVec3[2];
	int iRec7[2];
	
  public:
	
	int getNumInputsFaust_MoogSIG0() {
		return 0;
	}
	int getNumOutputsFaust_MoogSIG0() {
		return 1;
	}
	
	void instanceInitFaust_MoogSIG0(int sample_rate) {
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			iVec3[l5] = 0;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			iRec7[l6] = 0;
		}
	}
	
	void fillFaust_MoogSIG0(int count, float* table) {
		for (int i1 = 0; i1 < count; i1 = i1 + 1) {
			iVec3[0] = 1;
			iRec7[0] = (iVec3[1] + iRec7[1]) % 65536;
			table[i1] = std::sin(9.58738e-05f * float(iRec7[0]));
			iVec3[1] = iVec3[0];
			iRec7[1] = iRec7[0];
		}
	}

};

static Faust_MoogSIG0* newFaust_MoogSIG0() { return (Faust_MoogSIG0*)new Faust_MoogSIG0(); }
static void deleteFaust_MoogSIG0(Faust_MoogSIG0* dsp) { delete dsp; }

static float ftbl0Faust_MoogSIG0[65536];
static float Faust_Moog_faustpower4_f(float value) {
	return value * value * value * value;
}

class Faust_Moog : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int iVec1[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	FAUSTFLOAT fHslider1;
	float fVec2[2];
	float fRec1[2];
	FAUSTFLOAT fHslider2;
	float fConst2;
	int iRec8[2];
	int iRec9[2];
	FAUSTFLOAT fHslider3;
	FAUSTFLOAT fHslider4;
	float fVec4[2];
	float fRec10[2];
	float fConst3;
	FAUSTFLOAT fHslider5;
	float fVec5[2];
	float fRec11[2];
	float fRec2[2];
	float fRec3[2];
	float fRec4[2];
	float fRec5[2];
	
 public:
	Faust_Moog() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Moog -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "moog.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "moog");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.9.6");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.8.2");
		m->declare("vaeffects.lib/moogLadder:author", "Dario Sanfilippo");
		m->declare("vaeffects.lib/moogLadder:license", "LicenseRef-STK-4.3");
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
		Faust_MoogSIG0* sig0 = newFaust_MoogSIG0();
		sig0->instanceInitFaust_MoogSIG0(sample_rate);
		sig0->fillFaust_MoogSIG0(65536, ftbl0Faust_MoogSIG0);
		deleteFaust_MoogSIG0(sig0);
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		float fConst0 = std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
		fConst1 = 1.0f - 44.1f / fConst0;
		fConst2 = 1.0f / fConst0;
		fConst3 = 6.2831855f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(1e+02f);
		fHslider2 = FAUSTFLOAT(0.5f);
		fHslider3 = FAUSTFLOAT(0.0f);
		fHslider4 = FAUSTFLOAT(1.2e+03f);
		fHslider5 = FAUSTFLOAT(3e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			iVec1[l1] = 0;
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
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			iRec8[l7] = 0;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			iRec9[l8] = 0;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fVec4[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec10[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fVec5[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec11[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec2[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec3[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec4[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec5[l16] = 0.0f;
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
	
	virtual Faust_Moog* clone() {
		return new Faust_Moog();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("moog");
		ui_interface->addHorizontalSlider("Cutoff", &fHslider4, FAUSTFLOAT(1.2e+03f), FAUSTFLOAT(4e+01f), FAUSTFLOAT(1.2e+04f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Drive", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(24.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("LFO Depth", &fHslider3, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("LFO Rate", &fHslider2, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.02f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Mix", &fHslider1, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Resonance", &fHslider5, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = std::pow(1e+01f, 0.05f * float(fHslider0));
		float fSlow1 = float(fHslider1);
		float fSlow2 = 0.01f * fSlow1;
		float fSlow3 = fConst2 * float(fHslider2);
		float fSlow4 = 1.0737418e+09f * (fSlow3 - std::rint(fSlow3));
		float fSlow5 = std::floor(fSlow4);
		int iSlow6 = int(16777216.0f * (fSlow4 - fSlow5));
		int iSlow7 = int(fSlow5);
		float fSlow8 = 0.03f * float(fHslider3);
		float fSlow9 = float(fHslider4);
		float fSlow10 = 0.093f * float(fHslider5);
		float fSlow11 = fSlow10 + 0.707f;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			iVec1[0] = 1;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow0 + fRec0[0];
			fVec2[0] = fSlow1;
			float fTemp2 = fConst1 * (fRec1[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow1 - fVec2[1]))));
			fRec1[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			int iTemp3 = 1 - iVec1[1];
			int iTemp4 = iSlow6 + iRec9[1];
			iRec8[0] = ((iTemp3) ? 0 : (iSlow7 + iRec8[1] + (iTemp4 >> 24)) & 1073741823);
			iRec9[0] = ((iTemp3) ? 0 : iTemp4 & 16777215);
			float fTemp5 = std::min<float>(1.8e+04f, fSlow9 * std::pow(2.0f, fSlow8 * ftbl0Faust_MoogSIG0[std::max<int>(0, std::min<int>(int(0.00390625f * float(iRec8[0] >> 6)), 65535))]));
			fVec4[0] = fTemp5;
			float fTemp6 = fConst1 * (fRec10[1] + std::min<float>(3.4028235e+38f, fVec4[1] - fTemp5));
			fRec10[0] = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = std::tan(fConst3 * std::pow(1e+01f, 3.0f * std::min<float>(1.0f, 0.33333334f * std::log10(0.05f * std::max<float>(fRec10[0] + fTemp5, 2e+01f))) + 1.0f));
			float fTemp8 = fTemp7 + 1.0f;
			fVec5[0] = fSlow11;
			float fTemp9 = fConst1 * (std::min<float>(3.4028235e+38f, fVec5[1] + -0.707f - fSlow10) + fRec11[1]);
			fRec11[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			float fTemp10 = fSlow10 + fRec11[0] + -0.00010678119f;
			float fTemp11 = float(input0[i0]);
			float fTemp12 = fTemp7 * ((fTemp11 * fTemp1 - 0.1646572f * fTemp10 * (1.0f - fTemp7 / fTemp8) * (fRec5[1] + fTemp7 * (fRec4[1] + fTemp7 * (fRec3[1] + fTemp7 * fRec2[1] / fTemp8) / fTemp8) / fTemp8)) / (0.1646572f * (Faust_Moog_faustpower4_f(fTemp7) * fTemp10 / Faust_Moog_faustpower4_f(fTemp8)) + 1.0f) - fRec2[1]) / fTemp8;
			float fTemp13 = fRec2[1] + 2.0f * fTemp12;
			fRec2[0] = ((std::fabs(fTemp13) > 1.1754944e-38f) ? fTemp13 : 0.0f);
			float fTemp14 = fTemp7 * (fRec2[1] + fTemp12 - fRec3[1]) / fTemp8;
			float fTemp15 = fRec3[1] + 2.0f * fTemp14;
			fRec3[0] = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = fTemp7 * (fRec3[1] + fTemp14 - fRec4[1]) / fTemp8;
			float fTemp17 = fRec4[1] + 2.0f * fTemp16;
			fRec4[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fTemp7 * (fRec4[1] + fTemp16 - fRec5[1]) / fTemp8;
			float fTemp19 = fRec5[1] + 2.0f * fTemp18;
			fRec5[0] = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = fRec5[1] + fTemp18;
			float fRec6 = ((std::fabs(fTemp20) > 1.1754944e-38f) ? fTemp20 : 0.0f);
			output0[i0] = FAUSTFLOAT(fTemp11 * (1.0f - fRec1[0] - fSlow2) + fRec6 * (fSlow2 + fRec1[0]) / std::sqrt(fTemp1));
			fVec0[1] = fVec0[0];
			iVec1[1] = iVec1[0];
			fRec0[1] = fRec0[0];
			fVec2[1] = fVec2[0];
			fRec1[1] = fRec1[0];
			iRec8[1] = iRec8[0];
			iRec9[1] = iRec9[0];
			fVec4[1] = fVec4[0];
			fRec10[1] = fRec10[0];
			fVec5[1] = fVec5[0];
			fRec11[1] = fRec11[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fRec5[1] = fRec5[0];
		}
	}

};

#endif
