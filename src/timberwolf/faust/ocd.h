// D:\workspace\Fanan_web_hub\src\timberwolf\faust\ocd.h
// Written by the Faust compiler (2.70.3) from ocd.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "ocd"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Ocd -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Ocd_H__
#define  __Faust_Ocd_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Ocd
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

static float Faust_Ocd_faustpower2_f(float value) {
	return value * value;
}

class Faust_Ocd : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	float fConst3;
	FAUSTFLOAT fHslider1;
	float fVec1[2];
	float fRec8[2];
	FAUSTFLOAT fHslider2;
	float fVec2[2];
	float fRec9[2];
	float fConst4;
	float fConst5;
	float fConst6;
	float fConst7;
	FAUSTFLOAT fHslider3;
	float fVec3[2];
	float fRec21[2];
	float fConst8;
	float fConst11;
	float fRec22[2];
	float fConst12;
	float fRec18[2];
	float fRec19[2];
	float fRec20[12];
	float fConst13;
	float fConst15;
	float fConst20;
	float fConst23;
	float fConst26;
	float fConst29;
	float fConst32;
	float fConst35;
	float fConst38;
	float fConst41;
	float fConst44;
	float fConst47;
	float fConst50;
	float fConst53;
	float fConst56;
	float fConst59;
	float fConst62;
	float fConst65;
	float fConst68;
	float fConst71;
	float fConst73;
	float fConst74;
	float fRec13[2];
	float fRec14[12];
	float fRec15[12];
	float fRec16[12];
	float fRec17[12];
	float fConst75;
	float fConst76;
	float fRec10[2];
	float fRec11[2];
	float fRec2[2];
	int iRec3[2];
	int iRec4[2];
	float fRec5[2];
	float fRec6[2];
	int iRec7[2];
	float fConst77;
	float fRec1[2];
	
 public:
	Faust_Ocd() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Ocd -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("filename", "ocd.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/dcblockerat:author", "Julius O. Smith III");
		m->declare("filters.lib/dcblockerat:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/dcblockerat:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/fir:author", "Julius O. Smith III");
		m->declare("filters.lib/fir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/fir:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/pole:author", "Julius O. Smith III");
		m->declare("filters.lib/pole:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/pole:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf1s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf2s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2s:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("filters.lib/zero:author", "Julius O. Smith III");
		m->declare("filters.lib/zero:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/zero:license", "LicenseRef-STK-4.3");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "ocd");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.8.2");
		m->declare("vaeffects.lib/fulltoneOCD:author", "David Braun");
		m->declare("vaeffects.lib/fulltoneOCD:license", "MIT");
		m->declare("vaeffects.lib/name", "Faust Virtual Analog Filter Effect Library");
		m->declare("vaeffects.lib/version", "1.7.2");
		m->declare("wdmodels.lib/builddown:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/builddown:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/builddown:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/buildout:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/buildout:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/buildout:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/buildtree:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/buildtree:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/buildtree:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/buildup:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/buildup:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/buildup:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/capacitor:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/capacitor:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/capacitor:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/getres:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/getres:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/getres:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/name", "Faust Wave Digital Model Library");
		m->declare("wdmodels.lib/parallel:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/parallel:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/parallel:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/parres:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/parres:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/parres:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/resistor:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/resistor:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/resistor:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/resistor_Vout:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/resistor_Vout:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/resistor_Vout:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/series:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/series:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/series:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/u_voltage:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/u_voltage:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/u_voltage:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/version", "2.0.1");
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
		float fConst2 = 15.707963f / fConst0;
		fConst3 = 1.0f - fConst2;
		fConst4 = 10638298.0f / fConst0;
		fConst5 = 5e+04f / fConst0;
		fConst6 = std::tan(std::max<float>(0.0f, std::min<float>(3141.5928f / fConst0, 1.5676547f)));
		fConst7 = 4.863686f * fConst6;
		fConst8 = 523353.22f * fConst6;
		float fConst9 = std::tan(std::max<float>(0.0f, std::min<float>(47.348484f / fConst0, 1.5676547f)));
		float fConst10 = std::min<float>(3.4028235e+38f, 1.0f / (fConst9 + 1.0f));
		fConst11 = 2.0f * fConst9 * fConst10;
		fConst12 = 0.9791667f * fConst10;
		fConst13 = 8e-08f * fConst0;
		float fConst14 = fConst13 + 0.000104347826f;
		fConst15 = 1.0f / fConst14;
		float fConst16 = 0.006830428f / fConst14;
		float fConst17 = (0.20171258f - fConst16) / (fConst16 + 0.20171258f);
		float fConst18 = 0.0033658212f / fConst14;
		float fConst19 = (0.08928118f - fConst18) / (fConst18 + 0.08928118f);
		fConst20 = fConst19 - fConst17;
		float fConst21 = 0.005880822f / fConst14;
		float fConst22 = (0.20579536f - fConst21) / (fConst21 + 0.20579536f);
		fConst23 = fConst17 - fConst22;
		float fConst24 = 0.004913794f / fConst14;
		float fConst25 = (0.2109272f - fConst24) / (fConst24 + 0.2109272f);
		fConst26 = fConst22 - fConst25;
		float fConst27 = 0.0039286525f / fConst14;
		float fConst28 = (0.21797082f - fConst27) / (fConst27 + 0.21797082f);
		fConst29 = fConst25 - fConst28;
		float fConst30 = 0.0029245226f / fConst14;
		float fConst31 = (0.2291942f - fConst30) / (fConst30 + 0.2291942f);
		fConst32 = fConst28 - fConst31;
		float fConst33 = 0.0018993135f / fConst14;
		float fConst34 = (0.25327152f - fConst33) / (fConst33 + 0.25327152f);
		fConst35 = fConst31 - fConst34;
		float fConst36 = 0.00082778226f / fConst14;
		float fConst37 = (0.4499385f - fConst36) / (fConst36 + 0.4499385f);
		fConst38 = fConst34 - fConst37;
		float fConst39 = 2.4911412e-06f / fConst14;
		float fConst40 = (4.165879f - fConst39) / (fConst39 + 4.165879f);
		fConst41 = fConst37 - fConst40;
		float fConst42 = 0.0004611941f / fConst14;
		float fConst43 = (0.26385236f - fConst42) / (fConst42 + 0.26385236f);
		fConst44 = fConst40 - fConst43;
		float fConst45 = 0.0015121456f / fConst14;
		float fConst46 = (0.19996175f - fConst45) / (fConst45 + 0.19996175f);
		fConst47 = fConst43 - fConst46;
		float fConst48 = 0.0025440762f / fConst14;
		float fConst49 = (0.19784294f - fConst48) / (fConst48 + 0.19784294f);
		fConst50 = fConst46 - fConst49;
		float fConst51 = 0.003555314f / fConst14;
		float fConst52 = (0.19578983f - fConst51) / (fConst51 + 0.19578983f);
		fConst53 = fConst49 - fConst52;
		float fConst54 = 0.004546789f / fConst14;
		float fConst55 = (0.19379906f - fConst54) / (fConst54 + 0.19379906f);
		fConst56 = fConst52 - fConst55;
		float fConst57 = 0.0055193696f / fConst14;
		float fConst58 = (0.19186755f - fConst57) / (fConst57 + 0.19186755f);
		fConst59 = fConst55 - fConst58;
		float fConst60 = 0.006473869f / fConst14;
		float fConst61 = (0.1899924f - fConst60) / (fConst60 + 0.1899924f);
		fConst62 = fConst58 - fConst61;
		float fConst63 = 0.0074110483f / fConst14;
		float fConst64 = (0.18817092f - fConst63) / (fConst63 + 0.18817092f);
		fConst65 = fConst61 - fConst64;
		float fConst66 = 0.008331621f / fConst14;
		float fConst67 = (0.18640058f - fConst66) / (fConst66 + 0.18640058f);
		fConst68 = fConst64 - fConst67;
		float fConst69 = 0.008379573f / fConst14;
		float fConst70 = (0.16834211f - fConst69) / (fConst69 + 0.16834211f);
		fConst71 = fConst67 - fConst70;
		float fConst72 = fConst70 + fConst19;
		fConst73 = (-4.0f - 0.04873621f / fConst14) * fConst72 + fConst71 * std::fabs(-0.16834211f - fConst69) + fConst68 * std::fabs(-0.3547427f - 0.016711194f / fConst14) + fConst65 * std::fabs(-0.5429136f - 0.024122242f / fConst14) + fConst62 * std::fabs(-0.73290604f - 0.030596111f / fConst14) + fConst59 * std::fabs(-0.9247736f - 0.036115482f / fConst14) + fConst56 * std::fabs(-1.1185726f - 0.04066227f / fConst14) + fConst53 * std::fabs(-1.3143624f - 0.044217583f / fConst14) + fConst50 * std::fabs(-1.5122054f - 0.04676166f / fConst14) + fConst47 * std::fabs(-1.7121671f - 0.048273806f / fConst14) + fConst44 * std::fabs(-1.9760195f - 0.048735f / fConst14) + fConst41 * std::fabs(-6.141898f - 0.048737492f / fConst14) + fConst38 * std::fabs(-6.591837f - 0.049565274f / fConst14) + fConst35 * std::fabs(-6.845108f - 0.051464587f / fConst14) + fConst32 * std::fabs(-7.074302f - 0.054389108f / fConst14) + fConst29 * std::fabs(-7.292273f - 0.058317762f / fConst14) + fConst26 * std::fabs(-7.5032005f - 0.06323156f / fConst14) + fConst23 * std::fabs(-7.708996f - 0.069112375f / fConst14) + fConst20 * std::fabs(-7.9107084f - 0.07594281f / fConst14);
		fConst74 = 0.5f * fConst72;
		fConst75 = std::min<float>(3.4028235e+38f, 1.0f / (4.863686f * fConst6 * (0.04046655f * fConst6 + 1.0f) + 1.0f));
		fConst76 = 4.863686f * fConst6 * fConst75;
		fConst77 = 1.0f / (fConst2 + 1.0f);
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(5e+01f);
		fHslider1 = FAUSTFLOAT(1.0f);
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
			fRec8[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec9[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fVec3[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec21[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec22[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec18[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec19[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 12; l11 = l11 + 1) {
			fRec20[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec13[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 12; l13 = l13 + 1) {
			fRec14[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 12; l14 = l14 + 1) {
			fRec15[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 12; l15 = l15 + 1) {
			fRec16[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 12; l16 = l16 + 1) {
			fRec17[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec10[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec11[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fRec2[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			iRec3[l20] = 0;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			iRec4[l21] = 0;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fRec5[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec6[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			iRec7[l24] = 0;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec1[l25] = 0.0f;
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
	
	virtual Faust_Ocd* clone() {
		return new Faust_Ocd();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("ocd");
		ui_interface->addHorizontalSlider("Drive", &fHslider3, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Peak", &fHslider1, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(1.0f));
		ui_interface->addHorizontalSlider("Tone", &fHslider2, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Volume", &fHslider0, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.01f * fSlow0;
		float fSlow2 = float(fHslider1);
		float fSlow3 = float(fHslider2);
		float fSlow4 = 0.01f * fSlow3;
		float fSlow5 = float(fHslider3);
		float fSlow6 = 0.01f * fSlow5;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			float fTemp0 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec0[1]))) + fRec0[1]);
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			fVec1[0] = fSlow2;
			float fTemp1 = fConst1 * (std::min<float>(3.4028235e+38f, fVec1[1] - fSlow2) + fRec8[1]);
			fRec8[0] = ((std::fabs(fTemp1) > 1.1754944e-38f) ? fTemp1 : 0.0f);
			float fTemp2 = 3.3e+04f - 1.98e+04f * std::max<float>(0.0f, std::min<float>(1.0f, fSlow2 + fRec8[0]));
			fVec2[0] = fSlow3;
			float fTemp3 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow3 - fVec2[1]))) + fRec9[1]);
			fRec9[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = std::max<float>(1.0f, 1e+04f * std::max<float>(0.0f, std::min<float>(1.0f, fSlow4 + fRec9[0])));
			float fTemp5 = fConst4 + fTemp4;
			float fTemp6 = 1.0f / fTemp5 + 2e-06f;
			float fTemp7 = 1.0f / fTemp6 + fTemp2;
			float fTemp8 = fConst5 + fTemp7;
			float fTemp9 = fConst4 + fTemp4 + 5e+05f;
			float fTemp10 = float(iRec7[1]);
			float fTemp11 = fRec6[1] + fTemp10;
			float fTemp12 = float(iRec4[1]);
			float fTemp13 = fTemp12 * fTemp5 - 5e+05f * fTemp11;
			float fTemp14 = fTemp13 / fTemp9;
			float fTemp15 = float(-iRec3[1]);
			float fTemp16 = fConst5 * (fTemp15 - fTemp14);
			fVec3[0] = fSlow5;
			float fTemp17 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow5 - fVec3[1]))) + fRec21[1]);
			fRec21[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = 5e+05f * Faust_Ocd_faustpower2_f(std::max<float>(0.0f, std::min<float>(1.0f, fSlow6 + fRec21[0]))) + 1.8e+04f;
			float fTemp19 = 2.2e-10f * fTemp18 + 0.00022f;
			float fTemp20 = std::min<float>(3.4028235e+38f, 1.0f / (fConst6 * ((fConst8 + 3.2883254e+09f * fTemp19) / fTemp18) + 1.0f));
			float fTemp21 = std::max<float>(std::fabs(3.2883254e+09f * (fTemp19 / fTemp18)), std::sqrt(std::fabs(523353.22f / fTemp18)));
			float fTemp22 = fTemp18 * fTemp21;
			float fTemp23 = float(input0[i0]) - fRec22[1];
			float fTemp24 = fRec22[1] + fConst11 * fTemp23;
			fRec22[0] = ((std::fabs(fTemp24) > 1.1754944e-38f) ? fTemp24 : 0.0f);
			float fTemp25 = fConst12 * fTemp23;
			float fRec23 = ((std::fabs(fTemp25) > 1.1754944e-38f) ? fTemp25 : 0.0f);
			float fTemp26 = fRec23 - (3.2883254e+09f * fTemp19 * fRec18[1] + 523353.22f * ((fConst6 * fTemp21 * fRec18[1] + fRec19[1]) / fTemp21)) / fTemp22;
			float fTemp27 = fRec18[1] + fConst6 * fTemp21 * fTemp26 * fTemp20;
			float fTemp28 = 2.0f * fTemp27 - fRec18[1];
			fRec18[0] = ((std::fabs(fTemp28) > 1.1754944e-38f) ? fTemp28 : 0.0f);
			float fTemp29 = fRec19[1] + fConst6 * fTemp21 * fTemp27;
			float fTemp30 = 2.0f * fTemp29 - fRec19[1];
			fRec19[0] = ((std::fabs(fTemp30) > 1.1754944e-38f) ? fTemp30 : 0.0f);
			float fTemp31 = fTemp26 * fTemp20 + (3.2883254e+09f * fTemp27 * (1.0022e-07f * fTemp18 + 0.00022f) + 523353.22f * (fTemp29 / fTemp21)) / fTemp22;
			fRec20[0] = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			float fTemp32 = fConst13 * fRec13[1] + 0.0001f * (0.0037884237f * fRec20[1] + 0.046747785f * fRec20[3] + 0.20949881f * fRec20[5] + 0.8799858f * fRec20[6] + 0.02112335f * fRec20[9] + 0.0023888566f * fRec20[11] - (0.00025234505f * fRec20[0] + 0.016762448f * fRec20[2] + 0.10065608f * fRec20[4] + 0.009853004f * fRec20[7] + 0.026253322f * fRec20[8] + 0.009654191f * fRec20[10]));
			float fTemp33 = fConst73 - (fConst71 * std::fabs(fConst15 * (fTemp32 + 0.040356636f) + 3.831658f) + fConst68 * std::fabs(fConst15 * (fTemp32 + 0.032025017f) + 3.6452572f) + fConst65 * std::fabs(fConst15 * (fTemp32 + 0.024613967f) + 3.4570863f) + fConst62 * std::fabs(fConst15 * (fTemp32 + 0.018140098f) + 3.267094f) + fConst59 * std::fabs(fConst15 * (fTemp32 + 0.012620729f) + 3.0752265f) + fConst56 * std::fabs(fConst15 * (fTemp32 + 0.00807394f) + 2.8814273f) + fConst53 * std::fabs(fConst15 * (fTemp32 + 0.0045186263f) + 2.6856375f) + fConst50 * std::fabs(fConst15 * (fTemp32 + 0.00197455f) + 2.4877946f) + fConst47 * std::fabs(fConst15 * (fTemp32 + 0.00046240442f) + 2.2878327f) + fConst44 * std::fabs(fConst15 * (fTemp32 + 1.2103139e-06f) + 2.0239804f) + fConst41 * std::fabs(fConst15 * (fTemp32 + -1.2808272e-06f) + -2.1418982f) + fConst38 * std::fabs(fConst15 * (fTemp32 + -0.00082906306f) + -2.5918367f) + fConst35 * std::fabs(fConst15 * (fTemp32 + -0.0027283765f) + -2.8451083f) + fConst32 * std::fabs(fConst15 * (fTemp32 + -0.0056528994f) + -3.0743024f) + fConst29 * std::fabs(fConst15 * (fTemp32 + -0.009581552f) + -3.2922733f) + fConst26 * std::fabs(fConst15 * (fTemp32 + -0.014495346f) + -3.5032005f) + fConst23 * std::fabs(fConst15 * (fTemp32 + -0.020376168f) + -3.7089958f) + fConst20 * std::fabs(fConst15 * (fTemp32 + -0.027206596f) + -3.9107084f));
			float fTemp34 = 0.5f * fTemp33;
			float fTemp35 = fTemp32 + fConst74 * fTemp32 + 0.04873621f;
			float fTemp36 = fConst15 * fTemp35;
			float fTemp37 = fConst13 * (fTemp36 + (-4.0f - (fRec13[1] + fTemp34))) + 0.0001f * (0.002968143f * fRec20[1] + 0.055817958f * fRec20[3] + 0.48142695f * fRec20[5] + 0.7307934f * fRec20[6] + 0.032341596f * fRec20[8] + 0.0007282737f * fRec20[11] - (0.00015138523f * fRec20[0] + 0.016454924f * fRec20[2] + 0.15112622f * fRec20[4] + 0.13127643f * fRec20[7] + 0.0034562123f * fRec20[9] + 0.0017128049f * fRec20[10]));
			float fTemp38 = fTemp37 + fConst74 * fTemp37 + 0.04873621f;
			float fTemp39 = fTemp35 - fTemp38;
			float fTemp40 = fConst73 - (fConst71 * std::fabs(fConst15 * (fTemp37 + 0.040356636f) + 3.831658f) + fConst68 * std::fabs(fConst15 * (fTemp37 + 0.032025017f) + 3.6452572f) + fConst65 * std::fabs(fConst15 * (fTemp37 + 0.024613967f) + 3.4570863f) + fConst62 * std::fabs(fConst15 * (fTemp37 + 0.018140098f) + 3.267094f) + fConst59 * std::fabs(fConst15 * (fTemp37 + 0.012620729f) + 3.0752265f) + fConst56 * std::fabs(fConst15 * (fTemp37 + 0.00807394f) + 2.8814273f) + fConst53 * std::fabs(fConst15 * (fTemp37 + 0.0045186263f) + 2.6856375f) + fConst50 * std::fabs(fConst15 * (fTemp37 + 0.00197455f) + 2.4877946f) + fConst47 * std::fabs(fConst15 * (fTemp37 + 0.00046240442f) + 2.2878327f) + fConst44 * std::fabs(fConst15 * (fTemp37 + 1.2103139e-06f) + 2.0239804f) + fConst41 * std::fabs(fConst15 * (fTemp37 + -1.2808272e-06f) + -2.1418982f) + fConst38 * std::fabs(fConst15 * (fTemp37 + -0.00082906306f) + -2.5918367f) + fConst35 * std::fabs(fConst15 * (fTemp37 + -0.0027283765f) + -2.8451083f) + fConst32 * std::fabs(fConst15 * (fTemp37 + -0.0056528994f) + -3.0743024f) + fConst29 * std::fabs(fConst15 * (fTemp37 + -0.009581552f) + -3.2922733f) + fConst26 * std::fabs(fConst15 * (fTemp37 + -0.014495346f) + -3.5032005f) + fConst23 * std::fabs(fConst15 * (fTemp37 + -0.020376168f) + -3.7089958f) + fConst20 * std::fabs(fConst15 * (fTemp37 + -0.027206596f) + -3.9107084f));
			float fTemp41 = fTemp33 - fTemp40;
			float fTemp42 = fConst13 * (fRec13[1] + 0.5f * fTemp41 - fConst15 * fTemp39) + 0.0001f * (0.0007282737f * fRec20[0] + 0.032341596f * fRec20[3] + 0.7307934f * fRec20[5] + 0.48142695f * fRec20[6] + 0.055817958f * fRec20[8] + 0.002968143f * fRec20[10] - (0.0017128049f * fRec20[1] + 0.0034562123f * fRec20[2] + 0.13127643f * fRec20[4] + 0.15112622f * fRec20[7] + 0.016454924f * fRec20[9] + 0.00015138523f * fRec20[11]));
			float fTemp43 = fConst73 - (fConst71 * std::fabs(fConst15 * (fTemp42 + 0.040356636f) + 3.831658f) + fConst68 * std::fabs(fConst15 * (fTemp42 + 0.032025017f) + 3.6452572f) + fConst65 * std::fabs(fConst15 * (fTemp42 + 0.024613967f) + 3.4570863f) + fConst62 * std::fabs(fConst15 * (fTemp42 + 0.018140098f) + 3.267094f) + fConst59 * std::fabs(fConst15 * (fTemp42 + 0.012620729f) + 3.0752265f) + fConst56 * std::fabs(fConst15 * (fTemp42 + 0.00807394f) + 2.8814273f) + fConst53 * std::fabs(fConst15 * (fTemp42 + 0.0045186263f) + 2.6856375f) + fConst50 * std::fabs(fConst15 * (fTemp42 + 0.00197455f) + 2.4877946f) + fConst47 * std::fabs(fConst15 * (fTemp42 + 0.00046240442f) + 2.2878327f) + fConst44 * std::fabs(fConst15 * (fTemp42 + 1.2103139e-06f) + 2.0239804f) + fConst41 * std::fabs(fConst15 * (fTemp42 + -1.2808272e-06f) + -2.1418982f) + fConst38 * std::fabs(fConst15 * (fTemp42 + -0.00082906306f) + -2.5918367f) + fConst35 * std::fabs(fConst15 * (fTemp42 + -0.0027283765f) + -2.8451083f) + fConst32 * std::fabs(fConst15 * (fTemp42 + -0.0056528994f) + -3.0743024f) + fConst29 * std::fabs(fConst15 * (fTemp42 + -0.009581552f) + -3.2922733f) + fConst26 * std::fabs(fConst15 * (fTemp42 + -0.014495346f) + -3.5032005f) + fConst23 * std::fabs(fConst15 * (fTemp42 + -0.020376168f) + -3.7089958f) + fConst20 * std::fabs(fConst15 * (fTemp42 + -0.027206596f) + -3.9107084f));
			float fTemp44 = fTemp41 + fTemp43;
			float fTemp45 = fTemp42 + fConst74 * fTemp42 + 0.04873621f;
			float fTemp46 = fTemp39 + fTemp45;
			float fTemp47 = fConst13 * (fConst15 * fTemp46 + (-4.0f - (fRec13[1] + 0.5f * fTemp44))) + 0.0001f * (0.0023888566f * fRec20[0] + 0.02112335f * fRec20[2] + 0.8799858f * fRec20[5] + 0.20949881f * fRec20[6] + 0.046747785f * fRec20[8] + 0.0037884237f * fRec20[10] - (0.009654191f * fRec20[1] + 0.026253322f * fRec20[3] + 0.009853004f * fRec20[4] + 0.10065608f * fRec20[7] + 0.016762448f * fRec20[9] + 0.00025234505f * fRec20[11]));
			float fTemp48 = fTemp47 + fConst74 * fTemp47 + 0.04873621f;
			float fTemp49 = fConst73 - (fConst71 * std::fabs(fConst15 * (fTemp47 + 0.040356636f) + 3.831658f) + fConst68 * std::fabs(fConst15 * (fTemp47 + 0.032025017f) + 3.6452572f) + fConst65 * std::fabs(fConst15 * (fTemp47 + 0.024613967f) + 3.4570863f) + fConst62 * std::fabs(fConst15 * (fTemp47 + 0.018140098f) + 3.267094f) + fConst59 * std::fabs(fConst15 * (fTemp47 + 0.012620729f) + 3.0752265f) + fConst56 * std::fabs(fConst15 * (fTemp47 + 0.00807394f) + 2.8814273f) + fConst53 * std::fabs(fConst15 * (fTemp47 + 0.0045186263f) + 2.6856375f) + fConst50 * std::fabs(fConst15 * (fTemp47 + 0.00197455f) + 2.4877946f) + fConst47 * std::fabs(fConst15 * (fTemp47 + 0.00046240442f) + 2.2878327f) + fConst44 * std::fabs(fConst15 * (fTemp47 + 1.2103139e-06f) + 2.0239804f) + fConst41 * std::fabs(fConst15 * (fTemp47 + -1.2808272e-06f) + -2.1418982f) + fConst38 * std::fabs(fConst15 * (fTemp47 + -0.00082906306f) + -2.5918367f) + fConst35 * std::fabs(fConst15 * (fTemp47 + -0.0027283765f) + -2.8451083f) + fConst32 * std::fabs(fConst15 * (fTemp47 + -0.0056528994f) + -3.0743024f) + fConst29 * std::fabs(fConst15 * (fTemp47 + -0.009581552f) + -3.2922733f) + fConst26 * std::fabs(fConst15 * (fTemp47 + -0.014495346f) + -3.5032005f) + fConst23 * std::fabs(fConst15 * (fTemp47 + -0.020376168f) + -3.7089958f) + fConst20 * std::fabs(fConst15 * (fTemp47 + -0.027206596f) + -3.9107084f));
			float fTemp50 = fRec13[1] + 0.5f * (fTemp44 - fTemp49) - fConst15 * (fTemp46 - fTemp48);
			fRec13[0] = ((std::fabs(fTemp50) > 1.1754944e-38f) ? fTemp50 : 0.0f);
			float fTemp51 = 0.5f * (fConst15 * fTemp48 + (-4.0f - 0.5f * fTemp49));
			fRec14[0] = ((std::fabs(fTemp51) > 1.1754944e-38f) ? fTemp51 : 0.0f);
			float fTemp52 = 0.5f * (fConst15 * fTemp45 + (-4.0f - 0.5f * fTemp43));
			fRec15[0] = ((std::fabs(fTemp52) > 1.1754944e-38f) ? fTemp52 : 0.0f);
			float fTemp53 = 0.5f * (fConst15 * fTemp38 + (-4.0f - 0.5f * fTemp40));
			fRec16[0] = ((std::fabs(fTemp53) > 1.1754944e-38f) ? fTemp53 : 0.0f);
			float fTemp54 = 0.5f * (fTemp36 + (-4.0f - fTemp34));
			fRec17[0] = ((std::fabs(fTemp54) > 1.1754944e-38f) ? fTemp54 : 0.0f);
			float fTemp55 = 0.95652175f * (0.0052808374f * (fRec14[9] + fRec17[2]) + 0.21999645f * (fRec14[6] + fRec17[5]) + 0.052374702f * (fRec14[5] + fRec17[6]) + 0.011686946f * (fRec14[3] + fRec17[8]) + 0.00094710593f * (fRec14[1] + fRec17[10]) + 0.00059721415f * (fRec17[0] + fRec14[11]) + 0.008085399f * (fRec15[8] + fRec16[3]) + 0.18269835f * (fRec15[6] + fRec16[5]) + 0.12035674f * (fRec15[5] + fRec16[6]) + 0.0139544895f * (fRec15[3] + fRec16[8]) + 0.00018206843f * (fRec16[0] + fRec15[11]) + 0.00074203574f * (fRec15[1] + fRec16[10]) - (0.0008640531f * (fRec15[9] + fRec16[2]) + 0.032819107f * (fRec15[7] + fRec16[4]) + 0.004113731f * (fRec15[2] + fRec16[9]) + 0.00042820122f * (fRec15[10] + fRec16[1]) + 0.037781555f * (fRec15[4] + fRec16[7]) + 3.784631e-05f * (fRec15[0] + fRec16[11]) + 6.308626e-05f * (fRec14[0] + fRec17[11]) + 0.004190612f * (fRec14[2] + fRec17[9]) + 0.02516402f * (fRec14[4] + fRec17[7]) + 0.002463251f * (fRec14[7] + fRec17[4]) + 0.0065633305f * (fRec14[8] + fRec17[3]) + 0.0024135478f * (fRec14[10] + fRec17[1]))) - (fRec10[1] + 0.008320141f * (fConst7 * fRec10[1] + fRec11[1]));
			float fTemp56 = fRec10[1] + fConst76 * fTemp55;
			float fTemp57 = 2.0f * fTemp56 - fRec10[1];
			fRec10[0] = ((std::fabs(fTemp57) > 1.1754944e-38f) ? fTemp57 : 0.0f);
			float fTemp58 = fRec11[1] + fConst7 * fTemp56;
			float fTemp59 = 2.0f * fTemp58 - fRec11[1];
			fRec11[0] = ((std::fabs(fTemp59) > 1.1754944e-38f) ? fTemp59 : 0.0f);
			float fTemp60 = fConst75 * fTemp55 + 4.8138824f * fTemp56 + 0.008320141f * fTemp58;
			float fRec12 = ((std::fabs(fTemp60) > 1.1754944e-38f) ? fTemp60 : 0.0f);
			float fTemp61 = fTemp15 + 2.0f * fRec12 + fRec2[1] - fTemp14;
			float fTemp62 = (fRec2[1] * fTemp7 - fConst5 * fTemp61 - fTemp16) / fTemp8;
			fRec2[0] = ((std::fabs(fTemp62) > 1.1754944e-38f) ? fTemp62 : 0.0f);
			iRec3[0] = 0;
			iRec4[0] = 0;
			float fTemp63 = (((fTemp16 - fTemp7 * (fRec2[1] + fTemp61)) / fTemp8 + float(iRec3[1])) / fTemp6 - fTemp13 * fTemp2 / fTemp9) / fTemp7;
			float fTemp64 = 0.5f * (5e+05f * (fTemp11 / fTemp9) + fTemp63 + 5e+05f * (fTemp12 / fTemp9));
			fRec5[0] = ((std::fabs(-fTemp64) > 1.1754944e-38f) ? -fTemp64 : 0.0f);
			float fTemp65 = (fTemp4 * fRec6[1] - fConst4 * (fTemp5 * (fTemp12 + fTemp11) / fTemp9 - fTemp63) - fConst4 * fTemp10) / fTemp5;
			fRec6[0] = ((std::fabs(fTemp65) > 1.1754944e-38f) ? fTemp65 : 0.0f);
			iRec7[0] = 0;
			float fTemp66 = fConst77 * (fRec5[0] - fRec5[1] + fConst3 * fRec1[1]);
			fRec1[0] = ((std::fabs(fTemp66) > 1.1754944e-38f) ? fTemp66 : 0.0f);
			output0[i0] = FAUSTFLOAT(fRec1[0] * (fSlow1 + fRec0[0]));
			fVec0[1] = fVec0[0];
			fRec0[1] = fRec0[0];
			fVec1[1] = fVec1[0];
			fRec8[1] = fRec8[0];
			fVec2[1] = fVec2[0];
			fRec9[1] = fRec9[0];
			fVec3[1] = fVec3[0];
			fRec21[1] = fRec21[0];
			fRec22[1] = fRec22[0];
			fRec18[1] = fRec18[0];
			fRec19[1] = fRec19[0];
			for (int j0 = 11; j0 > 0; j0 = j0 - 1) {
				fRec20[j0] = fRec20[j0 - 1];
			}
			fRec13[1] = fRec13[0];
			for (int j1 = 11; j1 > 0; j1 = j1 - 1) {
				fRec14[j1] = fRec14[j1 - 1];
			}
			for (int j2 = 11; j2 > 0; j2 = j2 - 1) {
				fRec15[j2] = fRec15[j2 - 1];
			}
			for (int j3 = 11; j3 > 0; j3 = j3 - 1) {
				fRec16[j3] = fRec16[j3 - 1];
			}
			for (int j4 = 11; j4 > 0; j4 = j4 - 1) {
				fRec17[j4] = fRec17[j4 - 1];
			}
			fRec10[1] = fRec10[0];
			fRec11[1] = fRec11[0];
			fRec2[1] = fRec2[0];
			iRec3[1] = iRec3[0];
			iRec4[1] = iRec4[0];
			fRec5[1] = fRec5[0];
			fRec6[1] = fRec6[0];
			iRec7[1] = iRec7[0];
			fRec1[1] = fRec1[0];
		}
	}

};

#endif
