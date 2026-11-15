// D:\workspace\Fanan_web_hub\src\timberwolf\faust\flanger.h
// Written by the Faust compiler (2.70.3) from flanger.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "flanger"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Flanger -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Flanger_H__
#define  __Faust_Flanger_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Flanger
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

class Faust_FlangerSIG0 {
	
  private:
	
	int iVec4[2];
	int iRec3[2];
	
  public:
	
	int getNumInputsFaust_FlangerSIG0() {
		return 0;
	}
	int getNumOutputsFaust_FlangerSIG0() {
		return 1;
	}
	
	void instanceInitFaust_FlangerSIG0(int sample_rate) {
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			iVec4[l6] = 0;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			iRec3[l7] = 0;
		}
	}
	
	void fillFaust_FlangerSIG0(int count, float* table) {
		for (int i1 = 0; i1 < count; i1 = i1 + 1) {
			iVec4[0] = 1;
			iRec3[0] = (iVec4[1] + iRec3[1]) % 65536;
			table[i1] = std::sin(9.58738e-05f * float(iRec3[0]));
			iVec4[1] = iVec4[0];
			iRec3[1] = iRec3[0];
		}
	}

};

static Faust_FlangerSIG0* newFaust_FlangerSIG0() { return (Faust_FlangerSIG0*)new Faust_FlangerSIG0(); }
static void deleteFaust_FlangerSIG0(Faust_FlangerSIG0* dsp) { delete dsp; }

class Faust_FlangerSIG1 {
	
  private:
	
	int iVec8[2];
	int iRec9[2];
	
  public:
	
	int getNumInputsFaust_FlangerSIG1() {
		return 0;
	}
	int getNumOutputsFaust_FlangerSIG1() {
		return 1;
	}
	
	void instanceInitFaust_FlangerSIG1(int sample_rate) {
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			iVec8[l16] = 0;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			iRec9[l17] = 0;
		}
	}
	
	void fillFaust_FlangerSIG1(int count, float* table) {
		for (int i2 = 0; i2 < count; i2 = i2 + 1) {
			iVec8[0] = 1;
			iRec9[0] = (iVec8[1] + iRec9[1]) % 65536;
			table[i2] = std::cos(9.58738e-05f * float(iRec9[0]));
			iVec8[1] = iVec8[0];
			iRec9[1] = iRec9[0];
		}
	}

};

static Faust_FlangerSIG1* newFaust_FlangerSIG1() { return (Faust_FlangerSIG1*)new Faust_FlangerSIG1(); }
static void deleteFaust_FlangerSIG1(Faust_FlangerSIG1* dsp) { delete dsp; }

static float ftbl0Faust_FlangerSIG0[65536];
static float ftbl1Faust_FlangerSIG1[65536];

class Faust_Flanger : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	int iVec0[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	FAUSTFLOAT fHslider2;
	float fVec2[2];
	float fRec2[2];
	int IOTA0;
	float fVec3[4096];
	FAUSTFLOAT fHslider3;
	float fConst2;
	int iRec4[2];
	int iRec5[2];
	FAUSTFLOAT fHslider4;
	float fVec5[2];
	float fRec6[2];
	FAUSTFLOAT fHslider5;
	float fVec6[2];
	float fRec7[2];
	float fConst3;
	float fRec1[2];
	float fVec7[4096];
	FAUSTFLOAT fHslider6;
	float fRec8[2];
	
 public:
	Faust_Flanger() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Flanger -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "flanger.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "flanger");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.9.6");
		m->declare("phaflangers.lib/name", "Faust Phaser and Flanger Library");
		m->declare("phaflangers.lib/version", "1.1.3");
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
		Faust_FlangerSIG0* sig0 = newFaust_FlangerSIG0();
		sig0->instanceInitFaust_FlangerSIG0(sample_rate);
		sig0->fillFaust_FlangerSIG0(65536, ftbl0Faust_FlangerSIG0);
		Faust_FlangerSIG1* sig1 = newFaust_FlangerSIG1();
		sig1->instanceInitFaust_FlangerSIG1(sample_rate);
		sig1->fillFaust_FlangerSIG1(65536, ftbl1Faust_FlangerSIG1);
		deleteFaust_FlangerSIG0(sig0);
		deleteFaust_FlangerSIG1(sig1);
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		float fConst0 = std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
		fConst1 = 1.0f - 44.1f / fConst0;
		fConst2 = 1.0f / fConst0;
		fConst3 = 0.001f * fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(1e+02f);
		fHslider2 = FAUSTFLOAT(5e+01f);
		fHslider3 = FAUSTFLOAT(0.3f);
		fHslider4 = FAUSTFLOAT(6e+01f);
		fHslider5 = FAUSTFLOAT(1.5f);
		fHslider6 = FAUSTFLOAT(9e+01f);
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
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fVec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec2[l4] = 0.0f;
		}
		IOTA0 = 0;
		for (int l5 = 0; l5 < 4096; l5 = l5 + 1) {
			fVec3[l5] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			iRec4[l8] = 0;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			iRec5[l9] = 0;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fVec5[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec6[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fVec6[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec7[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec1[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 4096; l15 = l15 + 1) {
			fVec7[l15] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec8[l18] = 0.0f;
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
	
	virtual Faust_Flanger* clone() {
		return new Faust_Flanger();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("flanger");
		ui_interface->addHorizontalSlider("Depth", &fHslider4, FAUSTFLOAT(6e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Feedback", &fHslider2, FAUSTFLOAT(5e+01f), FAUSTFLOAT(-95.0f), FAUSTFLOAT(95.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Invert", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Manual", &fHslider5, FAUSTFLOAT(1.5f), FAUSTFLOAT(0.1f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Mix", &fHslider1, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Rate", &fHslider3, FAUSTFLOAT(0.3f), FAUSTFLOAT(0.02f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Spread", &fHslider6, FAUSTFLOAT(9e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.8e+02f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		int iSlow0 = int(float(fHslider0));
		float fSlow1 = float(fHslider1);
		float fSlow2 = 0.01f * fSlow1;
		float fSlow3 = float(fHslider2);
		float fSlow4 = 0.01f * fSlow3;
		float fSlow5 = fConst2 * float(fHslider3);
		float fSlow6 = 1.0737418e+09f * (fSlow5 - std::rint(fSlow5));
		float fSlow7 = std::floor(fSlow6);
		int iSlow8 = int(16777216.0f * (fSlow6 - fSlow7));
		int iSlow9 = int(fSlow7);
		float fSlow10 = float(fHslider4);
		float fSlow11 = 0.01f * fSlow10;
		float fSlow12 = float(fHslider5);
		float fSlow13 = 0.017453292f * float(fHslider6);
		float fSlow14 = std::sin(fSlow13);
		float fSlow15 = std::cos(fSlow13);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			fVec1[0] = fSlow1;
			float fTemp0 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow1 - fVec1[1]))) + fRec0[1]);
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow2 + fRec0[0];
			float fTemp2 = ((iSlow0) ? -fTemp1 : fTemp1);
			fVec2[0] = fSlow3;
			float fTemp3 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow3 - fVec2[1]))) + fRec2[1]);
			fRec2[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = fSlow4 + fRec2[0];
			float fTemp5 = float(input0[i0]);
			float fTemp6 = fTemp5 + fRec1[1] * fTemp4;
			fVec3[IOTA0 & 4095] = fTemp6;
			int iTemp7 = 1 - iVec0[1];
			int iTemp8 = iSlow8 + iRec5[1];
			iRec4[0] = ((iTemp7) ? 0 : (iSlow9 + iRec4[1] + (iTemp8 >> 24)) & 1073741823);
			iRec5[0] = ((iTemp7) ? 0 : iTemp8 & 16777215);
			int iTemp9 = std::max<int>(0, std::min<int>(int(0.00390625f * float(iRec4[0] >> 6)), 65535));
			float fTemp10 = ftbl0Faust_FlangerSIG0[iTemp9];
			fVec5[0] = fSlow10;
			float fTemp11 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow10 - fVec5[1]))) + fRec6[1]);
			fRec6[0] = ((std::fabs(fTemp11) > 1.1754944e-38f) ? fTemp11 : 0.0f);
			float fTemp12 = fSlow11 + fRec6[0];
			fVec6[0] = fSlow12;
			float fTemp13 = fConst1 * (std::min<float>(3.4028235e+38f, fVec6[1] - fSlow12) + fRec7[1]);
			fRec7[0] = ((std::fabs(fTemp13) > 1.1754944e-38f) ? fTemp13 : 0.0f);
			float fTemp14 = std::max<float>(0.0f, fConst3 * (fSlow12 + fRec7[0] + 2.0f * fTemp12 * (fTemp10 + 1.0f)) + -1.0f);
			int iTemp15 = int(std::min<float>(2049.0f, std::max<float>(-1.0f, fTemp14)));
			float fTemp16 = std::floor(fTemp14);
			float fTemp17 = fVec3[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp15))) & 4095] * (fTemp16 + (1.0f - fTemp14)) + (fTemp14 - fTemp16) * fVec3[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp15 + 1))) & 4095];
			fRec1[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			output0[i0] = FAUSTFLOAT(0.5f * (fTemp5 + fRec1[1] * fTemp2));
			float fTemp18 = float(input1[i0]);
			float fTemp19 = fTemp18 + fTemp4 * fRec8[1];
			fVec7[IOTA0 & 4095] = fTemp19;
			float fTemp20 = std::max<float>(0.0f, fConst3 * (fSlow12 + fRec7[0] + 2.0f * fTemp12 * (fSlow15 * fTemp10 + fSlow14 * ftbl1Faust_FlangerSIG1[iTemp9] + 1.0f)) + -1.0f);
			int iTemp21 = int(std::min<float>(2049.0f, std::max<float>(-1.0f, fTemp20)));
			float fTemp22 = std::floor(fTemp20);
			float fTemp23 = fVec7[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp21))) & 4095] * (fTemp22 + (1.0f - fTemp20)) + (fTemp20 - fTemp22) * fVec7[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp21 + 1))) & 4095];
			fRec8[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			output1[i0] = FAUSTFLOAT(0.5f * (fTemp18 + fTemp2 * fRec8[1]));
			iVec0[1] = iVec0[0];
			fVec1[1] = fVec1[0];
			fRec0[1] = fRec0[0];
			fVec2[1] = fVec2[0];
			fRec2[1] = fRec2[0];
			IOTA0 = IOTA0 + 1;
			iRec4[1] = iRec4[0];
			iRec5[1] = iRec5[0];
			fVec5[1] = fVec5[0];
			fRec6[1] = fRec6[0];
			fVec6[1] = fVec6[0];
			fRec7[1] = fRec7[0];
			fRec1[1] = fRec1[0];
			fRec8[1] = fRec8[0];
		}
	}

};

#endif
