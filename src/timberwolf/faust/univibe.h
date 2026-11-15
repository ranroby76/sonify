// D:\workspace\Fanan_web_hub\src\timberwolf\faust\univibe.h
// Written by the Faust compiler (2.70.3) from univibe.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "univibe"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Univibe -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Univibe_H__
#define  __Faust_Univibe_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

/* link with : "" */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Univibe
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

static float Faust_Univibe_faustpower2_f(float value) {
	return value * value;
}

class Faust_Univibe : public FaustDsp {
	
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
	float fVec3[2];
	float fRec9[2];
	float fConst7;
	float fConst8;
	FAUSTFLOAT fHslider3;
	float fVec4[2];
	float fRec12[2];
	float fConst9;
	int iRec10[2];
	int iRec11[2];
	FAUSTFLOAT fHslider4;
	float fVec5[2];
	float fRec13[2];
	float fConst10;
	float fConst11;
	float fRec6[2];
	float fRec7[2];
	float fConst12;
	float fRec3[2];
	float fRec4[2];
	float fConst13;
	float fRec14[2];
	float fRec15[2];
	float fConst14;
	float fRec17[2];
	float fRec18[2];
	float fRec2[2];
	
 public:
	Faust_Univibe() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Univibe -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "univibe.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "univibe");
		m->declare("oscillators.lib/lf_sawpos:author", "Bart Brouns, revised by Stéphane Letz");
		m->declare("oscillators.lib/lf_sawpos:licence", "LicenseRef-STK-4.3");
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
		float fConst2 = 0.5f * std::max<float>(-(2513.2742f / fConst0), -8e+01f);
		float fConst3 = sinhf(fConst2);
		float fConst4 = std::exp(fConst2);
		float fConst5 = fConst4 * fConst3;
		float fConst6 = fConst5 * (fConst5 + 1.0f);
		fConst7 = 16.0f * fConst6;
		fConst8 = 8.0f * fConst6;
		fConst9 = 1.0f / fConst0;
		fConst10 = 2.0f * fConst5 + 1.0f;
		fConst11 = Faust_Univibe_faustpower2_f(fConst4) * Faust_Univibe_faustpower2_f(fConst3);
		fConst12 = 1.8f / fConst0;
		fConst13 = 3.24f / fConst0;
		fConst14 = 5.832f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(0.0f);
		fHslider2 = FAUSTFLOAT(2e+01f);
		fHslider3 = FAUSTFLOAT(3.0f);
		fHslider4 = FAUSTFLOAT(7e+01f);
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
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fVec3[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec9[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fVec4[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec12[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			iRec10[l9] = 0;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			iRec11[l10] = 0;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fVec5[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec13[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec6[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec7[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec3[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec4[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec14[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec15[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fRec17[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec18[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec2[l21] = 0.0f;
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
	
	virtual Faust_Univibe* clone() {
		return new Faust_Univibe();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("univibe");
		ui_interface->addHorizontalSlider("Feedback", &fHslider2, FAUSTFLOAT(2e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(9e+01f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Intensity", &fHslider4, FAUSTFLOAT(7e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Level", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-18.0f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mode", &fHslider1, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Speed", &fHslider3, FAUSTFLOAT(3.0f), FAUSTFLOAT(0.1f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.01f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = std::pow(1e+01f, 0.05f * float(fHslider0));
		float fSlow1 = float(fHslider1);
		float fSlow2 = float(fHslider2);
		float fSlow3 = 0.01f * fSlow2;
		float fSlow4 = float(fHslider3);
		float fSlow5 = float(fHslider4);
		float fSlow6 = 0.01f * fSlow5;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			iVec1[0] = 1;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			fVec2[0] = fSlow1;
			float fTemp1 = fConst1 * (fRec1[1] + std::min<float>(3.4028235e+38f, fVec2[1] - fSlow1));
			fRec1[0] = ((std::fabs(fTemp1) > 1.1754944e-38f) ? fTemp1 : 0.0f);
			float fTemp2 = fSlow1 + fRec1[0];
			float fTemp3 = float(input0[i0]);
			fVec3[0] = fSlow2;
			float fTemp4 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec3[1]))) + fRec9[1]);
			fRec9[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = fTemp3 + fRec2[1] * (fSlow3 + fRec9[0]);
			int iTemp6 = 1 - iVec1[1];
			fVec4[0] = fSlow4;
			float fTemp7 = fConst1 * (std::min<float>(3.4028235e+38f, fVec4[1] - fSlow4) + fRec12[1]);
			fRec12[0] = ((std::fabs(fTemp7) > 1.1754944e-38f) ? fTemp7 : 0.0f);
			float fTemp8 = fConst9 * (fSlow4 + fRec12[0]);
			float fTemp9 = 1.0737418e+09f * (fTemp8 - std::rint(fTemp8));
			float fTemp10 = std::floor(fTemp9);
			int iTemp11 = iRec11[1] + int(16777216.0f * (fTemp9 - fTemp10));
			iRec10[0] = ((iTemp6) ? 0 : (iRec10[1] + int(fTemp10) + (iTemp11 >> 24)) & 1073741823);
			iRec11[0] = ((iTemp6) ? 0 : iTemp11 & 16777215);
			fVec5[0] = fSlow5;
			float fTemp12 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow5 - fVec5[1]))) + fRec13[1]);
			fRec13[0] = ((std::fabs(fTemp12) > 1.1754944e-38f) ? fTemp12 : 0.0f);
			float fTemp13 = 6e+02f * (1.0f - fRec13[0] - fSlow6) + 1.2e+02f;
			float fTemp14 = 6.2831855f * fTemp13 - 3.1415927f * (fTemp13 - (3e+03f * (fSlow6 + fRec13[0]) + 9e+02f)) * (1.0f - std::cos(3.7450704e-07f * float(iRec10[0] >> 6)));
			float fTemp15 = 0.5f * std::min<float>(2.984513f, fConst9 * fTemp14);
			float fTemp16 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::sin(fTemp15));
			float fTemp17 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::cos(fTemp15));
			float fTemp18 = std::min<float>(3.4028235e+38f, 0.25f / fTemp17);
			float fTemp19 = std::sqrt(4.0f * fTemp16 * fTemp18);
			float fTemp20 = fTemp17 * fTemp19 * (fTemp18 * std::min<float>(3.4028235e+38f, 1.0f / fTemp19) * ((4.0f * fTemp16 - fConst8) * fRec6[1] + fConst7 * fTemp5) + fRec7[1]);
			float fTemp21 = fRec6[1] - 2.0f * fTemp20;
			fRec6[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fRec6[1] - fTemp20;
			float fTemp23 = fRec7[1] + 2.0f * fTemp19 * fTemp22;
			fRec7[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fRec8 = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			float fTemp24 = 0.5f * std::min<float>(2.984513f, fConst12 * fTemp14);
			float fTemp25 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::sin(fTemp24));
			float fTemp26 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::cos(fTemp24));
			float fTemp27 = std::min<float>(3.4028235e+38f, 0.25f / fTemp26);
			float fTemp28 = std::sqrt(4.0f * fTemp25 * fTemp27);
			float fTemp29 = fTemp26 * fTemp28 * (fTemp27 * std::min<float>(3.4028235e+38f, 1.0f / fTemp28) * ((4.0f * fTemp25 - fConst8) * fRec3[1] + fConst7 * (fTemp5 - fRec8)) + fRec4[1]);
			float fTemp30 = fRec3[1] - 2.0f * fTemp29;
			fRec3[0] = ((std::fabs(fTemp30) > 1.1754944e-38f) ? fTemp30 : 0.0f);
			float fTemp31 = fRec3[1] - fTemp29;
			float fTemp32 = fRec4[1] + 2.0f * fTemp28 * fTemp31;
			fRec4[0] = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fRec5 = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp33 = fRec8 + fRec5;
			float fTemp34 = 0.5f * std::min<float>(2.984513f, fConst13 * fTemp14);
			float fTemp35 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::sin(fTemp34));
			float fTemp36 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::cos(fTemp34));
			float fTemp37 = std::min<float>(3.4028235e+38f, 0.25f / fTemp36);
			float fTemp38 = std::sqrt(4.0f * fTemp35 * fTemp37);
			float fTemp39 = fTemp36 * fTemp38 * (fTemp37 * std::min<float>(3.4028235e+38f, 1.0f / fTemp38) * ((4.0f * fTemp35 - fConst8) * fRec14[1] + fConst7 * (fTemp5 - fTemp33)) + fRec15[1]);
			float fTemp40 = fRec14[1] - 2.0f * fTemp39;
			fRec14[0] = ((std::fabs(fTemp40) > 1.1754944e-38f) ? fTemp40 : 0.0f);
			float fTemp41 = fRec14[1] - fTemp39;
			float fTemp42 = fRec15[1] + 2.0f * fTemp38 * fTemp41;
			fRec15[0] = ((std::fabs(fTemp42) > 1.1754944e-38f) ? fTemp42 : 0.0f);
			float fRec16 = ((std::fabs(fTemp41) > 1.1754944e-38f) ? fTemp41 : 0.0f);
			float fTemp43 = fRec16 + fTemp33;
			float fTemp44 = 0.5f * std::min<float>(2.984513f, fConst14 * fTemp14);
			float fTemp45 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::sin(fTemp44));
			float fTemp46 = fConst11 + fConst10 * Faust_Univibe_faustpower2_f(std::cos(fTemp44));
			float fTemp47 = std::min<float>(3.4028235e+38f, 0.25f / fTemp46);
			float fTemp48 = std::sqrt(4.0f * fTemp45 * fTemp47);
			float fTemp49 = fTemp46 * fTemp48 * (fTemp47 * std::min<float>(3.4028235e+38f, 1.0f / fTemp48) * ((4.0f * fTemp45 - fConst8) * fRec17[1] + fConst7 * (fTemp5 - fTemp43)) + fRec18[1]);
			float fTemp50 = fRec17[1] - 2.0f * fTemp49;
			fRec17[0] = ((std::fabs(fTemp50) > 1.1754944e-38f) ? fTemp50 : 0.0f);
			float fTemp51 = fRec17[1] - fTemp49;
			float fTemp52 = fRec18[1] + 2.0f * fTemp48 * fTemp51;
			fRec18[0] = ((std::fabs(fTemp52) > 1.1754944e-38f) ? fTemp52 : 0.0f);
			float fRec19 = ((std::fabs(fTemp51) > 1.1754944e-38f) ? fTemp51 : 0.0f);
			float fTemp53 = fTemp5 - (fRec19 + fTemp43);
			fRec2[0] = ((std::fabs(fTemp53) > 1.1754944e-38f) ? fTemp53 : 0.0f);
			output0[i0] = FAUSTFLOAT((fRec2[0] * (1.0f - 0.5f * fTemp2) + 0.5f * fTemp3 * fTemp2) * (fSlow0 + fRec0[0]));
			fVec0[1] = fVec0[0];
			iVec1[1] = iVec1[0];
			fRec0[1] = fRec0[0];
			fVec2[1] = fVec2[0];
			fRec1[1] = fRec1[0];
			fVec3[1] = fVec3[0];
			fRec9[1] = fRec9[0];
			fVec4[1] = fVec4[0];
			fRec12[1] = fRec12[0];
			iRec10[1] = iRec10[0];
			iRec11[1] = iRec11[0];
			fVec5[1] = fVec5[0];
			fRec13[1] = fRec13[0];
			fRec6[1] = fRec6[0];
			fRec7[1] = fRec7[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fRec14[1] = fRec14[0];
			fRec15[1] = fRec15[0];
			fRec17[1] = fRec17[0];
			fRec18[1] = fRec18[0];
			fRec2[1] = fRec2[0];
		}
	}

};

#endif
