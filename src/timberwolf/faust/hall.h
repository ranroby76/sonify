// D:\workspace\Fanan_web_hub\src\timberwolf\faust\hall.h
// Written by the Faust compiler (2.70.3) from hall.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "hall"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Hall -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Hall_H__
#define  __Faust_Hall_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Hall
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

static float Faust_Hall_faustpower2_f(float value) {
	return value * value;
}

class Faust_Hall : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	int IOTA0;
	float fVec1[65536];
	FAUSTFLOAT fHslider1;
	float fConst2;
	FAUSTFLOAT fHslider2;
	float fConst3;
	float fRec11[2];
	FAUSTFLOAT fHslider3;
	float fConst5;
	FAUSTFLOAT fHslider4;
	float fVec2[2];
	float fConst6;
	FAUSTFLOAT fHslider5;
	float fConst7;
	float fRec13[2];
	float fVec3[65536];
	int iConst9;
	float fVec4[8192];
	int iConst10;
	float fRec9[2];
	float fVec5[65536];
	float fRec16[2];
	float fConst12;
	float fVec6[2];
	float fRec18[2];
	float fVec7[32768];
	int iConst14;
	float fVec8[4096];
	int iConst15;
	float fRec14[2];
	float fRec21[2];
	float fConst17;
	float fVec9[2];
	float fRec23[2];
	float fVec10[32768];
	int iConst19;
	float fVec11[8192];
	int iConst20;
	float fRec19[2];
	float fRec26[2];
	float fConst22;
	float fVec12[2];
	float fRec28[2];
	float fVec13[32768];
	int iConst24;
	float fVec14[4096];
	int iConst25;
	float fRec24[2];
	float fRec31[2];
	float fConst27;
	float fVec15[2];
	float fRec33[2];
	float fVec16[65536];
	int iConst29;
	float fVec17[8192];
	int iConst30;
	float fRec29[2];
	float fRec36[2];
	float fConst32;
	float fVec18[2];
	float fRec38[2];
	float fVec19[65536];
	int iConst34;
	float fVec20[8192];
	int iConst35;
	float fRec34[2];
	float fRec41[2];
	float fConst37;
	float fVec21[2];
	float fRec43[2];
	float fVec22[65536];
	int iConst39;
	float fVec23[8192];
	int iConst40;
	float fRec39[2];
	float fRec46[2];
	float fConst42;
	float fVec24[2];
	float fRec48[2];
	float fVec25[65536];
	int iConst44;
	float fVec26[4096];
	int iConst45;
	float fRec44[2];
	float fRec1[2];
	float fRec2[2];
	float fRec3[2];
	float fRec4[2];
	float fRec5[2];
	float fRec6[2];
	float fRec7[2];
	float fRec8[2];
	
 public:
	Faust_Hall() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Hall -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "hall.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/allpass_comb:author", "Julius O. Smith III");
		m->declare("filters.lib/allpass_comb:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/allpass_comb:license", "LicenseRef-STK-4.3");
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
		m->declare("name", "hall");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("reverbs.lib/name", "Faust Reverb Library");
		m->declare("reverbs.lib/version", "1.6.2");
		m->declare("routes.lib/hadamard:author", "Remy Muller, revised by Romain Michon");
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
		fConst2 = 0.001f * fConst0;
		fConst3 = 3.1415927f / fConst0;
		float fConst4 = std::floor(0.174713f * fConst0 + 0.5f);
		fConst5 = 6.9077554f * (fConst4 / fConst0);
		fConst6 = 0.49f * fConst0;
		fConst7 = 6.2831855f / fConst0;
		float fConst8 = std::floor(0.022904f * fConst0 + 0.5f);
		iConst9 = int(std::min<float>(32768.0f, std::max<float>(0.0f, fConst4 + (-1.0f - fConst8))));
		iConst10 = int(std::min<float>(8192.0f, std::max<float>(0.0f, fConst8 + -1.0f)));
		float fConst11 = std::floor(0.153129f * fConst0 + 0.5f);
		fConst12 = 6.9077554f * (fConst11 / fConst0);
		float fConst13 = std::floor(0.020346f * fConst0 + 0.5f);
		iConst14 = int(std::min<float>(32768.0f, std::max<float>(0.0f, fConst11 + (-1.0f - fConst13))));
		iConst15 = int(std::min<float>(4096.0f, std::max<float>(0.0f, fConst13 + -1.0f)));
		float fConst16 = std::floor(0.127837f * fConst0 + 0.5f);
		fConst17 = 6.9077554f * (fConst16 / fConst0);
		float fConst18 = std::floor(0.031604f * fConst0 + 0.5f);
		iConst19 = int(std::min<float>(32768.0f, std::max<float>(0.0f, fConst16 + (-1.0f - fConst18))));
		iConst20 = int(std::min<float>(8192.0f, std::max<float>(0.0f, fConst18 + -1.0f)));
		float fConst21 = std::floor(0.125f * fConst0 + 0.5f);
		fConst22 = 6.9077554f * (fConst21 / fConst0);
		float fConst23 = std::floor(0.013458f * fConst0 + 0.5f);
		iConst24 = int(std::min<float>(32768.0f, std::max<float>(0.0f, fConst21 + (-1.0f - fConst23))));
		iConst25 = int(std::min<float>(4096.0f, std::max<float>(0.0f, fConst23 + -1.0f)));
		float fConst26 = std::floor(0.210389f * fConst0 + 0.5f);
		fConst27 = 6.9077554f * (fConst26 / fConst0);
		float fConst28 = std::floor(0.024421f * fConst0 + 0.5f);
		iConst29 = int(std::min<float>(65536.0f, std::max<float>(0.0f, fConst26 + (-1.0f - fConst28))));
		iConst30 = int(std::min<float>(8192.0f, std::max<float>(0.0f, fConst28 + -1.0f)));
		float fConst31 = std::floor(0.192303f * fConst0 + 0.5f);
		fConst32 = 6.9077554f * (fConst31 / fConst0);
		float fConst33 = std::floor(0.029291f * fConst0 + 0.5f);
		iConst34 = int(std::min<float>(32768.0f, std::max<float>(0.0f, fConst31 + (-1.0f - fConst33))));
		iConst35 = int(std::min<float>(8192.0f, std::max<float>(0.0f, fConst33 + -1.0f)));
		float fConst36 = std::floor(0.256891f * fConst0 + 0.5f);
		fConst37 = 6.9077554f * (fConst36 / fConst0);
		float fConst38 = std::floor(0.027333f * fConst0 + 0.5f);
		iConst39 = int(std::min<float>(65536.0f, std::max<float>(0.0f, fConst36 + (-1.0f - fConst38))));
		iConst40 = int(std::min<float>(8192.0f, std::max<float>(0.0f, fConst38 + -1.0f)));
		float fConst41 = std::floor(0.219991f * fConst0 + 0.5f);
		fConst42 = 6.9077554f * (fConst41 / fConst0);
		float fConst43 = std::floor(0.019123f * fConst0 + 0.5f);
		iConst44 = int(std::min<float>(65536.0f, std::max<float>(0.0f, fConst41 + (-1.0f - fConst43))));
		iConst45 = int(std::min<float>(4096.0f, std::max<float>(0.0f, fConst43 + -1.0f)));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(3e+01f);
		fHslider1 = FAUSTFLOAT(4e+01f);
		fHslider2 = FAUSTFLOAT(2e+02f);
		fHslider3 = FAUSTFLOAT(2.0f);
		fHslider4 = FAUSTFLOAT(3.0f);
		fHslider5 = FAUSTFLOAT(6e+03f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec0[l1] = 0.0f;
		}
		IOTA0 = 0;
		for (int l2 = 0; l2 < 65536; l2 = l2 + 1) {
			fVec1[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec11[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec13[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 65536; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 8192; l7 = l7 + 1) {
			fVec4[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec9[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 65536; l9 = l9 + 1) {
			fVec5[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec16[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fVec6[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec18[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 32768; l13 = l13 + 1) {
			fVec7[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 4096; l14 = l14 + 1) {
			fVec8[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec14[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec21[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fVec9[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec23[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 32768; l19 = l19 + 1) {
			fVec10[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 8192; l20 = l20 + 1) {
			fVec11[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec19[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fRec26[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fVec12[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fRec28[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 32768; l25 = l25 + 1) {
			fVec13[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 4096; l26 = l26 + 1) {
			fVec14[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec24[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec31[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fVec15[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2; l30 = l30 + 1) {
			fRec33[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 65536; l31 = l31 + 1) {
			fVec16[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 8192; l32 = l32 + 1) {
			fVec17[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec29[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			fRec36[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 2; l35 = l35 + 1) {
			fVec18[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			fRec38[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 65536; l37 = l37 + 1) {
			fVec19[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 8192; l38 = l38 + 1) {
			fVec20[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 2; l39 = l39 + 1) {
			fRec34[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 2; l40 = l40 + 1) {
			fRec41[l40] = 0.0f;
		}
		for (int l41 = 0; l41 < 2; l41 = l41 + 1) {
			fVec21[l41] = 0.0f;
		}
		for (int l42 = 0; l42 < 2; l42 = l42 + 1) {
			fRec43[l42] = 0.0f;
		}
		for (int l43 = 0; l43 < 65536; l43 = l43 + 1) {
			fVec22[l43] = 0.0f;
		}
		for (int l44 = 0; l44 < 8192; l44 = l44 + 1) {
			fVec23[l44] = 0.0f;
		}
		for (int l45 = 0; l45 < 2; l45 = l45 + 1) {
			fRec39[l45] = 0.0f;
		}
		for (int l46 = 0; l46 < 2; l46 = l46 + 1) {
			fRec46[l46] = 0.0f;
		}
		for (int l47 = 0; l47 < 2; l47 = l47 + 1) {
			fVec24[l47] = 0.0f;
		}
		for (int l48 = 0; l48 < 2; l48 = l48 + 1) {
			fRec48[l48] = 0.0f;
		}
		for (int l49 = 0; l49 < 65536; l49 = l49 + 1) {
			fVec25[l49] = 0.0f;
		}
		for (int l50 = 0; l50 < 4096; l50 = l50 + 1) {
			fVec26[l50] = 0.0f;
		}
		for (int l51 = 0; l51 < 2; l51 = l51 + 1) {
			fRec44[l51] = 0.0f;
		}
		for (int l52 = 0; l52 < 2; l52 = l52 + 1) {
			fRec1[l52] = 0.0f;
		}
		for (int l53 = 0; l53 < 2; l53 = l53 + 1) {
			fRec2[l53] = 0.0f;
		}
		for (int l54 = 0; l54 < 2; l54 = l54 + 1) {
			fRec3[l54] = 0.0f;
		}
		for (int l55 = 0; l55 < 2; l55 = l55 + 1) {
			fRec4[l55] = 0.0f;
		}
		for (int l56 = 0; l56 < 2; l56 = l56 + 1) {
			fRec5[l56] = 0.0f;
		}
		for (int l57 = 0; l57 < 2; l57 = l57 + 1) {
			fRec6[l57] = 0.0f;
		}
		for (int l58 = 0; l58 < 2; l58 = l58 + 1) {
			fRec7[l58] = 0.0f;
		}
		for (int l59 = 0; l59 < 2; l59 = l59 + 1) {
			fRec8[l59] = 0.0f;
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
	
	virtual Faust_Hall* clone() {
		return new Faust_Hall();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("hall");
		ui_interface->addHorizontalSlider("Damp", &fHslider5, FAUSTFLOAT(6e+03f), FAUSTFLOAT(1.5e+03f), FAUSTFLOAT(2.3e+04f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Low Time", &fHslider4, FAUSTFLOAT(3.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(8.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Low X", &fHslider2, FAUSTFLOAT(2e+02f), FAUSTFLOAT(5e+01f), FAUSTFLOAT(1e+03f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Mid Time", &fHslider3, FAUSTFLOAT(2.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(8.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Mix", &fHslider0, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Pre-delay", &fHslider1, FAUSTFLOAT(4e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2e+02f), FAUSTFLOAT(1.0f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.01f * fSlow0;
		int iSlow2 = int(std::min<float>(32768.0f, std::max<float>(0.0f, std::floor(fConst2 * (float(fHslider1) + -2e+01f) + 0.5f))));
		float fSlow3 = std::tan(std::max<float>(0.0f, std::min<float>(fConst3 * float(fHslider2), 1.5676547f)));
		float fSlow4 = fSlow3 * std::min<float>(3.4028235e+38f, 1.0f / (fSlow3 + 1.0f));
		float fSlow5 = 2.0f * fSlow4;
		float fSlow6 = float(fHslider3);
		float fSlow7 = std::exp(-(fConst5 / fSlow6));
		float fSlow8 = float(fHslider4);
		float fSlow9 = std::exp(-(fConst5 / fSlow8)) / fSlow7 + -1.0f;
		float fSlow10 = float(fHslider5);
		float fSlow11 = ((fSlow10 > fConst6) ? -1.0f : std::cos(fConst7 * fSlow10));
		float fSlow12 = Faust_Hall_faustpower2_f(fSlow7);
		float fSlow13 = 1.0f - fSlow12 * fSlow11;
		float fSlow14 = 1.0f - fSlow12;
		float fSlow15 = 1.0f / fSlow14 * fSlow13 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow14) * Faust_Hall_faustpower2_f(fSlow13) + -1.0f));
		float fSlow16 = std::exp(-(fConst12 / fSlow6));
		float fSlow17 = std::exp(-(fConst12 / fSlow8)) / fSlow16 + -1.0f;
		float fSlow18 = Faust_Hall_faustpower2_f(fSlow16);
		float fSlow19 = 1.0f - fSlow18 * fSlow11;
		float fSlow20 = 1.0f - fSlow18;
		float fSlow21 = 1.0f / fSlow20 * fSlow19 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow20) * Faust_Hall_faustpower2_f(fSlow19) + -1.0f));
		float fSlow22 = std::exp(-(fConst17 / fSlow6));
		float fSlow23 = std::exp(-(fConst17 / fSlow8)) / fSlow22 + -1.0f;
		float fSlow24 = Faust_Hall_faustpower2_f(fSlow22);
		float fSlow25 = 1.0f - fSlow24 * fSlow11;
		float fSlow26 = 1.0f - fSlow24;
		float fSlow27 = 1.0f / fSlow26 * fSlow25 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow26) * Faust_Hall_faustpower2_f(fSlow25) + -1.0f));
		float fSlow28 = std::exp(-(fConst22 / fSlow6));
		float fSlow29 = std::exp(-(fConst22 / fSlow8)) / fSlow28 + -1.0f;
		float fSlow30 = Faust_Hall_faustpower2_f(fSlow28);
		float fSlow31 = 1.0f - fSlow30 * fSlow11;
		float fSlow32 = 1.0f - fSlow30;
		float fSlow33 = 1.0f / fSlow32 * fSlow31 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow32) * Faust_Hall_faustpower2_f(fSlow31) + -1.0f));
		float fSlow34 = std::exp(-(fConst27 / fSlow6));
		float fSlow35 = std::exp(-(fConst27 / fSlow8)) / fSlow34 + -1.0f;
		float fSlow36 = Faust_Hall_faustpower2_f(fSlow34);
		float fSlow37 = 1.0f - fSlow36 * fSlow11;
		float fSlow38 = 1.0f - fSlow36;
		float fSlow39 = 1.0f / fSlow38 * fSlow37 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow38) * Faust_Hall_faustpower2_f(fSlow37) + -1.0f));
		float fSlow40 = std::exp(-(fConst32 / fSlow6));
		float fSlow41 = std::exp(-(fConst32 / fSlow8)) / fSlow40 + -1.0f;
		float fSlow42 = Faust_Hall_faustpower2_f(fSlow40);
		float fSlow43 = 1.0f - fSlow42 * fSlow11;
		float fSlow44 = 1.0f - fSlow42;
		float fSlow45 = 1.0f / fSlow44 * fSlow43 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow44) * Faust_Hall_faustpower2_f(fSlow43) + -1.0f));
		float fSlow46 = std::exp(-(fConst37 / fSlow6));
		float fSlow47 = std::exp(-(fConst37 / fSlow8)) / fSlow46 + -1.0f;
		float fSlow48 = Faust_Hall_faustpower2_f(fSlow46);
		float fSlow49 = 1.0f - fSlow48 * fSlow11;
		float fSlow50 = 1.0f - fSlow48;
		float fSlow51 = 1.0f / fSlow50 * fSlow49 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow50) * Faust_Hall_faustpower2_f(fSlow49) + -1.0f));
		float fSlow52 = std::exp(-(fConst42 / fSlow6));
		float fSlow53 = std::exp(-(fConst42 / fSlow8)) / fSlow52 + -1.0f;
		float fSlow54 = Faust_Hall_faustpower2_f(fSlow52);
		float fSlow55 = 1.0f - fSlow54 * fSlow11;
		float fSlow56 = 1.0f - fSlow54;
		float fSlow57 = 1.0f / fSlow56 * fSlow55 - std::sqrt(std::max<float>(0.0f, 1.0f / Faust_Hall_faustpower2_f(fSlow56) * Faust_Hall_faustpower2_f(fSlow55) + -1.0f));
		float fSlow58 = 0.525f / std::sqrt(fSlow6);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec0[1]))));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow1 + fRec0[0];
			float fTemp2 = float(input1[i0]);
			fVec1[IOTA0 & 65535] = fTemp2;
			float fTemp3 = 0.3f * fVec1[(IOTA0 - iSlow2) & 65535];
			float fTemp4 = fRec5[1] - fRec11[1];
			float fTemp5 = fRec11[1] + fSlow5 * fTemp4;
			fRec11[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			float fTemp6 = fRec11[1] + fSlow4 * fTemp4;
			float fRec12 = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = fSlow7 * (fRec5[1] + fSlow9 * fRec12);
			fVec2[0] = fTemp7;
			float fTemp8 = fSlow15 * (fRec13[1] + std::min<float>(3.4028235e+38f, fVec2[1] - fTemp7));
			fRec13[0] = ((std::fabs(fTemp8) > 1.1754944e-38f) ? fTemp8 : 0.0f);
			fVec3[IOTA0 & 65535] = 0.35355338f * (fRec13[0] + fTemp7) + 1e-20f;
			float fTemp9 = fVec3[(IOTA0 - iConst9) & 65535] + fTemp3 - 0.6f * fRec9[1];
			fVec4[IOTA0 & 8191] = fTemp9;
			float fTemp10 = fVec4[(IOTA0 - iConst10) & 8191];
			fRec9[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = 0.6f * fTemp9;
			float fRec10 = ((std::fabs(fTemp11) > 1.1754944e-38f) ? fTemp11 : 0.0f);
			float fTemp12 = float(input0[i0]);
			fVec5[IOTA0 & 65535] = fTemp12;
			float fTemp13 = 0.3f * fVec5[(IOTA0 - iSlow2) & 65535];
			float fTemp14 = fRec1[1] - fRec16[1];
			float fTemp15 = fRec16[1] + fSlow5 * fTemp14;
			fRec16[0] = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = fRec16[1] + fSlow4 * fTemp14;
			float fRec17 = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			float fTemp17 = fSlow16 * (fRec1[1] + fSlow17 * fRec17);
			fVec6[0] = fTemp17;
			float fTemp18 = fSlow21 * (fRec18[1] + std::min<float>(3.4028235e+38f, fVec6[1] - fTemp17));
			fRec18[0] = ((std::fabs(fTemp18) > 1.1754944e-38f) ? fTemp18 : 0.0f);
			fVec7[IOTA0 & 32767] = 0.35355338f * (fRec18[0] + fTemp17) + 1e-20f;
			float fTemp19 = fVec7[(IOTA0 - iConst14) & 32767] + fTemp13 - 0.6f * fRec14[1];
			fVec8[IOTA0 & 4095] = fTemp19;
			float fTemp20 = fVec8[(IOTA0 - iConst15) & 4095];
			fRec14[0] = ((std::fabs(fTemp20) > 1.1754944e-38f) ? fTemp20 : 0.0f);
			float fTemp21 = 0.6f * fTemp19;
			float fRec15 = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fRec15 + fRec10;
			float fTemp23 = fRec3[1] - fRec21[1];
			float fTemp24 = fRec21[1] + fSlow5 * fTemp23;
			fRec21[0] = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			float fTemp25 = fRec21[1] + fSlow4 * fTemp23;
			float fRec22 = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			float fTemp26 = fSlow22 * (fRec3[1] + fSlow23 * fRec22);
			fVec9[0] = fTemp26;
			float fTemp27 = fSlow27 * (fRec23[1] + std::min<float>(3.4028235e+38f, fVec9[1] - fTemp26));
			fRec23[0] = ((std::fabs(fTemp27) > 1.1754944e-38f) ? fTemp27 : 0.0f);
			fVec10[IOTA0 & 32767] = 0.35355338f * (fRec23[0] + fTemp26) + 1e-20f;
			float fTemp28 = fVec10[(IOTA0 - iConst19) & 32767] - (fTemp13 + 0.6f * fRec19[1]);
			fVec11[IOTA0 & 8191] = fTemp28;
			float fTemp29 = fVec11[(IOTA0 - iConst20) & 8191];
			fRec19[0] = ((std::fabs(fTemp29) > 1.1754944e-38f) ? fTemp29 : 0.0f);
			float fTemp30 = 0.6f * fTemp28;
			float fRec20 = ((std::fabs(fTemp30) > 1.1754944e-38f) ? fTemp30 : 0.0f);
			float fTemp31 = fRec7[1] - fRec26[1];
			float fTemp32 = fRec26[1] + fSlow5 * fTemp31;
			fRec26[0] = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fTemp33 = fRec26[1] + fSlow4 * fTemp31;
			float fRec27 = ((std::fabs(fTemp33) > 1.1754944e-38f) ? fTemp33 : 0.0f);
			float fTemp34 = fSlow28 * (fRec7[1] + fSlow29 * fRec27);
			fVec12[0] = fTemp34;
			float fTemp35 = fSlow33 * (fRec28[1] + std::min<float>(3.4028235e+38f, fVec12[1] - fTemp34));
			fRec28[0] = ((std::fabs(fTemp35) > 1.1754944e-38f) ? fTemp35 : 0.0f);
			fVec13[IOTA0 & 32767] = 0.35355338f * (fRec28[0] + fTemp34) + 1e-20f;
			float fTemp36 = fVec13[(IOTA0 - iConst24) & 32767] - (fTemp3 + 0.6f * fRec24[1]);
			fVec14[IOTA0 & 4095] = fTemp36;
			float fTemp37 = fVec14[(IOTA0 - iConst25) & 4095];
			fRec24[0] = ((std::fabs(fTemp37) > 1.1754944e-38f) ? fTemp37 : 0.0f);
			float fTemp38 = 0.6f * fTemp36;
			float fRec25 = ((std::fabs(fTemp38) > 1.1754944e-38f) ? fTemp38 : 0.0f);
			float fTemp39 = fRec25 + fRec20 + fTemp22;
			float fTemp40 = fRec2[1] - fRec31[1];
			float fTemp41 = fRec31[1] + fSlow5 * fTemp40;
			fRec31[0] = ((std::fabs(fTemp41) > 1.1754944e-38f) ? fTemp41 : 0.0f);
			float fTemp42 = fRec31[1] + fSlow4 * fTemp40;
			float fRec32 = ((std::fabs(fTemp42) > 1.1754944e-38f) ? fTemp42 : 0.0f);
			float fTemp43 = fSlow34 * (fRec2[1] + fSlow35 * fRec32);
			fVec15[0] = fTemp43;
			float fTemp44 = fSlow39 * (fRec33[1] + std::min<float>(3.4028235e+38f, fVec15[1] - fTemp43));
			fRec33[0] = ((std::fabs(fTemp44) > 1.1754944e-38f) ? fTemp44 : 0.0f);
			fVec16[IOTA0 & 65535] = 0.35355338f * (fRec33[0] + fTemp43) + 1e-20f;
			float fTemp45 = fVec16[(IOTA0 - iConst29) & 65535] + fTemp13 + 0.6f * fRec29[1];
			fVec17[IOTA0 & 8191] = fTemp45;
			float fTemp46 = fVec17[(IOTA0 - iConst30) & 8191];
			fRec29[0] = ((std::fabs(fTemp46) > 1.1754944e-38f) ? fTemp46 : 0.0f);
			float fTemp47 = 0.6f * fTemp45;
			float fRec30 = ((std::fabs(-fTemp47) > 1.1754944e-38f) ? -fTemp47 : 0.0f);
			float fTemp48 = fRec6[1] - fRec36[1];
			float fTemp49 = fRec36[1] + fSlow5 * fTemp48;
			fRec36[0] = ((std::fabs(fTemp49) > 1.1754944e-38f) ? fTemp49 : 0.0f);
			float fTemp50 = fRec36[1] + fSlow4 * fTemp48;
			float fRec37 = ((std::fabs(fTemp50) > 1.1754944e-38f) ? fTemp50 : 0.0f);
			float fTemp51 = fSlow40 * (fRec6[1] + fSlow41 * fRec37);
			fVec18[0] = fTemp51;
			float fTemp52 = fSlow45 * (fRec38[1] + std::min<float>(3.4028235e+38f, fVec18[1] - fTemp51));
			fRec38[0] = ((std::fabs(fTemp52) > 1.1754944e-38f) ? fTemp52 : 0.0f);
			fVec19[IOTA0 & 65535] = 0.35355338f * (fRec38[0] + fTemp51) + 1e-20f;
			float fTemp53 = fVec19[(IOTA0 - iConst34) & 65535] + fTemp3 + 0.6f * fRec34[1];
			fVec20[IOTA0 & 8191] = fTemp53;
			float fTemp54 = fVec20[(IOTA0 - iConst35) & 8191];
			fRec34[0] = ((std::fabs(fTemp54) > 1.1754944e-38f) ? fTemp54 : 0.0f);
			float fTemp55 = 0.6f * fTemp53;
			float fRec35 = ((std::fabs(-fTemp55) > 1.1754944e-38f) ? -fTemp55 : 0.0f);
			float fTemp56 = fRec4[1] - fRec41[1];
			float fTemp57 = fRec41[1] + fSlow5 * fTemp56;
			fRec41[0] = ((std::fabs(fTemp57) > 1.1754944e-38f) ? fTemp57 : 0.0f);
			float fTemp58 = fRec41[1] + fSlow4 * fTemp56;
			float fRec42 = ((std::fabs(fTemp58) > 1.1754944e-38f) ? fTemp58 : 0.0f);
			float fTemp59 = fSlow46 * (fRec4[1] + fSlow47 * fRec42);
			fVec21[0] = fTemp59;
			float fTemp60 = fSlow51 * (fRec43[1] + std::min<float>(3.4028235e+38f, fVec21[1] - fTemp59));
			fRec43[0] = ((std::fabs(fTemp60) > 1.1754944e-38f) ? fTemp60 : 0.0f);
			fVec22[IOTA0 & 65535] = 0.35355338f * (fRec43[0] + fTemp59) + 1e-20f;
			float fTemp61 = 0.6f * fRec39[1] + fVec22[(IOTA0 - iConst39) & 65535];
			fVec23[IOTA0 & 8191] = fTemp61 - fTemp13;
			float fTemp62 = fVec23[(IOTA0 - iConst40) & 8191];
			fRec39[0] = ((std::fabs(fTemp62) > 1.1754944e-38f) ? fTemp62 : 0.0f);
			float fTemp63 = 0.6f * (fTemp13 - fTemp61);
			float fRec40 = ((std::fabs(fTemp63) > 1.1754944e-38f) ? fTemp63 : 0.0f);
			float fTemp64 = fRec8[1] - fRec46[1];
			float fTemp65 = fRec46[1] + fSlow5 * fTemp64;
			fRec46[0] = ((std::fabs(fTemp65) > 1.1754944e-38f) ? fTemp65 : 0.0f);
			float fTemp66 = fRec46[1] + fSlow4 * fTemp64;
			float fRec47 = ((std::fabs(fTemp66) > 1.1754944e-38f) ? fTemp66 : 0.0f);
			float fTemp67 = fSlow52 * (fRec8[1] + fSlow53 * fRec47);
			fVec24[0] = fTemp67;
			float fTemp68 = fSlow57 * (fRec48[1] + std::min<float>(3.4028235e+38f, fVec24[1] - fTemp67));
			fRec48[0] = ((std::fabs(fTemp68) > 1.1754944e-38f) ? fTemp68 : 0.0f);
			fVec25[IOTA0 & 65535] = 0.35355338f * (fRec48[0] + fTemp67) + 1e-20f;
			float fTemp69 = 0.6f * fRec44[1] + fVec25[(IOTA0 - iConst44) & 65535];
			fVec26[IOTA0 & 4095] = fTemp69 - fTemp3;
			float fTemp70 = fVec26[(IOTA0 - iConst45) & 4095];
			fRec44[0] = ((std::fabs(fTemp70) > 1.1754944e-38f) ? fTemp70 : 0.0f);
			float fTemp71 = 0.6f * (fTemp3 - fTemp69);
			float fRec45 = ((std::fabs(fTemp71) > 1.1754944e-38f) ? fTemp71 : 0.0f);
			float fTemp72 = fRec44[1] + fRec39[1] + fRec34[1] + fRec29[1] + fRec24[1] + fRec19[1] + fRec9[1] + fRec14[1] + fRec45 + fRec40 + fRec35 + fRec30 + fTemp39;
			fRec1[0] = ((std::fabs(fTemp72) > 1.1754944e-38f) ? fTemp72 : 0.0f);
			float fTemp73 = fRec24[1] + fRec19[1] + fRec9[1] + fRec14[1] + fTemp39 - (fRec44[1] + fRec39[1] + fRec34[1] + fRec29[1] + fRec45 + fRec40 + fRec30 + fRec35);
			fRec2[0] = ((std::fabs(fTemp73) > 1.1754944e-38f) ? fTemp73 : 0.0f);
			float fTemp74 = fRec20 + fRec25;
			float fTemp75 = fRec34[1] + fRec29[1] + fRec9[1] + fRec14[1] + fRec35 + fRec30 + fTemp22 - (fRec44[1] + fRec39[1] + fRec24[1] + fRec19[1] + fRec45 + fRec40 + fTemp74);
			fRec3[0] = ((std::fabs(fTemp75) > 1.1754944e-38f) ? fTemp75 : 0.0f);
			float fTemp76 = fRec44[1] + fRec39[1] + fRec9[1] + fRec14[1] + fRec45 + fRec40 + fTemp22 - (fRec34[1] + fRec29[1] + fRec24[1] + fRec19[1] + fRec35 + fRec30 + fTemp74);
			fRec4[0] = ((std::fabs(fTemp76) > 1.1754944e-38f) ? fTemp76 : 0.0f);
			float fTemp77 = fRec10 + fRec25;
			float fTemp78 = fRec15 + fRec20;
			float fTemp79 = fRec39[1] + fRec29[1] + fRec19[1] + fRec14[1] + fRec40 + fRec30 + fTemp78 - (fRec44[1] + fRec34[1] + fRec24[1] + fRec9[1] + fRec45 + fRec35 + fTemp77);
			fRec5[0] = ((std::fabs(fTemp79) > 1.1754944e-38f) ? fTemp79 : 0.0f);
			float fTemp80 = fRec44[1] + fRec34[1] + fRec19[1] + fRec14[1] + fRec45 + fRec35 + fTemp78 - (fRec39[1] + fRec29[1] + fRec24[1] + fRec9[1] + fRec40 + fRec30 + fTemp77);
			fRec6[0] = ((std::fabs(fTemp80) > 1.1754944e-38f) ? fTemp80 : 0.0f);
			float fTemp81 = fRec10 + fRec20;
			float fTemp82 = fRec15 + fRec25;
			float fTemp83 = fRec44[1] + fRec29[1] + fRec24[1] + fRec14[1] + fRec45 + fRec30 + fTemp82 - (fRec39[1] + fRec34[1] + fRec19[1] + fRec9[1] + fRec40 + fRec35 + fTemp81);
			fRec7[0] = ((std::fabs(fTemp83) > 1.1754944e-38f) ? fTemp83 : 0.0f);
			float fTemp84 = fRec39[1] + fRec34[1] + fRec24[1] + fRec14[1] + fRec40 + fRec35 + fTemp82 - (fRec44[1] + fRec29[1] + fRec19[1] + fRec9[1] + fRec45 + fRec30 + fTemp81);
			fRec8[0] = ((std::fabs(fTemp84) > 1.1754944e-38f) ? fTemp84 : 0.0f);
			float fTemp85 = 1.0f - fRec0[0] - fSlow1;
			output0[i0] = FAUSTFLOAT(fTemp12 * fTemp85 + fSlow58 * (fRec2[0] + fRec3[0]) * fTemp1);
			output1[i0] = FAUSTFLOAT(fTemp2 * fTemp85 + fSlow58 * (fRec2[0] - fRec3[0]) * fTemp1);
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			IOTA0 = IOTA0 + 1;
			fRec11[1] = fRec11[0];
			fVec2[1] = fVec2[0];
			fRec13[1] = fRec13[0];
			fRec9[1] = fRec9[0];
			fRec16[1] = fRec16[0];
			fVec6[1] = fVec6[0];
			fRec18[1] = fRec18[0];
			fRec14[1] = fRec14[0];
			fRec21[1] = fRec21[0];
			fVec9[1] = fVec9[0];
			fRec23[1] = fRec23[0];
			fRec19[1] = fRec19[0];
			fRec26[1] = fRec26[0];
			fVec12[1] = fVec12[0];
			fRec28[1] = fRec28[0];
			fRec24[1] = fRec24[0];
			fRec31[1] = fRec31[0];
			fVec15[1] = fVec15[0];
			fRec33[1] = fRec33[0];
			fRec29[1] = fRec29[0];
			fRec36[1] = fRec36[0];
			fVec18[1] = fVec18[0];
			fRec38[1] = fRec38[0];
			fRec34[1] = fRec34[0];
			fRec41[1] = fRec41[0];
			fVec21[1] = fVec21[0];
			fRec43[1] = fRec43[0];
			fRec39[1] = fRec39[0];
			fRec46[1] = fRec46[0];
			fVec24[1] = fVec24[0];
			fRec48[1] = fRec48[0];
			fRec44[1] = fRec44[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fRec5[1] = fRec5[0];
			fRec6[1] = fRec6[0];
			fRec7[1] = fRec7[0];
			fRec8[1] = fRec8[0];
		}
	}

};

#endif
