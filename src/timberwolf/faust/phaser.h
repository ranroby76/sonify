// D:\workspace\Fanan_web_hub\src\timberwolf\faust\phaser.h
// Written by the Faust compiler (2.70.3) from phaser.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "phaser"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Phaser -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Phaser_H__
#define  __Faust_Phaser_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

/* link with : "" */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Phaser
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

static float Faust_Phaser_faustpower2_f(float value) {
	return value * value;
}

class Faust_Phaser : public FaustDsp {
	
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
	float fRec8[2];
	float fConst2;
	FAUSTFLOAT fHslider3;
	float fVec3[2];
	float fRec9[2];
	FAUSTFLOAT fHslider4;
	float fVec4[2];
	float fRec12[2];
	float fConst3;
	int iRec10[2];
	int iRec11[2];
	FAUSTFLOAT fHslider5;
	float fVec5[2];
	float fRec13[2];
	FAUSTFLOAT fHslider6;
	float fVec6[2];
	float fRec14[2];
	float fRec5[2];
	float fRec6[2];
	float fConst4;
	float fRec2[2];
	float fRec3[2];
	float fConst5;
	float fRec15[2];
	float fRec16[2];
	float fConst6;
	float fRec18[2];
	float fRec19[2];
	float fRec1[2];
	float fRec25[2];
	float fRec26[2];
	float fRec22[2];
	float fRec23[2];
	float fRec28[2];
	float fRec29[2];
	float fRec31[2];
	float fRec32[2];
	float fRec21[2];
	
 public:
	Faust_Phaser() {}

	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Phaser -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "phaser.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "phaser");
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
		fConst2 = 3.1415927f / fConst0;
		fConst3 = 1.0f / fConst0;
		fConst4 = 1.5f / fConst0;
		fConst5 = 2.25f / fConst0;
		fConst6 = 3.375f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(1e+02f);
		fHslider2 = FAUSTFLOAT(1e+03f);
		fHslider3 = FAUSTFLOAT(5e+01f);
		fHslider4 = FAUSTFLOAT(0.5f);
		fHslider5 = FAUSTFLOAT(3e+03f);
		fHslider6 = FAUSTFLOAT(2e+02f);
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
			fRec8[l4] = 0.0f;
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
			fVec6[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec14[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec5[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec6[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec2[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec3[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fRec15[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec16[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec18[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fRec19[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec1[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fRec25[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec26[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 2; l26 = l26 + 1) {
			fRec22[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec23[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec28[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec29[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2; l30 = l30 + 1) {
			fRec31[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 2; l31 = l31 + 1) {
			fRec32[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fRec21[l32] = 0.0f;
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
	
	virtual Faust_Phaser* clone() {
		return new Faust_Phaser();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("phaser");
		ui_interface->addHorizontalSlider("Depth", &fHslider1, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Feedback", &fHslider3, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(95.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("High", &fHslider5, FAUSTFLOAT(3e+03f), FAUSTFLOAT(8e+02f), FAUSTFLOAT(1e+04f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Invert", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Low", &fHslider6, FAUSTFLOAT(2e+02f), FAUSTFLOAT(5e+01f), FAUSTFLOAT(2e+03f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Rate", &fHslider4, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.02f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Width", &fHslider2, FAUSTFLOAT(1e+03f), FAUSTFLOAT(5e+01f), FAUSTFLOAT(3e+03f), FAUSTFLOAT(1.0f));
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
		float fSlow4 = float(fHslider3);
		float fSlow5 = 0.01f * fSlow4;
		float fSlow6 = float(fHslider4);
		float fSlow7 = float(fHslider5);
		float fSlow8 = float(fHslider6);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			fVec1[0] = fSlow1;
			float fTemp0 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow1 - fVec1[1]))) + fRec0[1]);
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow2 + fRec0[0];
			float fTemp2 = 0.5f * fTemp1;
			float fTemp3 = ((iSlow0) ? -0.5f * fTemp1 : fTemp2);
			fVec2[0] = fSlow3;
			float fTemp4 = fConst1 * (std::min<float>(3.4028235e+38f, fVec2[1] - fSlow3) + fRec8[1]);
			fRec8[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = 0.5f * std::max<float>(-(fConst2 * (fSlow3 + fRec8[0])), -8e+01f);
			float fTemp6 = sinhf(fTemp5);
			float fTemp7 = std::exp(fTemp5);
			float fTemp8 = fTemp7 * fTemp6;
			float fTemp9 = fTemp8 + 1.0f;
			fVec3[0] = fSlow4;
			float fTemp10 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow4 - fVec3[1]))) + fRec9[1]);
			fRec9[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = fSlow5 + fRec9[0];
			float fTemp12 = float(input0[i0]);
			float fTemp13 = fTemp12 + fRec1[1] * fTemp11;
			float fTemp14 = fTemp8 * fTemp9;
			float fTemp15 = 8.0f * fTemp14;
			int iTemp16 = 1 - iVec0[1];
			fVec4[0] = fSlow6;
			float fTemp17 = fConst1 * (std::min<float>(3.4028235e+38f, fVec4[1] - fSlow6) + fRec12[1]);
			fRec12[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fConst3 * (fSlow6 + fRec12[0]);
			float fTemp19 = 1.0737418e+09f * (fTemp18 - std::rint(fTemp18));
			float fTemp20 = std::floor(fTemp19);
			int iTemp21 = iRec11[1] + int(16777216.0f * (fTemp19 - fTemp20));
			iRec10[0] = ((iTemp16) ? 0 : (iRec10[1] + int(fTemp20) + (iTemp21 >> 24)) & 1073741823);
			iRec11[0] = ((iTemp16) ? 0 : iTemp21 & 16777215);
			float fTemp22 = 3.7450704e-07f * float(iRec10[0] >> 6);
			fVec5[0] = fSlow7;
			float fTemp23 = fConst1 * (std::min<float>(3.4028235e+38f, fVec5[1] - fSlow7) + fRec13[1]);
			fRec13[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			fVec6[0] = fSlow8;
			float fTemp24 = fConst1 * (std::min<float>(3.4028235e+38f, fVec6[1] - fSlow8) + fRec14[1]);
			fRec14[0] = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			float fTemp25 = fSlow8 + fRec14[0];
			float fTemp26 = fTemp25 - (fSlow7 + fRec13[0]);
			float fTemp27 = 6.2831855f * fTemp25;
			float fTemp28 = fTemp27 - 3.1415927f * fTemp26 * (1.0f - std::sin(fTemp22));
			float fTemp29 = 0.5f * std::min<float>(2.984513f, fConst3 * fTemp28);
			float fTemp30 = 2.0f * fTemp8 + 1.0f;
			float fTemp31 = Faust_Phaser_faustpower2_f(fTemp7) * Faust_Phaser_faustpower2_f(fTemp6);
			float fTemp32 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp29));
			float fTemp33 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp29));
			float fTemp34 = std::min<float>(3.4028235e+38f, 0.25f / fTemp33);
			float fTemp35 = std::sqrt(4.0f * fTemp32 * fTemp34);
			float fTemp36 = fTemp33 * fTemp35 * (fTemp34 * std::min<float>(3.4028235e+38f, 1.0f / fTemp35) * ((4.0f * fTemp32 - fTemp15) * fRec5[1] + 16.0f * fTemp13 * fTemp7 * fTemp6 * fTemp9) + fRec6[1]);
			float fTemp37 = fRec5[1] - 2.0f * fTemp36;
			fRec5[0] = ((std::fabs(fTemp37) > 1.1754944e-38f) ? fTemp37 : 0.0f);
			float fTemp38 = fRec5[1] - fTemp36;
			float fTemp39 = fRec6[1] + 2.0f * fTemp35 * fTemp38;
			fRec6[0] = ((std::fabs(fTemp39) > 1.1754944e-38f) ? fTemp39 : 0.0f);
			float fRec7 = ((std::fabs(fTemp38) > 1.1754944e-38f) ? fTemp38 : 0.0f);
			float fTemp40 = 0.5f * std::min<float>(2.984513f, fConst4 * fTemp28);
			float fTemp41 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp40));
			float fTemp42 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp40));
			float fTemp43 = std::min<float>(3.4028235e+38f, 0.25f / fTemp42);
			float fTemp44 = std::sqrt(4.0f * fTemp41 * fTemp43);
			float fTemp45 = fTemp42 * fTemp44 * (fTemp43 * std::min<float>(3.4028235e+38f, 1.0f / fTemp44) * ((4.0f * fTemp41 - fTemp15) * fRec2[1] + 16.0f * fTemp14 * (fTemp13 - fRec7)) + fRec3[1]);
			float fTemp46 = fRec2[1] - 2.0f * fTemp45;
			fRec2[0] = ((std::fabs(fTemp46) > 1.1754944e-38f) ? fTemp46 : 0.0f);
			float fTemp47 = fRec2[1] - fTemp45;
			float fTemp48 = fRec3[1] + 2.0f * fTemp44 * fTemp47;
			fRec3[0] = ((std::fabs(fTemp48) > 1.1754944e-38f) ? fTemp48 : 0.0f);
			float fRec4 = ((std::fabs(fTemp47) > 1.1754944e-38f) ? fTemp47 : 0.0f);
			float fTemp49 = fRec7 + fRec4;
			float fTemp50 = 0.5f * std::min<float>(2.984513f, fConst5 * fTemp28);
			float fTemp51 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp50));
			float fTemp52 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp50));
			float fTemp53 = std::min<float>(3.4028235e+38f, 0.25f / fTemp52);
			float fTemp54 = std::sqrt(4.0f * fTemp51 * fTemp53);
			float fTemp55 = fTemp52 * fTemp54 * (fTemp53 * std::min<float>(3.4028235e+38f, 1.0f / fTemp54) * ((4.0f * fTemp51 - fTemp15) * fRec15[1] + 16.0f * fTemp14 * (fTemp13 - fTemp49)) + fRec16[1]);
			float fTemp56 = fRec15[1] - 2.0f * fTemp55;
			fRec15[0] = ((std::fabs(fTemp56) > 1.1754944e-38f) ? fTemp56 : 0.0f);
			float fTemp57 = fRec15[1] - fTemp55;
			float fTemp58 = fRec16[1] + 2.0f * fTemp54 * fTemp57;
			fRec16[0] = ((std::fabs(fTemp58) > 1.1754944e-38f) ? fTemp58 : 0.0f);
			float fRec17 = ((std::fabs(fTemp57) > 1.1754944e-38f) ? fTemp57 : 0.0f);
			float fTemp59 = fRec17 + fTemp49;
			float fTemp60 = 0.5f * std::min<float>(2.984513f, fConst6 * fTemp28);
			float fTemp61 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp60));
			float fTemp62 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp60));
			float fTemp63 = std::min<float>(3.4028235e+38f, 0.25f / fTemp62);
			float fTemp64 = std::sqrt(4.0f * fTemp61 * fTemp63);
			float fTemp65 = fTemp62 * fTemp64 * (fTemp63 * std::min<float>(3.4028235e+38f, 1.0f / fTemp64) * ((4.0f * fTemp61 - fTemp15) * fRec18[1] + 16.0f * fTemp14 * (fTemp13 - fTemp59)) + fRec19[1]);
			float fTemp66 = fRec18[1] - 2.0f * fTemp65;
			fRec18[0] = ((std::fabs(fTemp66) > 1.1754944e-38f) ? fTemp66 : 0.0f);
			float fTemp67 = fRec18[1] - fTemp65;
			float fTemp68 = fRec19[1] + 2.0f * fTemp64 * fTemp67;
			fRec19[0] = ((std::fabs(fTemp68) > 1.1754944e-38f) ? fTemp68 : 0.0f);
			float fRec20 = ((std::fabs(fTemp67) > 1.1754944e-38f) ? fTemp67 : 0.0f);
			float fTemp69 = fTemp13 - (fRec20 + fTemp59);
			fRec1[0] = ((std::fabs(fTemp69) > 1.1754944e-38f) ? fTemp69 : 0.0f);
			float fTemp70 = 1.0f - fTemp2;
			output0[i0] = FAUSTFLOAT(fTemp12 * fTemp70 + fRec1[0] * fTemp3);
			float fTemp71 = float(input1[i0]);
			float fTemp72 = fTemp71 + fTemp11 * fRec21[1];
			float fTemp73 = fTemp27 - 3.1415927f * fTemp26 * (1.0f - std::cos(fTemp22));
			float fTemp74 = 0.5f * std::min<float>(2.984513f, fConst3 * fTemp73);
			float fTemp75 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp74));
			float fTemp76 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp74));
			float fTemp77 = std::min<float>(3.4028235e+38f, 0.25f / fTemp76);
			float fTemp78 = std::sqrt(4.0f * fTemp75 * fTemp77);
			float fTemp79 = fTemp76 * fTemp78 * (fTemp77 * std::min<float>(3.4028235e+38f, 1.0f / fTemp78) * ((4.0f * fTemp75 - fTemp15) * fRec25[1] + 16.0f * fTemp14 * fTemp72) + fRec26[1]);
			float fTemp80 = fRec25[1] - 2.0f * fTemp79;
			fRec25[0] = ((std::fabs(fTemp80) > 1.1754944e-38f) ? fTemp80 : 0.0f);
			float fTemp81 = fRec25[1] - fTemp79;
			float fTemp82 = fRec26[1] + 2.0f * fTemp78 * fTemp81;
			fRec26[0] = ((std::fabs(fTemp82) > 1.1754944e-38f) ? fTemp82 : 0.0f);
			float fRec27 = ((std::fabs(fTemp81) > 1.1754944e-38f) ? fTemp81 : 0.0f);
			float fTemp83 = 0.5f * std::min<float>(2.984513f, fConst4 * fTemp73);
			float fTemp84 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp83));
			float fTemp85 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp83));
			float fTemp86 = std::min<float>(3.4028235e+38f, 0.25f / fTemp85);
			float fTemp87 = std::sqrt(4.0f * fTemp84 * fTemp86);
			float fTemp88 = fTemp85 * fTemp87 * (fTemp86 * std::min<float>(3.4028235e+38f, 1.0f / fTemp87) * ((4.0f * fTemp84 - fTemp15) * fRec22[1] + 16.0f * fTemp14 * (fTemp72 - fRec27)) + fRec23[1]);
			float fTemp89 = fRec22[1] - 2.0f * fTemp88;
			fRec22[0] = ((std::fabs(fTemp89) > 1.1754944e-38f) ? fTemp89 : 0.0f);
			float fTemp90 = fRec22[1] - fTemp88;
			float fTemp91 = fRec23[1] + 2.0f * fTemp87 * fTemp90;
			fRec23[0] = ((std::fabs(fTemp91) > 1.1754944e-38f) ? fTemp91 : 0.0f);
			float fRec24 = ((std::fabs(fTemp90) > 1.1754944e-38f) ? fTemp90 : 0.0f);
			float fTemp92 = fRec27 + fRec24;
			float fTemp93 = 0.5f * std::min<float>(2.984513f, fConst5 * fTemp73);
			float fTemp94 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp93));
			float fTemp95 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp93));
			float fTemp96 = std::min<float>(3.4028235e+38f, 0.25f / fTemp95);
			float fTemp97 = std::sqrt(4.0f * fTemp94 * fTemp96);
			float fTemp98 = fTemp95 * fTemp97 * (fTemp96 * std::min<float>(3.4028235e+38f, 1.0f / fTemp97) * ((4.0f * fTemp94 - fTemp15) * fRec28[1] + 16.0f * fTemp14 * (fTemp72 - fTemp92)) + fRec29[1]);
			float fTemp99 = fRec28[1] - 2.0f * fTemp98;
			fRec28[0] = ((std::fabs(fTemp99) > 1.1754944e-38f) ? fTemp99 : 0.0f);
			float fTemp100 = fRec28[1] - fTemp98;
			float fTemp101 = fRec29[1] + 2.0f * fTemp97 * fTemp100;
			fRec29[0] = ((std::fabs(fTemp101) > 1.1754944e-38f) ? fTemp101 : 0.0f);
			float fRec30 = ((std::fabs(fTemp100) > 1.1754944e-38f) ? fTemp100 : 0.0f);
			float fTemp102 = fRec30 + fTemp92;
			float fTemp103 = 0.5f * std::min<float>(2.984513f, fConst6 * fTemp73);
			float fTemp104 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::sin(fTemp103));
			float fTemp105 = fTemp31 + fTemp30 * Faust_Phaser_faustpower2_f(std::cos(fTemp103));
			float fTemp106 = std::min<float>(3.4028235e+38f, 0.25f / fTemp105);
			float fTemp107 = std::sqrt(4.0f * fTemp104 * fTemp106);
			float fTemp108 = fTemp105 * fTemp107 * (fTemp106 * std::min<float>(3.4028235e+38f, 1.0f / fTemp107) * ((4.0f * fTemp104 - fTemp15) * fRec31[1] + 16.0f * fTemp14 * (fTemp72 - fTemp102)) + fRec32[1]);
			float fTemp109 = fRec31[1] - 2.0f * fTemp108;
			fRec31[0] = ((std::fabs(fTemp109) > 1.1754944e-38f) ? fTemp109 : 0.0f);
			float fTemp110 = fRec31[1] - fTemp108;
			float fTemp111 = fRec32[1] + 2.0f * fTemp107 * fTemp110;
			fRec32[0] = ((std::fabs(fTemp111) > 1.1754944e-38f) ? fTemp111 : 0.0f);
			float fRec33 = ((std::fabs(fTemp110) > 1.1754944e-38f) ? fTemp110 : 0.0f);
			float fTemp112 = fTemp72 - (fRec33 + fTemp102);
			fRec21[0] = ((std::fabs(fTemp112) > 1.1754944e-38f) ? fTemp112 : 0.0f);
			output1[i0] = FAUSTFLOAT(fTemp71 * fTemp70 + fRec21[0] * fTemp3);
			iVec0[1] = iVec0[0];
			fVec1[1] = fVec1[0];
			fRec0[1] = fRec0[0];
			fVec2[1] = fVec2[0];
			fRec8[1] = fRec8[0];
			fVec3[1] = fVec3[0];
			fRec9[1] = fRec9[0];
			fVec4[1] = fVec4[0];
			fRec12[1] = fRec12[0];
			iRec10[1] = iRec10[0];
			iRec11[1] = iRec11[0];
			fVec5[1] = fVec5[0];
			fRec13[1] = fRec13[0];
			fVec6[1] = fVec6[0];
			fRec14[1] = fRec14[0];
			fRec5[1] = fRec5[0];
			fRec6[1] = fRec6[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec15[1] = fRec15[0];
			fRec16[1] = fRec16[0];
			fRec18[1] = fRec18[0];
			fRec19[1] = fRec19[0];
			fRec1[1] = fRec1[0];
			fRec25[1] = fRec25[0];
			fRec26[1] = fRec26[0];
			fRec22[1] = fRec22[0];
			fRec23[1] = fRec23[0];
			fRec28[1] = fRec28[0];
			fRec29[1] = fRec29[0];
			fRec31[1] = fRec31[0];
			fRec32[1] = fRec32[0];
			fRec21[1] = fRec21[0];
		}
	}

};

#endif
