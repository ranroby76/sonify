// D:\workspace\Fanan_web_hub\src\timberwolf\faust\spring.h
// Written by the Faust compiler (2.70.3) from spring.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "spring"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Spring -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Spring_H__
#define  __Faust_Spring_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Spring
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

static float Faust_Spring_faustpower2_f(float value) {
	return value * value;
}

class Faust_Spring : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fRec0[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec1[2];
	float fConst2;
	int IOTA0;
	float fVec2[8192];
	int iConst5;
	int iConst6;
	int iConst7;
	int iConst8;
	int iConst9;
	int iConst10;
	int iConst11;
	int iConst12;
	float fVec3[8192];
	int iConst13;
	float fVec4[4096];
	int iConst14;
	float fVec5[8192];
	int iConst15;
	float fVec6[4096];
	int iConst16;
	float fVec7[8192];
	int iConst17;
	float fVec8[4096];
	int iConst18;
	float fVec9[4096];
	int iConst19;
	float fVec10[2048];
	int iConst20;
	float fVec11[8192];
	int iConst21;
	float fVec12[4096];
	int iConst22;
	float fVec13[4096];
	int iConst23;
	float fVec14[4096];
	int iConst24;
	float fVec15[4096];
	int iConst25;
	float fVec16[4096];
	int iConst26;
	float fVec17[4096];
	int iConst27;
	float fVec18[2048];
	int iConst28;
	float fVec19[2048];
	FAUSTFLOAT fHslider2;
	float fVec20[2];
	float fRec14[2];
	float fVec21[16384];
	float fConst29;
	FAUSTFLOAT fHslider3;
	float fVec22[2];
	float fRec15[2];
	float fRec12[2];
	float fRec13[2];
	float fRec4[2];
	float fVec23[4096];
	float fVec24[16384];
	float fRec16[2];
	float fRec17[2];
	float fRec5[2];
	float fVec25[4096];
	int iConst30;
	float fVec26[16384];
	float fRec18[2];
	float fRec19[2];
	float fRec6[2];
	float fVec27[4096];
	int iConst31;
	float fVec28[16384];
	float fRec20[2];
	float fRec21[2];
	float fRec7[2];
	float fVec29[4096];
	int iConst32;
	float fVec30[16384];
	float fRec22[2];
	float fRec23[2];
	float fRec8[2];
	float fVec31[8192];
	int iConst33;
	float fVec32[16384];
	float fRec24[2];
	float fRec25[2];
	float fRec9[2];
	float fVec33[8192];
	int iConst34;
	float fVec34[16384];
	float fRec26[2];
	float fRec27[2];
	float fRec10[2];
	float fVec35[8192];
	int iConst35;
	float fVec36[16384];
	float fRec28[2];
	float fRec29[2];
	float fRec11[2];
	float fConst37;
	float fConst38;
	float fRec2[2];
	
 public:
	Faust_Spring() {}

	void metadata(Meta* m) { 
		m->declare("aanl.lib/ADAA1:author", "Dario Sanfilippo");
		m->declare("aanl.lib/ADAA1:copyright", "Copyright (C) 2021 Dario Sanfilippo     <sanfilippo.dario@gmail.com>");
		m->declare("aanl.lib/ADAA1:license", "MIT");
		m->declare("aanl.lib/hardclip:author", "Dario Sanfilippo");
		m->declare("aanl.lib/hardclip:copyright", "Copyright (C) 2021 Dario Sanfilippo     <sanfilippo.dario@gmail.com>");
		m->declare("aanl.lib/hardclip:license", "MIT");
		m->declare("aanl.lib/name", "Faust Antialiased Nonlinearities");
		m->declare("aanl.lib/version", "1.6.2");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Spring -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "spring.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/highpass:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/highpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass0_highpass1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass0_highpass1:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "LicenseRef-STK-4.3");
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
		m->declare("name", "spring");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("reverbs.lib/name", "Faust Reverb Library");
		m->declare("reverbs.lib/springreverb:author", "Daniel Leonov");
		m->declare("reverbs.lib/version", "1.6.2");
		m->declare("routes.lib/hadamard:author", "Remy Muller, revised by Romain Michon");
		m->declare("routes.lib/name", "Faust Signal Routing Library");
		m->declare("routes.lib/version", "1.4.1");
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
		fConst1 = 1.0f - 44.1f / fConst0;
		fConst2 = 3.1415927f / fConst0;
		float fConst3 = std::round(0.035f * fConst0);
		float fConst4 = std::max<float>(0.0f, fConst3);
		iConst5 = int(std::min<float>(std::round(0.02375f * fConst0), fConst4));
		iConst6 = int(std::min<float>(std::round(0.03875f * fConst0), fConst4));
		iConst7 = int(std::min<float>(std::round(0.03125f * fConst0), fConst4));
		iConst8 = int(std::min<float>(std::round(0.04625f * fConst0), fConst4));
		iConst9 = int(std::min<float>(std::round(0.0275f * fConst0), fConst4));
		iConst10 = int(std::min<float>(std::round(0.0425f * fConst0), fConst4));
		iConst11 = int(std::min<float>(fConst3, fConst4));
		iConst12 = int(std::min<float>(std::round(0.05f * fConst0), fConst4));
		iConst13 = int(std::min<float>(std::round(0.027375f * fConst0), fConst4));
		iConst14 = int(std::min<float>(std::round(0.016875f * fConst0), fConst4));
		iConst15 = int(std::min<float>(std::round(0.022125f * fConst0), fConst4));
		iConst16 = int(std::min<float>(std::round(0.011625f * fConst0), fConst4));
		iConst17 = int(std::min<float>(std::round(0.02475f * fConst0), fConst4));
		iConst18 = int(std::min<float>(std::round(0.01425f * fConst0), fConst4));
		iConst19 = int(std::min<float>(std::round(0.0195f * fConst0), fConst4));
		iConst20 = int(std::min<float>(std::round(0.009f * fConst0), fConst4));
		iConst21 = int(std::min<float>(std::round(0.023125f * fConst0), fConst4));
		iConst22 = int(std::min<float>(std::round(0.015625f * fConst0), fConst4));
		iConst23 = int(std::min<float>(std::round(0.019375f * fConst0), fConst4));
		iConst24 = int(std::min<float>(std::round(0.011875f * fConst0), fConst4));
		iConst25 = int(std::min<float>(std::round(0.02125f * fConst0), fConst4));
		iConst26 = int(std::min<float>(std::round(0.01375f * fConst0), fConst4));
		iConst27 = int(std::min<float>(std::round(0.0175f * fConst0), fConst4));
		iConst28 = int(std::min<float>(std::round(0.01f * fConst0), fConst4));
		fConst29 = std::round(0.08f * fConst0);
		iConst30 = int(std::min<float>(std::round(0.01475f * fConst0), fConst4));
		iConst31 = int(std::min<float>(std::round(0.017625f * fConst0), fConst4));
		iConst32 = int(std::min<float>(std::round(0.0205f * fConst0), fConst4));
		iConst33 = int(std::min<float>(std::round(0.023375f * fConst0), fConst4));
		iConst34 = int(std::min<float>(std::round(0.02625f * fConst0), fConst4));
		iConst35 = int(std::min<float>(std::round(0.029125f * fConst0), fConst4));
		float fConst36 = std::tan(std::max<float>(0.0f, std::min<float>(471.2389f / fConst0, 1.5676547f)));
		fConst37 = std::min<float>(3.4028235e+38f, 1.0f / (fConst36 + 1.0f));
		fConst38 = 2.0f * fConst36 * fConst37;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(5e+01f);
		fHslider1 = FAUSTFLOAT(5e+01f);
		fHslider2 = FAUSTFLOAT(5e+01f);
		fHslider3 = FAUSTFLOAT(5e+01f);
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
		IOTA0 = 0;
		for (int l4 = 0; l4 < 8192; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 8192; l5 = l5 + 1) {
			fVec3[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 4096; l6 = l6 + 1) {
			fVec4[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 8192; l7 = l7 + 1) {
			fVec5[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 4096; l8 = l8 + 1) {
			fVec6[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 8192; l9 = l9 + 1) {
			fVec7[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 4096; l10 = l10 + 1) {
			fVec8[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 4096; l11 = l11 + 1) {
			fVec9[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2048; l12 = l12 + 1) {
			fVec10[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 8192; l13 = l13 + 1) {
			fVec11[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 4096; l14 = l14 + 1) {
			fVec12[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 4096; l15 = l15 + 1) {
			fVec13[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 4096; l16 = l16 + 1) {
			fVec14[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 4096; l17 = l17 + 1) {
			fVec15[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 4096; l18 = l18 + 1) {
			fVec16[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 4096; l19 = l19 + 1) {
			fVec17[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2048; l20 = l20 + 1) {
			fVec18[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2048; l21 = l21 + 1) {
			fVec19[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fVec20[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec14[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 16384; l24 = l24 + 1) {
			fVec21[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fVec22[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 2; l26 = l26 + 1) {
			fRec15[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec12[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec13[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec4[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 4096; l30 = l30 + 1) {
			fVec23[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 16384; l31 = l31 + 1) {
			fVec24[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fRec16[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec17[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			fRec5[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 4096; l35 = l35 + 1) {
			fVec25[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 16384; l36 = l36 + 1) {
			fVec26[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fRec18[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec19[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 2; l39 = l39 + 1) {
			fRec6[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 4096; l40 = l40 + 1) {
			fVec27[l40] = 0.0f;
		}
		for (int l41 = 0; l41 < 16384; l41 = l41 + 1) {
			fVec28[l41] = 0.0f;
		}
		for (int l42 = 0; l42 < 2; l42 = l42 + 1) {
			fRec20[l42] = 0.0f;
		}
		for (int l43 = 0; l43 < 2; l43 = l43 + 1) {
			fRec21[l43] = 0.0f;
		}
		for (int l44 = 0; l44 < 2; l44 = l44 + 1) {
			fRec7[l44] = 0.0f;
		}
		for (int l45 = 0; l45 < 4096; l45 = l45 + 1) {
			fVec29[l45] = 0.0f;
		}
		for (int l46 = 0; l46 < 16384; l46 = l46 + 1) {
			fVec30[l46] = 0.0f;
		}
		for (int l47 = 0; l47 < 2; l47 = l47 + 1) {
			fRec22[l47] = 0.0f;
		}
		for (int l48 = 0; l48 < 2; l48 = l48 + 1) {
			fRec23[l48] = 0.0f;
		}
		for (int l49 = 0; l49 < 2; l49 = l49 + 1) {
			fRec8[l49] = 0.0f;
		}
		for (int l50 = 0; l50 < 8192; l50 = l50 + 1) {
			fVec31[l50] = 0.0f;
		}
		for (int l51 = 0; l51 < 16384; l51 = l51 + 1) {
			fVec32[l51] = 0.0f;
		}
		for (int l52 = 0; l52 < 2; l52 = l52 + 1) {
			fRec24[l52] = 0.0f;
		}
		for (int l53 = 0; l53 < 2; l53 = l53 + 1) {
			fRec25[l53] = 0.0f;
		}
		for (int l54 = 0; l54 < 2; l54 = l54 + 1) {
			fRec9[l54] = 0.0f;
		}
		for (int l55 = 0; l55 < 8192; l55 = l55 + 1) {
			fVec33[l55] = 0.0f;
		}
		for (int l56 = 0; l56 < 16384; l56 = l56 + 1) {
			fVec34[l56] = 0.0f;
		}
		for (int l57 = 0; l57 < 2; l57 = l57 + 1) {
			fRec26[l57] = 0.0f;
		}
		for (int l58 = 0; l58 < 2; l58 = l58 + 1) {
			fRec27[l58] = 0.0f;
		}
		for (int l59 = 0; l59 < 2; l59 = l59 + 1) {
			fRec10[l59] = 0.0f;
		}
		for (int l60 = 0; l60 < 8192; l60 = l60 + 1) {
			fVec35[l60] = 0.0f;
		}
		for (int l61 = 0; l61 < 16384; l61 = l61 + 1) {
			fVec36[l61] = 0.0f;
		}
		for (int l62 = 0; l62 < 2; l62 = l62 + 1) {
			fRec28[l62] = 0.0f;
		}
		for (int l63 = 0; l63 < 2; l63 = l63 + 1) {
			fRec29[l63] = 0.0f;
		}
		for (int l64 = 0; l64 < 2; l64 = l64 + 1) {
			fRec11[l64] = 0.0f;
		}
		for (int l65 = 0; l65 < 2; l65 = l65 + 1) {
			fRec2[l65] = 0.0f;
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
	
	virtual Faust_Spring* clone() {
		return new Faust_Spring();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("spring");
		ui_interface->addHorizontalSlider("Dwell", &fHslider2, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mix", &fHslider1, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Tension", &fHslider3, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Tone", &fHslider0, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.01f * fSlow0;
		float fSlow2 = float(fHslider1);
		float fSlow3 = 0.01f * fSlow2;
		float fSlow4 = float(fHslider2);
		float fSlow5 = 0.01f * fSlow4;
		float fSlow6 = float(fHslider3);
		float fSlow7 = 0.01f * fSlow6;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec0[1]))) + fRec0[1]);
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = std::max<float>(0.0f, std::min<float>(1e+01f, 1e+01f * (fSlow1 + fRec0[0])));
			float fTemp2 = fTemp1 * (1.9e+02f * fTemp1 + 5e+01f) + 1.5e+03f;
			fVec1[0] = fSlow2;
			float fTemp3 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec1[1]))) + fRec1[1]);
			fRec1[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = std::tan(std::max<float>(0.0f, std::min<float>(fConst2 * fTemp2, 1.5676547f)));
			float fTemp5 = std::min<float>(3.4028235e+38f, 1.0f / (fTemp4 + 1.0f));
			float fTemp6 = float(input0[i0]);
			fVec2[IOTA0 & 8191] = fTemp6;
			float fTemp7 = fVec2[(IOTA0 - iConst5) & 8191];
			float fTemp8 = fVec2[(IOTA0 - iConst6) & 8191];
			float fTemp9 = fTemp8 - fTemp7;
			float fTemp10 = fVec2[(IOTA0 - iConst7) & 8191];
			float fTemp11 = fVec2[(IOTA0 - iConst8) & 8191];
			float fTemp12 = fTemp11 - fTemp10;
			float fTemp13 = fTemp12 - fTemp9;
			float fTemp14 = fVec2[(IOTA0 - iConst9) & 8191];
			float fTemp15 = fVec2[(IOTA0 - iConst10) & 8191];
			float fTemp16 = fTemp15 - fTemp14;
			float fTemp17 = fVec2[(IOTA0 - iConst11) & 8191];
			float fTemp18 = fVec2[(IOTA0 - iConst12) & 8191];
			float fTemp19 = fTemp18 - fTemp17;
			float fTemp20 = fTemp19 - fTemp16;
			fVec3[IOTA0 & 8191] = fTemp20 - fTemp13;
			float fTemp21 = fVec3[(IOTA0 - iConst13) & 8191];
			float fTemp22 = fTemp8 + fTemp7;
			float fTemp23 = fTemp11 + fTemp10;
			float fTemp24 = fTemp23 - fTemp22;
			float fTemp25 = fTemp15 + fTemp14;
			float fTemp26 = fTemp18 + fTemp17;
			float fTemp27 = fTemp26 - fTemp25;
			fVec4[IOTA0 & 4095] = fTemp27 - fTemp24;
			float fTemp28 = fVec4[(IOTA0 - iConst14) & 4095];
			float fTemp29 = fTemp28 + fTemp21;
			float fTemp30 = fTemp12 + fTemp9;
			float fTemp31 = fTemp19 + fTemp16;
			fVec5[IOTA0 & 8191] = fTemp31 - fTemp30;
			float fTemp32 = fVec5[(IOTA0 - iConst15) & 8191];
			float fTemp33 = fTemp23 + fTemp22;
			float fTemp34 = fTemp26 + fTemp25;
			fVec6[IOTA0 & 4095] = fTemp34 - fTemp33;
			float fTemp35 = fVec6[(IOTA0 - iConst16) & 4095];
			float fTemp36 = fTemp35 + fTemp32;
			float fTemp37 = fTemp36 + fTemp29;
			fVec7[IOTA0 & 8191] = fTemp20 + fTemp13;
			float fTemp38 = fVec7[(IOTA0 - iConst17) & 8191];
			fVec8[IOTA0 & 4095] = fTemp27 + fTemp24;
			float fTemp39 = fVec8[(IOTA0 - iConst18) & 4095];
			float fTemp40 = fTemp39 + fTemp38;
			fVec9[IOTA0 & 4095] = fTemp31 + fTemp30;
			float fTemp41 = fVec9[(IOTA0 - iConst19) & 4095];
			fVec10[IOTA0 & 2047] = fTemp34 + fTemp33;
			float fTemp42 = fVec10[(IOTA0 - iConst20) & 2047];
			float fTemp43 = fTemp42 + fTemp41;
			float fTemp44 = fTemp43 - fTemp40;
			fVec11[IOTA0 & 8191] = fTemp44 + fTemp37;
			float fTemp45 = fVec11[(IOTA0 - iConst21) & 8191];
			float fTemp46 = fTemp28 - fTemp21;
			float fTemp47 = fTemp35 - fTemp32;
			float fTemp48 = fTemp47 + fTemp46;
			float fTemp49 = fTemp39 - fTemp38;
			float fTemp50 = fTemp42 - fTemp41;
			float fTemp51 = fTemp50 - fTemp49;
			fVec12[IOTA0 & 4095] = fTemp51 + fTemp48;
			float fTemp52 = fVec12[(IOTA0 - iConst22) & 4095];
			float fTemp53 = fTemp52 + fTemp45;
			float fTemp54 = fTemp36 - fTemp29;
			float fTemp55 = fTemp43 + fTemp40;
			fVec13[IOTA0 & 4095] = fTemp55 + fTemp54;
			float fTemp56 = fVec13[(IOTA0 - iConst23) & 4095];
			float fTemp57 = fTemp47 - fTemp46;
			float fTemp58 = fTemp50 + fTemp49;
			fVec14[IOTA0 & 4095] = fTemp58 + fTemp57;
			float fTemp59 = fVec14[(IOTA0 - iConst24) & 4095];
			float fTemp60 = fTemp59 + fTemp56;
			float fTemp61 = fTemp60 - fTemp53;
			fVec15[IOTA0 & 4095] = fTemp44 - fTemp37;
			float fTemp62 = fVec15[(IOTA0 - iConst25) & 4095];
			fVec16[IOTA0 & 4095] = fTemp51 - fTemp48;
			float fTemp63 = fVec16[(IOTA0 - iConst26) & 4095];
			float fTemp64 = fTemp63 + fTemp62;
			fVec17[IOTA0 & 4095] = fTemp55 - fTemp54;
			float fTemp65 = fVec17[(IOTA0 - iConst27) & 4095];
			fVec18[IOTA0 & 2047] = fTemp58 - fTemp57;
			float fTemp66 = fVec18[(IOTA0 - iConst28) & 2047];
			float fTemp67 = fTemp66 - fTemp65;
			float fTemp68 = fTemp67 - fTemp64;
			fVec19[IOTA0 & 2047] = fTemp68 - fTemp61;
			fVec20[0] = fSlow4;
			float fTemp69 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow4 - fVec20[1]))) + fRec14[1]);
			fRec14[0] = ((std::fabs(fTemp69) > 1.1754944e-38f) ? fTemp69 : 0.0f);
			float fTemp70 = std::max<float>(0.0f, std::min<float>(1e+01f, 1e+01f * (fSlow5 + fRec14[0])));
			float fTemp71 = fTemp70 * (0.013f - 0.0006f * fTemp70) + 0.26f;
			float fTemp72 = fRec4[1] + fRec8[1];
			float fTemp73 = fRec10[1] + fTemp72 + fRec6[1];
			fVec21[IOTA0 & 16383] = (fRec11[1] + fRec7[1] + fRec9[1] + fTemp73 + fRec5[1]) * fTemp71 - 0.01f * fVec19[(IOTA0 - iConst20) & 2047];
			fVec22[0] = fSlow6;
			float fTemp74 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow6 - fVec22[1]))) + fRec15[1]);
			fRec15[0] = ((std::fabs(fTemp74) > 1.1754944e-38f) ? fTemp74 : 0.0f);
			float fTemp75 = std::max<float>(0.0f, std::min<float>(1e+01f, 1e+01f * (fSlow7 + fRec15[0])));
			float fTemp76 = fTemp75 * (0.00032f * fTemp75 + -0.0072f);
			float fTemp77 = fTemp4 * (fVec21[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.0705f)))))) & 16383] - fRec12[1]) * fTemp5;
			float fTemp78 = fRec12[1] + 2.0f * fTemp77;
			fRec12[0] = ((std::fabs(fTemp78) > 1.1754944e-38f) ? fTemp78 : 0.0f);
			float fTemp79 = fRec12[1] + fTemp77;
			fRec13[0] = ((std::fabs(fTemp79) > 1.1754944e-38f) ? fTemp79 : 0.0f);
			float fTemp80 = fRec13[0] - fRec13[1];
			float fTemp81 = ((std::fabs(fTemp80) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec13[0] + fRec13[1]))) : ((((fRec13[0] <= 1.0f) & (fRec13[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec13[0]) : fRec13[0] * float((fRec13[0] > 0.0f) - (fRec13[0] < 0.0f)) + -0.5f) - (((fRec13[1] <= 1.0f) & (fRec13[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec13[1]) : fRec13[1] * float((fRec13[1] > 0.0f) - (fRec13[1] < 0.0f)) + -0.5f)) / fTemp80);
			fRec4[0] = ((std::fabs(fTemp81) > 1.1754944e-38f) ? fTemp81 : 0.0f);
			fVec23[IOTA0 & 4095] = fTemp68 + fTemp61;
			fVec24[IOTA0 & 16383] = fTemp71 * (fTemp73 - (fRec11[1] + fRec5[1] + fRec9[1] + fRec7[1])) + 0.01f * fVec23[(IOTA0 - iConst24) & 4095];
			float fTemp82 = fTemp4 * fTemp5;
			float fTemp83 = fTemp82 * (fVec24[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.071f)))))) & 16383] - fRec16[1]);
			float fTemp84 = fRec16[1] + 2.0f * fTemp83;
			fRec16[0] = ((std::fabs(fTemp84) > 1.1754944e-38f) ? fTemp84 : 0.0f);
			float fTemp85 = fRec16[1] + fTemp83;
			fRec17[0] = ((std::fabs(fTemp85) > 1.1754944e-38f) ? fTemp85 : 0.0f);
			float fTemp86 = fRec17[0] - fRec17[1];
			float fTemp87 = ((std::fabs(fTemp86) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec17[0] + fRec17[1]))) : ((((fRec17[0] <= 1.0f) & (fRec17[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec17[0]) : fRec17[0] * float((fRec17[0] > 0.0f) - (fRec17[0] < 0.0f)) + -0.5f) - (((fRec17[1] <= 1.0f) & (fRec17[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec17[1]) : fRec17[1] * float((fRec17[1] > 0.0f) - (fRec17[1] < 0.0f)) + -0.5f)) / fTemp86);
			fRec5[0] = ((std::fabs(fTemp87) > 1.1754944e-38f) ? fTemp87 : 0.0f);
			float fTemp88 = fTemp60 + fTemp53;
			float fTemp89 = fTemp67 + fTemp64;
			fVec25[IOTA0 & 4095] = fTemp89 - fTemp88;
			float fTemp90 = fRec6[1] + fRec10[1];
			fVec26[IOTA0 & 16383] = fTemp71 * (fRec9[1] + fTemp72 + fRec5[1] - (fRec11[1] + fTemp90 + fRec7[1])) + 0.01f * fVec25[(IOTA0 - iConst30) & 4095];
			float fTemp91 = fTemp82 * (fVec26[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.0715f)))))) & 16383] - fRec18[1]);
			float fTemp92 = fRec18[1] + 2.0f * fTemp91;
			fRec18[0] = ((std::fabs(fTemp92) > 1.1754944e-38f) ? fTemp92 : 0.0f);
			float fTemp93 = fRec18[1] + fTemp91;
			fRec19[0] = ((std::fabs(fTemp93) > 1.1754944e-38f) ? fTemp93 : 0.0f);
			float fTemp94 = fRec19[0] - fRec19[1];
			float fTemp95 = ((std::fabs(fTemp94) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec19[0] + fRec19[1]))) : ((((fRec19[0] <= 1.0f) & (fRec19[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec19[0]) : fRec19[0] * float((fRec19[0] > 0.0f) - (fRec19[0] < 0.0f)) + -0.5f) - (((fRec19[1] <= 1.0f) & (fRec19[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec19[1]) : fRec19[1] * float((fRec19[1] > 0.0f) - (fRec19[1] < 0.0f)) + -0.5f)) / fTemp94);
			fRec6[0] = ((std::fabs(fTemp95) > 1.1754944e-38f) ? fTemp95 : 0.0f);
			fVec27[IOTA0 & 4095] = fTemp89 + fTemp88;
			fVec28[IOTA0 & 16383] = fTemp71 * (fRec11[1] + fTemp72 + fRec7[1] - (fRec9[1] + fTemp90 + fRec5[1])) - 0.01f * fVec27[(IOTA0 - iConst31) & 4095];
			float fTemp96 = fTemp82 * (fVec28[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.0725f)))))) & 16383] - fRec20[1]);
			float fTemp97 = fRec20[1] + 2.0f * fTemp96;
			fRec20[0] = ((std::fabs(fTemp97) > 1.1754944e-38f) ? fTemp97 : 0.0f);
			float fTemp98 = fRec20[1] + fTemp96;
			fRec21[0] = ((std::fabs(fTemp98) > 1.1754944e-38f) ? fTemp98 : 0.0f);
			float fTemp99 = fRec21[0] - fRec21[1];
			float fTemp100 = ((std::fabs(fTemp99) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec21[0] + fRec21[1]))) : ((((fRec21[0] <= 1.0f) & (fRec21[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec21[0]) : fRec21[0] * float((fRec21[0] > 0.0f) - (fRec21[0] < 0.0f)) + -0.5f) - (((fRec21[1] <= 1.0f) & (fRec21[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec21[1]) : fRec21[1] * float((fRec21[1] > 0.0f) - (fRec21[1] < 0.0f)) + -0.5f)) / fTemp99);
			fRec7[0] = ((std::fabs(fTemp100) > 1.1754944e-38f) ? fTemp100 : 0.0f);
			float fTemp101 = fTemp52 - fTemp45;
			float fTemp102 = fTemp59 - fTemp56;
			float fTemp103 = fTemp102 - fTemp101;
			float fTemp104 = fTemp63 - fTemp62;
			float fTemp105 = fTemp66 + fTemp65;
			float fTemp106 = fTemp105 - fTemp104;
			fVec29[IOTA0 & 4095] = fTemp106 - fTemp103;
			float fTemp107 = fRec8[1] + fRec10[1];
			float fTemp108 = fRec4[1] + fRec6[1];
			fVec30[IOTA0 & 16383] = fTemp71 * (fRec7[1] + fRec5[1] + fTemp108 - (fRec11[1] + fRec9[1] + fTemp107)) - 0.01f * fVec29[(IOTA0 - iConst32) & 4095];
			float fTemp109 = fTemp82 * (fVec30[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.0735f)))))) & 16383] - fRec22[1]);
			float fTemp110 = fRec22[1] + 2.0f * fTemp109;
			fRec22[0] = ((std::fabs(fTemp110) > 1.1754944e-38f) ? fTemp110 : 0.0f);
			float fTemp111 = fRec22[1] + fTemp109;
			fRec23[0] = ((std::fabs(fTemp111) > 1.1754944e-38f) ? fTemp111 : 0.0f);
			float fTemp112 = fRec23[0] - fRec23[1];
			float fTemp113 = ((std::fabs(fTemp112) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec23[0] + fRec23[1]))) : ((((fRec23[0] <= 1.0f) & (fRec23[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec23[0]) : fRec23[0] * float((fRec23[0] > 0.0f) - (fRec23[0] < 0.0f)) + -0.5f) - (((fRec23[1] <= 1.0f) & (fRec23[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec23[1]) : fRec23[1] * float((fRec23[1] > 0.0f) - (fRec23[1] < 0.0f)) + -0.5f)) / fTemp112);
			fRec8[0] = ((std::fabs(fTemp113) > 1.1754944e-38f) ? fTemp113 : 0.0f);
			fVec31[IOTA0 & 8191] = fTemp106 + fTemp103;
			fVec32[IOTA0 & 16383] = fTemp71 * (fRec11[1] + fRec9[1] + fTemp108 - (fRec7[1] + fRec5[1] + fTemp107)) - 0.01f * fVec31[(IOTA0 - iConst33) & 8191];
			float fTemp114 = fTemp82 * (fVec32[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.0755f)))))) & 16383] - fRec24[1]);
			float fTemp115 = fRec24[1] + 2.0f * fTemp114;
			fRec24[0] = ((std::fabs(fTemp115) > 1.1754944e-38f) ? fTemp115 : 0.0f);
			float fTemp116 = fRec24[1] + fTemp114;
			fRec25[0] = ((std::fabs(fTemp116) > 1.1754944e-38f) ? fTemp116 : 0.0f);
			float fTemp117 = fRec25[0] - fRec25[1];
			float fTemp118 = ((std::fabs(fTemp117) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec25[0] + fRec25[1]))) : ((((fRec25[0] <= 1.0f) & (fRec25[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec25[0]) : fRec25[0] * float((fRec25[0] > 0.0f) - (fRec25[0] < 0.0f)) + -0.5f) - (((fRec25[1] <= 1.0f) & (fRec25[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec25[1]) : fRec25[1] * float((fRec25[1] > 0.0f) - (fRec25[1] < 0.0f)) + -0.5f)) / fTemp117);
			fRec9[0] = ((std::fabs(fTemp118) > 1.1754944e-38f) ? fTemp118 : 0.0f);
			float fTemp119 = fTemp102 + fTemp101;
			float fTemp120 = fTemp105 + fTemp104;
			fVec33[IOTA0 & 8191] = fTemp120 - fTemp119;
			float fTemp121 = fRec8[1] + fRec6[1];
			float fTemp122 = fRec4[1] + fRec10[1];
			fVec34[IOTA0 & 16383] = fTemp71 * (fRec11[1] + fRec5[1] + fTemp122 - (fRec7[1] + fRec9[1] + fTemp121)) + 0.01f * fVec33[(IOTA0 - iConst34) & 8191];
			float fTemp123 = fTemp82 * (fVec34[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.0765f)))))) & 16383] - fRec26[1]);
			float fTemp124 = fRec26[1] + 2.0f * fTemp123;
			fRec26[0] = ((std::fabs(fTemp124) > 1.1754944e-38f) ? fTemp124 : 0.0f);
			float fTemp125 = fRec26[1] + fTemp123;
			fRec27[0] = ((std::fabs(fTemp125) > 1.1754944e-38f) ? fTemp125 : 0.0f);
			float fTemp126 = fRec27[0] - fRec27[1];
			float fTemp127 = ((std::fabs(fTemp126) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec27[0] + fRec27[1]))) : ((((fRec27[0] <= 1.0f) & (fRec27[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec27[0]) : fRec27[0] * float((fRec27[0] > 0.0f) - (fRec27[0] < 0.0f)) + -0.5f) - (((fRec27[1] <= 1.0f) & (fRec27[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec27[1]) : fRec27[1] * float((fRec27[1] > 0.0f) - (fRec27[1] < 0.0f)) + -0.5f)) / fTemp126);
			fRec10[0] = ((std::fabs(fTemp127) > 1.1754944e-38f) ? fTemp127 : 0.0f);
			fVec35[IOTA0 & 8191] = fTemp120 + fTemp119;
			fVec36[IOTA0 & 16383] = fTemp71 * (fRec7[1] + fRec9[1] + fTemp122 - (fRec11[1] + fRec5[1] + fTemp121)) - 0.01f * fVec35[(IOTA0 - iConst35) & 8191];
			float fTemp128 = fTemp82 * (fVec36[(IOTA0 - int(std::min<float>(fConst29, std::max<float>(0.0f, std::round(fConst0 * (fTemp76 + 0.0785f)))))) & 16383] - fRec28[1]);
			float fTemp129 = fRec28[1] + 2.0f * fTemp128;
			fRec28[0] = ((std::fabs(fTemp129) > 1.1754944e-38f) ? fTemp129 : 0.0f);
			float fTemp130 = fRec28[1] + fTemp128;
			fRec29[0] = ((std::fabs(fTemp130) > 1.1754944e-38f) ? fTemp130 : 0.0f);
			float fTemp131 = fRec29[0] - fRec29[1];
			float fTemp132 = ((std::fabs(fTemp131) <= 0.001f) ? std::max<float>(-1.0f, std::min<float>(1.0f, 0.5f * (fRec29[0] + fRec29[1]))) : ((((fRec29[0] <= 1.0f) & (fRec29[0] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec29[0]) : fRec29[0] * float((fRec29[0] > 0.0f) - (fRec29[0] < 0.0f)) + -0.5f) - (((fRec29[1] <= 1.0f) & (fRec29[1] >= -1.0f)) ? 0.5f * Faust_Spring_faustpower2_f(fRec29[1]) : fRec29[1] * float((fRec29[1] > 0.0f) - (fRec29[1] < 0.0f)) + -0.5f)) / fTemp131);
			fRec11[0] = ((std::fabs(fTemp132) > 1.1754944e-38f) ? fTemp132 : 0.0f);
			float fTemp133 = fRec4[0] + fRec5[0] + fRec6[0] + fRec7[0] + fRec8[0] + fRec9[0] + fRec10[0] + fRec11[0] - fRec2[1];
			float fTemp134 = fRec2[1] + fConst38 * fTemp133;
			fRec2[0] = ((std::fabs(fTemp134) > 1.1754944e-38f) ? fTemp134 : 0.0f);
			float fTemp135 = fConst37 * fTemp133;
			float fRec3 = ((std::fabs(fTemp135) > 1.1754944e-38f) ? fTemp135 : 0.0f);
			output0[i0] = FAUSTFLOAT(fTemp6 + 0.04f * fRec3 * std::max<float>(0.0f, std::min<float>(1e+01f, 1e+01f * (fSlow3 + fRec1[0]))) * std::min<float>(5.0f, 2e+03f / fTemp2 + 1.0f));
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec1[1] = fRec1[0];
			IOTA0 = IOTA0 + 1;
			fVec20[1] = fVec20[0];
			fRec14[1] = fRec14[0];
			fVec22[1] = fVec22[0];
			fRec15[1] = fRec15[0];
			fRec12[1] = fRec12[0];
			fRec13[1] = fRec13[0];
			fRec4[1] = fRec4[0];
			fRec16[1] = fRec16[0];
			fRec17[1] = fRec17[0];
			fRec5[1] = fRec5[0];
			fRec18[1] = fRec18[0];
			fRec19[1] = fRec19[0];
			fRec6[1] = fRec6[0];
			fRec20[1] = fRec20[0];
			fRec21[1] = fRec21[0];
			fRec7[1] = fRec7[0];
			fRec22[1] = fRec22[0];
			fRec23[1] = fRec23[0];
			fRec8[1] = fRec8[0];
			fRec24[1] = fRec24[0];
			fRec25[1] = fRec25[0];
			fRec9[1] = fRec9[0];
			fRec26[1] = fRec26[0];
			fRec27[1] = fRec27[0];
			fRec10[1] = fRec10[0];
			fRec28[1] = fRec28[0];
			fRec29[1] = fRec29[0];
			fRec11[1] = fRec11[0];
			fRec2[1] = fRec2[0];
		}
	}

};

#endif
