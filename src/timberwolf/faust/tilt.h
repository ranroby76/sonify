// D:\workspace\Fanan_web_hub\src\timberwolf\faust\tilt.h
// Written by the Faust compiler (2.70.3) from tilt.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "tilt"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Tilt -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Tilt_H__
#define  __Faust_Tilt_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Tilt
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


class Faust_Tilt : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	float fConst2;
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec1[2];
	float fConst3;
	float fConst4;
	float fConst8;
	float fConst9;
	float fConst13;
	float fConst14;
	float fConst18;
	float fConst19;
	float fConst23;
	float fConst25;
	float fConst27;
	float fRec10[2];
	float fConst28;
	float fConst29;
	float fConst30;
	float fConst31;
	float fConst32;
	float fRec8[2];
	float fConst33;
	float fConst34;
	float fConst35;
	float fConst36;
	float fRec6[2];
	float fConst37;
	float fConst38;
	float fConst39;
	float fConst40;
	float fRec4[2];
	float fConst41;
	float fConst42;
	float fConst43;
	float fConst44;
	float fRec2[2];
	float fConst45;
	float fRec20[2];
	float fRec18[2];
	float fRec16[2];
	float fRec14[2];
	float fRec12[2];
	
 public:
	Faust_Tilt() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Tilt -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "tilt.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/spectral_tilt:author", "Julius O. Smith III");
		m->declare("filters.lib/spectral_tilt:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/spectral_tilt:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf1s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "tilt");
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
		fConst2 = 157.07964f / fConst0;
		fConst3 = std::tan(fConst2);
		fConst4 = std::tan(37856.19f / fConst0);
		float fConst5 = 314.15927f * (fConst4 / fConst3);
		float fConst6 = std::fabs(fConst5) + float(fConst5 == 0.0f);
		float fConst7 = fConst3 * fConst6;
		fConst8 = 314.15927f * (fConst4 / fConst7);
		fConst9 = std::tan(9607.992f / fConst0);
		float fConst10 = 314.15927f * (fConst9 / fConst3);
		float fConst11 = std::fabs(fConst10) + float(fConst10 == 0.0f);
		float fConst12 = fConst3 * fConst11;
		fConst13 = 314.15927f * (fConst9 / fConst12);
		fConst14 = std::tan(2438.5317f / fConst0);
		float fConst15 = 314.15927f * (fConst14 / fConst3);
		float fConst16 = std::fabs(fConst15) + float(fConst15 == 0.0f);
		float fConst17 = fConst3 * fConst16;
		fConst18 = 314.15927f * (fConst14 / fConst17);
		fConst19 = std::tan(618.9052f / fConst0);
		float fConst20 = 314.15927f * (fConst19 / fConst3);
		float fConst21 = std::fabs(fConst20) + float(fConst20 == 0.0f);
		float fConst22 = fConst3 * fConst21;
		fConst23 = 314.15927f * (fConst19 / fConst22);
		float fConst24 = std::tan(std::max<float>(0.0f, std::min<float>(0.5f / fConst0, 1.5676547f)));
		fConst25 = std::min<float>(3.4028235e+38f, 1.0f / (314.15927f * fConst24 + 1.0f));
		float fConst26 = fConst24 * fConst25;
		fConst27 = 628.31854f * fConst26;
		fConst28 = 314.15927f * fConst26;
		fConst29 = 1.0f / fConst3;
		fConst30 = std::min<float>(3.4028235e+38f, 1.0f / (314.15927f * (fConst24 * fConst19 / fConst3) + 1.0f));
		fConst31 = fConst24 * fConst21 * fConst30;
		fConst32 = 2.0f * fConst31;
		fConst33 = 314.15927f / fConst22;
		fConst34 = std::min<float>(3.4028235e+38f, 1.0f / (314.15927f * (fConst24 * fConst14 / fConst3) + 1.0f));
		fConst35 = fConst24 * fConst16 * fConst34;
		fConst36 = 2.0f * fConst35;
		fConst37 = 314.15927f / fConst17;
		fConst38 = std::min<float>(3.4028235e+38f, 1.0f / (314.15927f * (fConst24 * fConst9 / fConst3) + 1.0f));
		fConst39 = fConst24 * fConst11 * fConst38;
		fConst40 = 2.0f * fConst39;
		fConst41 = 314.15927f / fConst12;
		fConst42 = std::min<float>(3.4028235e+38f, 1.0f / (314.15927f * (fConst24 * fConst4 / fConst3) + 1.0f));
		fConst43 = fConst24 * fConst6 * fConst42;
		fConst44 = 2.0f * fConst43;
		fConst45 = 314.15927f / fConst7;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(0.0f);
		fHslider1 = FAUSTFLOAT(0.0f);
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
			fRec10[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec8[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec6[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec4[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec2[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec20[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec18[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec16[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec14[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec12[l13] = 0.0f;
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
	
	virtual Faust_Tilt* clone() {
		return new Faust_Tilt();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("tilt");
		ui_interface->addHorizontalSlider("Level", &fHslider1, FAUSTFLOAT(0.0f), FAUSTFLOAT(-18.0f), FAUSTFLOAT(12.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Tilt", &fHslider0, FAUSTFLOAT(0.0f), FAUSTFLOAT(-1e+02f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.006f * fSlow0;
		float fSlow2 = std::pow(1e+01f, 0.05f * float(fHslider1));
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.006f * (fSlow0 - fVec0[1]))) + fRec0[1]);
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = std::tan(fConst2 * std::pow(3.940073f, 4.0f - fRec0[0] - fSlow1));
			fVec1[0] = fSlow2;
			float fTemp2 = fConst1 * (fRec1[1] + std::min<float>(3.4028235e+38f, fVec1[1] - fSlow2));
			fRec1[0] = ((std::fabs(fTemp2) > 1.1754944e-38f) ? fTemp2 : 0.0f);
			float fTemp3 = fSlow2 + fRec1[0];
			float fTemp4 = fSlow1 + fRec0[0];
			float fTemp5 = std::tan(fConst2 * std::pow(3.940073f, 3.0f - fTemp4));
			float fTemp6 = std::tan(fConst2 * std::pow(3.940073f, 2.0f - fTemp4));
			float fTemp7 = std::tan(fConst2 * std::pow(3.940073f, 1.0f - fTemp4));
			float fTemp8 = std::tan(fConst2 * std::pow(3.940073f, -fTemp4));
			float fTemp9 = float(input0[i0]) - fRec10[1];
			float fTemp10 = fRec10[1] + fConst27 * fTemp9;
			fRec10[0] = ((std::fabs(fTemp10) > 1.1754944e-38f) ? fTemp10 : 0.0f);
			float fTemp11 = fConst25 * fTemp9 + fConst29 * fTemp8 * (fRec10[1] + fConst28 * fTemp9);
			float fRec11 = ((std::fabs(fTemp11) > 1.1754944e-38f) ? fTemp11 : 0.0f);
			float fTemp12 = fConst3 * (fRec11 / fTemp8) - fConst23 * fRec8[1];
			float fTemp13 = fRec8[1] + fConst32 * fTemp12;
			fRec8[0] = ((std::fabs(fTemp13) > 1.1754944e-38f) ? fTemp13 : 0.0f);
			float fTemp14 = fConst30 * fTemp12 + fConst33 * fTemp7 * (fRec8[1] + fConst31 * fTemp12);
			float fRec9 = ((std::fabs(fTemp14) > 1.1754944e-38f) ? fTemp14 : 0.0f);
			float fTemp15 = fConst19 * (fRec9 / fTemp7) - fConst18 * fRec6[1];
			float fTemp16 = fRec6[1] + fConst36 * fTemp15;
			fRec6[0] = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			float fTemp17 = fConst34 * fTemp15 + fConst37 * fTemp6 * (fRec6[1] + fConst35 * fTemp15);
			float fRec7 = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fConst14 * (fRec7 / fTemp6) - fConst13 * fRec4[1];
			float fTemp19 = fRec4[1] + fConst40 * fTemp18;
			fRec4[0] = ((std::fabs(fTemp19) > 1.1754944e-38f) ? fTemp19 : 0.0f);
			float fTemp20 = fConst38 * fTemp18 + fConst41 * fTemp5 * (fRec4[1] + fConst39 * fTemp18);
			float fRec5 = ((std::fabs(fTemp20) > 1.1754944e-38f) ? fTemp20 : 0.0f);
			float fTemp21 = fConst9 * (fRec5 / fTemp5) - fConst8 * fRec2[1];
			float fTemp22 = fRec2[1] + fConst44 * fTemp21;
			fRec2[0] = ((std::fabs(fTemp22) > 1.1754944e-38f) ? fTemp22 : 0.0f);
			float fTemp23 = std::tan(fConst2 * std::pow(3.940073f, 4.0f - fTemp4));
			float fTemp24 = fConst42 * fTemp21 + fConst45 * fTemp23 * (fRec2[1] + fConst43 * fTemp21);
			float fRec3 = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			output0[i0] = FAUSTFLOAT(fConst4 * (fRec3 * fTemp3 / fTemp1));
			float fTemp25 = float(input1[i0]) - fRec20[1];
			float fTemp26 = fRec20[1] + fConst27 * fTemp25;
			fRec20[0] = ((std::fabs(fTemp26) > 1.1754944e-38f) ? fTemp26 : 0.0f);
			float fTemp27 = fConst25 * fTemp25 + fConst29 * fTemp8 * (fRec20[1] + fConst28 * fTemp25);
			float fRec21 = ((std::fabs(fTemp27) > 1.1754944e-38f) ? fTemp27 : 0.0f);
			float fTemp28 = fConst3 * (fRec21 / fTemp8) - fConst23 * fRec18[1];
			float fTemp29 = fRec18[1] + fConst32 * fTemp28;
			fRec18[0] = ((std::fabs(fTemp29) > 1.1754944e-38f) ? fTemp29 : 0.0f);
			float fTemp30 = fConst30 * fTemp28 + fConst33 * fTemp7 * (fRec18[1] + fConst31 * fTemp28);
			float fRec19 = ((std::fabs(fTemp30) > 1.1754944e-38f) ? fTemp30 : 0.0f);
			float fTemp31 = fConst19 * (fRec19 / fTemp7) - fConst18 * fRec16[1];
			float fTemp32 = fRec16[1] + fConst36 * fTemp31;
			fRec16[0] = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fTemp33 = fConst34 * fTemp31 + fConst37 * fTemp6 * (fRec16[1] + fConst35 * fTemp31);
			float fRec17 = ((std::fabs(fTemp33) > 1.1754944e-38f) ? fTemp33 : 0.0f);
			float fTemp34 = fConst14 * (fRec17 / fTemp6) - fConst13 * fRec14[1];
			float fTemp35 = fRec14[1] + fConst40 * fTemp34;
			fRec14[0] = ((std::fabs(fTemp35) > 1.1754944e-38f) ? fTemp35 : 0.0f);
			float fTemp36 = fConst38 * fTemp34 + fConst41 * fTemp5 * (fRec14[1] + fConst39 * fTemp34);
			float fRec15 = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			float fTemp37 = fConst9 * (fRec15 / fTemp5) - fConst8 * fRec12[1];
			float fTemp38 = fRec12[1] + fConst44 * fTemp37;
			fRec12[0] = ((std::fabs(fTemp38) > 1.1754944e-38f) ? fTemp38 : 0.0f);
			float fTemp39 = fConst42 * fTemp37 + fConst45 * fTemp23 * (fRec12[1] + fConst43 * fTemp37);
			float fRec13 = ((std::fabs(fTemp39) > 1.1754944e-38f) ? fTemp39 : 0.0f);
			output1[i0] = FAUSTFLOAT(fConst4 * (fRec13 * fTemp3 / fTemp1));
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec1[1] = fRec1[0];
			fRec10[1] = fRec10[0];
			fRec8[1] = fRec8[0];
			fRec6[1] = fRec6[0];
			fRec4[1] = fRec4[0];
			fRec2[1] = fRec2[0];
			fRec20[1] = fRec20[0];
			fRec18[1] = fRec18[0];
			fRec16[1] = fRec16[0];
			fRec14[1] = fRec14[0];
			fRec12[1] = fRec12[0];
		}
	}

};

#endif
