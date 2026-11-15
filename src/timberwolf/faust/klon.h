// D:\workspace\Fanan_web_hub\src\timberwolf\faust\klon.h
// Written by the Faust compiler (2.70.3) from klon.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "klon"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Klon -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __Faust_Klon_H__
#define  __Faust_Klon_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Klon
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

static float Faust_Klon_faustpower2_f(float value) {
	return value * value;
}

class Faust_Klon : public FaustDsp {
	
 private:
	
	int iVec0[2];
	int fSampleRate;
	float fConst1;
	float fConst2;
	FAUSTFLOAT fHslider0;
	float fVec1[2];
	float fConst3;
	float fRec6[2];
	float fRec5[2];
	float fVec2[2];
	float fConst4;
	float fRec3[2];
	float fRec4[2];
	FAUSTFLOAT fHslider1;
	float fVec3[2];
	float fRec12[2];
	float fRec11[2];
	float fVec4[2];
	float fRec9[2];
	float fRec10[2];
	float fConst5;
	float fConst7;
	int iRec14[2];
	FAUSTFLOAT fHslider2;
	float fVec5[2];
	float fRec29[2];
	float fConst8;
	float fConst10;
	float fConst11;
	float fConst12;
	float fConst13;
	float fConst14;
	float fConst15;
	float fConst16;
	float fConst18;
	float fConst19;
	float fConst21;
	float fConst23;
	float fConst25;
	float fConst26;
	float fConst28;
	float fConst29;
	float fRec30[2];
	float fConst30;
	float fRec15[2];
	float fConst31;
	float fConst32;
	float fConst33;
	float fConst34;
	float fConst35;
	float fRec16[2];
	int iRec17[2];
	int iRec18[2];
	float fConst36;
	float fConst37;
	float fConst38;
	float fRec20[2];
	int iRec21[2];
	int iRec22[2];
	int iRec23[2];
	int iRec24[2];
	float fRec25[2];
	int iRec26[2];
	int iRec27[2];
	float fRec28[2];
	float fConst39;
	float fConst40;
	float fConst41;
	float fConst42;
	float fConst43;
	float fConst44;
	float fConst45;
	float fConst47;
	float fConst49;
	float fRec31[2];
	float fConst50;
	float fRec32[2];
	int iRec33[2];
	float fConst51;
	float fRec35[2];
	int iRec37[2];
	float fConst52;
	float fConst53;
	float fConst54;
	float fRec39[2];
	int iRec40[2];
	float fRec54[2];
	float fVec6[2];
	float fRec52[2];
	float fRec53[2];
	float fConst55;
	float fVec7[2];
	float fRec55[3];
	float fVec8[2];
	float fRec51[2];
	float fRec50[2];
	float fRec49[2];
	float fRec48[2];
	float fConst60;
	float fConst62;
	float fConst63;
	float fConst65;
	float fConst66;
	float fConst67;
	float fConst69;
	float fConst70;
	float fRec58[2];
	float fRec57[2];
	float fRec56[2];
	float fConst71;
	float fRec44[2];
	float fConst72;
	float fRec45[2];
	float fConst73;
	float fRec46[2];
	float fConst74;
	float fRec47[2];
	float fRec43[2];
	float fRec42[2];
	float fRec41[2];
	float fRec61[2];
	float fRec60[2];
	float fRec59[2];
	float fVec9[2];
	float fConst75;
	float fRec13[2];
	float fVec10[2];
	float fRec8[2];
	float fConst76;
	float fRec7[2];
	float fConst78;
	float fConst79;
	float fRec0[2];
	float fConst80;
	float fRec1[2];
	float fConst81;
	
 public:
	Faust_Klon() {}

	void metadata(Meta* m) { 
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/sAndH:author", "Romain Michon");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Klon -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0");
		m->declare("filename", "klon.dsp");
		m->declare("filters.lib/SVFTPT:author", "Dario Sanfilippo");
		m->declare("filters.lib/SVFTPT:copyright", "Copyright (C) 2024 Dario Sanfilippo <sanfilippo.dario@gmail.com>");
		m->declare("filters.lib/SVFTPT:license", "MIT");
		m->declare("filters.lib/fir:author", "Julius O. Smith III");
		m->declare("filters.lib/fir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/fir:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/iir:author", "Julius O. Smith III");
		m->declare("filters.lib/iir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/iir:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/tf1:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/tf2:author", "Julius O. Smith III");
		m->declare("filters.lib/tf2:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf2:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/version", "1.13.6");
		m->declare("interpolators.lib/name", "Faust Interpolator Library");
		m->declare("interpolators.lib/remap:author", "David Braun");
		m->declare("interpolators.lib/version", "1.6.2");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LicenseRef-LGPL-2.1-or-later-with-Faust-exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.5");
		m->declare("name", "klon");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.8.2");
		m->declare("vaeffects.lib/klonCentaur:author", "David Braun");
		m->declare("vaeffects.lib/klonCentaur:copyright", "Chowdhury-DSP");
		m->declare("vaeffects.lib/klonCentaur:license", "BSD-3-Clause");
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
		m->declare("wdmodels.lib/omega:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/parallel:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/parallel:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/parallel:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/parres:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/parres:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/parres:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/resVoltage:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/resVoltage:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/resVoltage:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/resVoltage_Vout:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/resVoltage_Vout:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/resVoltage_Vout:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/resistor:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/resistor:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/resistor:license", "LicenseRef-STK-4.3");
		m->declare("wdmodels.lib/resistor_Iout:author", "Dirk Roosenburg");
		m->declare("wdmodels.lib/resistor_Iout:copyright", "Copyright (C) 2020 by Dirk Roosenburg <dirk.roosenburg.30@gmail.com>");
		m->declare("wdmodels.lib/resistor_Iout:license", "LicenseRef-STK-4.3");
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
		fConst1 = std::tan(109.95574f / fConst0);
		fConst2 = fConst1 + 1.4142135f;
		fConst3 = 1.0f - 44.1f / fConst0;
		fConst4 = std::rint(0.05f * fConst0);
		fConst5 = 1e-05f / std::tan(1282.0513f / fConst0);
		float fConst6 = 0.00064288f * fConst0;
		fConst7 = 1.0f - fConst6;
		fConst8 = 2.2727272e+08f / fConst0;
		float fConst9 = fConst8 + 6.9e+04f;
		fConst10 = 18518518.0f / fConst0;
		fConst11 = fConst10 + 1.2e+04f;
		fConst12 = 1.0f / fConst11;
		fConst13 = fConst12 + 1.0f / fConst9;
		fConst14 = 1282051.2f / fConst0;
		fConst15 = fConst14 + 1e+03f;
		fConst16 = 1.0f / fConst15;
		float fConst17 = 1.36e-07f * fConst0;
		fConst18 = 1.0f / (fConst17 + 0.00019607843f);
		fConst19 = 1.0f / (fConst13 + 3.7037036e-05f);
		float fConst20 = fConst10 + 3.9e+04f;
		fConst21 = fConst9 / fConst20;
		float fConst22 = fConst12 + 3.7037036e-05f;
		fConst23 = 1.0f / fConst22;
		float fConst24 = fConst23 + fConst8 + 6.9e+04f;
		fConst25 = 1.0f / fConst24;
		fConst26 = 7352941.0f / fConst0;
		float fConst27 = fConst26 + 5.1e+03f;
		fConst28 = 1.0f / fConst27;
		fConst29 = 9.9009905f / (fConst0 * (4.9504952f / fConst0 + 1.0f));
		fConst30 = 7352941.0f / (fConst0 * fConst27);
		fConst31 = fConst8 + 2.2e+04f;
		fConst32 = 1.0f / fConst20;
		fConst33 = fConst9 / fConst24;
		fConst34 = 2.2727272e+08f / (fConst0 * fConst9);
		fConst35 = 1.0f / fConst31;
		fConst36 = 1.0638298e-05f / fConst9;
		fConst37 = fConst11 / fConst20;
		fConst38 = 1.0f / (fConst22 * fConst24);
		fConst39 = 1.0f / (fConst17 + 0.0001f);
		fConst40 = 1.0f / (2e-06f * fConst0 + 6.666667e-05f);
		fConst41 = fConst40 + 1.5e+03f;
		fConst42 = 1.0f / fConst41;
		fConst43 = 5e+06f / fConst0;
		fConst44 = fConst40 + fConst39;
		fConst45 = 5e+05f / fConst0;
		float fConst46 = fConst45 + 1.5e+04f;
		fConst47 = 1.0f / fConst46;
		float fConst48 = fConst26 + 1e+04f;
		fConst49 = 1.0f / fConst48;
		fConst50 = 7352941.0f / (fConst0 * fConst48);
		fConst51 = 1e+04f / fConst48;
		fConst52 = 1.5e+04f / fConst46;
		fConst53 = 1.5e+03f / fConst46;
		fConst54 = 5e+05f / (fConst0 * fConst46);
		fConst55 = 0.5f / fConst0;
		float fConst56 = 2.5e+05f / fConst0;
		float fConst57 = fConst56 + 4.7e+04f;
		float fConst58 = fConst56 + 1e+03f;
		float fConst59 = 1.0f / fConst58 + 1.0f / fConst57;
		fConst60 = 1.0f / (fConst58 * fConst59);
		float fConst61 = 0.0005802708f / fConst59;
		fConst62 = fConst61 + std::log(fConst61);
		fConst63 = 1.5e-05f / fConst59;
		float fConst64 = fConst56 + 1e-09f;
		fConst65 = fConst64 / fConst58;
		fConst66 = 2.0f * fConst65;
		fConst67 = 2e-09f / fConst64;
		float fConst68 = fConst0 * fConst57;
		fConst69 = 2.5e+05f / fConst68;
		fConst70 = 1e-09f / fConst64;
		fConst71 = 5e+05f / fConst68;
		fConst72 = 1.0f / fConst68;
		fConst73 = 2e-06f / fConst57;
		fConst74 = 0.5f / fConst57;
		fConst75 = 1.0f / (fConst6 + 1.0f);
		fConst76 = 20.146383f / (fConst0 * (10.073192f / fConst0 + 1.0f));
		float fConst77 = fConst1 * fConst2 + 1.0f;
		fConst78 = fConst1 / fConst77;
		fConst79 = 2.0f * fConst78;
		fConst80 = 2.0f * fConst1;
		fConst81 = 1.0f / fConst77;
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(5e+01f);
		fHslider1 = FAUSTFLOAT(5e+01f);
		fHslider2 = FAUSTFLOAT(5e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fVec1[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec6[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec5[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec3[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec4[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fVec3[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec12[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec11[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fVec4[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec9[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec10[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			iRec14[l13] = 0;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fVec5[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec29[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec30[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec15[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec16[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			iRec17[l19] = 0;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			iRec18[l20] = 0;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec20[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			iRec21[l22] = 0;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			iRec22[l23] = 0;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			iRec23[l24] = 0;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			iRec24[l25] = 0;
		}
		for (int l26 = 0; l26 < 2; l26 = l26 + 1) {
			fRec25[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			iRec26[l27] = 0;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			iRec27[l28] = 0;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec28[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2; l30 = l30 + 1) {
			fRec31[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 2; l31 = l31 + 1) {
			fRec32[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			iRec33[l32] = 0;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec35[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			iRec37[l34] = 0;
		}
		for (int l35 = 0; l35 < 2; l35 = l35 + 1) {
			fRec39[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			iRec40[l36] = 0;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fRec54[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fVec6[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 2; l39 = l39 + 1) {
			fRec52[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 2; l40 = l40 + 1) {
			fRec53[l40] = 0.0f;
		}
		for (int l41 = 0; l41 < 2; l41 = l41 + 1) {
			fVec7[l41] = 0.0f;
		}
		for (int l42 = 0; l42 < 3; l42 = l42 + 1) {
			fRec55[l42] = 0.0f;
		}
		for (int l43 = 0; l43 < 2; l43 = l43 + 1) {
			fVec8[l43] = 0.0f;
		}
		for (int l44 = 0; l44 < 2; l44 = l44 + 1) {
			fRec51[l44] = 0.0f;
		}
		for (int l45 = 0; l45 < 2; l45 = l45 + 1) {
			fRec50[l45] = 0.0f;
		}
		for (int l46 = 0; l46 < 2; l46 = l46 + 1) {
			fRec49[l46] = 0.0f;
		}
		for (int l47 = 0; l47 < 2; l47 = l47 + 1) {
			fRec48[l47] = 0.0f;
		}
		for (int l48 = 0; l48 < 2; l48 = l48 + 1) {
			fRec58[l48] = 0.0f;
		}
		for (int l49 = 0; l49 < 2; l49 = l49 + 1) {
			fRec57[l49] = 0.0f;
		}
		for (int l50 = 0; l50 < 2; l50 = l50 + 1) {
			fRec56[l50] = 0.0f;
		}
		for (int l51 = 0; l51 < 2; l51 = l51 + 1) {
			fRec44[l51] = 0.0f;
		}
		for (int l52 = 0; l52 < 2; l52 = l52 + 1) {
			fRec45[l52] = 0.0f;
		}
		for (int l53 = 0; l53 < 2; l53 = l53 + 1) {
			fRec46[l53] = 0.0f;
		}
		for (int l54 = 0; l54 < 2; l54 = l54 + 1) {
			fRec47[l54] = 0.0f;
		}
		for (int l55 = 0; l55 < 2; l55 = l55 + 1) {
			fRec43[l55] = 0.0f;
		}
		for (int l56 = 0; l56 < 2; l56 = l56 + 1) {
			fRec42[l56] = 0.0f;
		}
		for (int l57 = 0; l57 < 2; l57 = l57 + 1) {
			fRec41[l57] = 0.0f;
		}
		for (int l58 = 0; l58 < 2; l58 = l58 + 1) {
			fRec61[l58] = 0.0f;
		}
		for (int l59 = 0; l59 < 2; l59 = l59 + 1) {
			fRec60[l59] = 0.0f;
		}
		for (int l60 = 0; l60 < 2; l60 = l60 + 1) {
			fRec59[l60] = 0.0f;
		}
		for (int l61 = 0; l61 < 2; l61 = l61 + 1) {
			fVec9[l61] = 0.0f;
		}
		for (int l62 = 0; l62 < 2; l62 = l62 + 1) {
			fRec13[l62] = 0.0f;
		}
		for (int l63 = 0; l63 < 2; l63 = l63 + 1) {
			fVec10[l63] = 0.0f;
		}
		for (int l64 = 0; l64 < 2; l64 = l64 + 1) {
			fRec8[l64] = 0.0f;
		}
		for (int l65 = 0; l65 < 2; l65 = l65 + 1) {
			fRec7[l65] = 0.0f;
		}
		for (int l66 = 0; l66 < 2; l66 = l66 + 1) {
			fRec0[l66] = 0.0f;
		}
		for (int l67 = 0; l67 < 2; l67 = l67 + 1) {
			fRec1[l67] = 0.0f;
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
	
	virtual Faust_Klon* clone() {
		return new Faust_Klon();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("klon");
		ui_interface->addHorizontalSlider("Gain", &fHslider2, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Level", &fHslider0, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Treble", &fHslider1, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
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
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			int iTemp0 = 1 - iVec0[1];
			fVec1[0] = fSlow0;
			fRec6[0] = fConst3 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec1[1]))) + fRec6[1]);
			float fTemp1 = std::log(std::max<float>(std::max<float>(1e-05f, std::min<float>(1.0f, fSlow1 + fRec6[0])), 1e-05f));
			fRec5[0] = ((iTemp0) ? fTemp1 : fRec5[1]);
			float fTemp2 = fTemp1 - fRec5[0];
			fVec2[0] = fTemp2;
			float fTemp3 = ((fTemp2 != fVec2[1]) ? fConst4 : fRec3[1] + -1.0f);
			fRec3[0] = fTemp3;
			fRec4[0] = ((fTemp3 > 0.0f) ? fRec4[1] + (fTemp1 - (fRec5[0] + fRec4[1])) / fTemp3 : fTemp2);
			float fTemp4 = std::exp(fRec5[0] + fRec4[0]);
			fVec3[0] = fSlow2;
			fRec12[0] = fConst3 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow2 - fVec3[1]))) + fRec12[1]);
			float fTemp5 = std::max<float>(0.0f, std::min<float>(1.0f, fSlow3 + fRec12[0]));
			fRec11[0] = ((iTemp0) ? fTemp5 : fRec11[1]);
			float fTemp6 = fTemp5 - fRec11[0];
			fVec4[0] = fTemp6;
			float fTemp7 = ((fTemp6 != fVec4[1]) ? fConst4 : fRec9[1] + -1.0f);
			fRec9[0] = fTemp7;
			fRec10[0] = ((fTemp7 > 0.0f) ? fRec10[1] + (fTemp5 - (fRec11[0] + fRec10[1])) / fTemp7 : fTemp6);
			float fTemp8 = fRec11[0] + fRec10[0];
			float fTemp9 = 1.0f / (1e+04f * (1.0f - fTemp8) + 1.8e+03f);
			float fTemp10 = 1.0f / (1e+04f * fTemp8 + 4.7e+03f);
			float fTemp11 = 1e-05f * (fTemp10 + fTemp9);
			float fTemp12 = fConst5 * (fTemp10 + -1e-05f);
			float fTemp13 = fConst5 * (fTemp9 + 1e-05f);
			iRec14[0] = 0;
			fVec5[0] = fSlow4;
			fRec29[0] = fConst3 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow4 - fVec5[1]))) + fRec29[1]);
			float fTemp14 = std::max<float>(0.0f, std::min<float>(1.0f, fSlow5 + fRec29[0]));
			float fTemp15 = 1e+05f * fTemp14;
			float fTemp16 = std::max<float>(fTemp15, 1.0f);
			float fTemp17 = std::max<float>(1e+05f * (1.0f - fTemp14), 1.0f);
			float fTemp18 = fConst13 + 1.0f / fTemp17 + 3.7037036e-05f;
			float fTemp19 = 1.0f / fTemp18 + fTemp16;
			float fTemp20 = 1.0f / fTemp19;
			float fTemp21 = fConst16 + fTemp20 + 0.00066666666f;
			float fTemp22 = 1.0f / fTemp21 + 1e-09f;
			float fTemp23 = fConst18 + fTemp22;
			float fTemp24 = fConst16 + fTemp20;
			float fTemp25 = 1.0f / fTemp24 + 1.5e+03f;
			float fTemp26 = fConst14 + fTemp19 + 1e+03f;
			float fTemp27 = float(iRec26[1]);
			float fTemp28 = fRec25[1] + fTemp27;
			float fTemp29 = float(iRec24[1]);
			float fTemp30 = fConst19 + fTemp17;
			float fTemp31 = float(iRec23[1]);
			float fTemp32 = float(iRec21[1]);
			float fTemp33 = fRec20[1] + fTemp32;
			float fTemp34 = float(iRec22[1]);
			float fTemp35 = fConst11 * fTemp34 - 2.7e+04f * fTemp33;
			float fTemp36 = float(iRec18[1]);
			float fTemp37 = float(iRec17[1]);
			float fTemp38 = fRec16[1] + fTemp37;
			float fTemp39 = fTemp38 - fTemp36;
			float fTemp40 = fConst23 * fTemp39 + fConst21 * fTemp35;
			float fTemp41 = fConst25 * fTemp40 * fTemp17 + fConst19 * fTemp31;
			float fTemp42 = fTemp41 / fTemp30 + fTemp29;
			float fTemp43 = (fConst15 * fTemp42 + fTemp19 * fTemp28) / fTemp26;
			float fTemp44 = float(iRec27[1]);
			float fTemp45 = (fTemp44 / fTemp24 - 1.5e+03f * fTemp43) / fTemp25;
			float fTemp46 = fConst18 * (fTemp45 + fRec28[1]);
			float fTemp47 = float(iRec14[1]);
			float fTemp48 = fConst26 * fTemp47 + 5.1e+03f * fRec15[1];
			float fTemp49 = float(input0[i0]);
			fRec30[0] = fRec30[1] + fConst29 * (0.5f * fTemp49 - fRec30[1]);
			float fTemp50 = 2.0f * std::max<float>(-4.5f, std::min<float>(4.5f, 0.4950495f * (fTemp49 - (fRec30[0] + fRec30[1]))));
			float fTemp51 = fConst28 * fTemp48;
			float fTemp52 = fTemp51 - (fRec28[1] + fTemp50 + fTemp45);
			fRec15[0] = fConst30 * (fTemp47 - fRec15[1]) - (fConst18 * fTemp52 - fConst28 * fTemp48 * fTemp22 - fTemp46) / fTemp23;
			float fTemp53 = fConst31 * fTemp36;
			float fTemp54 = (fRec28[1] / fTemp21 - ((fTemp46 + fTemp22 * (fTemp51 + fTemp52)) / (fTemp21 * fTemp23) + 1e-09f * fTemp45)) / fTemp22;
			float fTemp55 = (fTemp43 + fTemp44) / (fTemp24 * fTemp25);
			float fTemp56 = ((fTemp55 - (fTemp54 + fTemp19 * (fTemp28 - fTemp42) / fTemp26)) / fTemp18 - fTemp41 * fTemp16 / fTemp30 + fTemp29 / fTemp18) / fTemp19;
			float fTemp57 = fConst32 * fTemp35;
			float fTemp58 = fConst19 * ((fTemp31 - fConst25 * fTemp40) / fTemp30);
			float fTemp59 = fTemp58 + fConst33 * (fTemp57 - fTemp39);
			fRec16[0] = fConst35 * (fConst34 * (fConst31 * (fTemp59 - fTemp56) + 4.7e+04f * fTemp38 + fTemp53) + 2.2e+04f * fRec16[1] - fConst8 * fTemp37);
			iRec17[0] = 0;
			iRec18[0] = 0;
			float fRec19 = fConst36 * (fTemp53 + 4.7e+04f * (fTemp38 + (fTemp56 - fTemp59)));
			fRec20[0] = fConst12 * (1.2e+04f * fRec20[1] - fConst10 * (fTemp58 + fConst38 * (fTemp39 - fTemp57) + fConst37 * (fTemp33 + fTemp34) - fTemp56) - fConst10 * fTemp32);
			iRec21[0] = 0;
			iRec22[0] = 0;
			iRec23[0] = 0;
			iRec24[0] = 0;
			fRec25[0] = fConst16 * (1e+03f * fRec25[1] - fConst14 * (fTemp55 - (fTemp54 + fConst15 * ((fTemp42 - fTemp28) / fTemp26))) - fConst14 * fTemp27);
			iRec26[0] = 0;
			iRec27[0] = 0;
			fRec28[0] = 0.0f;
			float fTemp60 = std::max<float>(fTemp15, 1e-09f);
			float fTemp61 = fConst39 + fTemp60;
			float fTemp62 = fConst42 + 1.0f / fTemp61;
			float fTemp63 = fConst43 + 1.0f / fTemp62;
			float fTemp64 = fConst44 + fTemp60 + 1.5e+03f;
			float fTemp65 = float(iRec40[1]);
			float fTemp66 = float(iRec37[1]);
			float fTemp67 = fConst45 * fTemp66 + 1.5e+04f * fRec39[1];
			float fTemp68 = fConst47 * fTemp67 + fTemp65;
			float fTemp69 = float(iRec33[1]);
			float fTemp70 = 1e+04f * fRec32[1] + fConst26 * fTemp69;
			float fTemp71 = fConst49 * fTemp70;
			float fTemp72 = fTemp71 + fRec35[1];
			float fTemp73 = (fConst41 * fTemp72 + fTemp61 * fTemp68) / fTemp64;
			float fTemp74 = fConst43 * fTemp73;
			float fTemp75 = fRec31[1] - (fTemp73 + fTemp50);
			fRec31[0] = (fRec31[1] / fTemp62 - fConst43 * fTemp75 + fTemp74) / fTemp63;
			float fTemp76 = fConst39 * fRec35[1];
			float fTemp77 = (fTemp74 + (fRec31[1] + fTemp75) / fTemp62) / fTemp63;
			float fTemp78 = fTemp77 + fTemp61 * (fTemp68 - fTemp72) / fTemp64;
			float fTemp79 = (fConst39 * fTemp78 + fConst49 * fTemp70 * fTemp60 - fTemp76) / fTemp61;
			fRec32[0] = fTemp79 + fConst50 * (fTemp69 - fRec32[1]);
			iRec33[0] = 0;
			float fRec34 = 0.5f * (fTemp79 + fConst51 * (fRec32[1] - fTemp69));
			fRec35[0] = 0.0f;
			float fRec36 = 0.5f * ((fTemp76 - fTemp60 * (fTemp71 - fTemp78)) / fTemp61);
			iRec37[0] = 0;
			float fTemp80 = fConst42 * (fConst53 * fTemp67 - fConst40 * (fConst41 * (fTemp68 / fTemp64) - (fConst41 * (fTemp72 / fTemp64) + fTemp77)) - fConst40 * fTemp65);
			float fRec38 = 3.3333334e-05f * (fTemp80 + fConst52 * (fRec39[1] - fTemp66));
			fRec39[0] = fTemp80 + fConst54 * (fTemp66 - fRec39[1]);
			iRec40[0] = 0;
			float fTemp81 = fRec44[1] + fRec45[1];
			float fTemp82 = std::log(std::max<float>(1.02e+05f - 1e+05f * std::max<float>(0.0f, std::min<float>(1.0f, fTemp14)), 2e+03f));
			fRec54[0] = ((iTemp0) ? fTemp82 : fRec54[1]);
			float fTemp83 = fTemp82 - fRec54[0];
			fVec6[0] = fTemp83;
			float fTemp84 = ((fTemp83 != fVec6[1]) ? fConst4 : fRec52[1] + -1.0f);
			fRec52[0] = fTemp84;
			fRec53[0] = ((fTemp84 > 0.0f) ? fRec53[1] + (fTemp82 - (fRec54[0] + fRec53[1])) / fTemp84 : fTemp83);
			float fTemp85 = std::exp(fRec54[0] + fRec53[0]);
			float fTemp86 = fTemp85 + 1.5e+04f;
			float fTemp87 = 0.00016458f * fTemp86 + 0.00123f * fTemp85;
			float fTemp88 = std::max<float>(2469948.2f * (std::sqrt(-(std::min<float>(0.0f, Faust_Klon_faustpower2_f(fTemp87) - 8.097336e-07f * fTemp85 * fTemp86))) / fTemp85), 1e-10f);
			float fTemp89 = std::tan(fConst55 * fTemp88);
			float fTemp90 = 2.024334e-07f * (fTemp85 * fTemp88 / fTemp89);
			float fTemp91 = fTemp85 + fTemp88 * (fTemp87 + fTemp90) / fTemp89 + 1.5e+04f;
			float fTemp92 = fTemp87 + 519.06f;
			float fTemp93 = fTemp85 * (1.0f - 2.024334e-07f * (Faust_Klon_faustpower2_f(fTemp88) / Faust_Klon_faustpower2_f(fTemp89)));
			fVec7[0] = fRec34 + fRec36;
			fRec55[0] = fVec7[1] - (fRec55[2] * (fTemp85 + fTemp88 * (fTemp90 - fTemp87) / fTemp89 + 1.5e+04f) + 2.0f * fRec55[1] * (fTemp93 + 1.5e+04f)) / fTemp91;
			float fTemp94 = std::max<float>(-4.5f, std::min<float>(4.5f, (fRec55[0] * (fTemp85 + fTemp88 * (fTemp90 + fTemp92) / fTemp89 + 4.37e+05f) + 2.0f * fRec55[1] * (fTemp93 + 4.37e+05f) + fRec55[2] * (fTemp85 + fTemp88 * (fTemp90 - fTemp92) / fTemp89 + 4.37e+05f)) / fTemp91));
			fVec8[0] = fTemp94;
			fRec51[0] = fVec8[1] + 0.045728147f * (fTemp94 - fRec51[1]);
			fRec50[0] = fRec51[1] + 0.3325011f * (fRec51[0] - fRec50[1]);
			fRec49[0] = fRec50[1] + 0.66320205f * (fRec50[0] - fRec49[1]);
			fRec48[0] = fRec49[1] + 0.93385583f * (fRec49[0] - fRec48[1]);
			float fTemp95 = fConst60 * (fRec48[0] - fTemp81);
			float fTemp96 = fRec45[1] + fTemp95;
			float fTemp97 = float((-fTemp96 > 0.0f) - (-fTemp96 < 0.0f));
			float fTemp98 = 38.68472f * fTemp96 * fTemp97;
			float fTemp99 = fConst62 - fTemp98;
			float fTemp100 = ((fTemp99 < -3.3414595f) ? 0.0f : ((fTemp99 < 8.0f) ? fTemp99 * (fTemp99 * (0.0013142931f * (fTemp98 - fConst62) + 0.047759313f) + 0.36319527f) + 0.63131833f : fConst62 - (fTemp98 + std::log(std::max<float>(fTemp99, 1e-10f)))));
			float fTemp101 = std::max<float>(fTemp100 - (fTemp100 - std::exp(fConst62 - (fTemp98 + fTemp100))) / (fTemp100 + 1.0f), 1e-12f);
			float fTemp102 = fTemp98 + fTemp101;
			float fTemp103 = (fTemp101 - std::exp(fConst62 - fTemp102)) / (fTemp101 + 1.0f);
			float fTemp104 = fTemp95 - fTemp97 * (fConst63 - 0.02585f * (fTemp101 - (fTemp103 + (fTemp101 - (fTemp103 + std::exp(fConst62 + (fTemp103 - fTemp102)))) / (fTemp101 + (1.0f - fTemp103)))));
			float fTemp105 = fRec48[0] - (fTemp81 + fTemp104);
			float fTemp106 = fRec44[1] + fConst66 * fTemp105;
			float fTemp107 = fRec44[1] - fTemp106;
			float fTemp108 = fConst65 * fTemp105;
			float fTemp109 = fRec45[1] - (fRec45[1] + 2.0f * fTemp104);
			float fTemp110 = fRec45[1] - fConst69 * fTemp109;
			float fTemp111 = fConst70 * fTemp107;
			float fTemp112 = fRec48[0] - (fTemp106 + fRec48[0] + fTemp111);
			fRec58[0] = fVec8[1] - 0.16808754f * (fRec58[1] - fTemp94);
			fRec57[0] = fRec58[1] + 0.5044857f * (fRec58[0] - fRec57[1]);
			fRec56[0] = fRec57[1] + 0.80378085f * (fRec57[0] - fRec56[1]);
			float fTemp113 = fConst60 * (fRec56[0] + fTemp112 - fTemp110);
			float fTemp114 = fTemp110 + fTemp113;
			float fTemp115 = float((-fTemp114 > 0.0f) - (-fTemp114 < 0.0f));
			float fTemp116 = 38.68472f * fTemp114 * fTemp115;
			float fTemp117 = fConst62 - fTemp116;
			float fTemp118 = ((fTemp117 < -3.3414595f) ? 0.0f : ((fTemp117 < 8.0f) ? fTemp117 * (fTemp117 * (0.0013142931f * (fTemp116 - fConst62) + 0.047759313f) + 0.36319527f) + 0.63131833f : fConst62 - (fTemp116 + std::log(std::max<float>(fTemp117, 1e-10f)))));
			float fTemp119 = std::max<float>(fTemp118 - (fTemp118 - std::exp(fConst62 - (fTemp116 + fTemp118))) / (fTemp118 + 1.0f), 1e-12f);
			float fTemp120 = fTemp116 + fTemp119;
			float fTemp121 = (fTemp119 - std::exp(fConst62 - fTemp120)) / (fTemp119 + 1.0f);
			float fTemp122 = fTemp113 - fTemp115 * (fConst63 - 0.02585f * (fTemp119 - (fTemp121 + (fTemp119 - (fTemp121 + std::exp(fConst62 + (fTemp121 - fTemp120)))) / (fTemp119 + (1.0f - fTemp121)))));
			float fTemp123 = fConst65 * (2.0f * fTemp105 + (fConst71 * fTemp109 + 2.0f * (fRec56[0] - (fTemp122 + fRec45[1] + fTemp108 + fRec44[1] + fTemp108)) - fConst67 * fTemp107));
			fRec44[0] = -(-(fRec44[1] + fTemp123 + fConst70 * (fTemp107 - (fTemp112 + fRec44[1] + fTemp111 + fTemp123))));
			float fTemp124 = 2.5e+05f * fTemp109;
			float fTemp125 = 5e+05f * fTemp122 - fTemp124;
			fRec45[0] = fRec45[1] + fConst72 * fTemp125;
			fRec46[0] = fConst73 * (fTemp124 + fTemp125);
			fRec47[0] = -(fConst74 * fTemp109);
			fRec43[0] = fRec46[1] + 0.19679797f * (fRec46[0] - fRec43[1]);
			fRec42[0] = fRec43[1] + 0.5731364f * (fRec43[0] - fRec42[1]);
			fRec41[0] = fRec42[1] + 0.9142937f * (fRec42[0] - fRec41[1]);
			fRec61[0] = fRec47[1] + 0.054217525f * (fRec47[0] - fRec61[1]);
			fRec60[0] = fRec61[1] + 0.38308734f * (fRec61[0] - fRec60[1]);
			fRec59[0] = fRec60[1] + 0.74872094f * (fRec60[0] - fRec59[1]);
			float fTemp126 = 0.5f * (fRec59[0] + fRec41[1]) + fRec38 + fRec19;
			fVec9[0] = fTemp126;
			fRec13[0] = fConst75 * (3.92e+05f * (fTemp126 + fVec9[1]) - fConst7 * fRec13[1]);
			float fTemp127 = std::max<float>(-13.1f, std::min<float>(11.7f, fRec13[0]));
			fVec10[0] = fTemp127;
			fRec8[0] = (fRec8[1] * (fTemp12 - fTemp11) - (fTemp127 * (fTemp11 + fTemp13) + fVec10[1] * (fTemp11 - fTemp13))) / (fTemp12 + fTemp11);
			float fTemp128 = std::max<float>(-13.1f, std::min<float>(11.7f, -fRec8[0]));
			fRec7[0] = fRec7[1] + fConst76 * (fTemp128 - fRec7[1]);
			float fTemp129 = (1e+04f * fTemp4 + 1.0f) * (fTemp128 - 0.5f * (fRec7[0] + fRec7[1])) / (1e+04f * (fTemp4 + (1.0f - fTemp4)) + 561.0f);
			float fTemp130 = fTemp129 - (fConst2 * fRec0[1] + fRec1[1]);
			fRec0[0] = fRec0[1] + fConst79 * fTemp130;
			float fTemp131 = fRec0[1] + fConst78 * fTemp130;
			fRec1[0] = fRec1[1] + fConst80 * fTemp131;
			float fTemp132 = fConst81 * fTemp130;
			float fRec2 = fTemp132;
			output0[i0] = FAUSTFLOAT(fRec2);
			iVec0[1] = iVec0[0];
			fVec1[1] = fVec1[0];
			fRec6[1] = fRec6[0];
			fRec5[1] = fRec5[0];
			fVec2[1] = fVec2[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fVec3[1] = fVec3[0];
			fRec12[1] = fRec12[0];
			fRec11[1] = fRec11[0];
			fVec4[1] = fVec4[0];
			fRec9[1] = fRec9[0];
			fRec10[1] = fRec10[0];
			iRec14[1] = iRec14[0];
			fVec5[1] = fVec5[0];
			fRec29[1] = fRec29[0];
			fRec30[1] = fRec30[0];
			fRec15[1] = fRec15[0];
			fRec16[1] = fRec16[0];
			iRec17[1] = iRec17[0];
			iRec18[1] = iRec18[0];
			fRec20[1] = fRec20[0];
			iRec21[1] = iRec21[0];
			iRec22[1] = iRec22[0];
			iRec23[1] = iRec23[0];
			iRec24[1] = iRec24[0];
			fRec25[1] = fRec25[0];
			iRec26[1] = iRec26[0];
			iRec27[1] = iRec27[0];
			fRec28[1] = fRec28[0];
			fRec31[1] = fRec31[0];
			fRec32[1] = fRec32[0];
			iRec33[1] = iRec33[0];
			fRec35[1] = fRec35[0];
			iRec37[1] = iRec37[0];
			fRec39[1] = fRec39[0];
			iRec40[1] = iRec40[0];
			fRec54[1] = fRec54[0];
			fVec6[1] = fVec6[0];
			fRec52[1] = fRec52[0];
			fRec53[1] = fRec53[0];
			fVec7[1] = fVec7[0];
			fRec55[2] = fRec55[1];
			fRec55[1] = fRec55[0];
			fVec8[1] = fVec8[0];
			fRec51[1] = fRec51[0];
			fRec50[1] = fRec50[0];
			fRec49[1] = fRec49[0];
			fRec48[1] = fRec48[0];
			fRec58[1] = fRec58[0];
			fRec57[1] = fRec57[0];
			fRec56[1] = fRec56[0];
			fRec44[1] = fRec44[0];
			fRec45[1] = fRec45[0];
			fRec46[1] = fRec46[0];
			fRec47[1] = fRec47[0];
			fRec43[1] = fRec43[0];
			fRec42[1] = fRec42[0];
			fRec41[1] = fRec41[0];
			fRec61[1] = fRec61[0];
			fRec60[1] = fRec60[0];
			fRec59[1] = fRec59[0];
			fVec9[1] = fVec9[0];
			fRec13[1] = fRec13[0];
			fVec10[1] = fVec10[0];
			fRec8[1] = fRec8[0];
			fRec7[1] = fRec7[0];
			fRec0[1] = fRec0[0];
			fRec1[1] = fRec1[0];
		}
	}

};

#endif
