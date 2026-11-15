// D:\workspace\Fanan_web_hub\src\timberwolf\faust\room.h
// Written by the Faust compiler (2.70.3) from room.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "room"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Room -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Room_H__
#define  __Faust_Room_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Room
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


class Faust_Room : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst1;
	float fRec9[2];
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec11[2];
	float fRec10[2];
	int IOTA0;
	float fVec2[8192];
	int iConst5;
	float fRec8[2];
	float fRec13[2];
	float fVec3[8192];
	int iConst8;
	float fRec12[2];
	float fRec15[2];
	float fVec4[8192];
	int iConst11;
	float fRec14[2];
	float fRec17[2];
	float fVec5[8192];
	int iConst14;
	float fRec16[2];
	float fRec19[2];
	float fVec6[8192];
	int iConst17;
	float fRec18[2];
	float fRec21[2];
	float fVec7[8192];
	int iConst20;
	float fRec20[2];
	float fRec23[2];
	float fVec8[8192];
	int iConst23;
	float fRec22[2];
	float fRec25[2];
	float fVec9[8192];
	int iConst26;
	float fRec24[2];
	float fVec10[4096];
	int iConst29;
	float fRec6[2];
	float fVec11[2048];
	int iConst32;
	float fRec4[2];
	float fVec12[2048];
	int iConst35;
	float fRec2[2];
	float fVec13[1024];
	int iConst38;
	float fRec0[2];
	FAUSTFLOAT fHslider2;
	float fVec14[2];
	float fRec26[2];
	float fRec36[2];
	float fVec15[8192];
	int iConst39;
	float fRec35[2];
	float fRec38[2];
	float fVec16[8192];
	int iConst40;
	float fRec37[2];
	float fRec40[2];
	float fVec17[8192];
	int iConst41;
	float fRec39[2];
	float fRec42[2];
	float fVec18[8192];
	int iConst42;
	float fRec41[2];
	float fRec44[2];
	float fVec19[8192];
	int iConst43;
	float fRec43[2];
	float fRec46[2];
	float fVec20[8192];
	int iConst44;
	float fRec45[2];
	float fRec48[2];
	float fVec21[8192];
	int iConst45;
	float fRec47[2];
	float fRec50[2];
	float fVec22[8192];
	int iConst46;
	float fRec49[2];
	float fVec23[4096];
	int iConst47;
	float fRec33[2];
	float fVec24[2048];
	int iConst48;
	float fRec31[2];
	float fVec25[2048];
	int iConst49;
	float fRec29[2];
	float fVec26[1024];
	int iConst50;
	float fRec27[2];
	
 public:
	Faust_Room() {}

	void metadata(Meta* m) { 
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Room -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "room.dsp");
		m->declare("filters.lib/allpass_comb:author", "Julius O. Smith III");
		m->declare("filters.lib/allpass_comb:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/allpass_comb:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "room");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("reverbs.lib/mono_freeverb:author", "Romain Michon");
		m->declare("reverbs.lib/name", "Faust Reverb Library");
		m->declare("reverbs.lib/stereo_freeverb:author", "Romain Michon");
		m->declare("reverbs.lib/version", "1.6.2");
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
		int iConst2 = int(fConst0);
		int iConst3 = 1116 * iConst2;
		int iConst4 = int(2.2675737e-05f * float(iConst3 - iConst3 % 44100) + 0.5f);
		iConst5 = std::max<int>(0, iConst4 + -1);
		int iConst6 = 1188 * iConst2;
		int iConst7 = int(2.2675737e-05f * float(iConst6 - iConst6 % 44100) + 0.5f);
		iConst8 = std::max<int>(0, iConst7 + -1);
		int iConst9 = 1277 * iConst2;
		int iConst10 = int(2.2675737e-05f * float(iConst9 - iConst9 % 44100) + 0.5f);
		iConst11 = std::max<int>(0, iConst10 + -1);
		int iConst12 = 1356 * iConst2;
		int iConst13 = int(2.2675737e-05f * float(iConst12 - iConst12 % 44100) + 0.5f);
		iConst14 = std::max<int>(0, iConst13 + -1);
		int iConst15 = 1422 * iConst2;
		int iConst16 = int(2.2675737e-05f * float(iConst15 - iConst15 % 44100) + 0.5f);
		iConst17 = std::max<int>(0, iConst16 + -1);
		int iConst18 = 1491 * iConst2;
		int iConst19 = int(2.2675737e-05f * float(iConst18 - iConst18 % 44100) + 0.5f);
		iConst20 = std::max<int>(0, iConst19 + -1);
		int iConst21 = 1557 * iConst2;
		int iConst22 = int(2.2675737e-05f * float(iConst21 - iConst21 % 44100) + 0.5f);
		iConst23 = std::max<int>(0, iConst22 + -1);
		int iConst24 = 1617 * iConst2;
		int iConst25 = int(2.2675737e-05f * float(iConst24 - iConst24 % 44100) + 0.5f);
		iConst26 = std::max<int>(0, iConst25 + -1);
		int iConst27 = 556 * iConst2;
		int iConst28 = int(2.2675737e-05f * float(iConst27 - iConst27 % 44100) + 0.5f);
		iConst29 = std::min<int>(4096, std::max<int>(0, iConst28 + -1));
		int iConst30 = 441 * iConst2;
		int iConst31 = int(2.2675737e-05f * float(iConst30 - iConst30 % 44100) + 0.5f);
		iConst32 = std::min<int>(4096, std::max<int>(0, iConst31 + -1));
		int iConst33 = 341 * iConst2;
		int iConst34 = int(2.2675737e-05f * float(iConst33 - iConst33 % 44100) + 0.5f);
		iConst35 = std::min<int>(4096, std::max<int>(0, iConst34 + -1));
		int iConst36 = 225 * iConst2;
		int iConst37 = int(2.2675737e-05f * float(iConst36 - iConst36 % 44100) + 0.5f);
		iConst38 = std::min<int>(4096, std::max<int>(0, iConst37 + -1));
		iConst39 = std::max<int>(0, iConst4 + 22);
		iConst40 = std::max<int>(0, iConst7 + 22);
		iConst41 = std::max<int>(0, iConst10 + 22);
		iConst42 = std::max<int>(0, iConst13 + 22);
		iConst43 = std::max<int>(0, iConst16 + 22);
		iConst44 = std::max<int>(0, iConst19 + 22);
		iConst45 = std::max<int>(0, iConst22 + 22);
		iConst46 = std::max<int>(0, iConst25 + 22);
		iConst47 = std::min<int>(4096, std::max<int>(0, iConst28 + 22));
		iConst48 = std::min<int>(4096, std::max<int>(0, iConst31 + 22));
		iConst49 = std::min<int>(4096, std::max<int>(0, iConst34 + 22));
		iConst50 = std::min<int>(4096, std::max<int>(0, iConst37 + 22));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(6e+01f);
		fHslider1 = FAUSTFLOAT(4e+01f);
		fHslider2 = FAUSTFLOAT(25.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec9[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fVec1[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec11[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec10[l4] = 0.0f;
		}
		IOTA0 = 0;
		for (int l5 = 0; l5 < 8192; l5 = l5 + 1) {
			fVec2[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec8[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec13[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 8192; l8 = l8 + 1) {
			fVec3[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec12[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec15[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 8192; l11 = l11 + 1) {
			fVec4[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec14[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec17[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 8192; l14 = l14 + 1) {
			fVec5[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec16[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec19[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 8192; l17 = l17 + 1) {
			fVec6[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec18[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fRec21[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 8192; l20 = l20 + 1) {
			fVec7[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec20[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fRec23[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 8192; l23 = l23 + 1) {
			fVec8[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fRec22[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec25[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 8192; l26 = l26 + 1) {
			fVec9[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec24[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 4096; l28 = l28 + 1) {
			fVec10[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec6[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2048; l30 = l30 + 1) {
			fVec11[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 2; l31 = l31 + 1) {
			fRec4[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2048; l32 = l32 + 1) {
			fVec12[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec2[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 1024; l34 = l34 + 1) {
			fVec13[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 2; l35 = l35 + 1) {
			fRec0[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			fVec14[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fRec26[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec36[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 8192; l39 = l39 + 1) {
			fVec15[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 2; l40 = l40 + 1) {
			fRec35[l40] = 0.0f;
		}
		for (int l41 = 0; l41 < 2; l41 = l41 + 1) {
			fRec38[l41] = 0.0f;
		}
		for (int l42 = 0; l42 < 8192; l42 = l42 + 1) {
			fVec16[l42] = 0.0f;
		}
		for (int l43 = 0; l43 < 2; l43 = l43 + 1) {
			fRec37[l43] = 0.0f;
		}
		for (int l44 = 0; l44 < 2; l44 = l44 + 1) {
			fRec40[l44] = 0.0f;
		}
		for (int l45 = 0; l45 < 8192; l45 = l45 + 1) {
			fVec17[l45] = 0.0f;
		}
		for (int l46 = 0; l46 < 2; l46 = l46 + 1) {
			fRec39[l46] = 0.0f;
		}
		for (int l47 = 0; l47 < 2; l47 = l47 + 1) {
			fRec42[l47] = 0.0f;
		}
		for (int l48 = 0; l48 < 8192; l48 = l48 + 1) {
			fVec18[l48] = 0.0f;
		}
		for (int l49 = 0; l49 < 2; l49 = l49 + 1) {
			fRec41[l49] = 0.0f;
		}
		for (int l50 = 0; l50 < 2; l50 = l50 + 1) {
			fRec44[l50] = 0.0f;
		}
		for (int l51 = 0; l51 < 8192; l51 = l51 + 1) {
			fVec19[l51] = 0.0f;
		}
		for (int l52 = 0; l52 < 2; l52 = l52 + 1) {
			fRec43[l52] = 0.0f;
		}
		for (int l53 = 0; l53 < 2; l53 = l53 + 1) {
			fRec46[l53] = 0.0f;
		}
		for (int l54 = 0; l54 < 8192; l54 = l54 + 1) {
			fVec20[l54] = 0.0f;
		}
		for (int l55 = 0; l55 < 2; l55 = l55 + 1) {
			fRec45[l55] = 0.0f;
		}
		for (int l56 = 0; l56 < 2; l56 = l56 + 1) {
			fRec48[l56] = 0.0f;
		}
		for (int l57 = 0; l57 < 8192; l57 = l57 + 1) {
			fVec21[l57] = 0.0f;
		}
		for (int l58 = 0; l58 < 2; l58 = l58 + 1) {
			fRec47[l58] = 0.0f;
		}
		for (int l59 = 0; l59 < 2; l59 = l59 + 1) {
			fRec50[l59] = 0.0f;
		}
		for (int l60 = 0; l60 < 8192; l60 = l60 + 1) {
			fVec22[l60] = 0.0f;
		}
		for (int l61 = 0; l61 < 2; l61 = l61 + 1) {
			fRec49[l61] = 0.0f;
		}
		for (int l62 = 0; l62 < 4096; l62 = l62 + 1) {
			fVec23[l62] = 0.0f;
		}
		for (int l63 = 0; l63 < 2; l63 = l63 + 1) {
			fRec33[l63] = 0.0f;
		}
		for (int l64 = 0; l64 < 2048; l64 = l64 + 1) {
			fVec24[l64] = 0.0f;
		}
		for (int l65 = 0; l65 < 2; l65 = l65 + 1) {
			fRec31[l65] = 0.0f;
		}
		for (int l66 = 0; l66 < 2048; l66 = l66 + 1) {
			fVec25[l66] = 0.0f;
		}
		for (int l67 = 0; l67 < 2; l67 = l67 + 1) {
			fRec29[l67] = 0.0f;
		}
		for (int l68 = 0; l68 < 1024; l68 = l68 + 1) {
			fVec26[l68] = 0.0f;
		}
		for (int l69 = 0; l69 < 2; l69 = l69 + 1) {
			fRec27[l69] = 0.0f;
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
	
	virtual Faust_Room* clone() {
		return new Faust_Room();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("room");
		ui_interface->addHorizontalSlider("Damp", &fHslider1, FAUSTFLOAT(4e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mix", &fHslider2, FAUSTFLOAT(25.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Size", &fHslider0, FAUSTFLOAT(6e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = 0.0028f * float(fHslider0);
		float fSlow1 = fSlow0 + 0.7f;
		float fSlow2 = float(fHslider1);
		float fSlow3 = 0.01f * fSlow2;
		float fSlow4 = float(fHslider2);
		float fSlow5 = 0.01f * fSlow4;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow1;
			float fTemp0 = fConst1 * (std::min<float>(3.4028235e+38f, fVec0[1] + -0.7f - fSlow0) + fRec9[1]);
			fRec9[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow0 + fRec9[0] + 0.7f;
			fVec1[0] = fSlow2;
			float fTemp2 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec1[1]))) + fRec11[1]);
			fRec11[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = 1.0f - fRec11[0] - fSlow3;
			float fTemp4 = fSlow3 + fRec11[0];
			float fTemp5 = fRec10[1] * fTemp4 + fRec8[1] * fTemp3;
			fRec10[0] = ((std::fabs(fTemp5) > 1.1754944e-38f) ? fTemp5 : 0.0f);
			float fTemp6 = float(input1[i0]);
			float fTemp7 = float(input0[i0]);
			float fTemp8 = fTemp7 + fTemp6;
			fVec2[IOTA0 & 8191] = fTemp8 + fRec10[0] * fTemp1;
			float fTemp9 = fVec2[(IOTA0 - iConst5) & 8191];
			fRec8[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			float fTemp10 = fTemp4 * fRec13[1] + fTemp3 * fRec12[1];
			fRec13[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			fVec3[IOTA0 & 8191] = fTemp8 + fRec13[0] * fTemp1;
			float fTemp11 = fVec3[(IOTA0 - iConst8) & 8191];
			fRec12[0] = ((std::fabs(fTemp11) > 1.1754944e-38f) ? fTemp11 : 0.0f);
			float fTemp12 = fTemp4 * fRec15[1] + fTemp3 * fRec14[1];
			fRec15[0] = ((std::fabs(fTemp12) > 1.1754944e-38f) ? fTemp12 : 0.0f);
			fVec4[IOTA0 & 8191] = fTemp8 + fRec15[0] * fTemp1;
			float fTemp13 = fVec4[(IOTA0 - iConst11) & 8191];
			fRec14[0] = ((std::fabs(fTemp13) > 1.1754944e-38f) ? fTemp13 : 0.0f);
			float fTemp14 = fTemp4 * fRec17[1] + fTemp3 * fRec16[1];
			fRec17[0] = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			fVec5[IOTA0 & 8191] = fTemp8 + fRec17[0] * fTemp1;
			float fTemp15 = fVec5[(IOTA0 - iConst14) & 8191];
			fRec16[0] = ((std::fabs(fTemp15) > 1.1754944e-38f) ? fTemp15 : 0.0f);
			float fTemp16 = fTemp4 * fRec19[1] + fTemp3 * fRec18[1];
			fRec19[0] = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			fVec6[IOTA0 & 8191] = fTemp8 + fRec19[0] * fTemp1;
			float fTemp17 = fVec6[(IOTA0 - iConst17) & 8191];
			fRec18[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fTemp4 * fRec21[1] + fTemp3 * fRec20[1];
			fRec21[0] = ((std::fabs(fTemp18) > 1.1754944e-38f) ? fTemp18 : 0.0f);
			fVec7[IOTA0 & 8191] = fTemp8 + fRec21[0] * fTemp1;
			float fTemp19 = fVec7[(IOTA0 - iConst20) & 8191];
			fRec20[0] = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = fTemp4 * fRec23[1] + fTemp3 * fRec22[1];
			fRec23[0] = ((std::fabs(fTemp20) > 1.1754944e-38f) ? fTemp20 : 0.0f);
			fVec8[IOTA0 & 8191] = fTemp8 + fRec23[0] * fTemp1;
			float fTemp21 = fVec8[(IOTA0 - iConst23) & 8191];
			fRec22[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			float fTemp22 = fTemp4 * fRec25[1] + fTemp3 * fRec24[1];
			fRec25[0] = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			fVec9[IOTA0 & 8191] = fTemp8 + fRec25[0] * fTemp1;
			float fTemp23 = fVec9[(IOTA0 - iConst26) & 8191];
			fRec24[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fTemp24 = fRec24[1] + fRec22[1] + fRec20[1] + fRec18[1] + fRec16[1] + fRec14[1] + fRec12[1] + 0.5f * fRec6[1] + fRec8[1];
			fVec10[IOTA0 & 4095] = fTemp24;
			float fTemp25 = fVec10[(IOTA0 - iConst29) & 4095];
			fRec6[0] = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			float fTemp26 = 0.5f * fTemp24;
			float fRec7 = ((std::fabs(-fTemp26) > 1.1754944e-38f) ? -fTemp26 : 0.0f);
			float fTemp27 = fRec6[1] + fRec7 + 0.5f * fRec4[1];
			fVec11[IOTA0 & 2047] = fTemp27;
			float fTemp28 = fVec11[(IOTA0 - iConst32) & 2047];
			fRec4[0] = ((std::fabs(fTemp28) > 1.1754944e-38f) ? fTemp28 : 0.0f);
			float fTemp29 = 0.5f * fTemp27;
			float fRec5 = ((std::fabs(-fTemp29) > 1.1754944e-38f) ? -fTemp29 : 0.0f);
			float fTemp30 = fRec4[1] + fRec5 + 0.5f * fRec2[1];
			fVec12[IOTA0 & 2047] = fTemp30;
			float fTemp31 = fVec12[(IOTA0 - iConst35) & 2047];
			fRec2[0] = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp32 = 0.5f * fTemp30;
			float fRec3 = ((std::fabs(-fTemp32) > 1.1754944e-38f) ? -fTemp32 : 0.0f);
			float fTemp33 = fRec2[1] + fRec3 + 0.5f * fRec0[1];
			fVec13[IOTA0 & 1023] = fTemp33;
			float fTemp34 = fVec13[(IOTA0 - iConst38) & 1023];
			fRec0[0] = ((std::fabs(fTemp34) > 1.1754944e-38f) ? fTemp34 : 0.0f);
			float fTemp35 = 0.5f * fTemp33;
			float fRec1 = ((std::fabs(-fTemp35) > 1.1754944e-38f) ? -fTemp35 : 0.0f);
			fVec14[0] = fSlow4;
			float fTemp36 = fConst1 * (fRec26[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow4 - fVec14[1]))));
			fRec26[0] = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fTemp37 = fSlow5 + fRec26[0];
			float fTemp38 = 1.0f - fRec26[0] - fSlow5;
			output0[i0] = FAUSTFLOAT(fTemp7 * fTemp38 + 0.12f * fTemp37 * (fRec1 + fRec0[1]));
			float fTemp39 = fTemp4 * fRec36[1] + fTemp3 * fRec35[1];
			fRec36[0] = ((std::fabs(fTemp39) > 1.1754944e-38f) ? fTemp39 : 0.0f);
			fVec15[IOTA0 & 8191] = fTemp8 + fRec36[0] * fTemp1;
			float fTemp40 = fVec15[(IOTA0 - iConst39) & 8191];
			fRec35[0] = ((std::fabs(fTemp40) > 1.1754944e-38f) ? fTemp40 : 0.0f);
			float fTemp41 = fTemp4 * fRec38[1] + fTemp3 * fRec37[1];
			fRec38[0] = ((std::fabs(fTemp41) > 1.1754944e-38f) ? fTemp41 : 0.0f);
			fVec16[IOTA0 & 8191] = fTemp8 + fRec38[0] * fTemp1;
			float fTemp42 = fVec16[(IOTA0 - iConst40) & 8191];
			fRec37[0] = ((std::fabs(fTemp42) > 1.1754944e-38f) ? fTemp42 : 0.0f);
			float fTemp43 = fTemp4 * fRec40[1] + fTemp3 * fRec39[1];
			fRec40[0] = ((std::fabs(fTemp43) > 1.1754944e-38f) ? fTemp43 : 0.0f);
			fVec17[IOTA0 & 8191] = fTemp8 + fRec40[0] * fTemp1;
			float fTemp44 = fVec17[(IOTA0 - iConst41) & 8191];
			fRec39[0] = ((std::fabs(fTemp44) > 1.1754944e-38f) ? fTemp44 : 0.0f);
			float fTemp45 = fTemp4 * fRec42[1] + fTemp3 * fRec41[1];
			fRec42[0] = ((std::fabs(fTemp45) > 1.1754944e-38f) ? fTemp45 : 0.0f);
			fVec18[IOTA0 & 8191] = fTemp8 + fRec42[0] * fTemp1;
			float fTemp46 = fVec18[(IOTA0 - iConst42) & 8191];
			fRec41[0] = ((std::fabs(fTemp46) > 1.1754944e-38f) ? fTemp46 : 0.0f);
			float fTemp47 = fTemp4 * fRec44[1] + fTemp3 * fRec43[1];
			fRec44[0] = ((std::fabs(fTemp47) > 1.1754944e-38f) ? fTemp47 : 0.0f);
			fVec19[IOTA0 & 8191] = fTemp8 + fRec44[0] * fTemp1;
			float fTemp48 = fVec19[(IOTA0 - iConst43) & 8191];
			fRec43[0] = ((std::fabs(fTemp48) > 1.1754944e-38f) ? fTemp48 : 0.0f);
			float fTemp49 = fTemp4 * fRec46[1] + fTemp3 * fRec45[1];
			fRec46[0] = ((std::fabs(fTemp49) > 1.1754944e-38f) ? fTemp49 : 0.0f);
			fVec20[IOTA0 & 8191] = fTemp8 + fRec46[0] * fTemp1;
			float fTemp50 = fVec20[(IOTA0 - iConst44) & 8191];
			fRec45[0] = ((std::fabs(fTemp50) > 1.1754944e-38f) ? fTemp50 : 0.0f);
			float fTemp51 = fTemp4 * fRec48[1] + fTemp3 * fRec47[1];
			fRec48[0] = ((std::fabs(fTemp51) > 1.1754944e-38f) ? fTemp51 : 0.0f);
			fVec21[IOTA0 & 8191] = fTemp8 + fRec48[0] * fTemp1;
			float fTemp52 = fVec21[(IOTA0 - iConst45) & 8191];
			fRec47[0] = ((std::fabs(fTemp52) > 1.1754944e-38f) ? fTemp52 : 0.0f);
			float fTemp53 = fTemp4 * fRec50[1] + fTemp3 * fRec49[1];
			fRec50[0] = ((std::fabs(fTemp53) > 1.1754944e-38f) ? fTemp53 : 0.0f);
			fVec22[IOTA0 & 8191] = fTemp8 + fRec50[0] * fTemp1;
			float fTemp54 = fVec22[(IOTA0 - iConst46) & 8191];
			fRec49[0] = ((std::fabs(fTemp54) > 1.1754944e-38f) ? fTemp54 : 0.0f);
			float fTemp55 = fRec49[1] + fRec47[1] + fRec45[1] + fRec43[1] + fRec41[1] + fRec39[1] + fRec37[1] + 0.5f * fRec33[1] + fRec35[1];
			fVec23[IOTA0 & 4095] = fTemp55;
			float fTemp56 = fVec23[(IOTA0 - iConst47) & 4095];
			fRec33[0] = ((std::fabs(fTemp56) > 1.1754944e-38f) ? fTemp56 : 0.0f);
			float fTemp57 = 0.5f * fTemp55;
			float fRec34 = ((std::fabs(-fTemp57) > 1.1754944e-38f) ? -fTemp57 : 0.0f);
			float fTemp58 = fRec33[1] + fRec34 + 0.5f * fRec31[1];
			fVec24[IOTA0 & 2047] = fTemp58;
			float fTemp59 = fVec24[(IOTA0 - iConst48) & 2047];
			fRec31[0] = ((std::fabs(fTemp59) > 1.1754944e-38f) ? fTemp59 : 0.0f);
			float fTemp60 = 0.5f * fTemp58;
			float fRec32 = ((std::fabs(-fTemp60) > 1.1754944e-38f) ? -fTemp60 : 0.0f);
			float fTemp61 = fRec31[1] + fRec32 + 0.5f * fRec29[1];
			fVec25[IOTA0 & 2047] = fTemp61;
			float fTemp62 = fVec25[(IOTA0 - iConst49) & 2047];
			fRec29[0] = ((std::fabs(fTemp62) > 1.1754944e-38f) ? fTemp62 : 0.0f);
			float fTemp63 = 0.5f * fTemp61;
			float fRec30 = ((std::fabs(-fTemp63) > 1.1754944e-38f) ? -fTemp63 : 0.0f);
			float fTemp64 = fRec29[1] + fRec30 + 0.5f * fRec27[1];
			fVec26[IOTA0 & 1023] = fTemp64;
			float fTemp65 = fVec26[(IOTA0 - iConst50) & 1023];
			fRec27[0] = ((std::fabs(fTemp65) > 1.1754944e-38f) ? fTemp65 : 0.0f);
			float fTemp66 = 0.5f * fTemp64;
			float fRec28 = ((std::fabs(-fTemp66) > 1.1754944e-38f) ? -fTemp66 : 0.0f);
			output1[i0] = FAUSTFLOAT(fTemp6 * fTemp38 + 0.12f * fTemp37 * (fRec28 + fRec27[1]));
			fVec0[1] = fVec0[0];
			fRec9[1] = fRec9[0];
			fVec1[1] = fVec1[0];
			fRec11[1] = fRec11[0];
			fRec10[1] = fRec10[0];
			IOTA0 = IOTA0 + 1;
			fRec8[1] = fRec8[0];
			fRec13[1] = fRec13[0];
			fRec12[1] = fRec12[0];
			fRec15[1] = fRec15[0];
			fRec14[1] = fRec14[0];
			fRec17[1] = fRec17[0];
			fRec16[1] = fRec16[0];
			fRec19[1] = fRec19[0];
			fRec18[1] = fRec18[0];
			fRec21[1] = fRec21[0];
			fRec20[1] = fRec20[0];
			fRec23[1] = fRec23[0];
			fRec22[1] = fRec22[0];
			fRec25[1] = fRec25[0];
			fRec24[1] = fRec24[0];
			fRec6[1] = fRec6[0];
			fRec4[1] = fRec4[0];
			fRec2[1] = fRec2[0];
			fRec0[1] = fRec0[0];
			fVec14[1] = fVec14[0];
			fRec26[1] = fRec26[0];
			fRec36[1] = fRec36[0];
			fRec35[1] = fRec35[0];
			fRec38[1] = fRec38[0];
			fRec37[1] = fRec37[0];
			fRec40[1] = fRec40[0];
			fRec39[1] = fRec39[0];
			fRec42[1] = fRec42[0];
			fRec41[1] = fRec41[0];
			fRec44[1] = fRec44[0];
			fRec43[1] = fRec43[0];
			fRec46[1] = fRec46[0];
			fRec45[1] = fRec45[0];
			fRec48[1] = fRec48[0];
			fRec47[1] = fRec47[0];
			fRec50[1] = fRec50[0];
			fRec49[1] = fRec49[0];
			fRec33[1] = fRec33[0];
			fRec31[1] = fRec31[0];
			fRec29[1] = fRec29[0];
			fRec27[1] = fRec27[0];
		}
	}

};

#endif
