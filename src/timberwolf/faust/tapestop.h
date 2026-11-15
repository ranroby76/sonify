// D:\workspace\Fanan_web_hub\src\timberwolf\faust\tapestop.h
// Written by the Faust compiler (2.70.3) from tapestop.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "tapestop"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Tapestop -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Tapestop_H__
#define  __Faust_Tapestop_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Tapestop
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


class Faust_Tapestop : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int iRec3[2];
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	float fConst0;
	FAUSTFLOAT fHslider2;
	float fRec0[2];
	FAUSTFLOAT fHslider3;
	int IOTA0;
	float fVec1[1048576];
	float fRec4[2];
	float fVec2[1048576];
	
 public:
	Faust_Tapestop() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Tapestop -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/fdelayltv:author", "Julius O. Smith III");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "tapestop.dsp");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("misceffects.lib/name", "Misc Effects Library");
		m->declare("misceffects.lib/tapeStop:author", "David Braun");
		m->declare("misceffects.lib/tapeStop:copyright", "Copyright (C) 2024 by David Braun <braun@ccrma.stanford.edu>");
		m->declare("misceffects.lib/tapeStop:license", "LicenseRef-STK-4.3");
		m->declare("misceffects.lib/version", "2.7.4");
		m->declare("name", "tapestop");
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
		fConst0 = 1e+03f / std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(8e+02f);
		fHslider2 = FAUSTFLOAT(1.0f);
		fHslider3 = FAUSTFLOAT(1.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			iRec3[l1] = 0;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec0[l2] = 0.0f;
		}
		IOTA0 = 0;
		for (int l3 = 0; l3 < 1048576; l3 = l3 + 1) {
			fVec1[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec4[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 1048576; l5 = l5 + 1) {
			fVec2[l5] = 0.0f;
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
	
	virtual Faust_Tapestop* clone() {
		return new Faust_Tapestop();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("tapestop");
		ui_interface->addHorizontalSlider("Curve", &fHslider2, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.05f), FAUSTFLOAT(2.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Fade", &fHslider3, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.05f), FAUSTFLOAT(2.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Stop", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Time", &fHslider1, FAUSTFLOAT(8e+02f), FAUSTFLOAT(5e+01f), FAUSTFLOAT(4e+03f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = float(fHslider0);
		int iSlow1 = int(fSlow0);
		float fSlow2 = fConst0 / float(fHslider1);
		float fSlow3 = float(fHslider2);
		float fSlow4 = float(fHslider3);
		float fSlow5 = float(fSlow0 >= 1.0f);
		float fSlow6 = 1.0f / float(((iSlow1) ? 0 : 2048));
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			int iTemp0 = iSlow1 & int(1.0f - fVec0[1]);
			iRec3[0] = iRec3[1] * ((iTemp0) ? 0 : 1) + 1;
			float fTemp1 = std::max<float>(0.0f, 1.0f - fSlow2 * float(iRec3[0] + -1));
			float fTemp2 = ((iTemp0) ? 1.0f : fRec0[1]) + (1.0f - std::pow(fTemp1, fSlow3));
			fRec0[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = std::pow(fTemp1, fSlow4);
			float fTemp4 = float(input0[i0]);
			fVec1[IOTA0 & 1048575] = fTemp4;
			float fTemp5 = std::max<float>(fTemp2, 1.0f);
			float fTemp6 = fTemp5 + -0.999995f;
			int iTemp7 = int(std::min<float>(768003.0f, std::max<float>(-3.0f, fTemp6)));
			int iTemp8 = std::min<int>(768003, std::max<int>(0, iTemp7 + 3));
			float fTemp9 = std::floor(fTemp6);
			float fTemp10 = fTemp5 + (-2.0f - fTemp9);
			float fTemp11 = fTemp5 + (-1.0f - fTemp9);
			float fTemp12 = fTemp5 - fTemp9;
			float fTemp13 = fTemp12 * fTemp11;
			float fTemp14 = fTemp13 * fTemp10;
			int iTemp15 = std::min<int>(768003, std::max<int>(0, iTemp7 + 2));
			int iTemp16 = std::min<int>(768003, std::max<int>(0, iTemp7));
			int iTemp17 = std::min<int>(768003, std::max<int>(0, iTemp7 + 1));
			float fTemp18 = fTemp5 + (-3.0f - fTemp9);
			float fTemp19 = fSlow6 + fRec4[1];
			float fTemp20 = fRec4[1] - fSlow6;
			float fTemp21 = ((fTemp19 < fSlow5) ? fTemp19 : ((fTemp20 > fSlow5) ? fTemp20 : fSlow5));
			fRec4[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = 1.0f - fRec4[0];
			float fTemp23 = fVec1[(IOTA0 - 1) & 1048575] * fTemp22 + fRec4[0] * (fTemp18 * (fTemp10 * (0.5f * fTemp12 * fVec1[(IOTA0 - iTemp17) & 1048575] - 0.16666667f * fVec1[(IOTA0 - iTemp16) & 1048575] * fTemp11) - 0.5f * fTemp13 * fVec1[(IOTA0 - iTemp15) & 1048575]) + 0.16666667f * fTemp14 * fVec1[(IOTA0 - iTemp8) & 1048575]) * fTemp3;
			float fRec1 = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fTemp24 = float(input1[i0]);
			fVec2[IOTA0 & 1048575] = fTemp24;
			float fTemp25 = fVec2[(IOTA0 - 1) & 1048575] * fTemp22 + fRec4[0] * fTemp3 * (fTemp18 * (fTemp10 * (0.5f * fTemp12 * fVec2[(IOTA0 - iTemp17) & 1048575] - 0.16666667f * fTemp11 * fVec2[(IOTA0 - iTemp16) & 1048575]) - 0.5f * fTemp13 * fVec2[(IOTA0 - iTemp15) & 1048575]) + 0.16666667f * fTemp14 * fVec2[(IOTA0 - iTemp8) & 1048575]);
			float fRec2 = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			output0[i0] = FAUSTFLOAT(fRec1);
			output1[i0] = FAUSTFLOAT(fRec2);
			fVec0[1] = fVec0[0];
			iRec3[1] = iRec3[0];
			fRec0[1] = fRec0[0];
			IOTA0 = IOTA0 + 1;
			fRec4[1] = fRec4[0];
		}
	}

};

#endif
