// D:\workspace\Fanan_web_hub\src\timberwolf\faust\peq.h
// Written by the Faust compiler (2.70.3) from peq.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "peq"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Peq -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Peq_H__
#define  __Faust_Peq_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Peq
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


class Faust_Peq : public FaustDsp {
	
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
	float fConst2;
	FAUSTFLOAT fHslider3;
	float fVec2[2];
	float fRec11[2];
	float fConst3;
	FAUSTFLOAT fHslider4;
	float fVec3[2];
	float fRec12[2];
	float fConst4;
	FAUSTFLOAT fHslider5;
	float fVec4[2];
	float fRec13[2];
	FAUSTFLOAT fHslider6;
	float fRec18[2];
	float fRec14[2];
	float fRec15[2];
	FAUSTFLOAT fHslider7;
	float fVec5[2];
	float fRec20[2];
	float fRec24[2];
	float fRec21[2];
	float fRec22[2];
	float fRec8[2];
	float fRec9[2];
	float fRec6[2];
	float fRec2[2];
	float fRec3[2];
	float fRec29[2];
	float fRec26[2];
	float fRec27[2];
	float fRec44[2];
	float fRec40[2];
	float fRec41[2];
	float fRec49[2];
	float fRec46[2];
	float fRec47[2];
	float fRec37[2];
	float fRec38[2];
	float fRec35[2];
	float fRec31[2];
	float fRec32[2];
	float fRec54[2];
	float fRec51[2];
	float fRec52[2];
	
 public:
	Faust_Peq() {}

	void metadata(Meta* m) { 
		m->declare("analyzers.lib/analyzer_tpt_df:author", "Julius O. Smith III");
		m->declare("analyzers.lib/analyzer_tpt_df:license", "MIT");
		m->declare("analyzers.lib/name", "Faust Analyzer Library");
		m->declare("analyzers.lib/version", "1.6.3");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Peq -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "peq.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/_tpt_df:license", "MIT");
		m->declare("filters.lib/filterbank_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/filterbank_tpt_df:license", "MIT");
		m->declare("filters.lib/fir:author", "Julius O. Smith III");
		m->declare("filters.lib/fir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/fir:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/high_shelf:author", "Julius O. Smith III");
		m->declare("filters.lib/high_shelf:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/high_shelf:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/highpass:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/highpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/highpass_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass_tpt_df:license", "MIT");
		m->declare("filters.lib/highshelf:author", "Julius O. Smith III");
		m->declare("filters.lib/highshelf:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/highshelf:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/iir:author", "Julius O. Smith III");
		m->declare("filters.lib/iir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/iir:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/low_shelf:author", "Julius O. Smith III");
		m->declare("filters.lib/low_shelf:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/low_shelf:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass0_highpass1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass0_highpass1:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass_tpt_df:license", "MIT");
		m->declare("filters.lib/lowshelf:author", "Julius O. Smith III");
		m->declare("filters.lib/lowshelf:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowshelf:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/peak_eq:author", "Julius O. Smith III");
		m->declare("filters.lib/peak_eq:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/peak_eq:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/svf:author", "Oleg Nesterov");
		m->declare("filters.lib/svf:copyright", "Copyright (C) 2020 Oleg Nesterov <oleg@redhat.com>");
		m->declare("filters.lib/svf:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf1:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf1s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf1s_df:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s_df:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s_df:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf2:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf2s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2s:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf2s_df:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2s_df:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2s_df:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "peq");
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
		fConst2 = 3.1415927f / fConst0;
		fConst3 = 0.499f * fConst0;
		fConst4 = 6.2831855f / fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(0.0f);
		fHslider2 = FAUSTFLOAT(4e+03f);
		fHslider3 = FAUSTFLOAT(8e+02f);
		fHslider4 = FAUSTFLOAT(0.0f);
		fHslider5 = FAUSTFLOAT(1.0f);
		fHslider6 = FAUSTFLOAT(1.2e+02f);
		fHslider7 = FAUSTFLOAT(0.0f);
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
			fRec11[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec12[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fVec4[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec13[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec18[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec14[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec15[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fVec5[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec20[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec24[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec21[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec22[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec8[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fRec9[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec6[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec2[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fRec3[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec29[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fRec26[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec27[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 2; l26 = l26 + 1) {
			fRec44[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec40[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec41[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec49[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2; l30 = l30 + 1) {
			fRec46[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 2; l31 = l31 + 1) {
			fRec47[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fRec37[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec38[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			fRec35[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 2; l35 = l35 + 1) {
			fRec31[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			fRec32[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fRec54[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec51[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 2; l39 = l39 + 1) {
			fRec52[l39] = 0.0f;
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
	
	virtual Faust_Peq* clone() {
		return new Faust_Peq();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("peq");
		ui_interface->addHorizontalSlider("High Freq", &fHslider2, FAUSTFLOAT(4e+03f), FAUSTFLOAT(1.5e+03f), FAUSTFLOAT(1.5e+04f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("High Gain", &fHslider1, FAUSTFLOAT(0.0f), FAUSTFLOAT(-15.0f), FAUSTFLOAT(15.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Level", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-18.0f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Low Freq", &fHslider6, FAUSTFLOAT(1.2e+02f), FAUSTFLOAT(3e+01f), FAUSTFLOAT(6e+02f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Low Gain", &fHslider7, FAUSTFLOAT(0.0f), FAUSTFLOAT(-15.0f), FAUSTFLOAT(15.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mid Freq", &fHslider3, FAUSTFLOAT(8e+02f), FAUSTFLOAT(1.5e+02f), FAUSTFLOAT(6e+03f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Mid Gain", &fHslider4, FAUSTFLOAT(0.0f), FAUSTFLOAT(-15.0f), FAUSTFLOAT(15.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mid Q", &fHslider5, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.2f), FAUSTFLOAT(8.0f), FAUSTFLOAT(0.01f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = std::pow(1e+01f, 0.05f * float(fHslider0));
		float fSlow1 = float(fHslider1);
		float fSlow2 = std::tan(std::max<float>(0.0f, std::min<float>(fConst2 * float(fHslider2), 1.5676547f)));
		float fSlow3 = fSlow2 + 1.0f;
		float fSlow4 = float(fHslider3);
		float fSlow5 = float(fHslider4);
		float fSlow6 = float(fHslider5);
		float fSlow7 = std::tan(std::max<float>(0.0f, std::min<float>(fConst2 * float(fHslider6), 1.5676547f)));
		float fSlow8 = fSlow7 + 1.0f;
		float fSlow9 = std::min<float>(3.4028235e+38f, 1.0f / (fSlow7 + 1.0f));
		float fSlow10 = fSlow7 * fSlow9;
		float fSlow11 = 2.0f * fSlow10;
		float fSlow12 = fSlow7 * std::min<float>(3.4028235e+38f, 1.0f / (fSlow7 * fSlow8 + 1.0f));
		float fSlow13 = 2.0f * fSlow12;
		float fSlow14 = 2.0f * fSlow7;
		float fSlow15 = float(fHslider7);
		float fSlow16 = std::min<float>(3.4028235e+38f, 1.0f / (fSlow2 + 1.0f));
		float fSlow17 = fSlow2 * fSlow16;
		float fSlow18 = 2.0f * fSlow17;
		float fSlow19 = fSlow2 * std::min<float>(3.4028235e+38f, 1.0f / (fSlow2 * fSlow3 + 1.0f));
		float fSlow20 = 2.0f * fSlow19;
		float fSlow21 = 2.0f * fSlow2;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, fVec0[1] - fSlow0));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow0 + fRec0[0];
			fVec1[0] = fSlow1;
			float fTemp2 = fConst1 * (std::min<float>(3.4028235e+38f, fVec1[1] - fSlow1) + fRec1[1]);
			fRec1[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = std::pow(1e+01f, 0.05f * (fSlow1 + fRec1[0]));
			fVec2[0] = fSlow4;
			float fTemp4 = fConst1 * (std::min<float>(3.4028235e+38f, fVec2[1] - fSlow4) + fRec11[1]);
			fRec11[0] = ((std::fabs(fTemp4) > 1.1754944e-38f) ? fTemp4 : 0.0f);
			float fTemp5 = fSlow4 + fRec11[0];
			float fTemp6 = std::max<float>(0.001f, std::min<float>(fTemp5, fConst3));
			float fTemp7 = std::tan(std::max<float>(0.0f, std::min<float>(fConst2 * fTemp6, 1.5676547f)));
			fVec3[0] = fSlow5;
			float fTemp8 = fConst1 * (std::min<float>(3.4028235e+38f, fVec3[1] - fSlow5) + fRec12[1]);
			fRec12[0] = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			float fTemp9 = fSlow5 + fRec12[0];
			int iTemp10 = fTemp9 > 0.0f;
			fVec4[0] = fSlow6;
			float fTemp11 = fConst1 * (std::min<float>(3.4028235e+38f, fVec4[1] - fSlow6) + fRec13[1]);
			fRec13[0] = ((std::fabs(fTemp11) > 1.1754944e-38f) ? fTemp11 : 0.0f);
			float fTemp12 = (fSlow6 + fRec13[0]) * std::sin(fConst4 * fTemp6);
			float fTemp13 = fConst4 * (fTemp5 * std::pow(1e+01f, 0.05f * std::fabs(fTemp9)) / fTemp12);
			float fTemp14 = fConst4 * (fTemp5 / fTemp12);
			float fTemp15 = ((iTemp10) ? fTemp14 : fTemp13);
			float fTemp16 = std::min<float>(3.4028235e+38f, 1.0f / (fTemp7 * (fTemp15 + fTemp7) + 1.0f));
			float fTemp17 = std::max<float>(std::fabs(fTemp15), 1.0f);
			float fTemp18 = fTemp17 * fTemp7;
			float fTemp19 = float(input0[i0]);
			float fTemp20 = fTemp19 - fRec18[1];
			float fTemp21 = fRec18[1] + fSlow11 * fTemp20;
			fRec18[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fSlow9 * fTemp20;
			float fRec19 = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			float fTemp23 = fRec19 - (fSlow8 * fRec14[1] + fRec15[1]);
			float fTemp24 = fRec14[1] + fSlow13 * fTemp23;
			fRec14[0] = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			float fTemp25 = fRec14[1] + fSlow12 * fTemp23;
			float fTemp26 = fRec15[1] + fSlow14 * fTemp25;
			fRec15[0] = ((std::fabs(fTemp26) > 1.1754944e-38f) ? fTemp26 : 0.0f);
			float fRec16 = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			float fTemp27 = fRec15[1] + fSlow7 * fTemp25;
			float fRec17 = ((std::fabs(fTemp27) > 1.1754944e-38f) ? fTemp27 : 0.0f);
			fVec5[0] = fSlow15;
			float fTemp28 = fConst1 * (std::min<float>(3.4028235e+38f, fVec5[1] - fSlow15) + fRec20[1]);
			fRec20[0] = ((std::fabs(fTemp28) > 1.1754944e-38f) ? fTemp28 : 0.0f);
			float fTemp29 = std::pow(1e+01f, 0.05f * (fSlow15 + fRec20[0]));
			float fTemp30 = fTemp19 - fRec24[1];
			float fTemp31 = fRec24[1] + fSlow11 * fTemp30;
			fRec24[0] = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp32 = fRec24[1] + fSlow10 * fTemp30;
			float fRec25 = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fTemp33 = fRec25 - (fSlow8 * fRec21[1] + fRec22[1]);
			float fTemp34 = fRec21[1] + fSlow13 * fTemp33;
			fRec21[0] = ((std::fabs(fTemp34) > 1.1754944e-38f) ? fTemp34 : 0.0f);
			float fTemp35 = fRec21[1] + fSlow12 * fTemp33;
			float fTemp36 = fRec22[1] + fSlow14 * fTemp35;
			fRec22[0] = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fTemp37 = fRec22[1] + fSlow7 * fTemp35;
			float fRec23 = ((std::fabs(fTemp37) > 1.1754944e-38f) ? fTemp37 : 0.0f);
			float fTemp38 = fRec19 + fRec23 * fTemp29 - (fRec17 + 1.0f * fRec16 + (fTemp15 * fRec8[1] + (fTemp18 * fRec8[1] + fRec9[1]) / fTemp17) / fTemp17);
			float fTemp39 = fRec8[1] + fTemp18 * fTemp38 * fTemp16;
			float fTemp40 = 2.0f * fTemp39 - fRec8[1];
			fRec8[0] = ((std::fabs(fTemp40) > 1.1754944e-38f) ? fTemp40 : 0.0f);
			float fTemp41 = fRec9[1] + fTemp18 * fTemp39;
			float fTemp42 = 2.0f * fTemp41 - fRec9[1];
			fRec9[0] = ((std::fabs(fTemp42) > 1.1754944e-38f) ? fTemp42 : 0.0f);
			float fTemp43 = ((iTemp10) ? fTemp13 : fTemp14);
			float fTemp44 = fTemp38 * fTemp16 + (fTemp39 * fTemp43 + fTemp41 / fTemp17) / fTemp17;
			float fRec10 = ((std::fabs(fTemp44) > 1.1754944e-38f) ? fTemp44 : 0.0f);
			float fTemp45 = fRec10 - fRec6[1];
			float fTemp46 = fRec6[1] + fSlow18 * fTemp45;
			fRec6[0] = ((std::fabs(fTemp46) > 1.1754944e-38f) ? fTemp46 : 0.0f);
			float fTemp47 = fSlow16 * fTemp45;
			float fRec7 = ((std::fabs(fTemp47) > 1.1754944e-38f) ? fTemp47 : 0.0f);
			float fTemp48 = fRec7 - (fSlow3 * fRec2[1] + fRec3[1]);
			float fTemp49 = fRec2[1] + fSlow20 * fTemp48;
			fRec2[0] = ((std::fabs(fTemp49) > 1.1754944e-38f) ? fTemp49 : 0.0f);
			float fTemp50 = fRec2[1] + fSlow19 * fTemp48;
			float fTemp51 = fRec3[1] + fSlow21 * fTemp50;
			fRec3[0] = ((std::fabs(fTemp51) > 1.1754944e-38f) ? fTemp51 : 0.0f);
			float fRec4 = ((std::fabs(fTemp50) > 1.1754944e-38f) ? fTemp50 : 0.0f);
			float fTemp52 = fRec3[1] + fSlow2 * fTemp50;
			float fRec5 = ((std::fabs(fTemp52) > 1.1754944e-38f) ? fTemp52 : 0.0f);
			float fTemp53 = fRec10 - fRec29[1];
			float fTemp54 = fRec29[1] + fSlow18 * fTemp53;
			fRec29[0] = ((std::fabs(fTemp54) > 1.1754944e-38f) ? fTemp54 : 0.0f);
			float fTemp55 = fRec29[1] + fSlow17 * fTemp53;
			float fRec30 = ((std::fabs(fTemp55) > 1.1754944e-38f) ? fTemp55 : 0.0f);
			float fTemp56 = fRec30 - (fSlow3 * fRec26[1] + fRec27[1]);
			float fTemp57 = fRec26[1] + fSlow20 * fTemp56;
			fRec26[0] = ((std::fabs(fTemp57) > 1.1754944e-38f) ? fTemp57 : 0.0f);
			float fTemp58 = fRec26[1] + fSlow19 * fTemp56;
			float fTemp59 = fRec27[1] + fSlow21 * fTemp58;
			fRec27[0] = ((std::fabs(fTemp59) > 1.1754944e-38f) ? fTemp59 : 0.0f);
			float fTemp60 = fRec27[1] + fSlow2 * fTemp58;
			float fRec28 = ((std::fabs(fTemp60) > 1.1754944e-38f) ? fTemp60 : 0.0f);
			output0[i0] = FAUSTFLOAT((fRec28 + (fRec7 - (fRec5 + 1.0f * fRec4)) * fTemp3) * fTemp1);
			float fTemp61 = float(input1[i0]);
			float fTemp62 = fTemp61 - fRec44[1];
			float fTemp63 = fRec44[1] + fSlow11 * fTemp62;
			fRec44[0] = ((std::fabs(fTemp63) > 1.1754944e-38f) ? fTemp63 : 0.0f);
			float fTemp64 = fSlow9 * fTemp62;
			float fRec45 = ((std::fabs(fTemp64) > 1.1754944e-38f) ? fTemp64 : 0.0f);
			float fTemp65 = fRec45 - (fSlow8 * fRec40[1] + fRec41[1]);
			float fTemp66 = fRec40[1] + fSlow13 * fTemp65;
			fRec40[0] = ((std::fabs(fTemp66) > 1.1754944e-38f) ? fTemp66 : 0.0f);
			float fTemp67 = fRec40[1] + fSlow12 * fTemp65;
			float fTemp68 = fRec41[1] + fSlow14 * fTemp67;
			fRec41[0] = ((std::fabs(fTemp68) > 1.1754944e-38f) ? fTemp68 : 0.0f);
			float fRec42 = ((std::fabs(fTemp67) > 1.1754944e-38f) ? fTemp67 : 0.0f);
			float fTemp69 = fRec41[1] + fSlow7 * fTemp67;
			float fRec43 = ((std::fabs(fTemp69) > 1.1754944e-38f) ? fTemp69 : 0.0f);
			float fTemp70 = fTemp61 - fRec49[1];
			float fTemp71 = fRec49[1] + fSlow11 * fTemp70;
			fRec49[0] = ((std::fabs(fTemp71) > 1.1754944e-38f) ? fTemp71 : 0.0f);
			float fTemp72 = fRec49[1] + fSlow10 * fTemp70;
			float fRec50 = ((std::fabs(fTemp72) > 1.1754944e-38f) ? fTemp72 : 0.0f);
			float fTemp73 = fRec50 - (fSlow8 * fRec46[1] + fRec47[1]);
			float fTemp74 = fRec46[1] + fSlow13 * fTemp73;
			fRec46[0] = ((std::fabs(fTemp74) > 1.1754944e-38f) ? fTemp74 : 0.0f);
			float fTemp75 = fRec46[1] + fSlow12 * fTemp73;
			float fTemp76 = fRec47[1] + fSlow14 * fTemp75;
			fRec47[0] = ((std::fabs(fTemp76) > 1.1754944e-38f) ? fTemp76 : 0.0f);
			float fTemp77 = fRec47[1] + fSlow7 * fTemp75;
			float fRec48 = ((std::fabs(fTemp77) > 1.1754944e-38f) ? fTemp77 : 0.0f);
			float fTemp78 = fRec45 + fRec48 * fTemp29 - (fRec43 + 1.0f * fRec42 + (fTemp15 * fRec37[1] + (fTemp18 * fRec37[1] + fRec38[1]) / fTemp17) / fTemp17);
			float fTemp79 = fRec37[1] + fTemp18 * fTemp16 * fTemp78;
			float fTemp80 = 2.0f * fTemp79 - fRec37[1];
			fRec37[0] = ((std::fabs(fTemp80) > 1.1754944e-38f) ? fTemp80 : 0.0f);
			float fTemp81 = fRec38[1] + fTemp18 * fTemp79;
			float fTemp82 = 2.0f * fTemp81 - fRec38[1];
			fRec38[0] = ((std::fabs(fTemp82) > 1.1754944e-38f) ? fTemp82 : 0.0f);
			float fTemp83 = fTemp16 * fTemp78 + (fTemp43 * fTemp79 + fTemp81 / fTemp17) / fTemp17;
			float fRec39 = ((std::fabs(fTemp83) > 1.1754944e-38f) ? fTemp83 : 0.0f);
			float fTemp84 = fRec39 - fRec35[1];
			float fTemp85 = fRec35[1] + fSlow18 * fTemp84;
			fRec35[0] = ((std::fabs(fTemp85) > 1.1754944e-38f) ? fTemp85 : 0.0f);
			float fTemp86 = fSlow16 * fTemp84;
			float fRec36 = ((std::fabs(fTemp86) > 1.1754944e-38f) ? fTemp86 : 0.0f);
			float fTemp87 = fRec36 - (fSlow3 * fRec31[1] + fRec32[1]);
			float fTemp88 = fRec31[1] + fSlow20 * fTemp87;
			fRec31[0] = ((std::fabs(fTemp88) > 1.1754944e-38f) ? fTemp88 : 0.0f);
			float fTemp89 = fRec31[1] + fSlow19 * fTemp87;
			float fTemp90 = fRec32[1] + fSlow21 * fTemp89;
			fRec32[0] = ((std::fabs(fTemp90) > 1.1754944e-38f) ? fTemp90 : 0.0f);
			float fRec33 = ((std::fabs(fTemp89) > 1.1754944e-38f) ? fTemp89 : 0.0f);
			float fTemp91 = fRec32[1] + fSlow2 * fTemp89;
			float fRec34 = ((std::fabs(fTemp91) > 1.1754944e-38f) ? fTemp91 : 0.0f);
			float fTemp92 = fRec39 - fRec54[1];
			float fTemp93 = fRec54[1] + fSlow18 * fTemp92;
			fRec54[0] = ((std::fabs(fTemp93) > 1.1754944e-38f) ? fTemp93 : 0.0f);
			float fTemp94 = fRec54[1] + fSlow17 * fTemp92;
			float fRec55 = ((std::fabs(fTemp94) > 1.1754944e-38f) ? fTemp94 : 0.0f);
			float fTemp95 = fRec55 - (fSlow3 * fRec51[1] + fRec52[1]);
			float fTemp96 = fRec51[1] + fSlow20 * fTemp95;
			fRec51[0] = ((std::fabs(fTemp96) > 1.1754944e-38f) ? fTemp96 : 0.0f);
			float fTemp97 = fRec51[1] + fSlow19 * fTemp95;
			float fTemp98 = fRec52[1] + fSlow21 * fTemp97;
			fRec52[0] = ((std::fabs(fTemp98) > 1.1754944e-38f) ? fTemp98 : 0.0f);
			float fTemp99 = fRec52[1] + fSlow2 * fTemp97;
			float fRec53 = ((std::fabs(fTemp99) > 1.1754944e-38f) ? fTemp99 : 0.0f);
			output1[i0] = FAUSTFLOAT(fTemp1 * (fRec53 + fTemp3 * (fRec36 - (fRec34 + 1.0f * fRec33))));
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec1[1] = fRec1[0];
			fVec2[1] = fVec2[0];
			fRec11[1] = fRec11[0];
			fVec3[1] = fVec3[0];
			fRec12[1] = fRec12[0];
			fVec4[1] = fVec4[0];
			fRec13[1] = fRec13[0];
			fRec18[1] = fRec18[0];
			fRec14[1] = fRec14[0];
			fRec15[1] = fRec15[0];
			fVec5[1] = fVec5[0];
			fRec20[1] = fRec20[0];
			fRec24[1] = fRec24[0];
			fRec21[1] = fRec21[0];
			fRec22[1] = fRec22[0];
			fRec8[1] = fRec8[0];
			fRec9[1] = fRec9[0];
			fRec6[1] = fRec6[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec29[1] = fRec29[0];
			fRec26[1] = fRec26[0];
			fRec27[1] = fRec27[0];
			fRec44[1] = fRec44[0];
			fRec40[1] = fRec40[0];
			fRec41[1] = fRec41[0];
			fRec49[1] = fRec49[0];
			fRec46[1] = fRec46[0];
			fRec47[1] = fRec47[0];
			fRec37[1] = fRec37[0];
			fRec38[1] = fRec38[0];
			fRec35[1] = fRec35[0];
			fRec31[1] = fRec31[0];
			fRec32[1] = fRec32[0];
			fRec54[1] = fRec54[0];
			fRec51[1] = fRec51[0];
			fRec52[1] = fRec52[0];
		}
	}

};

#endif
