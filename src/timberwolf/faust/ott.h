// D:\workspace\Fanan_web_hub\src\timberwolf\faust\ott.h
// Written by the Faust compiler (2.70.3) from ott.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "ott"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Ott -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Ott_H__
#define  __Faust_Ott_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

/* link with : "" */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Ott
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

static float Faust_Ott_faustpower2_f(float value) {
	return value * value;
}

class Faust_Ott : public FaustDsp {
	
 private:
	float fConst13;
	float fConst14;
	float fConst15;
	
	int fSampleRate;
	float fConst1;
	float fConst2;
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	float fConst3;
	float fRec9[2];
	int iRec10[2];
	float fConst4;
	float fConst5;
	float fRec5[2];
	float fConst6;
	float fRec6[2];
	float fRec1[2];
	float fRec2[2];
	float fConst7;
	float fConst8;
	float fConst9;
	float fConst10;
	float fRec11[2];
	float fConst11;
	float fRec12[2];
	float fRec15[2];
	float fRec16[2];
	float fRec23[2];
	float fRec24[2];
	float fRec19[2];
	float fRec20[2];
	float fRec27[2];
	float fRec28[2];
	float fRec31[2];
	float fRec32[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec35[2];
	float fConst12;
	float fRec0[2];
	FAUSTFLOAT fHslider2;
	float fVec2[2];
	float fRec36[2];
	FAUSTFLOAT fHslider3;
	float fVec3[2];
	float fRec37[2];
	FAUSTFLOAT fHslider4;
	float fVec4[2];
	float fRec38[2];
	float fRec40[2];
	float fRec41[2];
	float fRec43[2];
	float fRec44[2];
	float fRec39[2];
	float fRec50[2];
	float fRec51[2];
	float fRec47[2];
	float fRec48[2];
	float fRec56[2];
	float fRec57[2];
	float fRec53[2];
	float fRec54[2];
	float fRec46[2];
	FAUSTFLOAT fHslider5;
	float fVec5[2];
	float fRec59[2];
	
 public:
	Faust_Ott() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Ott -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("compressors.lib/name", "Faust Compressor Effect Library");
		m->declare("compressors.lib/version", "1.7.2");
		m->declare("compressors.lib/xfer_ott:author", "David Braun");
		m->declare("compressors.lib/xfer_ott:license", "MIT");
		m->declare("filename", "ott.dsp");
		m->declare("filters.lib/crossover2LR4:author", "Dario Sanfilippo");
		m->declare("filters.lib/crossover2LR4:copyright", "Copyright (C) 2022 Dario Sanfilippo <sanfilippo.dario@gmail.com>");
		m->declare("filters.lib/crossover2LR4:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/crossover3LR4:author", "Dario Sanfilippo");
		m->declare("filters.lib/crossover3LR4:copyright", "Copyright (C) 2022 Dario Sanfilippo <sanfilippo.dario@gmail.com>");
		m->declare("filters.lib/crossover3LR4:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/highpassLR4:author", "Dario Sanfilippo");
		m->declare("filters.lib/highpassLR4:copyright", "Copyright (C) 2022 Dario Sanfilippo <sanfilippo.dario@gmail.com>");
		m->declare("filters.lib/highpassLR4:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpassLR4:author", "Dario Sanfilippo");
		m->declare("filters.lib/lowpassLR4:copyright", "Copyright (C) 2022 Dario Sanfilippo <sanfilippo.dario@gmail.com>");
		m->declare("filters.lib/lowpassLR4:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/svf:author", "Oleg Nesterov");
		m->declare("filters.lib/svf:copyright", "Copyright (C) 2020 Oleg Nesterov <oleg@redhat.com>");
		m->declare("filters.lib/svf:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "ott");
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
		fConst1 = std::tan(std::max<float>(0.0f, std::min<float>(277.40262f / fConst0, 1.5676547f)));
		fConst2 = fConst1 + 1.4142135f;
		fConst3 = 1.0f - 44.1f / fConst0;
		fConst4 = fConst1 * std::min<float>(3.4028235e+38f, 1.0f / (fConst1 * fConst2 + 1.0f));
		fConst5 = 2.0f * fConst4;
		fConst6 = 2.0f * fConst1;
		fConst7 = std::tan(std::max<float>(0.0f, std::min<float>(7853.9814f / fConst0, 1.5676547f)));
		fConst8 = fConst7 + 1.4142135f;
		fConst9 = fConst7 * std::min<float>(3.4028235e+38f, 1.0f / (fConst7 * fConst8 + 1.0f));
		fConst10 = 2.0f * fConst9;
		fConst11 = 2.0f * fConst7;
		fConst12 = 1.0f / fConst0;
		fConst13 = 0.5f * std::max<float>(-(103.54432f / fConst0), -8e+01f);
		fConst14 = 0.5f * std::max<float>(-(166.66667f / fConst0), -8e+01f);
		fConst15 = 0.5f * std::max<float>(-(160.89146f / fConst0), -8e+01f);
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(1e+02f);
		fHslider2 = FAUSTFLOAT(1e+02f);
		fHslider3 = FAUSTFLOAT(1e+02f);
		fHslider4 = FAUSTFLOAT(1e+02f);
		fHslider5 = FAUSTFLOAT(0.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec9[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			iRec10[l2] = 0;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec5[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec6[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec1[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec2[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec11[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec12[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec15[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec16[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec23[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec24[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec19[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec20[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec27[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec28[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec31[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec32[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fVec1[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec35[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec0[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fVec2[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec36[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fVec3[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec37[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 2; l26 = l26 + 1) {
			fVec4[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec38[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec40[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec41[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2; l30 = l30 + 1) {
			fRec43[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 2; l31 = l31 + 1) {
			fRec44[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fRec39[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec50[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			fRec51[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 2; l35 = l35 + 1) {
			fRec47[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			fRec48[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fRec56[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec57[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 2; l39 = l39 + 1) {
			fRec53[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 2; l40 = l40 + 1) {
			fRec54[l40] = 0.0f;
		}
		for (int l41 = 0; l41 < 2; l41 = l41 + 1) {
			fRec46[l41] = 0.0f;
		}
		for (int l42 = 0; l42 < 2; l42 = l42 + 1) {
			fVec5[l42] = 0.0f;
		}
		for (int l43 = 0; l43 < 2; l43 = l43 + 1) {
			fRec59[l43] = 0.0f;
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
	
	virtual Faust_Ott* clone() {
		return new Faust_Ott();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("ott");
		ui_interface->addHorizontalSlider("Depth", &fHslider2, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Downward", &fHslider3, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("In Gain", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-24.0f), FAUSTFLOAT(19.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Out Gain", &fHslider5, FAUSTFLOAT(0.0f), FAUSTFLOAT(-24.0f), FAUSTFLOAT(19.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Time", &fHslider1, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+03f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Upward", &fHslider4, FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = float(fHslider0);
		float fSlow1 = float(fHslider1);
		float fSlow2 = 0.01f * fSlow1;
		float fSlow3 = float(fHslider2);
		float fSlow4 = 0.01f * fSlow3;
		float fSlow5 = float(fHslider3);
		float fSlow6 = 0.01f * fSlow5;
		float fSlow7 = float(fHslider4);
		float fSlow8 = 0.01f * fSlow7;
		float fSlow9 = float(fHslider5);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst3 * (std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0) + fRec9[1]);
			fRec9[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = std::pow(1e+01f, 0.05f * (fSlow0 + fRec9[0]));
			iRec10[0] = (iRec10[1] + 1) % 4;
			float fTemp2 = 1e-30f * (float(iRec10[0] == 0) + (1.0f - float(iRec10[0] == 2)));
			float fTemp3 = fTemp2 + float(input1[i0]) * fTemp1;
			float fTemp4 = fTemp3 - (fConst2 * fRec5[1] + fRec6[1]);
			float fTemp5 = fRec5[1] + fConst5 * fTemp4;
			fRec5[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			float fTemp6 = fRec5[1] + fConst4 * fTemp4;
			float fTemp7 = fRec6[1] + fConst6 * fTemp6;
			fRec6[0] = ((std::fabs(fTemp7) > 1.1754944e-38f) ? fTemp7 : 0.0f);
			float fRec7 = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp8 = fRec6[1] + fConst1 * fTemp6;
			float fRec8 = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			float fTemp9 = fRec8 + 1.4142135f * fRec7;
			float fTemp10 = fTemp3 - (fTemp9 + fConst2 * fRec1[1] + fRec2[1]);
			float fTemp11 = fRec1[1] + fConst5 * fTemp10;
			fRec1[0] = ((std::fabs(fTemp11) > 1.1754944e-38f) ? fTemp11 : 0.0f);
			float fTemp12 = fRec1[1] + fConst4 * fTemp10;
			float fTemp13 = fRec2[1] + fConst6 * fTemp12;
			fRec2[0] = ((std::fabs(fTemp13) > 1.1754944e-38f) ? fTemp13 : 0.0f);
			float fRec3 = ((std::fabs(fTemp12) > 1.1754944e-38f) ? fTemp12 : 0.0f);
			float fTemp14 = fRec2[1] + fConst1 * fTemp12;
			float fRec4 = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			float fTemp15 = fRec8 + fRec4;
			float fTemp16 = fRec7 + fRec3;
			float fTemp17 = 1.4142135f * fTemp16 + fTemp15;
			float fTemp18 = fTemp3 - (fTemp17 + fConst8 * fRec11[1] + fRec12[1]);
			float fTemp19 = fRec11[1] + fConst10 * fTemp18;
			fRec11[0] = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = fRec11[1] + fConst9 * fTemp18;
			float fTemp21 = fRec12[1] + fConst11 * fTemp20;
			fRec12[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fRec13 = ((std::fabs(fTemp20) > 1.1754944e-38f) ? fTemp20 : 0.0f);
			float fTemp22 = fRec12[1] + fConst7 * fTemp20;
			float fRec14 = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			float fTemp23 = fRec14 + fTemp15;
			float fTemp24 = fRec13 + fTemp16;
			float fTemp25 = 1.4142135f * fTemp24 + fTemp23;
			float fTemp26 = fTemp3 - (fTemp25 + fConst8 * fRec15[1] + fRec16[1]);
			float fTemp27 = fRec15[1] + fConst10 * fTemp26;
			fRec15[0] = ((std::fabs(fTemp27) > 1.1754944e-38f) ? fTemp27 : 0.0f);
			float fTemp28 = fRec15[1] + fConst9 * fTemp26;
			float fTemp29 = fRec16[1] + fConst11 * fTemp28;
			fRec16[0] = ((std::fabs(fTemp29) > 1.1754944e-38f) ? fTemp29 : 0.0f);
			float fRec17 = ((std::fabs(fTemp28) > 1.1754944e-38f) ? fTemp28 : 0.0f);
			float fTemp30 = fRec16[1] + fConst7 * fTemp28;
			float fRec18 = ((std::fabs(fTemp30) > 1.1754944e-38f) ? fTemp30 : 0.0f);
			float fTemp31 = fTemp3 - (1.4142135f * (fRec17 + fTemp24) + fRec18 + fTemp23);
			float fTemp32 = float(input0[i0]) * fTemp1 + fTemp2;
			float fTemp33 = fTemp32 - (fConst2 * fRec23[1] + fRec24[1]);
			float fTemp34 = fRec23[1] + fConst5 * fTemp33;
			fRec23[0] = ((std::fabs(fTemp34) > 1.1754944e-38f) ? fTemp34 : 0.0f);
			float fTemp35 = fRec23[1] + fConst4 * fTemp33;
			float fTemp36 = fRec24[1] + fConst6 * fTemp35;
			fRec24[0] = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fRec25 = ((std::fabs(fTemp35) > 1.1754944e-38f) ? fTemp35 : 0.0f);
			float fTemp37 = fRec24[1] + fConst1 * fTemp35;
			float fRec26 = ((std::fabs(fTemp37) > 1.1754944e-38f) ? fTemp37 : 0.0f);
			float fTemp38 = fRec26 + 1.4142135f * fRec25;
			float fTemp39 = fTemp32 - (fTemp38 + fConst2 * fRec19[1] + fRec20[1]);
			float fTemp40 = fRec19[1] + fConst5 * fTemp39;
			fRec19[0] = ((std::fabs(fTemp40) > 1.1754944e-38f) ? fTemp40 : 0.0f);
			float fTemp41 = fRec19[1] + fConst4 * fTemp39;
			float fTemp42 = fRec20[1] + fConst6 * fTemp41;
			fRec20[0] = ((std::fabs(fTemp42) > 1.1754944e-38f) ? fTemp42 : 0.0f);
			float fRec21 = ((std::fabs(fTemp41) > 1.1754944e-38f) ? fTemp41 : 0.0f);
			float fTemp43 = fRec20[1] + fConst1 * fTemp41;
			float fRec22 = ((std::fabs(fTemp43) > 1.1754944e-38f) ? fTemp43 : 0.0f);
			float fTemp44 = fRec26 + fRec22;
			float fTemp45 = fRec25 + fRec21;
			float fTemp46 = 1.4142135f * fTemp45 + fTemp44;
			float fTemp47 = fTemp32 - (fTemp46 + fConst8 * fRec27[1] + fRec28[1]);
			float fTemp48 = fRec27[1] + fConst10 * fTemp47;
			fRec27[0] = ((std::fabs(fTemp48) > 1.1754944e-38f) ? fTemp48 : 0.0f);
			float fTemp49 = fRec27[1] + fConst9 * fTemp47;
			float fTemp50 = fRec28[1] + fConst11 * fTemp49;
			fRec28[0] = ((std::fabs(fTemp50) > 1.1754944e-38f) ? fTemp50 : 0.0f);
			float fRec29 = ((std::fabs(fTemp49) > 1.1754944e-38f) ? fTemp49 : 0.0f);
			float fTemp51 = fRec28[1] + fConst7 * fTemp49;
			float fRec30 = ((std::fabs(fTemp51) > 1.1754944e-38f) ? fTemp51 : 0.0f);
			float fTemp52 = fRec30 + fTemp44;
			float fTemp53 = fRec29 + fTemp45;
			float fTemp54 = 1.4142135f * fTemp53 + fTemp52;
			float fTemp55 = fTemp32 - (fTemp54 + fConst8 * fRec31[1] + fRec32[1]);
			float fTemp56 = fRec31[1] + fConst10 * fTemp55;
			fRec31[0] = ((std::fabs(fTemp56) > 1.1754944e-38f) ? fTemp56 : 0.0f);
			float fTemp57 = fRec31[1] + fConst9 * fTemp55;
			float fTemp58 = fRec32[1] + fConst11 * fTemp57;
			fRec32[0] = ((std::fabs(fTemp58) > 1.1754944e-38f) ? fTemp58 : 0.0f);
			float fRec33 = ((std::fabs(fTemp57) > 1.1754944e-38f) ? fTemp57 : 0.0f);
			float fTemp59 = fRec32[1] + fConst7 * fTemp57;
			float fRec34 = ((std::fabs(fTemp59) > 1.1754944e-38f) ? fTemp59 : 0.0f);
			float fTemp60 = fTemp32 - (1.4142135f * (fRec33 + fTemp53) + fRec34 + fTemp52);
			float fTemp61 = 0.5f * (Faust_Ott_faustpower2_f(fTemp60) + Faust_Ott_faustpower2_f(fTemp31));
			fVec1[0] = fSlow1;
			float fTemp62 = fConst3 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow1 - fVec1[1]))) + fRec35[1]);
			fRec35[0] = ((std::fabs(fTemp62) > 1.1754944e-38f) ? fTemp62 : 0.0f);
			float fTemp63 = std::max<float>(0.0f, fSlow2 + fRec35[0]);
			float fTemp64 = std::min<float>(1e+02f, 1e+02f * fTemp63);
			int iTemp65 = fTemp64 < 5e+01f;
			int iTemp66 = fTemp64 < 1e+02f;
			float fTemp67 = fTemp64 + -5e+01f;
			float fTemp68 = std::max<float>(1.0f, fTemp63);
			float fTemp69 = 0.5f * std::max<float>(-(fConst12 / std::max<float>(0.0001f, fTemp68 * ((iTemp65) ? 0.00702632f : ((iTemp66) ? 6.69076e-05f * fTemp67 + 0.00702632f : 0.0103717f)))), -8e+01f);
			float fTemp70 = fRec0[1] + ((fTemp61 > fRec0[1]) ? -(2.0f * std::exp(fConst13) * sinhf(fConst13)) : -(2.0f * std::exp(fTemp69) * sinhf(fTemp69))) * (fTemp61 - fRec0[1]);
			float fTemp71 = fTemp70 * float(fTemp70 > 1e-30f);
			fRec0[0] = ((std::fabs(fTemp71) > 1.1754944e-38f) ? fTemp71 : 0.0f);
			float fTemp72 = 1e+01f * std::log10(std::max<float>(fRec0[0], 1e-20f));
			float fTemp73 = fTemp72 + 41.6879f;
			fVec2[0] = fSlow3;
			float fTemp74 = fConst3 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow3 - fVec2[1]))) + fRec36[1]);
			fRec36[0] = ((std::fabs(fTemp74) > 1.1754944e-38f) ? fTemp74 : 0.0f);
			float fTemp75 = fSlow4 + fRec36[0];
			fVec3[0] = fSlow5;
			float fTemp76 = fConst3 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow5 - fVec3[1]))) + fRec37[1]);
			fRec37[0] = ((std::fabs(fTemp76) > 1.1754944e-38f) ? fTemp76 : 0.0f);
			float fTemp77 = fSlow6 + fRec37[0];
			int iTemp78 = fTemp77 < 1.0f;
			int iTemp79 = fTemp77 < 0.8f;
			int iTemp80 = fTemp77 < 0.6f;
			int iTemp81 = fTemp77 < 0.4f;
			int iTemp82 = fTemp77 < 0.2f;
			float fTemp83 = ((iTemp78) ? ((iTemp79) ? ((iTemp80) ? ((iTemp81) ? ((iTemp82) ? ((fTemp77 < 0.0f) ? 0.0f : ((iTemp82) ? 0.94858f * fTemp77 : 0.189716f)) : ((iTemp81) ? 0.99824f * (fSlow6 + fRec37[0] + -0.2f) + 0.189716f : 0.389364f)) : ((iTemp80) ? 1.00337f * (fSlow6 + fRec37[0] + -0.4f) + 0.389364f : 0.590038f)) : ((iTemp79) ? 1.01539f * (fSlow6 + fRec37[0] + -0.6f) + 0.590038f : 0.793116f)) : ((iTemp78) ? 1.03442f * (fSlow6 + fRec37[0] + -0.8f) + 0.793116f : 1.0f)) : ((fTemp77 < 2.0f) ? fTemp77 : 2.0f));
			float fTemp84 = fTemp75 * fTemp83;
			float fTemp85 = -47.4393f - fTemp72;
			fVec4[0] = fSlow7;
			float fTemp86 = fConst3 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow7 - fVec4[1]))) + fRec38[1]);
			fRec38[0] = ((std::fabs(fTemp86) > 1.1754944e-38f) ? fTemp86 : 0.0f);
			float fTemp87 = std::max<float>(0.0f, fTemp75) * std::max<float>(0.0f, fSlow8 + fRec38[0]);
			float fTemp88 = std::pow(1e+01f, 0.05f * std::max<float>(-25.95f, 14.064f * fTemp75 * (fTemp75 * (0.38284f * fTemp75 + -1.22723f) + 1.8444f) + std::min<float>(35.872f, 0.74738f * fTemp87 * (std::max<float>(fTemp85, 0.0f) + 0.0338f * std::log(std::exp(-29.585798f * std::fabs(fTemp85)) + 1.0f))) - std::min<float>(1.0f, fTemp84 * (fTemp75 * (0.05778f - 0.03064f * fTemp75) + 0.97286f)) * (std::max<float>(fTemp73, 0.0f) + 0.5f * std::log(std::exp(-2.0f * std::fabs(fTemp73)) + 1.0f))));
			float fTemp89 = fRec14 - (fConst8 * fRec40[1] + fRec41[1]);
			float fTemp90 = fRec40[1] + fConst10 * fTemp89;
			fRec40[0] = ((std::fabs(fTemp90) > 1.1754944e-38f) ? fTemp90 : 0.0f);
			float fTemp91 = fRec40[1] + fConst9 * fTemp89;
			float fTemp92 = fRec41[1] + fConst11 * fTemp91;
			fRec41[0] = ((std::fabs(fTemp92) > 1.1754944e-38f) ? fTemp92 : 0.0f);
			float fTemp93 = fRec41[1] + fConst7 * fTemp91;
			float fRec42 = ((std::fabs(fTemp93) > 1.1754944e-38f) ? fTemp93 : 0.0f);
			float fTemp94 = fRec30 - (fConst8 * fRec43[1] + fRec44[1]);
			float fTemp95 = fRec43[1] + fConst10 * fTemp94;
			fRec43[0] = ((std::fabs(fTemp95) > 1.1754944e-38f) ? fTemp95 : 0.0f);
			float fTemp96 = fRec43[1] + fConst9 * fTemp94;
			float fTemp97 = fRec44[1] + fConst11 * fTemp96;
			fRec44[0] = ((std::fabs(fTemp97) > 1.1754944e-38f) ? fTemp97 : 0.0f);
			float fTemp98 = fRec44[1] + fConst7 * fTemp96;
			float fRec45 = ((std::fabs(fTemp98) > 1.1754944e-38f) ? fTemp98 : 0.0f);
			float fTemp99 = 0.5f * (Faust_Ott_faustpower2_f(fRec45) + Faust_Ott_faustpower2_f(fRec42));
			int iTemp100 = fTemp64 < 0.0f;
			float fTemp101 = 0.5f * std::max<float>(-(fConst12 / std::max<float>(0.0001f, fTemp68 * ((iTemp65) ? ((iTemp100) ? 0.0118089f : ((iTemp65) ? 0.00011245f * fTemp64 + 0.0118089f : 0.0174314f)) : ((iTemp66) ? 0.000268912f * fTemp67 + 0.0174314f : 0.030877f)))), -8e+01f);
			float fTemp102 = fRec39[1] + ((fTemp99 > fRec39[1]) ? -(2.0f * std::exp(fConst14) * sinhf(fConst14)) : -(2.0f * std::exp(fTemp101) * sinhf(fTemp101))) * (fTemp99 - fRec39[1]);
			float fTemp103 = fTemp102 * float(fTemp102 > 1e-30f);
			fRec39[0] = ((std::fabs(fTemp103) > 1.1754944e-38f) ? fTemp103 : 0.0f);
			float fTemp104 = 1e+01f * std::log10(std::max<float>(fRec39[0], 1e-20f));
			float fTemp105 = fTemp104 + 34.8204f;
			float fTemp106 = -46.5885f - fTemp104;
			float fTemp107 = std::pow(1e+01f, 0.05f * std::max<float>(-31.7f, 9.05203f * fTemp75 * (fTemp75 * (0.19373f * fTemp75 + -0.83224f) + 1.63851f) + std::min<float>(36.285f, 0.75601f * fTemp87 * (std::max<float>(fTemp106, 0.0f) + 0.4509f * std::log(std::exp(-2.2177866f * std::fabs(fTemp106)) + 1.0f))) - std::min<float>(1.0f, fTemp84 * (fTemp75 * (0.01634f * fTemp75 + 0.02205f) + 0.96161f)) * (std::max<float>(fTemp105, 0.0f) + 0.530023f * std::log(std::exp(-1.8867105f * std::fabs(fTemp105)) + 1.0f))));
			float fTemp108 = fRec8 - (fConst2 * fRec50[1] + fRec51[1]);
			float fTemp109 = fRec50[1] + fConst5 * fTemp108;
			fRec50[0] = ((std::fabs(fTemp109) > 1.1754944e-38f) ? fTemp109 : 0.0f);
			float fTemp110 = fRec50[1] + fConst4 * fTemp108;
			float fTemp111 = fRec51[1] + fConst6 * fTemp110;
			fRec51[0] = ((std::fabs(fTemp111) > 1.1754944e-38f) ? fTemp111 : 0.0f);
			float fTemp112 = fRec51[1] + fConst1 * fTemp110;
			float fRec52 = ((std::fabs(fTemp112) > 1.1754944e-38f) ? fTemp112 : 0.0f);
			float fTemp113 = fRec52 - (fConst8 * fRec47[1] + fRec48[1]);
			float fTemp114 = fRec47[1] + fConst10 * fTemp113;
			fRec47[0] = ((std::fabs(fTemp114) > 1.1754944e-38f) ? fTemp114 : 0.0f);
			float fTemp115 = fRec47[1] + fConst9 * fTemp113;
			float fTemp116 = fRec48[1] + fConst11 * fTemp115;
			fRec48[0] = ((std::fabs(fTemp116) > 1.1754944e-38f) ? fTemp116 : 0.0f);
			float fRec49 = ((std::fabs(fTemp115) > 1.1754944e-38f) ? fTemp115 : 0.0f);
			float fTemp117 = fRec52 - 2.828427f * fRec49;
			float fTemp118 = fRec26 - (fConst2 * fRec56[1] + fRec57[1]);
			float fTemp119 = fRec56[1] + fConst5 * fTemp118;
			fRec56[0] = ((std::fabs(fTemp119) > 1.1754944e-38f) ? fTemp119 : 0.0f);
			float fTemp120 = fRec56[1] + fConst4 * fTemp118;
			float fTemp121 = fRec57[1] + fConst6 * fTemp120;
			fRec57[0] = ((std::fabs(fTemp121) > 1.1754944e-38f) ? fTemp121 : 0.0f);
			float fTemp122 = fRec57[1] + fConst1 * fTemp120;
			float fRec58 = ((std::fabs(fTemp122) > 1.1754944e-38f) ? fTemp122 : 0.0f);
			float fTemp123 = fRec58 - (fConst8 * fRec53[1] + fRec54[1]);
			float fTemp124 = fRec53[1] + fConst10 * fTemp123;
			fRec53[0] = ((std::fabs(fTemp124) > 1.1754944e-38f) ? fTemp124 : 0.0f);
			float fTemp125 = fRec53[1] + fConst9 * fTemp123;
			float fTemp126 = fRec54[1] + fConst11 * fTemp125;
			fRec54[0] = ((std::fabs(fTemp126) > 1.1754944e-38f) ? fTemp126 : 0.0f);
			float fRec55 = ((std::fabs(fTemp125) > 1.1754944e-38f) ? fTemp125 : 0.0f);
			float fTemp127 = fRec58 - 2.828427f * fRec55;
			float fTemp128 = 0.5f * (Faust_Ott_faustpower2_f(fTemp127) + Faust_Ott_faustpower2_f(fTemp117));
			float fTemp129 = 0.5f * std::max<float>(-(fConst12 / std::max<float>(0.0001f, ((iTemp65) ? ((iTemp100) ? 0.0100376f : ((iTemp65) ? 9.5582e-05f * fTemp64 + 0.0100376f : 0.0148167f)) : ((iTemp66) ? 0.000321206f * fTemp67 + 0.0148167f : 0.030877f)) * fTemp68)), -8e+01f);
			float fTemp130 = fRec46[1] + ((fTemp128 > fRec46[1]) ? -(2.0f * std::exp(fConst15) * sinhf(fConst15)) : -(2.0f * std::exp(fTemp129) * sinhf(fTemp129))) * (fTemp128 - fRec46[1]);
			float fTemp131 = fTemp130 * float(fTemp130 > 1e-30f);
			fRec46[0] = ((std::fabs(fTemp131) > 1.1754944e-38f) ? fTemp131 : 0.0f);
			float fTemp132 = 1e+01f * std::log10(std::max<float>(fRec46[0], 1e-20f));
			float fTemp133 = fTemp132 + 38.9401f;
			float fTemp134 = -46.0171f - fTemp132;
			float fTemp135 = std::pow(1e+01f, 0.05f * std::max<float>(-26.26f, 14.0348f * fTemp75 * (fTemp75 * (0.37606f * fTemp75 + -1.21424f) + 1.83819f) + std::min<float>(35.901f, 0.74786f * fTemp87 * (std::max<float>(fTemp134, 0.0f) + 0.0552f * std::log(std::exp(-18.115942f * std::fabs(fTemp134)) + 1.0f))) - std::min<float>(1.0f, fTemp75 * (fTemp75 * (0.00337f * fTemp75 + 0.03154f) + 0.96509f) * fTemp83) * (std::max<float>(fTemp133, 0.0f) + 0.5f * std::log(std::exp(-2.0f * std::fabs(fTemp133)) + 1.0f))));
			fVec5[0] = fSlow9;
			float fTemp136 = fConst3 * (std::min<float>(3.4028235e+38f, fVec5[1] - fSlow9) + fRec59[1]);
			fRec59[0] = ((std::fabs(fTemp136) > 1.1754944e-38f) ? fTemp136 : 0.0f);
			float fTemp137 = std::pow(1e+01f, 0.05f * (fSlow9 + fRec59[0]));
			output0[i0] = FAUSTFLOAT(fTemp137 * (fTemp127 * fTemp135 + fRec45 * fTemp107 + fTemp60 * fTemp88));
			output1[i0] = FAUSTFLOAT(fTemp137 * (fTemp117 * fTemp135 + fRec42 * fTemp107 + fTemp31 * fTemp88));
			fVec0[1] = fVec0[0];
			fRec9[1] = fRec9[0];
			iRec10[1] = iRec10[0];
			fRec5[1] = fRec5[0];
			fRec6[1] = fRec6[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec11[1] = fRec11[0];
			fRec12[1] = fRec12[0];
			fRec15[1] = fRec15[0];
			fRec16[1] = fRec16[0];
			fRec23[1] = fRec23[0];
			fRec24[1] = fRec24[0];
			fRec19[1] = fRec19[0];
			fRec20[1] = fRec20[0];
			fRec27[1] = fRec27[0];
			fRec28[1] = fRec28[0];
			fRec31[1] = fRec31[0];
			fRec32[1] = fRec32[0];
			fVec1[1] = fVec1[0];
			fRec35[1] = fRec35[0];
			fRec0[1] = fRec0[0];
			fVec2[1] = fVec2[0];
			fRec36[1] = fRec36[0];
			fVec3[1] = fVec3[0];
			fRec37[1] = fRec37[0];
			fVec4[1] = fVec4[0];
			fRec38[1] = fRec38[0];
			fRec40[1] = fRec40[0];
			fRec41[1] = fRec41[0];
			fRec43[1] = fRec43[0];
			fRec44[1] = fRec44[0];
			fRec39[1] = fRec39[0];
			fRec50[1] = fRec50[0];
			fRec51[1] = fRec51[0];
			fRec47[1] = fRec47[0];
			fRec48[1] = fRec48[0];
			fRec56[1] = fRec56[0];
			fRec57[1] = fRec57[0];
			fRec53[1] = fRec53[0];
			fRec54[1] = fRec54[0];
			fRec46[1] = fRec46[0];
			fVec5[1] = fVec5[0];
			fRec59[1] = fRec59[0];
		}
	}

};

#endif
