// D:\workspace\Fanan_web_hub\src\timberwolf\faust\granular.h
// Written by the Faust compiler (2.70.3) from granular.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "granular"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Granular -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Granular_H__
#define  __Faust_Granular_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Granular
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


class Faust_Granular : public FaustDsp {
	
 private:
	
	int iVec0[2];
	FAUSTFLOAT fHslider0;
	int fSampleRate;
	float fConst0;
	float fConst1;
	int iRec0[2];
	int iVec1[2];
	int IOTA0;
	float fVec2[131072];
	FAUSTFLOAT fHslider1;
	float fVec3[2];
	float fConst2;
	float fRec1[2];
	int iRec3[2];
	FAUSTFLOAT fHslider2;
	FAUSTFLOAT fHslider3;
	float fVec4[2];
	float fRec11[2];
	float fRec2[2];
	int iVec5[2];
	float fRec12[2];
	int iVec6[2];
	float fRec13[2];
	int iVec7[2];
	float fRec14[2];
	int iVec8[2];
	float fRec15[2];
	int iVec9[2];
	float fRec16[2];
	int iVec10[2];
	float fRec17[2];
	int iVec11[2];
	float fRec18[2];
	FAUSTFLOAT fHslider4;
	float fVec12[2];
	float fRec19[2];
	
 public:
	Faust_Granular() {}

	void metadata(Meta* m) { 
		m->declare("analyzers.lib/name", "Faust Analyzer Library");
		m->declare("analyzers.lib/version", "1.6.3");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/sAndH:author", "Romain Michon");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Granular -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "granular.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("misceffects.lib/granular:license", "MIT");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/version", "2.7.4");
		m->declare("name", "granular");
		m->declare("noises.lib/name", "Faust Noise Generator Library");
		m->declare("noises.lib/version", "1.9.2");
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
		fConst0 = std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
		fConst1 = 0.001f * fConst0;
		fConst2 = 1.0f - 44.1f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(8e+01f);
		fHslider1 = FAUSTFLOAT(0.0f);
		fHslider2 = FAUSTFLOAT(3e+01f);
		fHslider3 = FAUSTFLOAT(1.2e+02f);
		fHslider4 = FAUSTFLOAT(5e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			iRec0[l1] = 0;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			iVec1[l2] = 0;
		}
		IOTA0 = 0;
		for (int l3 = 0; l3 < 131072; l3 = l3 + 1) {
			fVec2[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec3[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec1[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			iRec3[l6] = 0;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fVec4[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec11[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec2[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			iVec5[l10] = 0;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec12[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			iVec6[l12] = 0;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec13[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			iVec7[l14] = 0;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec14[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			iVec8[l16] = 0;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec15[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			iVec9[l18] = 0;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fRec16[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			iVec10[l20] = 0;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec17[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			iVec11[l22] = 0;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec18[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fVec12[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec19[l25] = 0.0f;
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
	
	virtual Faust_Granular* clone() {
		return new Faust_Granular();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("granular");
		ui_interface->addHorizontalSlider("Grain", &fHslider0, FAUSTFLOAT(8e+01f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(4e+02f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Jitter", &fHslider2, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(5e+02f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Mix", &fHslider4, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Pitch", &fHslider1, FAUSTFLOAT(0.0f), FAUSTFLOAT(-24.0f), FAUSTFLOAT(24.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Position", &fHslider3, FAUSTFLOAT(1.2e+02f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(1.5e+03f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		int iSlow0 = std::max<int>(1, int(fConst1 * float(fHslider0) + 0.5f));
		int iSlow1 = int(0.125f * float(7 * iSlow0));
		float fSlow2 = float(iSlow0);
		float fSlow3 = 1.0f / fSlow2;
		float fSlow4 = 6.2831855f / fSlow2;
		float fSlow5 = float(fHslider1);
		float fSlow6 = float(fHslider2);
		float fSlow7 = float(fHslider3);
		int iSlow8 = int(0.125f * float(6 * iSlow0));
		int iSlow9 = int(0.125f * float(5 * iSlow0));
		int iSlow10 = int(0.125f * float(4 * iSlow0));
		int iSlow11 = int(0.125f * float(3 * iSlow0));
		int iSlow12 = int(0.125f * float(2 * iSlow0));
		int iSlow13 = int(0.125f * fSlow2);
		float fSlow14 = float(fHslider4);
		float fSlow15 = 0.01f * fSlow14;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			iRec0[0] = (iVec0[1] + iRec0[1]) % iSlow0;
			int iTemp0 = iRec0[0] + 1;
			int iTemp1 = (iSlow1 + iTemp0) % iSlow0;
			iVec1[0] = iTemp1;
			float fTemp2 = float(iTemp1);
			float fTemp3 = fSlow3 * fTemp2;
			float fTemp4 = float(input0[i0]);
			fVec2[IOTA0 & 131071] = fTemp4;
			fVec3[0] = fSlow5;
			float fTemp5 = fConst2 * (std::min<float>(3.4028235e+38f, fVec3[1] - fSlow5) + fRec1[1]);
			fRec1[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			float fTemp6 = 1.0f - std::pow(2.0f, 0.083333336f * (fSlow5 + fRec1[0]));
			int iTemp7 = 1103515245 * (iRec3[1] + 12345);
			int iTemp8 = 1103515245 * (iTemp7 + 12345);
			int iTemp9 = 1103515245 * (iTemp8 + 12345);
			int iTemp10 = 1103515245 * (iTemp9 + 12345);
			int iTemp11 = 1103515245 * (iTemp10 + 12345);
			int iTemp12 = 1103515245 * (iTemp11 + 12345);
			int iTemp13 = 1103515245 * (iTemp12 + 12345);
			iRec3[0] = 1103515245 * (iTemp13 + 12345);
			int iRec4 = iTemp13;
			int iRec5 = iTemp12;
			int iRec6 = iTemp11;
			int iRec7 = iTemp10;
			int iRec8 = iTemp9;
			int iRec9 = iTemp8;
			int iRec10 = iTemp7;
			fVec4[0] = fSlow7;
			float fTemp14 = fConst2 * (std::min<float>(3.4028235e+38f, -(0.001f * (fSlow7 - fVec4[1]))) + fRec11[1]);
			fRec11[0] = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			float fTemp15 = ((iTemp1 < iVec1[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec10)))) : fRec2[1]);
			fRec2[0] = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec2[0] + fTemp6 * fTemp2));
			int iTemp17 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp16)));
			float fTemp18 = std::floor(fTemp16);
			int iTemp19 = (iSlow8 + iTemp0) % iSlow0;
			iVec5[0] = iTemp19;
			float fTemp20 = float(iTemp19);
			float fTemp21 = fSlow3 * fTemp20;
			float fTemp22 = ((iTemp19 < iVec5[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec9)))) : fRec12[1]);
			fRec12[0] = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			float fTemp23 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec12[0] + fTemp6 * fTemp20));
			int iTemp24 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp23)));
			float fTemp25 = std::floor(fTemp23);
			int iTemp26 = (iSlow9 + iTemp0) % iSlow0;
			iVec6[0] = iTemp26;
			float fTemp27 = float(iTemp26);
			float fTemp28 = fSlow3 * fTemp27;
			float fTemp29 = ((iTemp26 < iVec6[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec8)))) : fRec13[1]);
			fRec13[0] = ((std::fabs(fTemp29) > 1.1754944e-38f) ? fTemp29 : 0.0f);
			float fTemp30 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec13[0] + fTemp6 * fTemp27));
			int iTemp31 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp30)));
			float fTemp32 = std::floor(fTemp30);
			int iTemp33 = (iSlow10 + iTemp0) % iSlow0;
			iVec7[0] = iTemp33;
			float fTemp34 = float(iTemp33);
			float fTemp35 = fSlow3 * fTemp34;
			float fTemp36 = ((iTemp33 < iVec7[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec7)))) : fRec14[1]);
			fRec14[0] = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fTemp37 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec14[0] + fTemp6 * fTemp34));
			int iTemp38 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp37)));
			float fTemp39 = std::floor(fTemp37);
			int iTemp40 = (iSlow11 + iTemp0) % iSlow0;
			iVec8[0] = iTemp40;
			float fTemp41 = float(iTemp40);
			float fTemp42 = fSlow3 * fTemp41;
			float fTemp43 = ((iTemp40 < iVec8[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec6)))) : fRec15[1]);
			fRec15[0] = ((std::fabs(fTemp43) > 1.1754944e-38f) ? fTemp43 : 0.0f);
			float fTemp44 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec15[0] + fTemp6 * fTemp41));
			int iTemp45 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp44)));
			float fTemp46 = std::floor(fTemp44);
			int iTemp47 = (iSlow12 + iTemp0) % iSlow0;
			iVec9[0] = iTemp47;
			float fTemp48 = float(iTemp47);
			float fTemp49 = fSlow3 * fTemp48;
			float fTemp50 = ((iTemp47 < iVec9[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec5)))) : fRec16[1]);
			fRec16[0] = ((std::fabs(fTemp50) > 1.1754944e-38f) ? fTemp50 : 0.0f);
			float fTemp51 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec16[0] + fTemp6 * fTemp48));
			int iTemp52 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp51)));
			float fTemp53 = std::floor(fTemp51);
			int iTemp54 = (iSlow13 + iTemp0) % iSlow0;
			iVec10[0] = iTemp54;
			float fTemp55 = float(iTemp54);
			float fTemp56 = fSlow3 * fTemp55;
			float fTemp57 = ((iTemp54 < iVec10[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec4)))) : fRec17[1]);
			fRec17[0] = ((std::fabs(fTemp57) > 1.1754944e-38f) ? fTemp57 : 0.0f);
			float fTemp58 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec17[0] + fTemp6 * fTemp55));
			int iTemp59 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp58)));
			float fTemp60 = std::floor(fTemp58);
			int iTemp61 = iTemp0 % iSlow0;
			iVec11[0] = iTemp61;
			float fTemp62 = float(iTemp61);
			float fTemp63 = fSlow3 * fTemp62;
			float fTemp64 = ((iTemp61 < iVec11[1]) ? fConst0 * (fRec11[0] + 0.001f * (fSlow7 + fSlow6 * std::fabs(4.656613e-10f * float(iRec3[0])))) : fRec18[1]);
			fRec18[0] = ((std::fabs(fTemp64) > 1.1754944e-38f) ? fTemp64 : 0.0f);
			float fTemp65 = std::max<float>(0.0f, std::min<float>(65535.0f, fRec18[0] + fTemp6 * fTemp62));
			int iTemp66 = int(std::min<float>(65537.0f, std::max<float>(-1.0f, fTemp65)));
			float fTemp67 = std::floor(fTemp65);
			fVec12[0] = fSlow14;
			float fTemp68 = fConst2 * (fRec19[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow14 - fVec12[1]))));
			fRec19[0] = ((std::fabs(fTemp68) > 1.1754944e-38f) ? fTemp68 : 0.0f);
			output0[i0] = FAUSTFLOAT(fTemp4 * (1.0f - fRec19[0] - fSlow15) + 0.125f * (fSlow15 + fRec19[0]) * ((fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp66))) & 131071] * (fTemp67 + (1.0f - fTemp65)) + (fTemp65 - fTemp67) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp66 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp62)) * float((fTemp63 >= 0.0f) & (fTemp63 <= 1.0f)) + (fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp59))) & 131071] * (fTemp60 + (1.0f - fTemp58)) + (fTemp58 - fTemp60) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp59 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp55)) * float((fTemp56 >= 0.0f) & (fTemp56 <= 1.0f)) + (fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp52))) & 131071] * (fTemp53 + (1.0f - fTemp51)) + (fTemp51 - fTemp53) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp52 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp48)) * float((fTemp49 >= 0.0f) & (fTemp49 <= 1.0f)) + (fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp45))) & 131071] * (fTemp46 + (1.0f - fTemp44)) + (fTemp44 - fTemp46) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp45 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp41)) * float((fTemp42 >= 0.0f) & (fTemp42 <= 1.0f)) + (fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp38))) & 131071] * (fTemp39 + (1.0f - fTemp37)) + (fTemp37 - fTemp39) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp38 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp34)) * float((fTemp35 >= 0.0f) & (fTemp35 <= 1.0f)) + (fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp31))) & 131071] * (fTemp32 + (1.0f - fTemp30)) + (fTemp30 - fTemp32) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp31 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp27)) * float((fTemp28 >= 0.0f) & (fTemp28 <= 1.0f)) + (fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp24))) & 131071] * (fTemp25 + (1.0f - fTemp23)) + (fTemp23 - fTemp25) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp24 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp20)) * float((fTemp21 >= 0.0f) & (fTemp21 <= 1.0f)) + (fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp17))) & 131071] * (fTemp18 + (1.0f - fTemp16)) + (fTemp16 - fTemp18) * fVec2[(IOTA0 - std::min<int>(65537, std::max<int>(0, iTemp17 + 1))) & 131071]) * (1.0f - std::cos(fSlow4 * fTemp2)) * float((fTemp3 >= 0.0f) & (fTemp3 <= 1.0f))));
			iVec0[1] = iVec0[0];
			iRec0[1] = iRec0[0];
			iVec1[1] = iVec1[0];
			IOTA0 = IOTA0 + 1;
			fVec3[1] = fVec3[0];
			fRec1[1] = fRec1[0];
			iRec3[1] = iRec3[0];
			fVec4[1] = fVec4[0];
			fRec11[1] = fRec11[0];
			fRec2[1] = fRec2[0];
			iVec5[1] = iVec5[0];
			fRec12[1] = fRec12[0];
			iVec6[1] = iVec6[0];
			fRec13[1] = fRec13[0];
			iVec7[1] = iVec7[0];
			fRec14[1] = fRec14[0];
			iVec8[1] = iVec8[0];
			fRec15[1] = fRec15[0];
			iVec9[1] = iVec9[0];
			fRec16[1] = fRec16[0];
			iVec10[1] = iVec10[0];
			fRec17[1] = fRec17[0];
			iVec11[1] = iVec11[0];
			fRec18[1] = fRec18[0];
			fVec12[1] = fVec12[0];
			fRec19[1] = fRec19[0];
		}
	}

};

#endif
