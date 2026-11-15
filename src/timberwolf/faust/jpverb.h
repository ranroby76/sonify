// D:\workspace\Fanan_web_hub\src\timberwolf\faust\jpverb.h
// Written by the Faust compiler (2.70.3) from jpverb.dsp: see FaustGlue.h. Not edited by hand.
/* ------------------------------------------------------------
name: "jpverb"
Code generated with Faust 2.70.3 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -cn Faust_Jpverb -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1
------------------------------------------------------------ */

#ifndef  __Faust_Jpverb_H__
#define  __Faust_Jpverb_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Faust_Jpverb
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

const static int iFaust_JpverbSIG0Wave0[2048] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,619,631,641,643,647,653,659,661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,769,773,787,797,809,811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,929,937,941,947,953,967,971,977,983,991,997,1009,1013,1019,1021,1031,1033,1039,1049,1051,1061,1063,1069,1087,1091,1093,1097,1103,1109,1117,1123,1129,1151,1153,1163,1171,1181,1187,1193,1201,1213,1217,1223,1229,1231,1237,1249,1259,1277,1279,1283,1289,1291,1297,1301,1303,1307,1319,1321,1327,1361,1367,1373,1381,1399,1409,1423,1427,1429,1433,1439,1447,1451,1453,1459,1471,1481,1483,1487,1489,1493,1499,1511,1523,1531,1543,1549,1553,1559,1567,1571,1579,1583,1597,1601,1607,1609,1613,1619,1621,1627,1637,1657,1663,1667,1669,1693,1697,1699,1709,1721,1723,1733,1741,1747,1753,1759,1777,1783,1787,1789,1801,1811,1823,1831,1847,1861,1867,1871,1873,1877,1879,1889,1901,1907,1913,1931,1933,1949,1951,1973,1979,1987,1993,1997,1999,2003,2011,2017,2027,2029,2039,2053,2063,2069,2081,2083,2087,2089,2099,2111,2113,2129,2131,2137,2141,2143,2153,2161,2179,2203,2207,2213,2221,2237,2239,2243,2251,2267,2269,2273,2281,2287,2293,2297,2309,2311,2333,2339,2341,2347,2351,2357,2371,2377,2381,2383,2389,2393,2399,2411,2417,2423,2437,2441,2447,2459,2467,2473,2477,2503,2521,2531,2539,2543,2549,2551,2557,2579,2591,2593,2609,2617,2621,2633,2647,2657,2659,2663,2671,2677,2683,2687,2689,2693,2699,2707,2711,2713,2719,2729,2731,2741,2749,2753,2767,2777,2789,2791,2797,2801,2803,2819,2833,2837,2843,2851,2857,2861,2879,2887,2897,2903,2909,2917,2927,2939,2953,2957,2963,2969,2971,2999,3001,3011,3019,3023,3037,3041,3049,3061,3067,3079,3083,3089,3109,3119,3121,3137,3163,3167,3169,3181,3187,3191,3203,3209,3217,3221,3229,3251,3253,3257,3259,3271,3299,3301,3307,3313,3319,3323,3329,3331,3343,3347,3359,3361,3371,3373,3389,3391,3407,3413,3433,3449,3457,3461,3463,3467,3469,3491,3499,3511,3517,3527,3529,3533,3539,3541,3547,3557,3559,3571,3581,3583,3593,3607,3613,3617,3623,3631,3637,3643,3659,3671,3673,3677,3691,3697,3701,3709,3719,3727,3733,3739,3761,3767,3769,3779,3793,3797,3803,3821,3823,3833,3847,3851,3853,3863,3877,3881,3889,3907,3911,3917,3919,3923,3929,3931,3943,3947,3967,3989,4001,4003,4007,4013,4019,4021,4027,4049,4051,4057,4073,4079,4091,4093,4099,4111,4127,4129,4133,4139,4153,4157,4159,4177,4201,4211,4217,4219,4229,4231,4241,4243,4253,4259,4261,4271,4273,4283,4289,4297,4327,4337,4339,4349,4357,4363,4373,4391,4397,4409,4421,4423,4441,4447,4451,4457,4463,4481,4483,4493,4507,4513,4517,4519,4523,4547,4549,4561,4567,4583,4591,4597,4603,4621,4637,4639,4643,4649,4651,4657,4663,4673,4679,4691,4703,4721,4723,4729,4733,4751,4759,4783,4787,4789,4793,4799,4801,4813,4817,4831,4861,4871,4877,4889,4903,4909,4919,4931,4933,4937,4943,4951,4957,4967,4969,4973,4987,4993,4999,5003,5009,5011,5021,5023,5039,5051,5059,5077,5081,5087,5099,5101,5107,5113,5119,5147,5153,5167,5171,5179,5189,5197,5209,5227,5231,5233,5237,5261,5273,5279,5281,5297,5303,5309,5323,5333,5347,5351,5381,5387,5393,5399,5407,5413,5417,5419,5431,5437,5441,5443,5449,5471,5477,5479,5483,5501,5503,5507,5519,5521,5527,5531,5557,5563,5569,5573,5581,5591,5623,5639,5641,5647,5651,5653,5657,5659,5669,5683,5689,5693,5701,5711,5717,5737,5741,5743,5749,5779,5783,5791,5801,5807,5813,5821,5827,5839,5843,5849,5851,5857,5861,5867,5869,5879,5881,5897,5903,5923,5927,5939,5953,5981,5987,6007,6011,6029,6037,6043,6047,6053,6067,6073,6079,6089,6091,6101,6113,6121,6131,6133,6143,6151,6163,6173,6197,6199,6203,6211,6217,6221,6229,6247,6257,6263,6269,6271,6277,6287,6299,6301,6311,6317,6323,6329,6337,6343,6353,6359,6361,6367,6373,6379,6389,6397,6421,6427,6449,6451,6469,6473,6481,6491,6521,6529,6547,6551,6553,6563,6569,6571,6577,6581,6599,6607,6619,6637,6653,6659,6661,6673,6679,6689,6691,6701,6703,6709,6719,6733,6737,6761,6763,6779,6781,6791,6793,6803,6823,6827,6829,6833,6841,6857,6863,6869,6871,6883,6899,6907,6911,6917,6947,6949,6959,6961,6967,6971,6977,6983,6991,6997,7001,7013,7019,7027,7039,7043,7057,7069,7079,7103,7109,7121,7127,7129,7151,7159,7177,7187,7193,7207,7211,7213,7219,7229,7237,7243,7247,7253,7283,7297,7307,7309,7321,7331,7333,7349,7351,7369,7393,7411,7417,7433,7451,7457,7459,7477,7481,7487,7489,7499,7507,7517,7523,7529,7537,7541,7547,7549,7559,7561,7573,7577,7583,7589,7591,7603,7607,7621,7639,7643,7649,7669,7673,7681,7687,7691,7699,7703,7717,7723,7727,7741,7753,7757,7759,7789,7793,7817,7823,7829,7841,7853,7867,7873,7877,7879,7883,7901,7907,7919,7927,7933,7937,7949,7951,7963,7993,8009,8011,8017,8039,8053,8059,8069,8081,8087,8089,8093,8101,8111,8117,8123,8147,8161,8167,8171,8179,8191,8209,8219,8221,8231,8233,8237,8243,8263,8269,8273,8287,8291,8293,8297,8311,8317,8329,8353,8363,8369,8377,8387,8389,8419,8423,8429,8431,8443,8447,8461,8467,8501,8513,8521,8527,8537,8539,8543,8563,8573,8581,8597,8599,8609,8623,8627,8629,8641,8647,8663,8669,8677,8681,8689,8693,8699,8707,8713,8719,8731,8737,8741,8747,8753,8761,8779,8783,8803,8807,8819,8821,8831,8837,8839,8849,8861,8863,8867,8887,8893,8923,8929,8933,8941,8951,8963,8969,8971,8999,9001,9007,9011,9013,9029,9041,9043,9049,9059,9067,9091,9103,9109,9127,9133,9137,9151,9157,9161,9173,9181,9187,9199,9203,9209,9221,9227,9239,9241,9257,9277,9281,9283,9293,9311,9319,9323,9337,9341,9343,9349,9371,9377,9391,9397,9403,9413,9419,9421,9431,9433,9437,9439,9461,9463,9467,9473,9479,9491,9497,9511,9521,9533,9539,9547,9551,9587,9601,9613,9619,9623,9629,9631,9643,9649,9661,9677,9679,9689,9697,9719,9721,9733,9739,9743,9749,9767,9769,9781,9787,9791,9803,9811,9817,9829,9833,9839,9851,9857,9859,9871,9883,9887,9901,9907,9923,9929,9931,9941,9949,9967,9973,10007,10009,10037,10039,10061,10067,10069,10079,10091,10093,10099,10103,10111,10133,10139,10141,10151,10159,10163,10169,10177,10181,10193,10211,10223,10243,10247,10253,10259,10267,10271,10273,10289,10301,10303,10313,10321,10331,10333,10337,10343,10357,10369,10391,10399,10427,10429,10433,10453,10457,10459,10463,10477,10487,10499,10501,10513,10529,10531,10559,10567,10589,10597,10601,10607,10613,10627,10631,10639,10651,10657,10663,10667,10687,10691,10709,10711,10723,10729,10733,10739,10753,10771,10781,10789,10799,10831,10837,10847,10853,10859,10861,10867,10883,10889,10891,10903,10909,10937,10939,10949,10957,10973,10979,10987,10993,11003,11027,11047,11057,11059,11069,11071,11083,11087,11093,11113,11117,11119,11131,11149,11159,11161,11171,11173,11177,11197,11213,11239,11243,11251,11257,11261,11273,11279,11287,11299,11311,11317,11321,11329,11351,11353,11369,11383,11393,11399,11411,11423,11437,11443,11447,11467,11471,11483,11489,11491,11497,11503,11519,11527,11549,11551,11579,11587,11593,11597,11617,11621,11633,11657,11677,11681,11689,11699,11701,11717,11719,11731,11743,11777,11779,11783,11789,11801,11807,11813,11821,11827,11831,11833,11839,11863,11867,11887,11897,11903,11909,11923,11927,11933,11939,11941,11953,11959,11969,11971,11981,11987,12007,12011,12037,12041,12043,12049,12071,12073,12097,12101,12107,12109,12113,12119,12143,12149,12157,12161,12163,12197,12203,12211,12227,12239,12241,12251,12253,12263,12269,12277,12281,12289,12301,12323,12329,12343,12347,12373,12377,12379,12391,12401,12409,12413,12421,12433,12437,12451,12457,12473,12479,12487,12491,12497,12503,12511,12517,12527,12539,12541,12547,12553,12569,12577,12583,12589,12601,12611,12613,12619,12637,12641,12647,12653,12659,12671,12689,12697,12703,12713,12721,12739,12743,12757,12763,12781,12791,12799,12809,12821,12823,12829,12841,12853,12889,12893,12899,12907,12911,12917,12919,12923,12941,12953,12959,12967,12973,12979,12983,13001,13003,13007,13009,13033,13037,13043,13049,13063,13093,13099,13103,13109,13121,13127,13147,13151,13159,13163,13171,13177,13183,13187,13217,13219,13229,13241,13249,13259,13267,13291,13297,13309,13313,13327,13331,13337,13339,13367,13381,13397,13399,13411,13417,13421,13441,13451,13457,13463,13469,13477,13487,13499,13513,13523,13537,13553,13567,13577,13591,13597,13613,13619,13627,13633,13649,13669,13679,13681,13687,13691,13693,13697,13709,13711,13721,13723,13729,13751,13757,13759,13763,13781,13789,13799,13807,13829,13831,13841,13859,13873,13877,13879,13883,13901,13903,13907,13913,13921,13931,13933,13963,13967,13997,13999,14009,14011,14029,14033,14051,14057,14071,14081,14083,14087,14107,14143,14149,14153,14159,14173,14177,14197,14207,14221,14243,14249,14251,14281,14293,14303,14321,14323,14327,14341,14347,14369,14387,14389,14401,14407,14411,14419,14423,14431,14437,14447,14449,14461,14479,14489,14503,14519,14533,14537,14543,14549,14551,14557,14561,14563,14591,14593,14621,14627,14629,14633,14639,14653,14657,14669,14683,14699,14713,14717,14723,14731,14737,14741,14747,14753,14759,14767,14771,14779,14783,14797,14813,14821,14827,14831,14843,14851,14867,14869,14879,14887,14891,14897,14923,14929,14939,14947,14951,14957,14969,14983,15013,15017,15031,15053,15061,15073,15077,15083,15091,15101,15107,15121,15131,15137,15139,15149,15161,15173,15187,15193,15199,15217,15227,15233,15241,15259,15263,15269,15271,15277,15287,15289,15299,15307,15313,15319,15329,15331,15349,15359,15361,15373,15377,15383,15391,15401,15413,15427,15439,15443,15451,15461,15467,15473,15493,15497,15511,15527,15541,15551,15559,15569,15581,15583,15601,15607,15619,15629,15641,15643,15647,15649,15661,15667,15671,15679,15683,15727,15731,15733,15737,15739,15749,15761,15767,15773,15787,15791,15797,15803,15809,15817,15823,15859,15877,15881,15887,15889,15901,15907,15913,15919,15923,15937,15959,15971,15973,15991,16001,16007,16033,16057,16061,16063,16067,16069,16073,16087,16091,16097,16103,16111,16127,16139,16141,16183,16187,16189,16193,16217,16223,16229,16231,16249,16253,16267,16273,16301,16319,16333,16339,16349,16361,16363,16369,16381,16411,16417,16421,16427,16433,16447,16451,16453,16477,16481,16487,16493,16519,16529,16547,16553,16561,16567,16573,16603,16607,16619,16631,16633,16649,16651,16657,16661,16673,16691,16693,16699,16703,16729,16741,16747,16759,16763,16787,16811,16823,16829,16831,16843,16871,16879,16883,16889,16901,16903,16921,16927,16931,16937,16943,16963,16979,16981,16987,16993,17011,17021,17027,17029,17033,17041,17047,17053,17077,17093,17099,17107,17117,17123,17137,17159,17167,17183,17189,17191,17203,17207,17209,17231,17239,17257,17291,17293,17299,17317,17321,17327,17333,17341,17351,17359,17377,17383,17387,17389,17393,17401,17417,17419,17431,17443,17449,17467,17471,17477,17483,17489,17491,17497,17509,17519,17539,17551,17569,17573,17579,17581,17597,17599,17609,17623,17627,17657,17659,17669,17681,17683,17707,17713,17729,17737,17747,17749,17761,17783,17789,17791,17807,17827,17837,17839,17851,17863};
class Faust_JpverbSIG0 {
	
  private:
	
	int iFaust_JpverbSIG0Wave0_idx;
	
  public:
	
	int getNumInputsFaust_JpverbSIG0() {
		return 0;
	}
	int getNumOutputsFaust_JpverbSIG0() {
		return 1;
	}
	
	void instanceInitFaust_JpverbSIG0(int sample_rate) {
		iFaust_JpverbSIG0Wave0_idx = 0;
	}
	
	void fillFaust_JpverbSIG0(int count, int* table) {
		for (int i1 = 0; i1 < count; i1 = i1 + 1) {
			table[i1] = iFaust_JpverbSIG0Wave0[iFaust_JpverbSIG0Wave0_idx];
			iFaust_JpverbSIG0Wave0_idx = (1 + iFaust_JpverbSIG0Wave0_idx) % 2048;
		}
	}

};

static Faust_JpverbSIG0* newFaust_JpverbSIG0() { return (Faust_JpverbSIG0*)new Faust_JpverbSIG0(); }
static void deleteFaust_JpverbSIG0(Faust_JpverbSIG0* dsp) { delete dsp; }

static int itbl0Faust_JpverbSIG0[2048];

class Faust_Jpverb : public FaustDsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	float fVec0[2];
	int iVec1[2];
	int fSampleRate;
	float fConst1;
	float fRec0[2];
	float fConst2;
	float fConst3;
	float fConst4;
	float fConst5;
	float fConst6;
	float fConst7;
	FAUSTFLOAT fHslider1;
	float fRec24[2];
	float fRec26[2];
	float fRec30[2];
	int IOTA0;
	float fVec2[16384];
	float fVec3[2];
	float fRec29[2];
	float fRec27[2];
	float fRec32[2];
	float fVec4[16384];
	float fVec5[2];
	float fRec31[2];
	float fRec28[2];
	float fRec36[2];
	float fVec6[16384];
	float fVec7[2];
	float fRec35[2];
	float fRec33[2];
	float fRec38[2];
	float fVec8[16384];
	float fVec9[2];
	float fRec37[2];
	float fRec34[2];
	float fRec42[2];
	float fVec10[16384];
	float fVec11[2];
	float fRec41[2];
	float fRec39[2];
	float fRec44[2];
	float fVec12[16384];
	float fVec13[2];
	float fRec43[2];
	float fRec40[2];
	float fRec48[2];
	float fVec14[16384];
	float fVec15[2];
	float fRec47[2];
	float fRec45[2];
	float fRec50[2];
	float fVec16[16384];
	float fVec17[2];
	float fRec49[2];
	float fRec46[2];
	float fRec54[2];
	float fVec18[16384];
	float fVec19[2];
	float fRec53[2];
	float fRec51[2];
	float fRec56[2];
	float fVec20[16384];
	float fVec21[2];
	float fRec55[2];
	float fRec52[2];
	float fVec22[1024];
	FAUSTFLOAT fHslider2;
	float fVec23[2];
	float fRec59[2];
	float fConst8;
	float fRec57[2];
	float fRec58[2];
	FAUSTFLOAT fHslider3;
	float fVec24[2];
	float fRec60[2];
	float fVec25[16384];
	float fVec26[2];
	float fRec25[2];
	float fRec64[2];
	float fRec66[2];
	float fVec27[1024];
	float fVec28[16384];
	float fVec29[2];
	float fRec65[2];
	float fVec30[16384];
	float fVec31[2];
	float fRec63[2];
	float fRec61[2];
	float fRec68[2];
	float fVec32[16384];
	float fVec33[2];
	float fRec67[2];
	float fRec62[2];
	float fRec72[2];
	float fVec34[16384];
	float fVec35[2];
	float fRec71[2];
	float fRec69[2];
	float fRec74[2];
	float fVec36[16384];
	float fVec37[2];
	float fRec73[2];
	float fRec70[2];
	float fRec78[2];
	float fVec38[16384];
	float fVec39[2];
	float fRec77[2];
	float fRec75[2];
	float fRec80[2];
	float fVec40[16384];
	float fVec41[2];
	float fRec79[2];
	float fRec76[2];
	float fRec84[2];
	float fVec42[16384];
	float fVec43[2];
	float fRec83[2];
	float fRec81[2];
	float fRec86[2];
	float fVec44[16384];
	float fVec45[2];
	float fRec85[2];
	float fRec82[2];
	float fRec90[2];
	float fVec46[16384];
	float fVec47[2];
	float fRec89[2];
	float fRec87[2];
	float fRec92[2];
	float fVec48[16384];
	float fVec49[2];
	float fRec91[2];
	float fRec88[2];
	float fVec50[16384];
	float fVec51[16384];
	float fVec52[2];
	float fRec23[2];
	float fConst9;
	float fConst10;
	float fConst11;
	float fRec21[2];
	float fConst12;
	float fConst13;
	float fRec18[2];
	float fConst14;
	float fRec19[2];
	float fConst15;
	float fConst16;
	float fRec15[2];
	float fRec16[2];
	float fConst17;
	float fConst18;
	float fConst19;
	float fRec13[2];
	float fConst20;
	float fConst21;
	float fRec8[2];
	float fConst22;
	float fRec9[2];
	float fConst23;
	float fConst24;
	float fRec3[2];
	float fRec4[2];
	FAUSTFLOAT fHslider4;
	float fVec53[2];
	float fRec93[2];
	float fRec100[2];
	float fRec97[2];
	float fRec98[2];
	float fRec94[2];
	float fRec95[2];
	FAUSTFLOAT fHslider5;
	float fVec54[2];
	float fRec102[2];
	float fConst25;
	float fRec116[2];
	float fRec111[2];
	float fRec112[2];
	float fRec106[2];
	float fRec107[2];
	float fConst27;
	float fRec103[2];
	float fRec104[2];
	float fConst28;
	FAUSTFLOAT fHslider6;
	float fVec55[1024];
	float fVec56[2];
	FAUSTFLOAT fHslider7;
	float fVec57[2];
	float fRec119[2];
	float fRec118[2];
	FAUSTFLOAT fHslider8;
	float fVec58[2];
	float fRec120[2];
	float fRec145[2];
	float fVec59[16384];
	float fVec60[16384];
	float fVec61[2];
	float fRec144[2];
	float fRec142[2];
	float fRec139[2];
	float fRec140[2];
	float fRec136[2];
	float fRec137[2];
	float fRec134[2];
	float fRec129[2];
	float fRec130[2];
	float fRec124[2];
	float fRec125[2];
	float fRec152[2];
	float fRec149[2];
	float fRec150[2];
	float fRec146[2];
	float fRec147[2];
	float fRec167[2];
	float fRec162[2];
	float fRec163[2];
	float fRec157[2];
	float fRec158[2];
	float fRec154[2];
	float fRec155[2];
	float fVec62[1024];
	float fVec63[2];
	float fRec169[2];
	float fVec64[16384];
	float fVec65[2];
	float fRec123[2];
	float fRec121[2];
	float fRec171[2];
	float fVec66[16384];
	float fVec67[2];
	float fRec170[2];
	float fRec122[2];
	float fVec68[16384];
	float fVec69[2];
	float fRec174[2];
	float fRec172[2];
	float fVec70[16384];
	float fVec71[2];
	float fRec175[2];
	float fRec173[2];
	float fVec72[16384];
	float fVec73[2];
	float fRec178[2];
	float fRec176[2];
	float fRec180[2];
	float fVec74[16384];
	float fVec75[2];
	float fRec179[2];
	float fRec177[2];
	float fRec184[2];
	float fVec76[16384];
	float fVec77[2];
	float fRec183[2];
	float fRec181[2];
	float fVec78[16384];
	float fVec79[2];
	float fRec185[2];
	float fRec182[2];
	float fRec1[2];
	float fRec2[2];
	
 public:
	Faust_Jpverb() {}

	void metadata(Meta* m) { 
		m->declare("analyzers.lib/analyzer_tpt_df:author", "Julius O. Smith III");
		m->declare("analyzers.lib/analyzer_tpt_df:license", "MIT");
		m->declare("analyzers.lib/name", "Faust Analyzer Library");
		m->declare("analyzers.lib/version", "1.6.3");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.23.9");
		m->declare("compile_options", "-lang cpp -ct 1 -cn Faust_Jpverb -scn FaustDsp -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 1");
		m->declare("delays.lib/fdelay1a:author", "Julius O. Smith III");
		m->declare("delays.lib/fdelay4:author", "Julius O. Smith III");
		m->declare("delays.lib/fdelayltv:author", "Julius O. Smith III");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.3.4");
		m->declare("filename", "jpverb.dsp");
		m->declare("filters.lib/_tanhalf:author", "Julius O. Smith III");
		m->declare("filters.lib/_tanhalf:license", "MIT");
		m->declare("filters.lib/_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/_tpt_df:license", "MIT");
		m->declare("filters.lib/filterbank_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/filterbank_tpt_df:license", "MIT");
		m->declare("filters.lib/fir:author", "Julius O. Smith III");
		m->declare("filters.lib/fir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/fir:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/highpass:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/highpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/highpass_plus_lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass_plus_lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/highpass_plus_lowpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/highpass_plus_lowpass_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass_plus_lowpass_tpt_df:license", "MIT");
		m->declare("filters.lib/highpass_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/highpass_tpt_df:license", "MIT");
		m->declare("filters.lib/iir:author", "Julius O. Smith III");
		m->declare("filters.lib/iir:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/iir:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass0_highpass1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass0_highpass1:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "LicenseRef-STK-4.3");
		m->declare("filters.lib/lowpass_tpt_df:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass_tpt_df:license", "MIT");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/nlf2:author", "Julius O. Smith III");
		m->declare("filters.lib/nlf2:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/nlf2:license", "LicenseRef-STK-4.3");
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
		m->declare("name", "jpverb");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.9.6");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.2");
		m->declare("reverbs.lib/jpverb:author", "Julian Parker, bug fixes and minor interface changes by Till Bovermann");
		m->declare("reverbs.lib/jpverb:license", "MIT");
		m->declare("reverbs.lib/name", "Faust Reverb Library");
		m->declare("reverbs.lib/version", "1.6.2");
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
		Faust_JpverbSIG0* sig0 = newFaust_JpverbSIG0();
		sig0->instanceInitFaust_JpverbSIG0(sample_rate);
		sig0->fillFaust_JpverbSIG0(2048, itbl0Faust_JpverbSIG0);
		deleteFaust_JpverbSIG0(sig0);
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		float fConst0 = std::min<float>(1.92e+05f, std::max<float>(5e+01f, float(fSampleRate)));
		fConst1 = 1.0f - 44.1f / fConst0;
		fConst2 = std::tan(std::max<float>(0.0f, std::min<float>(1570.7964f / fConst0, 1.5676547f)));
		fConst3 = fConst2 + 0.618034f;
		fConst4 = fConst2 + 1.618034f;
		fConst5 = std::tan(std::max<float>(0.0f, std::min<float>(6283.1855f / fConst0, 1.5676547f)));
		fConst6 = fConst5 + 0.618034f;
		fConst7 = fConst5 + 1.618034f;
		fConst8 = 6.2831855f / fConst0;
		fConst9 = std::min<float>(3.4028235e+38f, 1.0f / (fConst5 + 1.0f));
		fConst10 = fConst5 * fConst9;
		fConst11 = 2.0f * fConst10;
		fConst12 = fConst5 * std::min<float>(3.4028235e+38f, 1.0f / (fConst5 * fConst7 + 1.0f));
		fConst13 = 2.0f * fConst12;
		fConst14 = 2.0f * fConst5;
		fConst15 = fConst5 * std::min<float>(3.4028235e+38f, 1.0f / (fConst5 * fConst6 + 1.0f));
		fConst16 = 2.0f * fConst15;
		fConst17 = std::min<float>(3.4028235e+38f, 1.0f / (fConst2 + 1.0f));
		fConst18 = fConst2 * fConst17;
		fConst19 = 2.0f * fConst18;
		fConst20 = fConst2 * std::min<float>(3.4028235e+38f, 1.0f / (fConst2 * fConst4 + 1.0f));
		fConst21 = 2.0f * fConst20;
		fConst22 = 2.0f * fConst2;
		fConst23 = fConst2 * std::min<float>(3.4028235e+38f, 1.0f / (fConst2 * fConst3 + 1.0f));
		fConst24 = 2.0f * fConst23;
		fConst25 = 1.618034f * fConst2;
		float fConst26 = std::min<float>(3.4028235e+38f, 1.0f / (1.618034f * fConst2 * (0.618034f * fConst2 + 1.0f) + 1.0f));
		fConst27 = 1.618034f * fConst2 * fConst26;
		fConst28 = fConst26 * (1.0f - fConst25);
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = FAUSTFLOAT(3e+01f);
		fHslider1 = FAUSTFLOAT(1.0f);
		fHslider2 = FAUSTFLOAT(2.0f);
		fHslider3 = FAUSTFLOAT(1e+01f);
		fHslider4 = FAUSTFLOAT(1.0f);
		fHslider5 = FAUSTFLOAT(1.0f);
		fHslider6 = FAUSTFLOAT(3.0f);
		fHslider7 = FAUSTFLOAT(0.0f);
		fHslider8 = FAUSTFLOAT(7e+01f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			iVec1[l1] = 0;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec0[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
			fRec24[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec26[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec30[l5] = 0.0f;
		}
		IOTA0 = 0;
		for (int l6 = 0; l6 < 16384; l6 = l6 + 1) {
			fVec2[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fVec3[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec29[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 2; l9 = l9 + 1) {
			fRec27[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec32[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 16384; l11 = l11 + 1) {
			fVec4[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fVec5[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 2; l13 = l13 + 1) {
			fRec31[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 2; l14 = l14 + 1) {
			fRec28[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec36[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 16384; l16 = l16 + 1) {
			fVec6[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fVec7[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 2; l18 = l18 + 1) {
			fRec35[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 2; l19 = l19 + 1) {
			fRec33[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec38[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 16384; l21 = l21 + 1) {
			fVec8[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fVec9[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 2; l23 = l23 + 1) {
			fRec37[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 2; l24 = l24 + 1) {
			fRec34[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 2; l25 = l25 + 1) {
			fRec42[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 16384; l26 = l26 + 1) {
			fVec10[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fVec11[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec41[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 2; l29 = l29 + 1) {
			fRec39[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 2; l30 = l30 + 1) {
			fRec44[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 16384; l31 = l31 + 1) {
			fVec12[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fVec13[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec43[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 2; l34 = l34 + 1) {
			fRec40[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 2; l35 = l35 + 1) {
			fRec48[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 16384; l36 = l36 + 1) {
			fVec14[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fVec15[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec47[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 2; l39 = l39 + 1) {
			fRec45[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 2; l40 = l40 + 1) {
			fRec50[l40] = 0.0f;
		}
		for (int l41 = 0; l41 < 16384; l41 = l41 + 1) {
			fVec16[l41] = 0.0f;
		}
		for (int l42 = 0; l42 < 2; l42 = l42 + 1) {
			fVec17[l42] = 0.0f;
		}
		for (int l43 = 0; l43 < 2; l43 = l43 + 1) {
			fRec49[l43] = 0.0f;
		}
		for (int l44 = 0; l44 < 2; l44 = l44 + 1) {
			fRec46[l44] = 0.0f;
		}
		for (int l45 = 0; l45 < 2; l45 = l45 + 1) {
			fRec54[l45] = 0.0f;
		}
		for (int l46 = 0; l46 < 16384; l46 = l46 + 1) {
			fVec18[l46] = 0.0f;
		}
		for (int l47 = 0; l47 < 2; l47 = l47 + 1) {
			fVec19[l47] = 0.0f;
		}
		for (int l48 = 0; l48 < 2; l48 = l48 + 1) {
			fRec53[l48] = 0.0f;
		}
		for (int l49 = 0; l49 < 2; l49 = l49 + 1) {
			fRec51[l49] = 0.0f;
		}
		for (int l50 = 0; l50 < 2; l50 = l50 + 1) {
			fRec56[l50] = 0.0f;
		}
		for (int l51 = 0; l51 < 16384; l51 = l51 + 1) {
			fVec20[l51] = 0.0f;
		}
		for (int l52 = 0; l52 < 2; l52 = l52 + 1) {
			fVec21[l52] = 0.0f;
		}
		for (int l53 = 0; l53 < 2; l53 = l53 + 1) {
			fRec55[l53] = 0.0f;
		}
		for (int l54 = 0; l54 < 2; l54 = l54 + 1) {
			fRec52[l54] = 0.0f;
		}
		for (int l55 = 0; l55 < 1024; l55 = l55 + 1) {
			fVec22[l55] = 0.0f;
		}
		for (int l56 = 0; l56 < 2; l56 = l56 + 1) {
			fVec23[l56] = 0.0f;
		}
		for (int l57 = 0; l57 < 2; l57 = l57 + 1) {
			fRec59[l57] = 0.0f;
		}
		for (int l58 = 0; l58 < 2; l58 = l58 + 1) {
			fRec57[l58] = 0.0f;
		}
		for (int l59 = 0; l59 < 2; l59 = l59 + 1) {
			fRec58[l59] = 0.0f;
		}
		for (int l60 = 0; l60 < 2; l60 = l60 + 1) {
			fVec24[l60] = 0.0f;
		}
		for (int l61 = 0; l61 < 2; l61 = l61 + 1) {
			fRec60[l61] = 0.0f;
		}
		for (int l62 = 0; l62 < 16384; l62 = l62 + 1) {
			fVec25[l62] = 0.0f;
		}
		for (int l63 = 0; l63 < 2; l63 = l63 + 1) {
			fVec26[l63] = 0.0f;
		}
		for (int l64 = 0; l64 < 2; l64 = l64 + 1) {
			fRec25[l64] = 0.0f;
		}
		for (int l65 = 0; l65 < 2; l65 = l65 + 1) {
			fRec64[l65] = 0.0f;
		}
		for (int l66 = 0; l66 < 2; l66 = l66 + 1) {
			fRec66[l66] = 0.0f;
		}
		for (int l67 = 0; l67 < 1024; l67 = l67 + 1) {
			fVec27[l67] = 0.0f;
		}
		for (int l68 = 0; l68 < 16384; l68 = l68 + 1) {
			fVec28[l68] = 0.0f;
		}
		for (int l69 = 0; l69 < 2; l69 = l69 + 1) {
			fVec29[l69] = 0.0f;
		}
		for (int l70 = 0; l70 < 2; l70 = l70 + 1) {
			fRec65[l70] = 0.0f;
		}
		for (int l71 = 0; l71 < 16384; l71 = l71 + 1) {
			fVec30[l71] = 0.0f;
		}
		for (int l72 = 0; l72 < 2; l72 = l72 + 1) {
			fVec31[l72] = 0.0f;
		}
		for (int l73 = 0; l73 < 2; l73 = l73 + 1) {
			fRec63[l73] = 0.0f;
		}
		for (int l74 = 0; l74 < 2; l74 = l74 + 1) {
			fRec61[l74] = 0.0f;
		}
		for (int l75 = 0; l75 < 2; l75 = l75 + 1) {
			fRec68[l75] = 0.0f;
		}
		for (int l76 = 0; l76 < 16384; l76 = l76 + 1) {
			fVec32[l76] = 0.0f;
		}
		for (int l77 = 0; l77 < 2; l77 = l77 + 1) {
			fVec33[l77] = 0.0f;
		}
		for (int l78 = 0; l78 < 2; l78 = l78 + 1) {
			fRec67[l78] = 0.0f;
		}
		for (int l79 = 0; l79 < 2; l79 = l79 + 1) {
			fRec62[l79] = 0.0f;
		}
		for (int l80 = 0; l80 < 2; l80 = l80 + 1) {
			fRec72[l80] = 0.0f;
		}
		for (int l81 = 0; l81 < 16384; l81 = l81 + 1) {
			fVec34[l81] = 0.0f;
		}
		for (int l82 = 0; l82 < 2; l82 = l82 + 1) {
			fVec35[l82] = 0.0f;
		}
		for (int l83 = 0; l83 < 2; l83 = l83 + 1) {
			fRec71[l83] = 0.0f;
		}
		for (int l84 = 0; l84 < 2; l84 = l84 + 1) {
			fRec69[l84] = 0.0f;
		}
		for (int l85 = 0; l85 < 2; l85 = l85 + 1) {
			fRec74[l85] = 0.0f;
		}
		for (int l86 = 0; l86 < 16384; l86 = l86 + 1) {
			fVec36[l86] = 0.0f;
		}
		for (int l87 = 0; l87 < 2; l87 = l87 + 1) {
			fVec37[l87] = 0.0f;
		}
		for (int l88 = 0; l88 < 2; l88 = l88 + 1) {
			fRec73[l88] = 0.0f;
		}
		for (int l89 = 0; l89 < 2; l89 = l89 + 1) {
			fRec70[l89] = 0.0f;
		}
		for (int l90 = 0; l90 < 2; l90 = l90 + 1) {
			fRec78[l90] = 0.0f;
		}
		for (int l91 = 0; l91 < 16384; l91 = l91 + 1) {
			fVec38[l91] = 0.0f;
		}
		for (int l92 = 0; l92 < 2; l92 = l92 + 1) {
			fVec39[l92] = 0.0f;
		}
		for (int l93 = 0; l93 < 2; l93 = l93 + 1) {
			fRec77[l93] = 0.0f;
		}
		for (int l94 = 0; l94 < 2; l94 = l94 + 1) {
			fRec75[l94] = 0.0f;
		}
		for (int l95 = 0; l95 < 2; l95 = l95 + 1) {
			fRec80[l95] = 0.0f;
		}
		for (int l96 = 0; l96 < 16384; l96 = l96 + 1) {
			fVec40[l96] = 0.0f;
		}
		for (int l97 = 0; l97 < 2; l97 = l97 + 1) {
			fVec41[l97] = 0.0f;
		}
		for (int l98 = 0; l98 < 2; l98 = l98 + 1) {
			fRec79[l98] = 0.0f;
		}
		for (int l99 = 0; l99 < 2; l99 = l99 + 1) {
			fRec76[l99] = 0.0f;
		}
		for (int l100 = 0; l100 < 2; l100 = l100 + 1) {
			fRec84[l100] = 0.0f;
		}
		for (int l101 = 0; l101 < 16384; l101 = l101 + 1) {
			fVec42[l101] = 0.0f;
		}
		for (int l102 = 0; l102 < 2; l102 = l102 + 1) {
			fVec43[l102] = 0.0f;
		}
		for (int l103 = 0; l103 < 2; l103 = l103 + 1) {
			fRec83[l103] = 0.0f;
		}
		for (int l104 = 0; l104 < 2; l104 = l104 + 1) {
			fRec81[l104] = 0.0f;
		}
		for (int l105 = 0; l105 < 2; l105 = l105 + 1) {
			fRec86[l105] = 0.0f;
		}
		for (int l106 = 0; l106 < 16384; l106 = l106 + 1) {
			fVec44[l106] = 0.0f;
		}
		for (int l107 = 0; l107 < 2; l107 = l107 + 1) {
			fVec45[l107] = 0.0f;
		}
		for (int l108 = 0; l108 < 2; l108 = l108 + 1) {
			fRec85[l108] = 0.0f;
		}
		for (int l109 = 0; l109 < 2; l109 = l109 + 1) {
			fRec82[l109] = 0.0f;
		}
		for (int l110 = 0; l110 < 2; l110 = l110 + 1) {
			fRec90[l110] = 0.0f;
		}
		for (int l111 = 0; l111 < 16384; l111 = l111 + 1) {
			fVec46[l111] = 0.0f;
		}
		for (int l112 = 0; l112 < 2; l112 = l112 + 1) {
			fVec47[l112] = 0.0f;
		}
		for (int l113 = 0; l113 < 2; l113 = l113 + 1) {
			fRec89[l113] = 0.0f;
		}
		for (int l114 = 0; l114 < 2; l114 = l114 + 1) {
			fRec87[l114] = 0.0f;
		}
		for (int l115 = 0; l115 < 2; l115 = l115 + 1) {
			fRec92[l115] = 0.0f;
		}
		for (int l116 = 0; l116 < 16384; l116 = l116 + 1) {
			fVec48[l116] = 0.0f;
		}
		for (int l117 = 0; l117 < 2; l117 = l117 + 1) {
			fVec49[l117] = 0.0f;
		}
		for (int l118 = 0; l118 < 2; l118 = l118 + 1) {
			fRec91[l118] = 0.0f;
		}
		for (int l119 = 0; l119 < 2; l119 = l119 + 1) {
			fRec88[l119] = 0.0f;
		}
		for (int l120 = 0; l120 < 16384; l120 = l120 + 1) {
			fVec50[l120] = 0.0f;
		}
		for (int l121 = 0; l121 < 16384; l121 = l121 + 1) {
			fVec51[l121] = 0.0f;
		}
		for (int l122 = 0; l122 < 2; l122 = l122 + 1) {
			fVec52[l122] = 0.0f;
		}
		for (int l123 = 0; l123 < 2; l123 = l123 + 1) {
			fRec23[l123] = 0.0f;
		}
		for (int l124 = 0; l124 < 2; l124 = l124 + 1) {
			fRec21[l124] = 0.0f;
		}
		for (int l125 = 0; l125 < 2; l125 = l125 + 1) {
			fRec18[l125] = 0.0f;
		}
		for (int l126 = 0; l126 < 2; l126 = l126 + 1) {
			fRec19[l126] = 0.0f;
		}
		for (int l127 = 0; l127 < 2; l127 = l127 + 1) {
			fRec15[l127] = 0.0f;
		}
		for (int l128 = 0; l128 < 2; l128 = l128 + 1) {
			fRec16[l128] = 0.0f;
		}
		for (int l129 = 0; l129 < 2; l129 = l129 + 1) {
			fRec13[l129] = 0.0f;
		}
		for (int l130 = 0; l130 < 2; l130 = l130 + 1) {
			fRec8[l130] = 0.0f;
		}
		for (int l131 = 0; l131 < 2; l131 = l131 + 1) {
			fRec9[l131] = 0.0f;
		}
		for (int l132 = 0; l132 < 2; l132 = l132 + 1) {
			fRec3[l132] = 0.0f;
		}
		for (int l133 = 0; l133 < 2; l133 = l133 + 1) {
			fRec4[l133] = 0.0f;
		}
		for (int l134 = 0; l134 < 2; l134 = l134 + 1) {
			fVec53[l134] = 0.0f;
		}
		for (int l135 = 0; l135 < 2; l135 = l135 + 1) {
			fRec93[l135] = 0.0f;
		}
		for (int l136 = 0; l136 < 2; l136 = l136 + 1) {
			fRec100[l136] = 0.0f;
		}
		for (int l137 = 0; l137 < 2; l137 = l137 + 1) {
			fRec97[l137] = 0.0f;
		}
		for (int l138 = 0; l138 < 2; l138 = l138 + 1) {
			fRec98[l138] = 0.0f;
		}
		for (int l139 = 0; l139 < 2; l139 = l139 + 1) {
			fRec94[l139] = 0.0f;
		}
		for (int l140 = 0; l140 < 2; l140 = l140 + 1) {
			fRec95[l140] = 0.0f;
		}
		for (int l141 = 0; l141 < 2; l141 = l141 + 1) {
			fVec54[l141] = 0.0f;
		}
		for (int l142 = 0; l142 < 2; l142 = l142 + 1) {
			fRec102[l142] = 0.0f;
		}
		for (int l143 = 0; l143 < 2; l143 = l143 + 1) {
			fRec116[l143] = 0.0f;
		}
		for (int l144 = 0; l144 < 2; l144 = l144 + 1) {
			fRec111[l144] = 0.0f;
		}
		for (int l145 = 0; l145 < 2; l145 = l145 + 1) {
			fRec112[l145] = 0.0f;
		}
		for (int l146 = 0; l146 < 2; l146 = l146 + 1) {
			fRec106[l146] = 0.0f;
		}
		for (int l147 = 0; l147 < 2; l147 = l147 + 1) {
			fRec107[l147] = 0.0f;
		}
		for (int l148 = 0; l148 < 2; l148 = l148 + 1) {
			fRec103[l148] = 0.0f;
		}
		for (int l149 = 0; l149 < 2; l149 = l149 + 1) {
			fRec104[l149] = 0.0f;
		}
		for (int l150 = 0; l150 < 1024; l150 = l150 + 1) {
			fVec55[l150] = 0.0f;
		}
		for (int l151 = 0; l151 < 2; l151 = l151 + 1) {
			fVec56[l151] = 0.0f;
		}
		for (int l152 = 0; l152 < 2; l152 = l152 + 1) {
			fVec57[l152] = 0.0f;
		}
		for (int l153 = 0; l153 < 2; l153 = l153 + 1) {
			fRec119[l153] = 0.0f;
		}
		for (int l154 = 0; l154 < 2; l154 = l154 + 1) {
			fRec118[l154] = 0.0f;
		}
		for (int l155 = 0; l155 < 2; l155 = l155 + 1) {
			fVec58[l155] = 0.0f;
		}
		for (int l156 = 0; l156 < 2; l156 = l156 + 1) {
			fRec120[l156] = 0.0f;
		}
		for (int l157 = 0; l157 < 2; l157 = l157 + 1) {
			fRec145[l157] = 0.0f;
		}
		for (int l158 = 0; l158 < 16384; l158 = l158 + 1) {
			fVec59[l158] = 0.0f;
		}
		for (int l159 = 0; l159 < 16384; l159 = l159 + 1) {
			fVec60[l159] = 0.0f;
		}
		for (int l160 = 0; l160 < 2; l160 = l160 + 1) {
			fVec61[l160] = 0.0f;
		}
		for (int l161 = 0; l161 < 2; l161 = l161 + 1) {
			fRec144[l161] = 0.0f;
		}
		for (int l162 = 0; l162 < 2; l162 = l162 + 1) {
			fRec142[l162] = 0.0f;
		}
		for (int l163 = 0; l163 < 2; l163 = l163 + 1) {
			fRec139[l163] = 0.0f;
		}
		for (int l164 = 0; l164 < 2; l164 = l164 + 1) {
			fRec140[l164] = 0.0f;
		}
		for (int l165 = 0; l165 < 2; l165 = l165 + 1) {
			fRec136[l165] = 0.0f;
		}
		for (int l166 = 0; l166 < 2; l166 = l166 + 1) {
			fRec137[l166] = 0.0f;
		}
		for (int l167 = 0; l167 < 2; l167 = l167 + 1) {
			fRec134[l167] = 0.0f;
		}
		for (int l168 = 0; l168 < 2; l168 = l168 + 1) {
			fRec129[l168] = 0.0f;
		}
		for (int l169 = 0; l169 < 2; l169 = l169 + 1) {
			fRec130[l169] = 0.0f;
		}
		for (int l170 = 0; l170 < 2; l170 = l170 + 1) {
			fRec124[l170] = 0.0f;
		}
		for (int l171 = 0; l171 < 2; l171 = l171 + 1) {
			fRec125[l171] = 0.0f;
		}
		for (int l172 = 0; l172 < 2; l172 = l172 + 1) {
			fRec152[l172] = 0.0f;
		}
		for (int l173 = 0; l173 < 2; l173 = l173 + 1) {
			fRec149[l173] = 0.0f;
		}
		for (int l174 = 0; l174 < 2; l174 = l174 + 1) {
			fRec150[l174] = 0.0f;
		}
		for (int l175 = 0; l175 < 2; l175 = l175 + 1) {
			fRec146[l175] = 0.0f;
		}
		for (int l176 = 0; l176 < 2; l176 = l176 + 1) {
			fRec147[l176] = 0.0f;
		}
		for (int l177 = 0; l177 < 2; l177 = l177 + 1) {
			fRec167[l177] = 0.0f;
		}
		for (int l178 = 0; l178 < 2; l178 = l178 + 1) {
			fRec162[l178] = 0.0f;
		}
		for (int l179 = 0; l179 < 2; l179 = l179 + 1) {
			fRec163[l179] = 0.0f;
		}
		for (int l180 = 0; l180 < 2; l180 = l180 + 1) {
			fRec157[l180] = 0.0f;
		}
		for (int l181 = 0; l181 < 2; l181 = l181 + 1) {
			fRec158[l181] = 0.0f;
		}
		for (int l182 = 0; l182 < 2; l182 = l182 + 1) {
			fRec154[l182] = 0.0f;
		}
		for (int l183 = 0; l183 < 2; l183 = l183 + 1) {
			fRec155[l183] = 0.0f;
		}
		for (int l184 = 0; l184 < 1024; l184 = l184 + 1) {
			fVec62[l184] = 0.0f;
		}
		for (int l185 = 0; l185 < 2; l185 = l185 + 1) {
			fVec63[l185] = 0.0f;
		}
		for (int l186 = 0; l186 < 2; l186 = l186 + 1) {
			fRec169[l186] = 0.0f;
		}
		for (int l187 = 0; l187 < 16384; l187 = l187 + 1) {
			fVec64[l187] = 0.0f;
		}
		for (int l188 = 0; l188 < 2; l188 = l188 + 1) {
			fVec65[l188] = 0.0f;
		}
		for (int l189 = 0; l189 < 2; l189 = l189 + 1) {
			fRec123[l189] = 0.0f;
		}
		for (int l190 = 0; l190 < 2; l190 = l190 + 1) {
			fRec121[l190] = 0.0f;
		}
		for (int l191 = 0; l191 < 2; l191 = l191 + 1) {
			fRec171[l191] = 0.0f;
		}
		for (int l192 = 0; l192 < 16384; l192 = l192 + 1) {
			fVec66[l192] = 0.0f;
		}
		for (int l193 = 0; l193 < 2; l193 = l193 + 1) {
			fVec67[l193] = 0.0f;
		}
		for (int l194 = 0; l194 < 2; l194 = l194 + 1) {
			fRec170[l194] = 0.0f;
		}
		for (int l195 = 0; l195 < 2; l195 = l195 + 1) {
			fRec122[l195] = 0.0f;
		}
		for (int l196 = 0; l196 < 16384; l196 = l196 + 1) {
			fVec68[l196] = 0.0f;
		}
		for (int l197 = 0; l197 < 2; l197 = l197 + 1) {
			fVec69[l197] = 0.0f;
		}
		for (int l198 = 0; l198 < 2; l198 = l198 + 1) {
			fRec174[l198] = 0.0f;
		}
		for (int l199 = 0; l199 < 2; l199 = l199 + 1) {
			fRec172[l199] = 0.0f;
		}
		for (int l200 = 0; l200 < 16384; l200 = l200 + 1) {
			fVec70[l200] = 0.0f;
		}
		for (int l201 = 0; l201 < 2; l201 = l201 + 1) {
			fVec71[l201] = 0.0f;
		}
		for (int l202 = 0; l202 < 2; l202 = l202 + 1) {
			fRec175[l202] = 0.0f;
		}
		for (int l203 = 0; l203 < 2; l203 = l203 + 1) {
			fRec173[l203] = 0.0f;
		}
		for (int l204 = 0; l204 < 16384; l204 = l204 + 1) {
			fVec72[l204] = 0.0f;
		}
		for (int l205 = 0; l205 < 2; l205 = l205 + 1) {
			fVec73[l205] = 0.0f;
		}
		for (int l206 = 0; l206 < 2; l206 = l206 + 1) {
			fRec178[l206] = 0.0f;
		}
		for (int l207 = 0; l207 < 2; l207 = l207 + 1) {
			fRec176[l207] = 0.0f;
		}
		for (int l208 = 0; l208 < 2; l208 = l208 + 1) {
			fRec180[l208] = 0.0f;
		}
		for (int l209 = 0; l209 < 16384; l209 = l209 + 1) {
			fVec74[l209] = 0.0f;
		}
		for (int l210 = 0; l210 < 2; l210 = l210 + 1) {
			fVec75[l210] = 0.0f;
		}
		for (int l211 = 0; l211 < 2; l211 = l211 + 1) {
			fRec179[l211] = 0.0f;
		}
		for (int l212 = 0; l212 < 2; l212 = l212 + 1) {
			fRec177[l212] = 0.0f;
		}
		for (int l213 = 0; l213 < 2; l213 = l213 + 1) {
			fRec184[l213] = 0.0f;
		}
		for (int l214 = 0; l214 < 16384; l214 = l214 + 1) {
			fVec76[l214] = 0.0f;
		}
		for (int l215 = 0; l215 < 2; l215 = l215 + 1) {
			fVec77[l215] = 0.0f;
		}
		for (int l216 = 0; l216 < 2; l216 = l216 + 1) {
			fRec183[l216] = 0.0f;
		}
		for (int l217 = 0; l217 < 2; l217 = l217 + 1) {
			fRec181[l217] = 0.0f;
		}
		for (int l218 = 0; l218 < 16384; l218 = l218 + 1) {
			fVec78[l218] = 0.0f;
		}
		for (int l219 = 0; l219 < 2; l219 = l219 + 1) {
			fVec79[l219] = 0.0f;
		}
		for (int l220 = 0; l220 < 2; l220 = l220 + 1) {
			fRec185[l220] = 0.0f;
		}
		for (int l221 = 0; l221 < 2; l221 = l221 + 1) {
			fRec182[l221] = 0.0f;
		}
		for (int l222 = 0; l222 < 2; l222 = l222 + 1) {
			fRec1[l222] = 0.0f;
		}
		for (int l223 = 0; l223 < 2; l223 = l223 + 1) {
			fRec2[l223] = 0.0f;
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
	
	virtual Faust_Jpverb* clone() {
		return new Faust_Jpverb();
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("jpverb");
		ui_interface->addHorizontalSlider("Damp", &fHslider7, FAUSTFLOAT(0.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(99.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Decay", &fHslider6, FAUSTFLOAT(3.0f), FAUSTFLOAT(0.1f), FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Diffusion", &fHslider8, FAUSTFLOAT(7e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(99.0f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("High", &fHslider5, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.1f), FAUSTFLOAT(2.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Low", &fHslider4, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.1f), FAUSTFLOAT(2.0f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Mix", &fHslider0, FAUSTFLOAT(3e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mod Depth", &fHslider3, FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->addHorizontalSlider("Mod Rate", &fHslider2, FAUSTFLOAT(2.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
		ui_interface->addHorizontalSlider("Size", &fHslider1, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.5f), FAUSTFLOAT(3.0f), FAUSTFLOAT(0.01f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = float(fHslider0);
		float fSlow1 = 0.01f * fSlow0;
		float fSlow2 = float(fHslider1);
		int iSlow3 = itbl0Faust_JpverbSIG0[int(134.0f * fSlow2)];
		float fSlow4 = 0.005f * float(iSlow3);
		int iSlow5 = itbl0Faust_JpverbSIG0[int(54.0f * fSlow2)];
		float fSlow6 = 0.005f * float(iSlow5);
		int iSlow7 = itbl0Faust_JpverbSIG0[int(1e+01f * fSlow2)];
		float fSlow8 = 0.0001f * float(iSlow7);
		int iSlow9 = itbl0Faust_JpverbSIG0[int(1.1e+02f * fSlow2)];
		float fSlow10 = 0.0001f * float(iSlow9);
		int iSlow11 = itbl0Faust_JpverbSIG0[int(4e+01f * fSlow2)];
		float fSlow12 = 0.0001f * float(iSlow11);
		int iSlow13 = itbl0Faust_JpverbSIG0[int(1.4e+02f * fSlow2)];
		float fSlow14 = 0.0001f * float(iSlow13);
		int iSlow15 = itbl0Faust_JpverbSIG0[int(7e+01f * fSlow2)];
		float fSlow16 = 0.0001f * float(iSlow15);
		int iSlow17 = itbl0Faust_JpverbSIG0[int(1.7e+02f * fSlow2)];
		float fSlow18 = 0.0001f * float(iSlow17);
		int iSlow19 = itbl0Faust_JpverbSIG0[int(1e+02f * fSlow2)];
		float fSlow20 = 0.0001f * float(iSlow19);
		int iSlow21 = itbl0Faust_JpverbSIG0[int(2e+02f * fSlow2)];
		float fSlow22 = 0.0001f * float(iSlow21);
		int iSlow23 = itbl0Faust_JpverbSIG0[int(1.3e+02f * fSlow2)];
		float fSlow24 = 0.0001f * float(iSlow23);
		int iSlow25 = itbl0Faust_JpverbSIG0[int(2.3e+02f * fSlow2)];
		float fSlow26 = 0.0001f * float(iSlow25);
		float fSlow27 = float(fHslider2);
		float fSlow28 = float(fHslider3);
		float fSlow29 = 0.01f * fSlow28;
		int iSlow30 = itbl0Faust_JpverbSIG0[int(125.0f * fSlow2)];
		float fSlow31 = 0.0001f * float(iSlow30);
		int iSlow32 = itbl0Faust_JpverbSIG0[int(204.0f * fSlow2)];
		float fSlow33 = 0.005f * float(iSlow32);
		int iSlow34 = itbl0Faust_JpverbSIG0[int(25.0f * fSlow2)];
		float fSlow35 = 0.0001f * float(iSlow34);
		int iSlow36 = itbl0Faust_JpverbSIG0[int(155.0f * fSlow2)];
		float fSlow37 = 0.0001f * float(iSlow36);
		int iSlow38 = itbl0Faust_JpverbSIG0[int(55.0f * fSlow2)];
		float fSlow39 = 0.0001f * float(iSlow38);
		int iSlow40 = itbl0Faust_JpverbSIG0[int(185.0f * fSlow2)];
		float fSlow41 = 0.0001f * float(iSlow40);
		int iSlow42 = itbl0Faust_JpverbSIG0[int(85.0f * fSlow2)];
		float fSlow43 = 0.0001f * float(iSlow42);
		int iSlow44 = itbl0Faust_JpverbSIG0[int(215.0f * fSlow2)];
		float fSlow45 = 0.0001f * float(iSlow44);
		int iSlow46 = itbl0Faust_JpverbSIG0[int(115.0f * fSlow2)];
		float fSlow47 = 0.0001f * float(iSlow46);
		int iSlow48 = itbl0Faust_JpverbSIG0[int(245.0f * fSlow2)];
		float fSlow49 = 0.0001f * float(iSlow48);
		int iSlow50 = itbl0Faust_JpverbSIG0[int(145.0f * fSlow2)];
		float fSlow51 = 0.0001f * float(iSlow50);
		float fSlow52 = float(fHslider4);
		float fSlow53 = float(fHslider5);
		float fSlow54 = std::pow(1e+01f, -(0.51f * ((1.25f * fSlow2 + -0.25f) / float(fHslider6))));
		float fSlow55 = float(fHslider7);
		float fSlow56 = 0.01f * fSlow55;
		float fSlow57 = float(fHslider8);
		float fSlow58 = 0.01f * fSlow57;
		int iSlow59 = itbl0Faust_JpverbSIG0[int(34.0f * fSlow2)];
		float fSlow60 = 0.005f * float(iSlow59);
		int iSlow61 = itbl0Faust_JpverbSIG0[int(2.4e+02f * fSlow2)];
		float fSlow62 = 0.0001f * float(iSlow61);
		int iSlow63 = itbl0Faust_JpverbSIG0[int(1.9e+02f * fSlow2)];
		float fSlow64 = 0.0001f * float(iSlow63);
		int iSlow65 = itbl0Faust_JpverbSIG0[int(175.0f * fSlow2)];
		float fSlow66 = 0.0001f * float(iSlow65);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			fVec0[0] = fSlow0;
			iVec1[0] = 1;
			float fTemp0 = fConst1 * (fRec0[1] + std::min<float>(3.4028235e+38f, -(0.01f * (fSlow0 - fVec0[1]))));
			fRec0[0] = ((std::fabs(fTemp0) > 1.1754944e-38f) ? fTemp0 : 0.0f);
			float fTemp1 = fSlow1 + fRec0[0];
			int iTemp2 = 1 - iVec1[1];
			float fTemp3 = 0.995f * (fRec24[1] + float(iTemp2 * iSlow3)) + fSlow4;
			fRec24[0] = ((std::fabs(fTemp3) > 1.1754944e-38f) ? fTemp3 : 0.0f);
			float fTemp4 = fRec24[0] + -1.49999f;
			float fTemp5 = std::floor(fTemp4);
			float fTemp6 = 0.995f * (fRec26[1] + float(iTemp2 * iSlow5)) + fSlow6;
			fRec26[0] = ((std::fabs(fTemp6) > 1.1754944e-38f) ? fTemp6 : 0.0f);
			float fTemp7 = fRec26[0] + -1.49999f;
			float fTemp8 = std::floor(fTemp7);
			float fTemp9 = 0.9999f * (fRec30[1] + float(iTemp2 * iSlow7)) + fSlow8;
			fRec30[0] = ((std::fabs(fTemp9) > 1.1754944e-38f) ? fTemp9 : 0.0f);
			float fTemp10 = fRec30[0] + -1.49999f;
			float fTemp11 = std::floor(fTemp10);
			float fTemp12 = 0.7602446f * fRec2[1];
			float fTemp13 = 0.6496369f * fRec28[1];
			float fTemp14 = 0.7602446f * fRec1[1] - 0.6496369f * fRec27[1];
			fVec2[IOTA0 & 16383] = 0.70710677f * fTemp14 + 0.70710677f * (fTemp13 - fTemp12);
			float fTemp15 = fVec2[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp10)))))) & 16383];
			fVec3[0] = fTemp15;
			float fTemp16 = fVec3[1] - (fTemp11 + (2.0f - fRec30[0])) * (fRec29[1] - fTemp15) / (fRec30[0] - fTemp11);
			fRec29[0] = ((std::fabs(fTemp16) > 1.1754944e-38f) ? fTemp16 : 0.0f);
			fRec27[0] = ((std::fabs(fRec29[0]) > 1.1754944e-38f) ? fRec29[0] : 0.0f);
			float fTemp17 = 0.9999f * (fRec32[1] + float(iTemp2 * iSlow9)) + fSlow10;
			fRec32[0] = ((std::fabs(fTemp17) > 1.1754944e-38f) ? fTemp17 : 0.0f);
			float fTemp18 = fRec32[0] + -1.49999f;
			float fTemp19 = std::floor(fTemp18);
			fVec4[IOTA0 & 16383] = 0.70710677f * fTemp14 + 0.70710677f * (fTemp12 - fTemp13);
			float fTemp20 = fVec4[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp18)))))) & 16383];
			fVec5[0] = fTemp20;
			float fTemp21 = fVec5[1] - (fTemp19 + (2.0f - fRec32[0])) * (fRec31[1] - fTemp20) / (fRec32[0] - fTemp19);
			fRec31[0] = ((std::fabs(fTemp21) > 1.1754944e-38f) ? fTemp21 : 0.0f);
			fRec28[0] = ((std::fabs(fRec31[0]) > 1.1754944e-38f) ? fRec31[0] : 0.0f);
			float fTemp22 = 0.7602446f * fRec27[1] + 0.6496369f * fRec1[1];
			float fTemp23 = 0.9999f * (fRec36[1] + float(iTemp2 * iSlow11)) + fSlow12;
			fRec36[0] = ((std::fabs(fTemp23) > 1.1754944e-38f) ? fTemp23 : 0.0f);
			float fTemp24 = fRec36[0] + -1.49999f;
			float fTemp25 = std::floor(fTemp24);
			float fTemp26 = 0.7602446f * fRec28[1] + 0.6496369f * fRec2[1];
			float fTemp27 = 0.7602446f * fTemp26;
			float fTemp28 = 0.6496369f * fRec34[1];
			float fTemp29 = 0.7602446f * fTemp22 - 0.6496369f * fRec33[1];
			fVec6[IOTA0 & 16383] = 0.70710677f * fTemp29 + 0.70710677f * (fTemp28 - fTemp27);
			float fTemp30 = fVec6[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp24)))))) & 16383];
			fVec7[0] = fTemp30;
			float fTemp31 = fVec7[1] - (fTemp25 + (2.0f - fRec36[0])) * (fRec35[1] - fTemp30) / (fRec36[0] - fTemp25);
			fRec35[0] = ((std::fabs(fTemp31) > 1.1754944e-38f) ? fTemp31 : 0.0f);
			fRec33[0] = ((std::fabs(fRec35[0]) > 1.1754944e-38f) ? fRec35[0] : 0.0f);
			float fTemp32 = 0.9999f * (fRec38[1] + float(iTemp2 * iSlow13)) + fSlow14;
			fRec38[0] = ((std::fabs(fTemp32) > 1.1754944e-38f) ? fTemp32 : 0.0f);
			float fTemp33 = fRec38[0] + -1.49999f;
			float fTemp34 = std::floor(fTemp33);
			fVec8[IOTA0 & 16383] = 0.70710677f * fTemp29 + 0.70710677f * (fTemp27 - fTemp28);
			float fTemp35 = fVec8[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp33)))))) & 16383];
			fVec9[0] = fTemp35;
			float fTemp36 = fVec9[1] - (fTemp34 + (2.0f - fRec38[0])) * (fRec37[1] - fTemp35) / (fRec38[0] - fTemp34);
			fRec37[0] = ((std::fabs(fTemp36) > 1.1754944e-38f) ? fTemp36 : 0.0f);
			fRec34[0] = ((std::fabs(fRec37[0]) > 1.1754944e-38f) ? fRec37[0] : 0.0f);
			float fTemp37 = 0.7602446f * fRec33[1] + 0.6496369f * fTemp22;
			float fTemp38 = 0.9999f * (fRec42[1] + float(iTemp2 * iSlow15)) + fSlow16;
			fRec42[0] = ((std::fabs(fTemp38) > 1.1754944e-38f) ? fTemp38 : 0.0f);
			float fTemp39 = fRec42[0] + -1.49999f;
			float fTemp40 = std::floor(fTemp39);
			float fTemp41 = 0.7602446f * fRec34[1] + 0.6496369f * fTemp26;
			float fTemp42 = 0.7602446f * fTemp41;
			float fTemp43 = 0.6496369f * fRec40[1];
			float fTemp44 = 0.7602446f * fTemp37 - 0.6496369f * fRec39[1];
			fVec10[IOTA0 & 16383] = 0.70710677f * fTemp44 + 0.70710677f * (fTemp43 - fTemp42);
			float fTemp45 = fVec10[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp39)))))) & 16383];
			fVec11[0] = fTemp45;
			float fTemp46 = fVec11[1] - (fTemp40 + (2.0f - fRec42[0])) * (fRec41[1] - fTemp45) / (fRec42[0] - fTemp40);
			fRec41[0] = ((std::fabs(fTemp46) > 1.1754944e-38f) ? fTemp46 : 0.0f);
			fRec39[0] = ((std::fabs(fRec41[0]) > 1.1754944e-38f) ? fRec41[0] : 0.0f);
			float fTemp47 = 0.9999f * (fRec44[1] + float(iTemp2 * iSlow17)) + fSlow18;
			fRec44[0] = ((std::fabs(fTemp47) > 1.1754944e-38f) ? fTemp47 : 0.0f);
			float fTemp48 = fRec44[0] + -1.49999f;
			float fTemp49 = std::floor(fTemp48);
			fVec12[IOTA0 & 16383] = 0.70710677f * fTemp44 + 0.70710677f * (fTemp42 - fTemp43);
			float fTemp50 = fVec12[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp48)))))) & 16383];
			fVec13[0] = fTemp50;
			float fTemp51 = fVec13[1] - (fTemp49 + (2.0f - fRec44[0])) * (fRec43[1] - fTemp50) / (fRec44[0] - fTemp49);
			fRec43[0] = ((std::fabs(fTemp51) > 1.1754944e-38f) ? fTemp51 : 0.0f);
			fRec40[0] = ((std::fabs(fRec43[0]) > 1.1754944e-38f) ? fRec43[0] : 0.0f);
			float fTemp52 = 0.7602446f * fRec39[1] + 0.6496369f * fTemp37;
			float fTemp53 = 0.9999f * (fRec48[1] + float(iTemp2 * iSlow19)) + fSlow20;
			fRec48[0] = ((std::fabs(fTemp53) > 1.1754944e-38f) ? fTemp53 : 0.0f);
			float fTemp54 = fRec48[0] + -1.49999f;
			float fTemp55 = std::floor(fTemp54);
			float fTemp56 = 0.7602446f * fRec40[1] + 0.6496369f * fTemp41;
			float fTemp57 = 0.7602446f * fTemp56;
			float fTemp58 = 0.6496369f * fRec46[1];
			float fTemp59 = 0.7602446f * fTemp52 - 0.6496369f * fRec45[1];
			fVec14[IOTA0 & 16383] = 0.70710677f * fTemp59 + 0.70710677f * (fTemp58 - fTemp57);
			float fTemp60 = fVec14[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp54)))))) & 16383];
			fVec15[0] = fTemp60;
			float fTemp61 = fVec15[1] - (fTemp55 + (2.0f - fRec48[0])) * (fRec47[1] - fTemp60) / (fRec48[0] - fTemp55);
			fRec47[0] = ((std::fabs(fTemp61) > 1.1754944e-38f) ? fTemp61 : 0.0f);
			fRec45[0] = ((std::fabs(fRec47[0]) > 1.1754944e-38f) ? fRec47[0] : 0.0f);
			float fTemp62 = 0.9999f * (fRec50[1] + float(iTemp2 * iSlow21)) + fSlow22;
			fRec50[0] = ((std::fabs(fTemp62) > 1.1754944e-38f) ? fTemp62 : 0.0f);
			float fTemp63 = fRec50[0] + -1.49999f;
			float fTemp64 = std::floor(fTemp63);
			fVec16[IOTA0 & 16383] = 0.70710677f * fTemp59 + 0.70710677f * (fTemp57 - fTemp58);
			float fTemp65 = fVec16[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp63)))))) & 16383];
			fVec17[0] = fTemp65;
			float fTemp66 = fVec17[1] - (fTemp64 + (2.0f - fRec50[0])) * (fRec49[1] - fTemp65) / (fRec50[0] - fTemp64);
			fRec49[0] = ((std::fabs(fTemp66) > 1.1754944e-38f) ? fTemp66 : 0.0f);
			fRec46[0] = ((std::fabs(fRec49[0]) > 1.1754944e-38f) ? fRec49[0] : 0.0f);
			float fTemp67 = 0.7602446f * fRec45[1] + 0.6496369f * fTemp52;
			float fTemp68 = 0.9999f * (fRec54[1] + float(iTemp2 * iSlow23)) + fSlow24;
			fRec54[0] = ((std::fabs(fTemp68) > 1.1754944e-38f) ? fTemp68 : 0.0f);
			float fTemp69 = fRec54[0] + -1.49999f;
			float fTemp70 = std::floor(fTemp69);
			float fTemp71 = 0.7602446f * fRec46[1] + 0.6496369f * fTemp56;
			float fTemp72 = 0.7602446f * fTemp71;
			float fTemp73 = 0.6496369f * fRec52[1];
			float fTemp74 = 0.7602446f * fTemp67 - 0.6496369f * fRec51[1];
			fVec18[IOTA0 & 16383] = 0.70710677f * fTemp74 + 0.70710677f * (fTemp73 - fTemp72);
			float fTemp75 = fVec18[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp69)))))) & 16383];
			fVec19[0] = fTemp75;
			float fTemp76 = fVec19[1] - (fTemp70 + (2.0f - fRec54[0])) * (fRec53[1] - fTemp75) / (fRec54[0] - fTemp70);
			fRec53[0] = ((std::fabs(fTemp76) > 1.1754944e-38f) ? fTemp76 : 0.0f);
			fRec51[0] = ((std::fabs(fRec53[0]) > 1.1754944e-38f) ? fRec53[0] : 0.0f);
			float fTemp77 = 0.9999f * (fRec56[1] + float(iTemp2 * iSlow25)) + fSlow26;
			fRec56[0] = ((std::fabs(fTemp77) > 1.1754944e-38f) ? fTemp77 : 0.0f);
			float fTemp78 = fRec56[0] + -1.49999f;
			float fTemp79 = std::floor(fTemp78);
			fVec20[IOTA0 & 16383] = 0.70710677f * fTemp74 + 0.70710677f * (fTemp72 - fTemp73);
			float fTemp80 = fVec20[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp78)))))) & 16383];
			fVec21[0] = fTemp80;
			float fTemp81 = fVec21[1] - (fTemp79 + (2.0f - fRec56[0])) * (fRec55[1] - fTemp80) / (fRec56[0] - fTemp79);
			fRec55[0] = ((std::fabs(fTemp81) > 1.1754944e-38f) ? fTemp81 : 0.0f);
			fRec52[0] = ((std::fabs(fRec55[0]) > 1.1754944e-38f) ? fRec55[0] : 0.0f);
			float fTemp82 = 0.7602446f * fRec51[1] + 0.6496369f * fTemp67;
			fVec22[IOTA0 & 1023] = fTemp82;
			fVec23[0] = fSlow27;
			float fTemp83 = fConst1 * (std::min<float>(3.4028235e+38f, fVec23[1] - fSlow27) + fRec59[1]);
			fRec59[0] = ((std::fabs(fTemp83) > 1.1754944e-38f) ? fTemp83 : 0.0f);
			float fTemp84 = fConst8 * (fSlow27 + fRec59[0]);
			float fTemp85 = std::cos(fTemp84);
			float fTemp86 = std::sin(fTemp84);
			float fTemp87 = fRec58[1] * fTemp86 + fRec57[1] * fTemp85;
			fRec57[0] = ((std::fabs(fTemp87) > 1.1754944e-38f) ? fTemp87 : 0.0f);
			float fTemp88 = float(iTemp2) + fRec58[1] * fTemp85 - fTemp86 * fRec57[1];
			fRec58[0] = ((std::fabs(fTemp88) > 1.1754944e-38f) ? fTemp88 : 0.0f);
			fVec24[0] = fSlow28;
			float fTemp89 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow28 - fVec24[1]))) + fRec60[1]);
			fRec60[0] = ((std::fabs(fTemp89) > 1.1754944e-38f) ? fTemp89 : 0.0f);
			float fTemp90 = fSlow29 + fRec60[0];
			float fTemp91 = 5e+01f * fTemp90 * (fRec58[0] + 1.0f);
			float fTemp92 = fTemp91 + 3.500005f;
			int iTemp93 = int(std::min<float>(516.0f, std::max<float>(-4.0f, fTemp92)));
			int iTemp94 = std::min<int>(516, std::max<int>(0, iTemp93 + 4));
			float fTemp95 = std::floor(fTemp92);
			float fTemp96 = fTemp91 + (2.0f - fTemp95);
			float fTemp97 = fTemp91 + (3.0f - fTemp95);
			float fTemp98 = fTemp91 + (4.0f - fTemp95);
			float fTemp99 = fTemp91 + (5.0f - fTemp95);
			float fTemp100 = fTemp99 * fTemp98;
			float fTemp101 = fTemp100 * fTemp97;
			float fTemp102 = fTemp101 * fTemp96;
			int iTemp103 = std::min<int>(516, std::max<int>(0, iTemp93 + 3));
			int iTemp104 = std::min<int>(516, std::max<int>(0, iTemp93 + 2));
			int iTemp105 = std::min<int>(516, std::max<int>(0, iTemp93 + 1));
			int iTemp106 = std::min<int>(516, std::max<int>(0, iTemp93));
			float fTemp107 = fTemp91 + (1.0f - fTemp95);
			fVec25[IOTA0 & 16383] = fTemp107 * (fTemp96 * (fTemp97 * (0.041666668f * fVec22[(IOTA0 - iTemp106) & 1023] * fTemp98 - 0.16666667f * fTemp99 * fVec22[(IOTA0 - iTemp105) & 1023]) + 0.25f * fTemp100 * fVec22[(IOTA0 - iTemp104) & 1023]) - 0.16666667f * fTemp101 * fVec22[(IOTA0 - iTemp103) & 1023]) + 0.041666668f * fTemp102 * fVec22[(IOTA0 - iTemp94) & 1023];
			float fTemp108 = fVec25[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp7)))))) & 16383];
			fVec26[0] = fTemp108;
			float fTemp109 = fVec26[1] - (fTemp8 + (2.0f - fRec26[0])) * (fRec25[1] - fTemp108) / (fRec26[0] - fTemp8);
			fRec25[0] = ((std::fabs(fTemp109) > 1.1754944e-38f) ? fTemp109 : 0.0f);
			float fTemp110 = 0.9999f * (fRec64[1] + float(iTemp2 * iSlow30)) + fSlow31;
			fRec64[0] = ((std::fabs(fTemp110) > 1.1754944e-38f) ? fTemp110 : 0.0f);
			float fTemp111 = fRec64[0] + -1.49999f;
			float fTemp112 = std::floor(fTemp111);
			float fTemp113 = 0.995f * (fRec66[1] + float(iTemp2 * iSlow32)) + fSlow33;
			fRec66[0] = ((std::fabs(fTemp113) > 1.1754944e-38f) ? fTemp113 : 0.0f);
			float fTemp114 = fRec66[0] + -1.49999f;
			float fTemp115 = std::floor(fTemp114);
			float fTemp116 = 0.7602446f * fRec52[1] + 0.6496369f * fTemp71;
			fVec27[IOTA0 & 1023] = fTemp116;
			float fTemp117 = 5e+01f * fTemp90 * (1.0f - fRec58[0]);
			float fTemp118 = fTemp117 + 3.500005f;
			int iTemp119 = int(std::min<float>(516.0f, std::max<float>(-4.0f, fTemp118)));
			float fTemp120 = std::floor(fTemp118);
			float fTemp121 = fTemp117 + (2.0f - fTemp120);
			float fTemp122 = fTemp117 + (3.0f - fTemp120);
			float fTemp123 = fTemp117 + (4.0f - fTemp120);
			float fTemp124 = fTemp117 + (5.0f - fTemp120);
			float fTemp125 = fTemp124 * fTemp123;
			float fTemp126 = fTemp125 * fTemp122;
			fVec28[IOTA0 & 16383] = (fTemp117 + (1.0f - fTemp120)) * (fTemp121 * (fTemp122 * (0.041666668f * fVec27[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp119))) & 1023] * fTemp123 - 0.16666667f * fTemp124 * fVec27[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp119 + 1))) & 1023]) + 0.25f * fTemp125 * fVec27[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp119 + 2))) & 1023]) - 0.16666667f * fTemp126 * fVec27[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp119 + 3))) & 1023]) + 0.041666668f * fTemp126 * fTemp121 * fVec27[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp119 + 4))) & 1023];
			float fTemp127 = fVec28[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp114)))))) & 16383];
			fVec29[0] = fTemp127;
			float fTemp128 = fVec29[1] - (fTemp115 + (2.0f - fRec66[0])) * (fRec65[1] - fTemp127) / (fRec66[0] - fTemp115);
			fRec65[0] = ((std::fabs(fTemp128) > 1.1754944e-38f) ? fTemp128 : 0.0f);
			float fTemp129 = 0.7602446f * fRec65[0];
			float fTemp130 = 0.6496369f * fRec62[1];
			float fTemp131 = 0.7602446f * fRec25[0] - 0.6496369f * fRec61[1];
			fVec30[IOTA0 & 16383] = 0.70710677f * fTemp131 + 0.70710677f * (fTemp130 - fTemp129);
			float fTemp132 = fVec30[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp111)))))) & 16383];
			fVec31[0] = fTemp132;
			float fTemp133 = fVec31[1] - (fTemp112 + (2.0f - fRec64[0])) * (fRec63[1] - fTemp132) / (fRec64[0] - fTemp112);
			fRec63[0] = ((std::fabs(fTemp133) > 1.1754944e-38f) ? fTemp133 : 0.0f);
			fRec61[0] = ((std::fabs(fRec63[0]) > 1.1754944e-38f) ? fRec63[0] : 0.0f);
			float fTemp134 = 0.9999f * (fRec68[1] + float(iTemp2 * iSlow34)) + fSlow35;
			fRec68[0] = ((std::fabs(fTemp134) > 1.1754944e-38f) ? fTemp134 : 0.0f);
			float fTemp135 = fRec68[0] + -1.49999f;
			float fTemp136 = std::floor(fTemp135);
			fVec32[IOTA0 & 16383] = 0.70710677f * fTemp131 + 0.70710677f * (fTemp129 - fTemp130);
			float fTemp137 = fVec32[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp135)))))) & 16383];
			fVec33[0] = fTemp137;
			float fTemp138 = fVec33[1] - (fTemp136 + (2.0f - fRec68[0])) * (fRec67[1] - fTemp137) / (fRec68[0] - fTemp136);
			fRec67[0] = ((std::fabs(fTemp138) > 1.1754944e-38f) ? fTemp138 : 0.0f);
			fRec62[0] = ((std::fabs(fRec67[0]) > 1.1754944e-38f) ? fRec67[0] : 0.0f);
			float fTemp139 = 0.7602446f * fRec61[1] + 0.6496369f * fRec25[0];
			float fTemp140 = 0.9999f * (fRec72[1] + float(iTemp2 * iSlow36)) + fSlow37;
			fRec72[0] = ((std::fabs(fTemp140) > 1.1754944e-38f) ? fTemp140 : 0.0f);
			float fTemp141 = fRec72[0] + -1.49999f;
			float fTemp142 = std::floor(fTemp141);
			float fTemp143 = 0.7602446f * fRec62[1] + 0.6496369f * fRec65[0];
			float fTemp144 = 0.7602446f * fTemp143;
			float fTemp145 = 0.6496369f * fRec70[1];
			float fTemp146 = 0.7602446f * fTemp139 - 0.6496369f * fRec69[1];
			fVec34[IOTA0 & 16383] = 0.70710677f * fTemp146 + 0.70710677f * (fTemp145 - fTemp144);
			float fTemp147 = fVec34[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp141)))))) & 16383];
			fVec35[0] = fTemp147;
			float fTemp148 = fVec35[1] - (fTemp142 + (2.0f - fRec72[0])) * (fRec71[1] - fTemp147) / (fRec72[0] - fTemp142);
			fRec71[0] = ((std::fabs(fTemp148) > 1.1754944e-38f) ? fTemp148 : 0.0f);
			fRec69[0] = ((std::fabs(fRec71[0]) > 1.1754944e-38f) ? fRec71[0] : 0.0f);
			float fTemp149 = 0.9999f * (fRec74[1] + float(iTemp2 * iSlow38)) + fSlow39;
			fRec74[0] = ((std::fabs(fTemp149) > 1.1754944e-38f) ? fTemp149 : 0.0f);
			float fTemp150 = fRec74[0] + -1.49999f;
			float fTemp151 = std::floor(fTemp150);
			float fTemp152 = fRec74[0] - fTemp151;
			fVec36[IOTA0 & 16383] = 0.70710677f * fTemp146 + 0.70710677f * (fTemp144 - fTemp145);
			int iTemp153 = std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp150)))));
			float fTemp154 = fVec36[(IOTA0 - iTemp153) & 16383];
			fVec37[0] = fTemp154;
			float fTemp155 = fTemp151 + (2.0f - fRec74[0]);
			float fTemp156 = fVec37[1] - fTemp155 * (fRec73[1] - fTemp154) / fTemp152;
			fRec73[0] = ((std::fabs(fTemp156) > 1.1754944e-38f) ? fTemp156 : 0.0f);
			fRec70[0] = ((std::fabs(fRec73[0]) > 1.1754944e-38f) ? fRec73[0] : 0.0f);
			float fTemp157 = 0.7602446f * fRec69[1] + 0.6496369f * fTemp139;
			float fTemp158 = 0.9999f * (fRec78[1] + float(iTemp2 * iSlow40)) + fSlow41;
			fRec78[0] = ((std::fabs(fTemp158) > 1.1754944e-38f) ? fTemp158 : 0.0f);
			float fTemp159 = fRec78[0] + -1.49999f;
			float fTemp160 = std::floor(fTemp159);
			float fTemp161 = 0.7602446f * fRec70[1] + 0.6496369f * fTemp143;
			float fTemp162 = 0.7602446f * fTemp161;
			float fTemp163 = 0.6496369f * fRec76[1];
			float fTemp164 = 0.7602446f * fTemp157 - 0.6496369f * fRec75[1];
			fVec38[IOTA0 & 16383] = 0.70710677f * fTemp164 + 0.70710677f * (fTemp163 - fTemp162);
			float fTemp165 = fVec38[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp159)))))) & 16383];
			fVec39[0] = fTemp165;
			float fTemp166 = fVec39[1] - (fTemp160 + (2.0f - fRec78[0])) * (fRec77[1] - fTemp165) / (fRec78[0] - fTemp160);
			fRec77[0] = ((std::fabs(fTemp166) > 1.1754944e-38f) ? fTemp166 : 0.0f);
			fRec75[0] = ((std::fabs(fRec77[0]) > 1.1754944e-38f) ? fRec77[0] : 0.0f);
			float fTemp167 = 0.9999f * (fRec80[1] + float(iTemp2 * iSlow42)) + fSlow43;
			fRec80[0] = ((std::fabs(fTemp167) > 1.1754944e-38f) ? fTemp167 : 0.0f);
			float fTemp168 = fRec80[0] + -1.49999f;
			float fTemp169 = std::floor(fTemp168);
			float fTemp170 = fRec80[0] - fTemp169;
			fVec40[IOTA0 & 16383] = 0.70710677f * fTemp164 + 0.70710677f * (fTemp162 - fTemp163);
			int iTemp171 = std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp168)))));
			float fTemp172 = fVec40[(IOTA0 - iTemp171) & 16383];
			fVec41[0] = fTemp172;
			float fTemp173 = fTemp169 + (2.0f - fRec80[0]);
			float fTemp174 = fVec41[1] - fTemp173 * (fRec79[1] - fTemp172) / fTemp170;
			fRec79[0] = ((std::fabs(fTemp174) > 1.1754944e-38f) ? fTemp174 : 0.0f);
			fRec76[0] = ((std::fabs(fRec79[0]) > 1.1754944e-38f) ? fRec79[0] : 0.0f);
			float fTemp175 = 0.7602446f * fRec75[1] + 0.6496369f * fTemp157;
			float fTemp176 = 0.9999f * (fRec84[1] + float(iTemp2 * iSlow44)) + fSlow45;
			fRec84[0] = ((std::fabs(fTemp176) > 1.1754944e-38f) ? fTemp176 : 0.0f);
			float fTemp177 = fRec84[0] + -1.49999f;
			float fTemp178 = std::floor(fTemp177);
			float fTemp179 = fRec84[0] - fTemp178;
			float fTemp180 = 0.7602446f * fRec76[1] + 0.6496369f * fTemp161;
			float fTemp181 = 0.7602446f * fTemp180;
			float fTemp182 = 0.6496369f * fRec82[1];
			float fTemp183 = 0.7602446f * fTemp175 - 0.6496369f * fRec81[1];
			fVec42[IOTA0 & 16383] = 0.70710677f * fTemp183 + 0.70710677f * (fTemp182 - fTemp181);
			int iTemp184 = std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp177)))));
			float fTemp185 = fVec42[(IOTA0 - iTemp184) & 16383];
			fVec43[0] = fTemp185;
			float fTemp186 = fTemp178 + (2.0f - fRec84[0]);
			float fTemp187 = fVec43[1] - fTemp186 * (fRec83[1] - fTemp185) / fTemp179;
			fRec83[0] = ((std::fabs(fTemp187) > 1.1754944e-38f) ? fTemp187 : 0.0f);
			fRec81[0] = ((std::fabs(fRec83[0]) > 1.1754944e-38f) ? fRec83[0] : 0.0f);
			float fTemp188 = 0.9999f * (fRec86[1] + float(iTemp2 * iSlow46)) + fSlow47;
			fRec86[0] = ((std::fabs(fTemp188) > 1.1754944e-38f) ? fTemp188 : 0.0f);
			float fTemp189 = fRec86[0] + -1.49999f;
			float fTemp190 = std::floor(fTemp189);
			float fTemp191 = fRec86[0] - fTemp190;
			fVec44[IOTA0 & 16383] = 0.70710677f * fTemp183 + 0.70710677f * (fTemp181 - fTemp182);
			int iTemp192 = std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp189)))));
			float fTemp193 = fVec44[(IOTA0 - iTemp192) & 16383];
			fVec45[0] = fTemp193;
			float fTemp194 = fTemp190 + (2.0f - fRec86[0]);
			float fTemp195 = fVec45[1] - fTemp194 * (fRec85[1] - fTemp193) / fTemp191;
			fRec85[0] = ((std::fabs(fTemp195) > 1.1754944e-38f) ? fTemp195 : 0.0f);
			fRec82[0] = ((std::fabs(fRec85[0]) > 1.1754944e-38f) ? fRec85[0] : 0.0f);
			float fTemp196 = 0.7602446f * fRec81[1] + 0.6496369f * fTemp175;
			float fTemp197 = 0.9999f * (fRec90[1] + float(iTemp2 * iSlow48)) + fSlow49;
			fRec90[0] = ((std::fabs(fTemp197) > 1.1754944e-38f) ? fTemp197 : 0.0f);
			float fTemp198 = fRec90[0] + -1.49999f;
			float fTemp199 = std::floor(fTemp198);
			float fTemp200 = 0.7602446f * fRec82[1] + 0.6496369f * fTemp180;
			float fTemp201 = 0.7602446f * fTemp200;
			float fTemp202 = 0.6496369f * fRec88[1];
			float fTemp203 = 0.7602446f * fTemp196 - 0.6496369f * fRec87[1];
			fVec46[IOTA0 & 16383] = 0.70710677f * fTemp203 + 0.70710677f * (fTemp202 - fTemp201);
			float fTemp204 = fVec46[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp198)))))) & 16383];
			fVec47[0] = fTemp204;
			float fTemp205 = fVec47[1] - (fTemp199 + (2.0f - fRec90[0])) * (fRec89[1] - fTemp204) / (fRec90[0] - fTemp199);
			fRec89[0] = ((std::fabs(fTemp205) > 1.1754944e-38f) ? fTemp205 : 0.0f);
			fRec87[0] = ((std::fabs(fRec89[0]) > 1.1754944e-38f) ? fRec89[0] : 0.0f);
			float fTemp206 = 0.9999f * (fRec92[1] + float(iTemp2 * iSlow50)) + fSlow51;
			fRec92[0] = ((std::fabs(fTemp206) > 1.1754944e-38f) ? fTemp206 : 0.0f);
			float fTemp207 = fRec92[0] + -1.49999f;
			float fTemp208 = std::floor(fTemp207);
			float fTemp209 = fRec92[0] - fTemp208;
			fVec48[IOTA0 & 16383] = 0.70710677f * fTemp203 + 0.70710677f * (fTemp201 - fTemp202);
			int iTemp210 = std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp207)))));
			float fTemp211 = fVec48[(IOTA0 - iTemp210) & 16383];
			fVec49[0] = fTemp211;
			float fTemp212 = fTemp208 + (2.0f - fRec92[0]);
			float fTemp213 = fVec49[1] - fTemp212 * (fRec91[1] - fTemp211) / fTemp209;
			fRec91[0] = ((std::fabs(fTemp213) > 1.1754944e-38f) ? fTemp213 : 0.0f);
			fRec88[0] = ((std::fabs(fRec91[0]) > 1.1754944e-38f) ? fRec91[0] : 0.0f);
			float fTemp214 = 0.7602446f * fRec87[1] + 0.6496369f * fTemp196;
			fVec50[IOTA0 & 16383] = fTemp214;
			float fTemp215 = 5e+01f * fTemp90 * (fRec57[0] + 1.0f);
			float fTemp216 = fTemp215 + 3.500005f;
			float fTemp217 = std::max<float>(-4.0f, fTemp216);
			int iTemp218 = int(std::min<float>(8196.0f, fTemp217));
			float fTemp219 = std::floor(fTemp216);
			float fTemp220 = fTemp215 + (2.0f - fTemp219);
			float fTemp221 = fTemp215 + (3.0f - fTemp219);
			float fTemp222 = fTemp215 + (4.0f - fTemp219);
			float fTemp223 = fTemp215 + (5.0f - fTemp219);
			float fTemp224 = fTemp223 * fTemp222;
			float fTemp225 = fTemp224 * fTemp221;
			float fTemp226 = fTemp225 * fTemp220;
			float fTemp227 = fTemp215 + (1.0f - fTemp219);
			fVec51[IOTA0 & 16383] = fTemp227 * (fTemp220 * (fTemp221 * (0.041666668f * fVec50[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp218))) & 16383] * fTemp222 - 0.16666667f * fTemp223 * fVec50[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp218 + 1))) & 16383]) + 0.25f * fTemp224 * fVec50[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp218 + 2))) & 16383]) - 0.16666667f * fTemp225 * fVec50[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp218 + 3))) & 16383]) + 0.041666668f * fTemp226 * fVec50[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp218 + 4))) & 16383];
			float fTemp228 = fVec51[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp4)))))) & 16383];
			fVec52[0] = fTemp228;
			float fTemp229 = fVec52[1] - (fTemp5 + (2.0f - fRec24[0])) * (fRec23[1] - fTemp228) / (fRec24[0] - fTemp5);
			fRec23[0] = ((std::fabs(fTemp229) > 1.1754944e-38f) ? fTemp229 : 0.0f);
			float fTemp230 = fRec23[0] - fRec21[1];
			float fTemp231 = fRec21[1] + fConst11 * fTemp230;
			fRec21[0] = ((std::fabs(fTemp231) > 1.1754944e-38f) ? fTemp231 : 0.0f);
			float fTemp232 = fRec21[1] + fConst10 * fTemp230;
			float fRec22 = ((std::fabs(fTemp232) > 1.1754944e-38f) ? fTemp232 : 0.0f);
			float fTemp233 = fRec22 - (fConst7 * fRec18[1] + fRec19[1]);
			float fTemp234 = fRec18[1] + fConst13 * fTemp233;
			fRec18[0] = ((std::fabs(fTemp234) > 1.1754944e-38f) ? fTemp234 : 0.0f);
			float fTemp235 = fRec18[1] + fConst12 * fTemp233;
			float fTemp236 = fRec19[1] + fConst14 * fTemp235;
			fRec19[0] = ((std::fabs(fTemp236) > 1.1754944e-38f) ? fTemp236 : 0.0f);
			float fTemp237 = fRec19[1] + fConst5 * fTemp235;
			float fRec20 = ((std::fabs(fTemp237) > 1.1754944e-38f) ? fTemp237 : 0.0f);
			float fTemp238 = fRec20 - (fConst6 * fRec15[1] + fRec16[1]);
			float fTemp239 = fRec15[1] + fConst16 * fTemp238;
			fRec15[0] = ((std::fabs(fTemp239) > 1.1754944e-38f) ? fTemp239 : 0.0f);
			float fTemp240 = fRec15[1] + fConst15 * fTemp238;
			float fTemp241 = fRec16[1] + fConst14 * fTemp240;
			fRec16[0] = ((std::fabs(fTemp241) > 1.1754944e-38f) ? fTemp241 : 0.0f);
			float fTemp242 = fRec16[1] + fConst5 * fTemp240;
			float fRec17 = ((std::fabs(fTemp242) > 1.1754944e-38f) ? fTemp242 : 0.0f);
			float fTemp243 = fRec17 - fRec13[1];
			float fTemp244 = fRec13[1] + fConst19 * fTemp243;
			fRec13[0] = ((std::fabs(fTemp244) > 1.1754944e-38f) ? fTemp244 : 0.0f);
			float fTemp245 = fConst17 * fTemp243;
			float fRec14 = ((std::fabs(fTemp245) > 1.1754944e-38f) ? fTemp245 : 0.0f);
			float fTemp246 = fRec14 - (fConst4 * fRec8[1] + fRec9[1]);
			float fTemp247 = fRec8[1] + fConst21 * fTemp246;
			fRec8[0] = ((std::fabs(fTemp247) > 1.1754944e-38f) ? fTemp247 : 0.0f);
			float fTemp248 = fRec8[1] + fConst20 * fTemp246;
			float fTemp249 = fRec9[1] + fConst22 * fTemp248;
			fRec9[0] = ((std::fabs(fTemp249) > 1.1754944e-38f) ? fTemp249 : 0.0f);
			float fRec10 = ((std::fabs(fRec14) > 1.1754944e-38f) ? fRec14 : 0.0f);
			float fRec11 = ((std::fabs(fTemp248) > 1.1754944e-38f) ? fTemp248 : 0.0f);
			float fTemp250 = fRec9[1] + fConst2 * fTemp248;
			float fRec12 = ((std::fabs(fTemp250) > 1.1754944e-38f) ? fTemp250 : 0.0f);
			float fTemp251 = fRec12 + 1.618034f * fRec11;
			float fTemp252 = fRec10 - (fTemp251 + fConst3 * fRec3[1] + fRec4[1]);
			float fTemp253 = fRec3[1] + fConst24 * fTemp252;
			fRec3[0] = ((std::fabs(fTemp253) > 1.1754944e-38f) ? fTemp253 : 0.0f);
			float fTemp254 = fRec3[1] + fConst23 * fTemp252;
			float fTemp255 = fRec4[1] + fConst22 * fTemp254;
			fRec4[0] = ((std::fabs(fTemp255) > 1.1754944e-38f) ? fTemp255 : 0.0f);
			float fTemp256 = fRec10 - fTemp251;
			float fRec5 = ((std::fabs(fTemp256) > 1.1754944e-38f) ? fTemp256 : 0.0f);
			float fRec6 = ((std::fabs(fTemp254) > 1.1754944e-38f) ? fTemp254 : 0.0f);
			float fTemp257 = fRec4[1] + fConst2 * fTemp254;
			float fRec7 = ((std::fabs(fTemp257) > 1.1754944e-38f) ? fTemp257 : 0.0f);
			fVec53[0] = fSlow52;
			float fTemp258 = fConst1 * (std::min<float>(3.4028235e+38f, fVec53[1] - fSlow52) + fRec93[1]);
			fRec93[0] = ((std::fabs(fTemp258) > 1.1754944e-38f) ? fTemp258 : 0.0f);
			float fTemp259 = fSlow52 + fRec93[0];
			float fTemp260 = fRec17 - fRec100[1];
			float fTemp261 = fRec100[1] + fConst19 * fTemp260;
			fRec100[0] = ((std::fabs(fTemp261) > 1.1754944e-38f) ? fTemp261 : 0.0f);
			float fTemp262 = fRec100[1] + fConst18 * fTemp260;
			float fRec101 = ((std::fabs(fTemp262) > 1.1754944e-38f) ? fTemp262 : 0.0f);
			float fTemp263 = fRec101 - (fConst4 * fRec97[1] + fRec98[1]);
			float fTemp264 = fRec97[1] + fConst21 * fTemp263;
			fRec97[0] = ((std::fabs(fTemp264) > 1.1754944e-38f) ? fTemp264 : 0.0f);
			float fTemp265 = fRec97[1] + fConst20 * fTemp263;
			float fTemp266 = fRec98[1] + fConst22 * fTemp265;
			fRec98[0] = ((std::fabs(fTemp266) > 1.1754944e-38f) ? fTemp266 : 0.0f);
			float fTemp267 = fRec98[1] + fConst2 * fTemp265;
			float fRec99 = ((std::fabs(fTemp267) > 1.1754944e-38f) ? fTemp267 : 0.0f);
			float fTemp268 = fRec99 - (fConst3 * fRec94[1] + fRec95[1]);
			float fTemp269 = fRec94[1] + fConst24 * fTemp268;
			fRec94[0] = ((std::fabs(fTemp269) > 1.1754944e-38f) ? fTemp269 : 0.0f);
			float fTemp270 = fRec94[1] + fConst23 * fTemp268;
			float fTemp271 = fRec95[1] + fConst22 * fTemp270;
			fRec95[0] = ((std::fabs(fTemp271) > 1.1754944e-38f) ? fTemp271 : 0.0f);
			float fTemp272 = fRec95[1] + fConst2 * fTemp270;
			float fRec96 = ((std::fabs(fTemp272) > 1.1754944e-38f) ? fTemp272 : 0.0f);
			fVec54[0] = fSlow53;
			float fTemp273 = fConst1 * (std::min<float>(3.4028235e+38f, fVec54[1] - fSlow53) + fRec102[1]);
			fRec102[0] = ((std::fabs(fTemp273) > 1.1754944e-38f) ? fTemp273 : 0.0f);
			float fTemp274 = fSlow53 + fRec102[0];
			float fTemp275 = fRec23[0] - fRec116[1];
			float fTemp276 = fRec116[1] + fConst11 * fTemp275;
			fRec116[0] = ((std::fabs(fTemp276) > 1.1754944e-38f) ? fTemp276 : 0.0f);
			float fTemp277 = fConst9 * fTemp275;
			float fRec117 = ((std::fabs(fTemp277) > 1.1754944e-38f) ? fTemp277 : 0.0f);
			float fTemp278 = fRec117 - (fConst7 * fRec111[1] + fRec112[1]);
			float fTemp279 = fRec111[1] + fConst13 * fTemp278;
			fRec111[0] = ((std::fabs(fTemp279) > 1.1754944e-38f) ? fTemp279 : 0.0f);
			float fTemp280 = fRec111[1] + fConst12 * fTemp278;
			float fTemp281 = fRec112[1] + fConst14 * fTemp280;
			fRec112[0] = ((std::fabs(fTemp281) > 1.1754944e-38f) ? fTemp281 : 0.0f);
			float fRec113 = ((std::fabs(fRec117) > 1.1754944e-38f) ? fRec117 : 0.0f);
			float fRec114 = ((std::fabs(fTemp280) > 1.1754944e-38f) ? fTemp280 : 0.0f);
			float fTemp282 = fRec112[1] + fConst5 * fTemp280;
			float fRec115 = ((std::fabs(fTemp282) > 1.1754944e-38f) ? fTemp282 : 0.0f);
			float fTemp283 = fRec115 + 1.618034f * fRec114;
			float fTemp284 = fRec113 - (fTemp283 + fConst6 * fRec106[1] + fRec107[1]);
			float fTemp285 = fRec106[1] + fConst16 * fTemp284;
			fRec106[0] = ((std::fabs(fTemp285) > 1.1754944e-38f) ? fTemp285 : 0.0f);
			float fTemp286 = fRec106[1] + fConst15 * fTemp284;
			float fTemp287 = fRec107[1] + fConst14 * fTemp286;
			fRec107[0] = ((std::fabs(fTemp287) > 1.1754944e-38f) ? fTemp287 : 0.0f);
			float fTemp288 = fRec113 - fTemp283;
			float fRec108 = ((std::fabs(fTemp288) > 1.1754944e-38f) ? fTemp288 : 0.0f);
			float fRec109 = ((std::fabs(fTemp286) > 1.1754944e-38f) ? fTemp286 : 0.0f);
			float fTemp289 = fRec107[1] + fConst5 * fTemp286;
			float fRec110 = ((std::fabs(fTemp289) > 1.1754944e-38f) ? fTemp289 : 0.0f);
			float fTemp290 = fRec108 - (fRec110 + 0.618034f * fRec109 + fRec103[1] + 0.38196602f * (fConst25 * fRec103[1] + fRec104[1]));
			float fTemp291 = fRec103[1] + fConst27 * fTemp290;
			float fTemp292 = 2.0f * fTemp291 - fRec103[1];
			fRec103[0] = ((std::fabs(fTemp292) > 1.1754944e-38f) ? fTemp292 : 0.0f);
			float fTemp293 = fRec104[1] + fConst25 * fTemp291;
			float fTemp294 = 2.0f * fTemp293 - fRec104[1];
			fRec104[0] = ((std::fabs(fTemp294) > 1.1754944e-38f) ? fTemp294 : 0.0f);
			float fTemp295 = 0.38196602f * fTemp293 + fConst28 * fTemp290 - fRec103[1];
			float fRec105 = ((std::fabs(fTemp295) > 1.1754944e-38f) ? fTemp295 : 0.0f);
			float fTemp296 = float(input0[i0]);
			float fTemp297 = fTemp296 + fSlow54 * (fRec5 + fRec105 * fTemp274 + fRec96 * fTemp259 - (fRec7 + 0.618034f * fRec6));
			fVec55[IOTA0 & 1023] = fTemp297;
			int iTemp298 = int(std::min<float>(516.0f, fTemp217));
			float fTemp299 = fTemp227 * (fTemp220 * (fTemp221 * (0.041666668f * fTemp222 * fVec55[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp298))) & 1023] - 0.16666667f * fTemp223 * fVec55[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp298 + 1))) & 1023]) + 0.25f * fTemp224 * fVec55[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp298 + 2))) & 1023]) - 0.16666667f * fTemp225 * fVec55[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp298 + 3))) & 1023]);
			float fTemp300 = 0.041666668f * fTemp226 * fVec55[(IOTA0 - std::min<int>(516, std::max<int>(0, iTemp298 + 4))) & 1023];
			float fTemp301 = fTemp299 + fTemp300;
			fVec56[0] = fTemp301;
			fVec57[0] = fSlow55;
			float fTemp302 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow55 - fVec57[1]))) + fRec119[1]);
			fRec119[0] = ((std::fabs(fTemp302) > 1.1754944e-38f) ? fTemp302 : 0.0f);
			float fTemp303 = fSlow56 + fRec119[0];
			float fTemp304 = fTemp303 * (fRec118[1] + std::min<float>(3.4028235e+38f, fVec56[1] - fTemp301));
			fRec118[0] = ((std::fabs(fTemp304) > 1.1754944e-38f) ? fTemp304 : 0.0f);
			float fTemp305 = fTemp300 + fRec118[0] + fTemp299;
			fVec58[0] = fSlow57;
			float fTemp306 = fConst1 * (std::min<float>(3.4028235e+38f, -(0.01f * (fSlow57 - fVec58[1]))) + fRec120[1]);
			fRec120[0] = ((std::fabs(fTemp306) > 1.1754944e-38f) ? fTemp306 : 0.0f);
			float fTemp307 = fSlow58 + fRec120[0];
			float fTemp308 = std::sin(fTemp307);
			float fTemp309 = std::cos(fTemp307);
			float fTemp310 = 0.995f * (fRec145[1] + float(iTemp2 * iSlow59)) + fSlow60;
			fRec145[0] = ((std::fabs(fTemp310) > 1.1754944e-38f) ? fTemp310 : 0.0f);
			float fTemp311 = fRec145[0] + -1.49999f;
			float fTemp312 = std::floor(fTemp311);
			float fTemp313 = 0.7602446f * fRec88[1] + 0.6496369f * fTemp200;
			fVec59[IOTA0 & 16383] = fTemp313;
			float fTemp314 = 5e+01f * fTemp90 * (1.0f - fRec57[0]);
			float fTemp315 = fTemp314 + 3.500005f;
			int iTemp316 = int(std::min<float>(8196.0f, std::max<float>(-4.0f, fTemp315)));
			float fTemp317 = std::floor(fTemp315);
			float fTemp318 = fTemp314 + (2.0f - fTemp317);
			float fTemp319 = fTemp314 + (3.0f - fTemp317);
			float fTemp320 = fTemp314 + (4.0f - fTemp317);
			float fTemp321 = fTemp314 + (5.0f - fTemp317);
			float fTemp322 = fTemp321 * fTemp320;
			float fTemp323 = fTemp322 * fTemp319;
			fVec60[IOTA0 & 16383] = (fTemp314 + (1.0f - fTemp317)) * (fTemp318 * (fTemp319 * (0.041666668f * fVec59[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp316))) & 16383] * fTemp320 - 0.16666667f * fTemp321 * fVec59[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp316 + 1))) & 16383]) + 0.25f * fTemp322 * fVec59[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp316 + 2))) & 16383]) - 0.16666667f * fTemp323 * fVec59[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp316 + 3))) & 16383]) + 0.041666668f * fTemp323 * fTemp318 * fVec59[(IOTA0 - std::min<int>(8196, std::max<int>(0, iTemp316 + 4))) & 16383];
			float fTemp324 = fVec60[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp311)))))) & 16383];
			fVec61[0] = fTemp324;
			float fTemp325 = fVec61[1] - (fTemp312 + (2.0f - fRec145[0])) * (fRec144[1] - fTemp324) / (fRec145[0] - fTemp312);
			fRec144[0] = ((std::fabs(fTemp325) > 1.1754944e-38f) ? fTemp325 : 0.0f);
			float fTemp326 = fRec144[0] - fRec142[1];
			float fTemp327 = fRec142[1] + fConst11 * fTemp326;
			fRec142[0] = ((std::fabs(fTemp327) > 1.1754944e-38f) ? fTemp327 : 0.0f);
			float fTemp328 = fRec142[1] + fConst10 * fTemp326;
			float fRec143 = ((std::fabs(fTemp328) > 1.1754944e-38f) ? fTemp328 : 0.0f);
			float fTemp329 = fRec143 - (fConst7 * fRec139[1] + fRec140[1]);
			float fTemp330 = fRec139[1] + fConst13 * fTemp329;
			fRec139[0] = ((std::fabs(fTemp330) > 1.1754944e-38f) ? fTemp330 : 0.0f);
			float fTemp331 = fRec139[1] + fConst12 * fTemp329;
			float fTemp332 = fRec140[1] + fConst14 * fTemp331;
			fRec140[0] = ((std::fabs(fTemp332) > 1.1754944e-38f) ? fTemp332 : 0.0f);
			float fTemp333 = fRec140[1] + fConst5 * fTemp331;
			float fRec141 = ((std::fabs(fTemp333) > 1.1754944e-38f) ? fTemp333 : 0.0f);
			float fTemp334 = fRec141 - (fConst6 * fRec136[1] + fRec137[1]);
			float fTemp335 = fRec136[1] + fConst16 * fTemp334;
			fRec136[0] = ((std::fabs(fTemp335) > 1.1754944e-38f) ? fTemp335 : 0.0f);
			float fTemp336 = fRec136[1] + fConst15 * fTemp334;
			float fTemp337 = fRec137[1] + fConst14 * fTemp336;
			fRec137[0] = ((std::fabs(fTemp337) > 1.1754944e-38f) ? fTemp337 : 0.0f);
			float fTemp338 = fRec137[1] + fConst5 * fTemp336;
			float fRec138 = ((std::fabs(fTemp338) > 1.1754944e-38f) ? fTemp338 : 0.0f);
			float fTemp339 = fRec138 - fRec134[1];
			float fTemp340 = fRec134[1] + fConst19 * fTemp339;
			fRec134[0] = ((std::fabs(fTemp340) > 1.1754944e-38f) ? fTemp340 : 0.0f);
			float fTemp341 = fConst17 * fTemp339;
			float fRec135 = ((std::fabs(fTemp341) > 1.1754944e-38f) ? fTemp341 : 0.0f);
			float fTemp342 = fRec135 - (fConst4 * fRec129[1] + fRec130[1]);
			float fTemp343 = fRec129[1] + fConst21 * fTemp342;
			fRec129[0] = ((std::fabs(fTemp343) > 1.1754944e-38f) ? fTemp343 : 0.0f);
			float fTemp344 = fRec129[1] + fConst20 * fTemp342;
			float fTemp345 = fRec130[1] + fConst22 * fTemp344;
			fRec130[0] = ((std::fabs(fTemp345) > 1.1754944e-38f) ? fTemp345 : 0.0f);
			float fRec131 = ((std::fabs(fRec135) > 1.1754944e-38f) ? fRec135 : 0.0f);
			float fRec132 = ((std::fabs(fTemp344) > 1.1754944e-38f) ? fTemp344 : 0.0f);
			float fTemp346 = fRec130[1] + fConst2 * fTemp344;
			float fRec133 = ((std::fabs(fTemp346) > 1.1754944e-38f) ? fTemp346 : 0.0f);
			float fTemp347 = fRec133 + 1.618034f * fRec132;
			float fTemp348 = fRec131 - (fTemp347 + fConst3 * fRec124[1] + fRec125[1]);
			float fTemp349 = fRec124[1] + fConst24 * fTemp348;
			fRec124[0] = ((std::fabs(fTemp349) > 1.1754944e-38f) ? fTemp349 : 0.0f);
			float fTemp350 = fRec124[1] + fConst23 * fTemp348;
			float fTemp351 = fRec125[1] + fConst22 * fTemp350;
			fRec125[0] = ((std::fabs(fTemp351) > 1.1754944e-38f) ? fTemp351 : 0.0f);
			float fTemp352 = fRec131 - fTemp347;
			float fRec126 = ((std::fabs(fTemp352) > 1.1754944e-38f) ? fTemp352 : 0.0f);
			float fRec127 = ((std::fabs(fTemp350) > 1.1754944e-38f) ? fTemp350 : 0.0f);
			float fTemp353 = fRec125[1] + fConst2 * fTemp350;
			float fRec128 = ((std::fabs(fTemp353) > 1.1754944e-38f) ? fTemp353 : 0.0f);
			float fTemp354 = fRec138 - fRec152[1];
			float fTemp355 = fRec152[1] + fConst19 * fTemp354;
			fRec152[0] = ((std::fabs(fTemp355) > 1.1754944e-38f) ? fTemp355 : 0.0f);
			float fTemp356 = fRec152[1] + fConst18 * fTemp354;
			float fRec153 = ((std::fabs(fTemp356) > 1.1754944e-38f) ? fTemp356 : 0.0f);
			float fTemp357 = fRec153 - (fConst4 * fRec149[1] + fRec150[1]);
			float fTemp358 = fRec149[1] + fConst21 * fTemp357;
			fRec149[0] = ((std::fabs(fTemp358) > 1.1754944e-38f) ? fTemp358 : 0.0f);
			float fTemp359 = fRec149[1] + fConst20 * fTemp357;
			float fTemp360 = fRec150[1] + fConst22 * fTemp359;
			fRec150[0] = ((std::fabs(fTemp360) > 1.1754944e-38f) ? fTemp360 : 0.0f);
			float fTemp361 = fRec150[1] + fConst2 * fTemp359;
			float fRec151 = ((std::fabs(fTemp361) > 1.1754944e-38f) ? fTemp361 : 0.0f);
			float fTemp362 = fRec151 - (fConst3 * fRec146[1] + fRec147[1]);
			float fTemp363 = fRec146[1] + fConst24 * fTemp362;
			fRec146[0] = ((std::fabs(fTemp363) > 1.1754944e-38f) ? fTemp363 : 0.0f);
			float fTemp364 = fRec146[1] + fConst23 * fTemp362;
			float fTemp365 = fRec147[1] + fConst22 * fTemp364;
			fRec147[0] = ((std::fabs(fTemp365) > 1.1754944e-38f) ? fTemp365 : 0.0f);
			float fTemp366 = fRec147[1] + fConst2 * fTemp364;
			float fRec148 = ((std::fabs(fTemp366) > 1.1754944e-38f) ? fTemp366 : 0.0f);
			float fTemp367 = fRec144[0] - fRec167[1];
			float fTemp368 = fRec167[1] + fConst11 * fTemp367;
			fRec167[0] = ((std::fabs(fTemp368) > 1.1754944e-38f) ? fTemp368 : 0.0f);
			float fTemp369 = fConst9 * fTemp367;
			float fRec168 = ((std::fabs(fTemp369) > 1.1754944e-38f) ? fTemp369 : 0.0f);
			float fTemp370 = fRec168 - (fConst7 * fRec162[1] + fRec163[1]);
			float fTemp371 = fRec162[1] + fConst13 * fTemp370;
			fRec162[0] = ((std::fabs(fTemp371) > 1.1754944e-38f) ? fTemp371 : 0.0f);
			float fTemp372 = fRec162[1] + fConst12 * fTemp370;
			float fTemp373 = fRec163[1] + fConst14 * fTemp372;
			fRec163[0] = ((std::fabs(fTemp373) > 1.1754944e-38f) ? fTemp373 : 0.0f);
			float fRec164 = ((std::fabs(fRec168) > 1.1754944e-38f) ? fRec168 : 0.0f);
			float fRec165 = ((std::fabs(fTemp372) > 1.1754944e-38f) ? fTemp372 : 0.0f);
			float fTemp374 = fRec163[1] + fConst5 * fTemp372;
			float fRec166 = ((std::fabs(fTemp374) > 1.1754944e-38f) ? fTemp374 : 0.0f);
			float fTemp375 = fRec166 + 1.618034f * fRec165;
			float fTemp376 = fRec164 - (fTemp375 + fConst6 * fRec157[1] + fRec158[1]);
			float fTemp377 = fRec157[1] + fConst16 * fTemp376;
			fRec157[0] = ((std::fabs(fTemp377) > 1.1754944e-38f) ? fTemp377 : 0.0f);
			float fTemp378 = fRec157[1] + fConst15 * fTemp376;
			float fTemp379 = fRec158[1] + fConst14 * fTemp378;
			fRec158[0] = ((std::fabs(fTemp379) > 1.1754944e-38f) ? fTemp379 : 0.0f);
			float fTemp380 = fRec164 - fTemp375;
			float fRec159 = ((std::fabs(fTemp380) > 1.1754944e-38f) ? fTemp380 : 0.0f);
			float fRec160 = ((std::fabs(fTemp378) > 1.1754944e-38f) ? fTemp378 : 0.0f);
			float fTemp381 = fRec158[1] + fConst5 * fTemp378;
			float fRec161 = ((std::fabs(fTemp381) > 1.1754944e-38f) ? fTemp381 : 0.0f);
			float fTemp382 = fRec159 - (fRec161 + 0.618034f * fRec160 + fRec154[1] + 0.38196602f * (fConst25 * fRec154[1] + fRec155[1]));
			float fTemp383 = fRec154[1] + fConst27 * fTemp382;
			float fTemp384 = 2.0f * fTemp383 - fRec154[1];
			fRec154[0] = ((std::fabs(fTemp384) > 1.1754944e-38f) ? fTemp384 : 0.0f);
			float fTemp385 = fRec155[1] + fConst25 * fTemp383;
			float fTemp386 = 2.0f * fTemp385 - fRec155[1];
			fRec155[0] = ((std::fabs(fTemp386) > 1.1754944e-38f) ? fTemp386 : 0.0f);
			float fTemp387 = 0.38196602f * fTemp385 + fConst28 * fTemp382 - fRec154[1];
			float fRec156 = ((std::fabs(fTemp387) > 1.1754944e-38f) ? fTemp387 : 0.0f);
			float fTemp388 = float(input1[i0]);
			float fTemp389 = fTemp388 + fSlow54 * (fRec126 + fTemp274 * fRec156 + fTemp259 * fRec148 - (fRec128 + 0.618034f * fRec127));
			fVec62[IOTA0 & 1023] = fTemp389;
			float fTemp390 = fTemp107 * (fTemp96 * (fTemp97 * (0.041666668f * fTemp98 * fVec62[(IOTA0 - iTemp106) & 1023] - 0.16666667f * fTemp99 * fVec62[(IOTA0 - iTemp105) & 1023]) + 0.25f * fTemp100 * fVec62[(IOTA0 - iTemp104) & 1023]) - 0.16666667f * fTemp101 * fVec62[(IOTA0 - iTemp103) & 1023]);
			float fTemp391 = 0.041666668f * fTemp102 * fVec62[(IOTA0 - iTemp94) & 1023];
			float fTemp392 = fTemp390 + fTemp391;
			fVec63[0] = fTemp392;
			float fTemp393 = fTemp303 * (fRec169[1] + std::min<float>(3.4028235e+38f, fVec63[1] - fTemp392));
			fRec169[0] = ((std::fabs(fTemp393) > 1.1754944e-38f) ? fTemp393 : 0.0f);
			float fTemp394 = fTemp391 + fRec169[0] + fTemp390;
			float fTemp395 = fTemp309 * fTemp394;
			float fTemp396 = fTemp308 * fRec122[1];
			float fTemp397 = fTemp305 * fTemp309 - fTemp308 * fRec121[1];
			fVec64[IOTA0 & 16383] = 0.70710677f * fTemp397 + 0.70710677f * (fTemp396 - fTemp395);
			float fTemp398 = fVec64[(IOTA0 - iTemp153) & 16383];
			fVec65[0] = fTemp398;
			float fTemp399 = fVec65[1] - fTemp155 * (fRec123[1] - fTemp398) / fTemp152;
			fRec123[0] = ((std::fabs(fTemp399) > 1.1754944e-38f) ? fTemp399 : 0.0f);
			fRec121[0] = ((std::fabs(fRec123[0]) > 1.1754944e-38f) ? fRec123[0] : 0.0f);
			float fTemp400 = 0.9999f * (fRec171[1] + float(iTemp2 * iSlow61)) + fSlow62;
			fRec171[0] = ((std::fabs(fTemp400) > 1.1754944e-38f) ? fTemp400 : 0.0f);
			float fTemp401 = fRec171[0] + -1.49999f;
			float fTemp402 = std::floor(fTemp401);
			fVec66[IOTA0 & 16383] = 0.70710677f * fTemp397 + 0.70710677f * (fTemp395 - fTemp396);
			float fTemp403 = fVec66[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp401)))))) & 16383];
			fVec67[0] = fTemp403;
			float fTemp404 = fVec67[1] - (fTemp402 + (2.0f - fRec171[0])) * (fRec170[1] - fTemp403) / (fRec171[0] - fTemp402);
			fRec170[0] = ((std::fabs(fTemp404) > 1.1754944e-38f) ? fTemp404 : 0.0f);
			fRec122[0] = ((std::fabs(fRec170[0]) > 1.1754944e-38f) ? fRec170[0] : 0.0f);
			float fTemp405 = fRec121[1] * fTemp309 + fTemp308 * fTemp305;
			float fTemp406 = fTemp309 * fRec122[1] + fTemp308 * fTemp394;
			float fTemp407 = fTemp309 * fTemp406;
			float fTemp408 = fTemp308 * fRec173[1];
			float fTemp409 = fTemp309 * fTemp405 - fTemp308 * fRec172[1];
			fVec68[IOTA0 & 16383] = 0.70710677f * fTemp409 + 0.70710677f * (fTemp408 - fTemp407);
			float fTemp410 = fVec68[(IOTA0 - iTemp184) & 16383];
			fVec69[0] = fTemp410;
			float fTemp411 = fVec69[1] - fTemp186 * (fRec174[1] - fTemp410) / fTemp179;
			fRec174[0] = ((std::fabs(fTemp411) > 1.1754944e-38f) ? fTemp411 : 0.0f);
			fRec172[0] = ((std::fabs(fRec174[0]) > 1.1754944e-38f) ? fRec174[0] : 0.0f);
			fVec70[IOTA0 & 16383] = 0.70710677f * fTemp409 + 0.70710677f * (fTemp407 - fTemp408);
			float fTemp412 = fVec70[(IOTA0 - iTemp171) & 16383];
			fVec71[0] = fTemp412;
			float fTemp413 = fVec71[1] - fTemp173 * (fRec175[1] - fTemp412) / fTemp170;
			fRec175[0] = ((std::fabs(fTemp413) > 1.1754944e-38f) ? fTemp413 : 0.0f);
			fRec173[0] = ((std::fabs(fRec175[0]) > 1.1754944e-38f) ? fRec175[0] : 0.0f);
			float fTemp414 = fRec172[1] * fTemp309 + fTemp308 * fTemp405;
			float fTemp415 = fTemp309 * fRec173[1] + fTemp308 * fTemp406;
			float fTemp416 = fTemp309 * fTemp415;
			float fTemp417 = fTemp308 * fRec177[1];
			float fTemp418 = fTemp309 * fTemp414 - fTemp308 * fRec176[1];
			fVec72[IOTA0 & 16383] = 0.70710677f * fTemp418 + 0.70710677f * (fTemp417 - fTemp416);
			float fTemp419 = fVec72[(IOTA0 - iTemp192) & 16383];
			fVec73[0] = fTemp419;
			float fTemp420 = fVec73[1] - fTemp194 * (fRec178[1] - fTemp419) / fTemp191;
			fRec178[0] = ((std::fabs(fTemp420) > 1.1754944e-38f) ? fTemp420 : 0.0f);
			fRec176[0] = ((std::fabs(fRec178[0]) > 1.1754944e-38f) ? fRec178[0] : 0.0f);
			float fTemp421 = 0.9999f * (fRec180[1] + float(iTemp2 * iSlow63)) + fSlow64;
			fRec180[0] = ((std::fabs(fTemp421) > 1.1754944e-38f) ? fTemp421 : 0.0f);
			float fTemp422 = fRec180[0] + -1.49999f;
			float fTemp423 = std::floor(fTemp422);
			fVec74[IOTA0 & 16383] = 0.70710677f * fTemp418 + 0.70710677f * (fTemp416 - fTemp417);
			float fTemp424 = fVec74[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp422)))))) & 16383];
			fVec75[0] = fTemp424;
			float fTemp425 = fVec75[1] - (fTemp423 + (2.0f - fRec180[0])) * (fRec179[1] - fTemp424) / (fRec180[0] - fTemp423);
			fRec179[0] = ((std::fabs(fTemp425) > 1.1754944e-38f) ? fTemp425 : 0.0f);
			fRec177[0] = ((std::fabs(fRec179[0]) > 1.1754944e-38f) ? fRec179[0] : 0.0f);
			float fTemp426 = fRec176[1] * fTemp309 + fTemp308 * fTemp414;
			float fTemp427 = 0.9999f * (fRec184[1] + float(iTemp2 * iSlow65)) + fSlow66;
			fRec184[0] = ((std::fabs(fTemp427) > 1.1754944e-38f) ? fTemp427 : 0.0f);
			float fTemp428 = fRec184[0] + -1.49999f;
			float fTemp429 = std::floor(fTemp428);
			float fTemp430 = fTemp309 * fRec177[1] + fTemp308 * fTemp415;
			float fTemp431 = fTemp309 * fTemp430;
			float fTemp432 = fTemp308 * fRec182[1];
			float fTemp433 = fTemp309 * fTemp426 - fRec181[1] * fTemp308;
			fVec76[IOTA0 & 16383] = 0.70710677f * fTemp433 + 0.70710677f * (fTemp432 - fTemp431);
			float fTemp434 = fVec76[(IOTA0 - std::min<int>(8192, std::max<int>(0, int(std::min<float>(8192.0f, std::max<float>(0.0f, fTemp428)))))) & 16383];
			fVec77[0] = fTemp434;
			float fTemp435 = fVec77[1] - (fTemp429 + (2.0f - fRec184[0])) * (fRec183[1] - fTemp434) / (fRec184[0] - fTemp429);
			fRec183[0] = ((std::fabs(fTemp435) > 1.1754944e-38f) ? fTemp435 : 0.0f);
			fRec181[0] = ((std::fabs(fRec183[0]) > 1.1754944e-38f) ? fRec183[0] : 0.0f);
			fVec78[IOTA0 & 16383] = 0.70710677f * fTemp433 + 0.70710677f * (fTemp431 - fTemp432);
			float fTemp436 = fVec78[(IOTA0 - iTemp210) & 16383];
			fVec79[0] = fTemp436;
			float fTemp437 = fVec79[1] - fTemp212 * (fRec185[1] - fTemp436) / fTemp209;
			fRec185[0] = ((std::fabs(fTemp437) > 1.1754944e-38f) ? fTemp437 : 0.0f);
			fRec182[0] = ((std::fabs(fRec185[0]) > 1.1754944e-38f) ? fRec185[0] : 0.0f);
			float fTemp438 = fRec181[1] * fTemp309 + fTemp308 * fTemp426;
			fRec1[0] = ((std::fabs(fTemp438) > 1.1754944e-38f) ? fTemp438 : 0.0f);
			float fTemp439 = fTemp309 * fRec182[1] + fTemp308 * fTemp430;
			fRec2[0] = ((std::fabs(fTemp439) > 1.1754944e-38f) ? fTemp439 : 0.0f);
			float fTemp440 = 1.0f - fRec0[0] - fSlow1;
			output0[i0] = FAUSTFLOAT(fTemp296 * fTemp440 + fRec1[0] * fTemp1);
			output1[i0] = FAUSTFLOAT(fTemp388 * fTemp440 + fRec2[0] * fTemp1);
			fVec0[1] = fVec0[0];
			iVec1[1] = iVec1[0];
			fRec0[1] = fRec0[0];
			fRec24[1] = fRec24[0];
			fRec26[1] = fRec26[0];
			fRec30[1] = fRec30[0];
			IOTA0 = IOTA0 + 1;
			fVec3[1] = fVec3[0];
			fRec29[1] = fRec29[0];
			fRec27[1] = fRec27[0];
			fRec32[1] = fRec32[0];
			fVec5[1] = fVec5[0];
			fRec31[1] = fRec31[0];
			fRec28[1] = fRec28[0];
			fRec36[1] = fRec36[0];
			fVec7[1] = fVec7[0];
			fRec35[1] = fRec35[0];
			fRec33[1] = fRec33[0];
			fRec38[1] = fRec38[0];
			fVec9[1] = fVec9[0];
			fRec37[1] = fRec37[0];
			fRec34[1] = fRec34[0];
			fRec42[1] = fRec42[0];
			fVec11[1] = fVec11[0];
			fRec41[1] = fRec41[0];
			fRec39[1] = fRec39[0];
			fRec44[1] = fRec44[0];
			fVec13[1] = fVec13[0];
			fRec43[1] = fRec43[0];
			fRec40[1] = fRec40[0];
			fRec48[1] = fRec48[0];
			fVec15[1] = fVec15[0];
			fRec47[1] = fRec47[0];
			fRec45[1] = fRec45[0];
			fRec50[1] = fRec50[0];
			fVec17[1] = fVec17[0];
			fRec49[1] = fRec49[0];
			fRec46[1] = fRec46[0];
			fRec54[1] = fRec54[0];
			fVec19[1] = fVec19[0];
			fRec53[1] = fRec53[0];
			fRec51[1] = fRec51[0];
			fRec56[1] = fRec56[0];
			fVec21[1] = fVec21[0];
			fRec55[1] = fRec55[0];
			fRec52[1] = fRec52[0];
			fVec23[1] = fVec23[0];
			fRec59[1] = fRec59[0];
			fRec57[1] = fRec57[0];
			fRec58[1] = fRec58[0];
			fVec24[1] = fVec24[0];
			fRec60[1] = fRec60[0];
			fVec26[1] = fVec26[0];
			fRec25[1] = fRec25[0];
			fRec64[1] = fRec64[0];
			fRec66[1] = fRec66[0];
			fVec29[1] = fVec29[0];
			fRec65[1] = fRec65[0];
			fVec31[1] = fVec31[0];
			fRec63[1] = fRec63[0];
			fRec61[1] = fRec61[0];
			fRec68[1] = fRec68[0];
			fVec33[1] = fVec33[0];
			fRec67[1] = fRec67[0];
			fRec62[1] = fRec62[0];
			fRec72[1] = fRec72[0];
			fVec35[1] = fVec35[0];
			fRec71[1] = fRec71[0];
			fRec69[1] = fRec69[0];
			fRec74[1] = fRec74[0];
			fVec37[1] = fVec37[0];
			fRec73[1] = fRec73[0];
			fRec70[1] = fRec70[0];
			fRec78[1] = fRec78[0];
			fVec39[1] = fVec39[0];
			fRec77[1] = fRec77[0];
			fRec75[1] = fRec75[0];
			fRec80[1] = fRec80[0];
			fVec41[1] = fVec41[0];
			fRec79[1] = fRec79[0];
			fRec76[1] = fRec76[0];
			fRec84[1] = fRec84[0];
			fVec43[1] = fVec43[0];
			fRec83[1] = fRec83[0];
			fRec81[1] = fRec81[0];
			fRec86[1] = fRec86[0];
			fVec45[1] = fVec45[0];
			fRec85[1] = fRec85[0];
			fRec82[1] = fRec82[0];
			fRec90[1] = fRec90[0];
			fVec47[1] = fVec47[0];
			fRec89[1] = fRec89[0];
			fRec87[1] = fRec87[0];
			fRec92[1] = fRec92[0];
			fVec49[1] = fVec49[0];
			fRec91[1] = fRec91[0];
			fRec88[1] = fRec88[0];
			fVec52[1] = fVec52[0];
			fRec23[1] = fRec23[0];
			fRec21[1] = fRec21[0];
			fRec18[1] = fRec18[0];
			fRec19[1] = fRec19[0];
			fRec15[1] = fRec15[0];
			fRec16[1] = fRec16[0];
			fRec13[1] = fRec13[0];
			fRec8[1] = fRec8[0];
			fRec9[1] = fRec9[0];
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fVec53[1] = fVec53[0];
			fRec93[1] = fRec93[0];
			fRec100[1] = fRec100[0];
			fRec97[1] = fRec97[0];
			fRec98[1] = fRec98[0];
			fRec94[1] = fRec94[0];
			fRec95[1] = fRec95[0];
			fVec54[1] = fVec54[0];
			fRec102[1] = fRec102[0];
			fRec116[1] = fRec116[0];
			fRec111[1] = fRec111[0];
			fRec112[1] = fRec112[0];
			fRec106[1] = fRec106[0];
			fRec107[1] = fRec107[0];
			fRec103[1] = fRec103[0];
			fRec104[1] = fRec104[0];
			fVec56[1] = fVec56[0];
			fVec57[1] = fVec57[0];
			fRec119[1] = fRec119[0];
			fRec118[1] = fRec118[0];
			fVec58[1] = fVec58[0];
			fRec120[1] = fRec120[0];
			fRec145[1] = fRec145[0];
			fVec61[1] = fVec61[0];
			fRec144[1] = fRec144[0];
			fRec142[1] = fRec142[0];
			fRec139[1] = fRec139[0];
			fRec140[1] = fRec140[0];
			fRec136[1] = fRec136[0];
			fRec137[1] = fRec137[0];
			fRec134[1] = fRec134[0];
			fRec129[1] = fRec129[0];
			fRec130[1] = fRec130[0];
			fRec124[1] = fRec124[0];
			fRec125[1] = fRec125[0];
			fRec152[1] = fRec152[0];
			fRec149[1] = fRec149[0];
			fRec150[1] = fRec150[0];
			fRec146[1] = fRec146[0];
			fRec147[1] = fRec147[0];
			fRec167[1] = fRec167[0];
			fRec162[1] = fRec162[0];
			fRec163[1] = fRec163[0];
			fRec157[1] = fRec157[0];
			fRec158[1] = fRec158[0];
			fRec154[1] = fRec154[0];
			fRec155[1] = fRec155[0];
			fVec63[1] = fVec63[0];
			fRec169[1] = fRec169[0];
			fVec65[1] = fVec65[0];
			fRec123[1] = fRec123[0];
			fRec121[1] = fRec121[0];
			fRec171[1] = fRec171[0];
			fVec67[1] = fVec67[0];
			fRec170[1] = fRec170[0];
			fRec122[1] = fRec122[0];
			fVec69[1] = fVec69[0];
			fRec174[1] = fRec174[0];
			fRec172[1] = fRec172[0];
			fVec71[1] = fVec71[0];
			fRec175[1] = fRec175[0];
			fRec173[1] = fRec173[0];
			fVec73[1] = fVec73[0];
			fRec178[1] = fRec178[0];
			fRec176[1] = fRec176[0];
			fRec180[1] = fRec180[0];
			fVec75[1] = fVec75[0];
			fRec179[1] = fRec179[0];
			fRec177[1] = fRec177[0];
			fRec184[1] = fRec184[0];
			fVec77[1] = fVec77[0];
			fRec183[1] = fRec183[0];
			fRec181[1] = fRec181[0];
			fVec79[1] = fVec79[0];
			fRec185[1] = fRec185[0];
			fRec182[1] = fRec182[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
		}
	}

};

#endif
