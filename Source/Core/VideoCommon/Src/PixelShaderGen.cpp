// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <stdio.h>
#include <cmath>
#include <assert.h>
#include <locale.h>

#include "LightingShaderGen.h"
#include "PixelShaderGen.h"
#include "XFMemory.h"  // for texture projection mode
#include "BPMemory.h"
#include "VideoConfig.h"
#include "NativeVertexFormat.h"


//   old tev->pixelshader notes
//
//   color for this stage (alpha, color) is given by bpmem.tevorders[0].colorchan0
//   konstant for this stage (alpha, color) is given by bpmem.tevksel
//   inputs are given by bpmem.combiners[0].colorC.a/b/c/d     << could be current chan color
//   according to GXTevColorArg table above
//   output is given by .outreg
//   tevtemp is set according to swapmodetables and

template<class T> static void WriteStage(T& out, pixel_shader_uid_data& uid_data, int n, API_TYPE ApiType);
template<class T> static void SampleTexture(T& out, const char *destination, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType);
template<class T> static void WriteAlphaTest(T& out, pixel_shader_uid_data& uid_data, API_TYPE ApiType,DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth);
template<class T> static void WriteFog(T& out, pixel_shader_uid_data& uid_data);

static const char *tevKSelTableC[] = // KCSEL
{
	"1.0f,1.0f,1.0f",    // 1   = 0x00
	"0.875f,0.875f,0.875f", // 7_8 = 0x01
	"0.75f,0.75f,0.75f",    // 3_4 = 0x02
	"0.625f,0.625f,0.625f", // 5_8 = 0x03
	"0.5f,0.5f,0.5f",       // 1_2 = 0x04
	"0.375f,0.375f,0.375f", // 3_8 = 0x05
	"0.25f,0.25f,0.25f",    // 1_4 = 0x06
	"0.125f,0.125f,0.125f", // 1_8 = 0x07
	"ERROR1", // 0x08
	"ERROR2", // 0x09
	"ERROR3", // 0x0a
	"ERROR4", // 0x0b
	I_KCOLORS"[0].rgb", // K0 = 0x0C
	I_KCOLORS"[1].rgb", // K1 = 0x0D
	I_KCOLORS"[2].rgb", // K2 = 0x0E
	I_KCOLORS"[3].rgb", // K3 = 0x0F
	I_KCOLORS"[0].rrr", // K0_R = 0x10
	I_KCOLORS"[1].rrr", // K1_R = 0x11
	I_KCOLORS"[2].rrr", // K2_R = 0x12
	I_KCOLORS"[3].rrr", // K3_R = 0x13
	I_KCOLORS"[0].ggg", // K0_G = 0x14
	I_KCOLORS"[1].ggg", // K1_G = 0x15
	I_KCOLORS"[2].ggg", // K2_G = 0x16
	I_KCOLORS"[3].ggg", // K3_G = 0x17
	I_KCOLORS"[0].bbb", // K0_B = 0x18
	I_KCOLORS"[1].bbb", // K1_B = 0x19
	I_KCOLORS"[2].bbb", // K2_B = 0x1A
	I_KCOLORS"[3].bbb", // K3_B = 0x1B
	I_KCOLORS"[0].aaa", // K0_A = 0x1C
	I_KCOLORS"[1].aaa", // K1_A = 0x1D
	I_KCOLORS"[2].aaa", // K2_A = 0x1E
	I_KCOLORS"[3].aaa", // K3_A = 0x1F
};

static const char *tevKSelTableA[] = // KASEL
{
	"1.0f",  // 1   = 0x00
	"0.875f",// 7_8 = 0x01
	"0.75f", // 3_4 = 0x02
	"0.625f",// 5_8 = 0x03
	"0.5f",  // 1_2 = 0x04
	"0.375f",// 3_8 = 0x05
	"0.25f", // 1_4 = 0x06
	"0.125f",// 1_8 = 0x07
	"ERROR5", // 0x08
	"ERROR6", // 0x09
	"ERROR7", // 0x0a
	"ERROR8", // 0x0b
	"ERROR9", // 0x0c
	"ERROR10", // 0x0d
	"ERROR11", // 0x0e
	"ERROR12", // 0x0f
	I_KCOLORS"[0].r", // K0_R = 0x10
	I_KCOLORS"[1].r", // K1_R = 0x11
	I_KCOLORS"[2].r", // K2_R = 0x12
	I_KCOLORS"[3].r", // K3_R = 0x13
	I_KCOLORS"[0].g", // K0_G = 0x14
	I_KCOLORS"[1].g", // K1_G = 0x15
	I_KCOLORS"[2].g", // K2_G = 0x16
	I_KCOLORS"[3].g", // K3_G = 0x17
	I_KCOLORS"[0].b", // K0_B = 0x18
	I_KCOLORS"[1].b", // K1_B = 0x19
	I_KCOLORS"[2].b", // K2_B = 0x1A
	I_KCOLORS"[3].b", // K3_B = 0x1B
	I_KCOLORS"[0].a", // K0_A = 0x1C
	I_KCOLORS"[1].a", // K1_A = 0x1D
	I_KCOLORS"[2].a", // K2_A = 0x1E
	I_KCOLORS"[3].a", // K3_A = 0x1F
};

static const char *tevScaleTable[] = // CS
{
	"1.0f",  // SCALE_1
	"2.0f",  // SCALE_2
	"4.0f",  // SCALE_4
	"0.5f",  // DIVIDE_2
};

static const char *tevBiasTable[] = // TB
{
	"",       // ZERO,
	"+0.5f",  // ADDHALF,
	"-0.5f",  // SUBHALF,
	"",
};

static const char *tevOpTable[] = { // TEV
	"+",      // TEVOP_ADD = 0,
	"-",      // TEVOP_SUB = 1,
};

static const char *tevCInputTable[] = // CC
{
	"(prev.rgb)",         // CPREV,
	"(prev.aaa)",         // APREV,
	"(c0.rgb)",           // C0,
	"(c0.aaa)",           // A0,
	"(c1.rgb)",           // C1,
	"(c1.aaa)",           // A1,
	"(c2.rgb)",           // C2,
	"(c2.aaa)",           // A2,
	"(textemp.rgb)",      // TEXC,
	"(textemp.aaa)",      // TEXA,
	"(rastemp.rgb)",      // RASC,
	"(rastemp.aaa)",      // RASA,
	"float3(1.0f, 1.0f, 1.0f)",              // ONE
	"float3(0.5f, 0.5f, 0.5f)",              // HALF
	"(konsttemp.rgb)", //"konsttemp.rgb",    // KONST
	"float3(0.0f, 0.0f, 0.0f)",              // ZERO
	///aded extra values to map clamped values
	"(cprev.rgb)",        // CPREV,
	"(cprev.aaa)",        // APREV,
	"(cc0.rgb)",          // C0,
	"(cc0.aaa)",          // A0,
	"(cc1.rgb)",          // C1,
	"(cc1.aaa)",          // A1,
	"(cc2.rgb)",          // C2,
	"(cc2.aaa)",          // A2,
	"(textemp.rgb)",      // TEXC,
	"(textemp.aaa)",      // TEXA,
	"(crastemp.rgb)",     // RASC,
	"(crastemp.aaa)",     // RASA,
	"float3(1.0f, 1.0f, 1.0f)",              // ONE
	"float3(0.5f, 0.5f, 0.5f)",              // HALF
	"(ckonsttemp.rgb)", //"konsttemp.rgb",   // KONST
	"float3(0.0f, 0.0f, 0.0f)",              // ZERO
	"PADERROR1", "PADERROR2", "PADERROR3", "PADERROR4"
};

static const char *tevAInputTable[] = // CA
{
	"prev",            // APREV,
	"c0",              // A0,
	"c1",              // A1,
	"c2",              // A2,
	"textemp",         // TEXA,
	"rastemp",         // RASA,
	"konsttemp",       // KONST,  (hw1 had quarter)
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // ZERO
	///aded extra values to map clamped values
	"cprev",            // APREV,
	"cc0",              // A0,
	"cc1",              // A1,
	"cc2",              // A2,
	"textemp",          // TEXA,
	"crastemp",         // RASA,
	"ckonsttemp",       // KONST,  (hw1 had quarter)
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // ZERO
	"PADERROR5", "PADERROR6", "PADERROR7", "PADERROR8",
	"PADERROR9", "PADERROR10", "PADERROR11", "PADERROR12",
};

static const char *tevRasTable[] =
{
	"colors_0",
	"colors_1",
	"ERROR13", //2
	"ERROR14", //3
	"ERROR15", //4
	"float4(alphabump,alphabump,alphabump,alphabump)", // use bump alpha
	"(float4(alphabump,alphabump,alphabump,alphabump)*(255.0f/248.0f))", //normalized
	"float4(0.0f, 0.0f, 0.0f, 0.0f)", // zero
};

//static const char *tevTexFunc[] = { "tex2D", "texRECT" };

static const char *tevCOutputTable[]  = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
static const char *tevAOutputTable[]  = { "prev.a", "c0.a", "c1.a", "c2.a" };
static const char *tevIndAlphaSel[]   = {"", "x", "y", "z"};
//static const char *tevIndAlphaScale[] = {"", "*32", "*16", "*8"};
static const char *tevIndAlphaScale[] = {"*(248.0f/255.0f)", "*(224.0f/255.0f)", "*(240.0f/255.0f)", "*(248.0f/255.0f)"};
static const char *tevIndBiasField[]  = {"", "x", "y", "xy", "z", "xz", "yz", "xyz"}; // indexed by bias
static const char *tevIndBiasAdd[]    = {"-128.0f", "1.0f", "1.0f", "1.0f" }; // indexed by fmt
static const char *tevIndWrapStart[]  = {"0.0f", "256.0f", "128.0f", "64.0f", "32.0f", "16.0f", "0.001f" };
static const char *tevIndFmtScale[]   = {"255.0f", "31.0f", "15.0f", "7.0f" };

static char swapModeTable[4][5];

static char text[16384];

struct RegisterState
{
	bool ColorNeedOverflowControl;
	bool AlphaNeedOverflowControl;
	bool AuxStored;
};

static RegisterState RegisterStates[4];

static void BuildSwapModeTable()
{
	static const char *swapColors = "rgba";
	for (int i = 0; i < 4; i++)
	{
		swapModeTable[i][0] = swapColors[bpmem.tevksel[i*2].swap1];
		swapModeTable[i][1] = swapColors[bpmem.tevksel[i*2].swap2];
		swapModeTable[i][2] = swapColors[bpmem.tevksel[i*2+1].swap1];
		swapModeTable[i][3] = swapColors[bpmem.tevksel[i*2+1].swap2];
		swapModeTable[i][4] = '\0';
	}
}

const char* WriteRegister(API_TYPE ApiType, const char *prefix, const u32 num)
{
	if (ApiType == API_OPENGL)
		return ""; // Nothing to do here

	static char result[64];
	sprintf(result, " : register(%s%d)", prefix, num);
	return result;
}

const char *WriteLocation(API_TYPE ApiType)
{
	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		return "";

	static char result[64];
	sprintf(result, "uniform ");
	return result;
}

template<class T>
void GeneratePixelShader(T& out, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	// TODO: Where does this TODO belong again...?
	// TODO: Can be optimized if using alpha pass

	// Non-uid template parameters will write to the dummy data (=> gets optimized out)
	pixel_shader_uid_data dummy_data;
	pixel_shader_uid_data& uid_data = (&out.template GetUidData<pixel_shader_uid_data>() != NULL)
										? out.template GetUidData<pixel_shader_uid_data>() : dummy_data;

	out.SetBuffer(text);
	if (out.GetBuffer() != NULL)
		setlocale(LC_NUMERIC, "C"); // Reset locale for compilation

	text[sizeof(text) - 1] = 0x7C;  // canary

	unsigned int numStages = bpmem.genMode.numtevstages + 1;
	unsigned int numTexgen = bpmem.genMode.numtexgens;

	bool per_pixel_depth = bpmem.ztex2.op != ZTEXTURE_DISABLE && !bpmem.zcontrol.early_ztest && bpmem.zmode.testenable;

	out.Write("//Pixel Shader for TEV stages\n");
	out.Write("//%i TEV stages, %i texgens, XXX IND stages\n",
		numStages, numTexgen/*, bpmem.genMode.numindstages*/);

//	uid_data.components = components; // TODO: Enable once per pixel lighting is implemented again
	uid_data.dstAlphaMode = dstAlphaMode;

	uid_data.genMode.numindstages = bpmem.genMode.numindstages;
	uid_data.genMode.numtevstages = bpmem.genMode.numtevstages;
	uid_data.genMode.numtexgens = bpmem.genMode.numtexgens;

	if (ApiType == API_OPENGL)
	{
		// Fmod implementation gleaned from Nvidia
		// At http://http.developer.nvidia.com/Cg/fmod.html
		out.Write("float fmod( float x, float y )\n");
		out.Write("{\n");
		out.Write("\tfloat z = fract( abs( x / y) ) * abs( y );\n");
		out.Write("\treturn (x < 0) ? -z : z;\n");
		out.Write("}\n\n");

		// Declare samplers
		for (int i = 0; i < 8; ++i)
			out.Write("uniform sampler2D samp%d;\n", i);
	}
	else
	{
		// Declare samplers
		for (int i = 0; i < 8; ++i)
			out.Write("%s samp%d %s;\n", (ApiType == API_D3D11) ? "sampler" : "uniform sampler2D", i, WriteRegister(ApiType, "s", i));

		if (ApiType == API_D3D11)
		{
			out.Write("\n");
			for (int i = 0; i < 8; ++i)
			{
				out.Write("Texture2D Tex%d : register(t%d);\n", i, i);
			}
		}
	}
	out.Write("\n");

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		out.Write("layout(std140) uniform PSBlock {\n");

	out.Write("\t%sfloat4 " I_COLORS"[4] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_COLORS)); // TODO: first element not used??
	out.Write("\t%sfloat4 " I_KCOLORS"[4] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_KCOLORS));
	out.Write("\t%sfloat4 " I_ALPHA"[1] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_ALPHA)); // TODO: Why is this an array...-.-
	out.Write("\t%sfloat4 " I_TEXDIMS"[8] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_TEXDIMS));
	out.Write("\t%sfloat4 " I_ZBIAS"[2] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_ZBIAS));
	out.Write("\t%sfloat4 " I_INDTEXSCALE"[2] %s;\n", WriteLocation(ApiType),  WriteRegister(ApiType, "c", C_INDTEXSCALE));
	out.Write("\t%sfloat4 " I_INDTEXMTX"[6] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_INDTEXMTX));
	out.Write("\t%sfloat4 " I_FOG"[3] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_FOG));

	// For pixel lighting - TODO: Should only be defined when per pixel lighting is enabled!
    out.Write("struct Light { float4 col; float4 cosatt; float4 distatt; float4 pos; float4 dir; };\n");
	out.Write("\t%sLight " I_PLIGHTS"[8] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_PLIGHTS));
	out.Write("\t%sfloat4 " I_PMATERIALS"[4] %s;\n", WriteLocation(ApiType), WriteRegister(ApiType, "c", C_PMATERIALS));

	if (g_ActiveConfig.backend_info.bSupportsGLSLUBO)
		out.Write("};\n");

	if (ApiType == API_OPENGL)
	{
		out.Write("out float4 ocol0;\n");
		if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
			out.Write("out float4 ocol1;\n");
		
		if (per_pixel_depth)
			out.Write("#define depth gl_FragDepth\n");
		out.Write("float4 rawpos = gl_FragCoord;\n");

		out.Write("VARYIN float4 colors_02;\n");
		out.Write("VARYIN float4 colors_12;\n");
		out.Write("float4 colors_0 = colors_02;\n");
		out.Write("float4 colors_1 = colors_12;\n");

		// compute window position if needed because binding semantic WPOS is not widely supported
		// Let's set up attributes
		if (xfregs.numTexGen.numTexGens < 7)
		{
			for (int i = 0; i < 8; ++i)
			{
				out.Write("VARYIN float3 uv%d_2;\n", i);
				out.Write("float3 uv%d = uv%d_2;\n", i, i);
			}
			out.Write("VARYIN float4 clipPos_2;\n");
			out.Write("float4 clipPos = clipPos_2;\n");
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				out.Write("VARYIN float4 Normal_2;\n");
				out.Write("float4 Normal = Normal_2;\n");
			}
		}
		else
		{
			// wpos is in w of first 4 texcoords
			if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				for (int i = 0; i < 8; ++i)
				{
					out.Write("VARYIN float4 uv%d_2;\n", i);
					out.Write("float4 uv%d = uv%d_2;\n", i, i);
				}
			}
			else
			{
				for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
				{
					out.Write("VARYIN float%d uv%d_2;\n", i < 4 ? 4 : 3 , i);
					out.Write("float%d uv%d = uv%d_2;\n", i < 4 ? 4 : 3 , i, i);
				}
			}
			out.Write("float4 clipPos;\n");
		}
		out.Write("void main()\n{\n");
	}
	else
	{
		out.Write("void main(\n");
		if(ApiType != API_D3D11)
		{
			out.Write("  out float4 ocol0 : COLOR0,%s%s\n  in float4 rawpos : %s,\n",
				dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : COLOR1," : "",
				per_pixel_depth ? "\n  out float depth : DEPTH," : "",
				ApiType & API_D3D9_SM20 ? "POSITION" : "VPOS");
		}
		else
		{
			out.Write("  out float4 ocol0 : SV_Target0,%s%s\n  in float4 rawpos : SV_Position,\n",
				dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND ? "\n  out float4 ocol1 : SV_Target1," : "",
				per_pixel_depth ? "\n  out float depth : SV_Depth," : "");
		}

		out.Write("  in float4 colors_0 : COLOR0,\n");
		out.Write("  in float4 colors_1 : COLOR1");

		// compute window position if needed because binding semantic WPOS is not widely supported
		if (numTexgen < 7)
		{
			for (unsigned int i = 0; i < numTexgen; ++i)
				out.Write(",\n  in float3 uv%d : TEXCOORD%d", i, i);
			out.Write(",\n  in float4 clipPos : TEXCOORD%d", numTexgen);
			if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
				out.Write(",\n  in float4 Normal : TEXCOORD%d", numTexgen + 1);
			out.Write("        ) {\n");
		}
		else
		{
			// wpos is in w of first 4 texcoords
			if(g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
			{
				for (int i = 0; i < 8; ++i)
					out.Write(",\n  in float4 uv%d : TEXCOORD%d", i, i);
			}
			else
			{
				for (unsigned int i = 0; i < xfregs.numTexGen.numTexGens; ++i)
					out.Write(",\n  in float%d uv%d : TEXCOORD%d", i < 4 ? 4 : 3 , i, i);
			}
			out.Write("        ) {\n");
			out.Write("\tfloat4 clipPos = float4(0.0f, 0.0f, 0.0f, 0.0f);");
		}
	}

	out.Write("  float4 c0 = " I_COLORS"[1], c1 = " I_COLORS"[2], c2 = " I_COLORS"[3], prev = float4(0.0f, 0.0f, 0.0f, 0.0f), textemp = float4(0.0f, 0.0f, 0.0f, 0.0f), rastemp = float4(0.0f, 0.0f, 0.0f, 0.0f), konsttemp = float4(0.0f, 0.0f, 0.0f, 0.0f);\n"
			"  float3 comp16 = float3(1.0f, 255.0f, 0.0f), comp24 = float3(1.0f, 255.0f, 255.0f*255.0f);\n"
			"  float alphabump=0.0f;\n"
			"  float3 tevcoord=float3(0.0f, 0.0f, 0.0f);\n"
			"  float2 wrappedcoord=float2(0.0f,0.0f), tempcoord=float2(0.0f,0.0f);\n"
			"  float4 cc0=float4(0.0f,0.0f,0.0f,0.0f), cc1=float4(0.0f,0.0f,0.0f,0.0f);\n"
			"  float4 cc2=float4(0.0f,0.0f,0.0f,0.0f), cprev=float4(0.0f,0.0f,0.0f,0.0f);\n"
			"  float4 crastemp=float4(0.0f,0.0f,0.0f,0.0f),ckonsttemp=float4(0.0f,0.0f,0.0f,0.0f);\n\n");

	if (g_ActiveConfig.bEnablePixelLighting && g_ActiveConfig.backend_info.bSupportsPixelLighting)
	{
		uid_data.xfregs_numTexGen_numTexGens = xfregs.numTexGen.numTexGens;
		if (xfregs.numTexGen.numTexGens < 7)
		{
			out.Write("\tfloat3 _norm0 = normalize(Normal.xyz);\n\n");
			out.Write("\tfloat3 pos = float3(clipPos.x,clipPos.y,Normal.w);\n");
		}
		else
		{
			out.Write("\tfloat3 _norm0 = normalize(float3(uv4.w,uv5.w,uv6.w));\n\n");
			out.Write("\tfloat3 pos = float3(uv0.w,uv1.w,uv7.w);\n");
		}

		out.Write("\tfloat4 mat, lacc;\n"
				"\tfloat3 ldir, h;\n"
				"\tfloat dist, dist2, attn;\n");

		out.SetConstantsUsed(C_PLIGHTS, C_PLIGHTS+39); // TODO: Can be optimized further
		out.SetConstantsUsed(C_PMATERIALS, C_PMATERIALS+3);
		GenerateLightingShader<T>(out, uid_data.lighting, components, I_PMATERIALS, I_PLIGHTS, "colors_", "colors_");
	}

	if (numTexgen < 7)
		out.Write("\tclipPos = float4(rawpos.x, rawpos.y, clipPos.z, clipPos.w);\n");
	else
		out.Write("\tfloat4 clipPos = float4(rawpos.x, rawpos.y, uv2.w, uv3.w);\n");

	// HACK to handle cases where the tex gen is not enabled
	if (numTexgen == 0)
	{
		out.Write("\tfloat3 uv0 = float3(0.0f, 0.0f, 0.0f);\n");
	}
	else
	{
		out.SetConstantsUsed(C_TEXDIMS, C_TEXDIMS+numTexgen-1);
		for (unsigned int i = 0; i < numTexgen; ++i)
		{
			// optional perspective divides
			uid_data.texMtxInfo[i].projection = xfregs.texMtxInfo[i].projection;
			if (xfregs.texMtxInfo[i].projection == XF_TEXPROJ_STQ)
			{
				out.Write("\tif (uv%d.z != 0.0f)", i);
				out.Write("\t\tuv%d.xy = uv%d.xy / uv%d.z;\n", i, i, i);
			}

			out.Write("uv%d.xy = uv%d.xy * " I_TEXDIMS"[%d].zw;\n", i, i, i);
		}
	}

	// indirect texture map lookup
	int nIndirectStagesUsed = 0;
	if (bpmem.genMode.numindstages > 0)
	{
		for (unsigned int i = 0; i < numStages; ++i)
		{
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				nIndirectStagesUsed |= 1 << bpmem.tevind[i].bt;
		}
	}

	uid_data.nIndirectStagesUsed = nIndirectStagesUsed;
	for (u32 i = 0; i < bpmem.genMode.numindstages; ++i)
	{
		if (nIndirectStagesUsed & (1 << i))
		{
			unsigned int texcoord = bpmem.tevindref.getTexCoord(i);
			unsigned int texmap = bpmem.tevindref.getTexMap(i);

			uid_data.tevindref.SetValues(i, texcoord, texmap);
			if (texcoord < numTexgen)
			{
				out.SetConstantsUsed(C_INDTEXSCALE+i/2,C_INDTEXSCALE+i/2);
				out.Write("\ttempcoord = uv%d.xy * " I_INDTEXSCALE"[%d].%s;\n", texcoord, i/2, (i&1)?"zw":"xy");
			}
			else
				out.Write("\ttempcoord = float2(0.0f, 0.0f);\n");

			char buffer[32];
			sprintf(buffer, "float3 indtex%d", i);
			SampleTexture<T>(out, buffer, "tempcoord", "abg", texmap, ApiType);
		}
	}

	RegisterStates[0].AlphaNeedOverflowControl = false;
	RegisterStates[0].ColorNeedOverflowControl = false;
	RegisterStates[0].AuxStored = false;
	for(int i = 1; i < 4; i++)
	{
		RegisterStates[i].AlphaNeedOverflowControl = true;
		RegisterStates[i].ColorNeedOverflowControl = true;
		RegisterStates[i].AuxStored = false;
	}

	// Uid fields for BuildSwapModeTable are set in WriteStage
	BuildSwapModeTable();
	for (unsigned int i = 0; i < numStages; i++)
		WriteStage<T>(out, uid_data, i, ApiType); // build the equation for this stage

	if (numStages)
	{
		// The results of the last texenv stage are put onto the screen,
		// regardless of the used destination register
		if(bpmem.combiners[numStages - 1].colorC.dest != 0)
		{
			bool retrieveFromAuxRegister = !RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].ColorNeedOverflowControl && RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].AuxStored;
			out.Write("\tprev.rgb = %s%s;\n", retrieveFromAuxRegister ? "c" : "" , tevCOutputTable[bpmem.combiners[numStages - 1].colorC.dest]);
			RegisterStates[0].ColorNeedOverflowControl = RegisterStates[bpmem.combiners[numStages - 1].colorC.dest].ColorNeedOverflowControl;
		}
		if(bpmem.combiners[numStages - 1].alphaC.dest != 0)
		{
			bool retrieveFromAuxRegister = !RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AlphaNeedOverflowControl && RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AuxStored;
			out.Write("\tprev.a = %s%s;\n", retrieveFromAuxRegister ? "c" : "" , tevAOutputTable[bpmem.combiners[numStages - 1].alphaC.dest]);
			RegisterStates[0].AlphaNeedOverflowControl = RegisterStates[bpmem.combiners[numStages - 1].alphaC.dest].AlphaNeedOverflowControl;
		}
	}
	// emulation of unsigned 8 overflow when casting if needed
	if(RegisterStates[0].AlphaNeedOverflowControl || RegisterStates[0].ColorNeedOverflowControl)
		out.Write("\tprev = frac(prev * (255.0f/256.0f)) * (256.0f/255.0f);\n");

	AlphaTest::TEST_RESULT Pretest = bpmem.alpha_test.TestResult();
	uid_data.Pretest = Pretest;
	if (Pretest == AlphaTest::UNDETERMINED)
		WriteAlphaTest<T>(out, uid_data, ApiType, dstAlphaMode, per_pixel_depth);

	
	// the screen space depth value = far z + (clip z / clip w) * z range
	if(ApiType == API_OPENGL || ApiType == API_D3D11)
		out.Write("float zCoord = rawpos.z;\n");
	else
	{
		out.SetConstantsUsed(C_ZBIAS+1, C_ZBIAS+1);
		// dx9 doesn't support 4 component position, so we have to calculate it again
		out.Write("float zCoord = " I_ZBIAS"[1].x + (clipPos.z / clipPos.w) * " I_ZBIAS"[1].y;\n");
	}

	// Note: depth textures are disabled if early depth test is enabled
	uid_data.ztex.op = bpmem.ztex2.op;
	uid_data.per_pixel_depth = per_pixel_depth;
	uid_data.fog.fsel = bpmem.fog.c_proj_fsel.fsel;

	// depth texture can safely be ignored if the result won't be written to the depth buffer (early_ztest) and isn't used for fog either
	bool skip_ztexture = !per_pixel_depth && !bpmem.fog.c_proj_fsel.fsel;
	if (bpmem.ztex2.op != ZTEXTURE_DISABLE && !skip_ztexture)
	{
		// use the texture input of the last texture stage (textemp), hopefully this has been read and is in correct format...
		out.SetConstantsUsed(C_ZBIAS, C_ZBIAS+1);
		out.Write("zCoord = dot(" I_ZBIAS"[0].xyzw, textemp.xyzw) + " I_ZBIAS"[1].w %s;\n",
									(bpmem.ztex2.op == ZTEXTURE_ADD) ? "+ zCoord" : "");

		// U24 overflow emulation
		out.Write("zCoord = zCoord * (16777215.0f/16777216.0f);\n");
		out.Write("zCoord = frac(zCoord);\n");
		out.Write("zCoord = zCoord * (16777216.0f/16777215.0f);\n");

		// Note: depth texture output is only written to depth buffer if late depth test is used
		// final depth value is used for fog calculation, though
		if (per_pixel_depth)
			out.Write("depth = zCoord;\n");
	}

	if (dstAlphaMode == DSTALPHA_ALPHA_PASS)
	{
		out.SetConstantsUsed(C_ALPHA, C_ALPHA);
		out.Write("\tocol0 = float4(prev.rgb, " I_ALPHA"[0].a);\n");
	}
	else
	{
		WriteFog<T>(out, uid_data);
		out.Write("\tocol0 = prev;\n");
	}

	// On D3D11, use dual-source color blending to perform dst alpha in a
	// single pass
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
	{
		out.SetConstantsUsed(C_ALPHA, C_ALPHA);
		// Colors will be blended against the alpha from ocol1...
		out.Write("\tocol1 = prev;\n");
		// ...and the alpha from ocol0 will be written to the framebuffer.
		out.Write("\tocol0.a = " I_ALPHA"[0].a;\n");
	}
	
	out.Write("}\n");

	if (text[sizeof(text) - 1] != 0x7C)
		PanicAlert("PixelShader generator - buffer too small, canary has been eaten!");

	if (out.GetBuffer() != NULL)
		setlocale(LC_NUMERIC, ""); // restore locale
}



//table with the color compare operations
static const char *TEVCMPColorOPTable[16] =
{
	"float3(0.0f, 0.0f, 0.0f)",//0
	"float3(0.0f, 0.0f, 0.0f)",//1
	"float3(0.0f, 0.0f, 0.0f)",//2
	"float3(0.0f, 0.0f, 0.0f)",//3
	"float3(0.0f, 0.0f, 0.0f)",//4
	"float3(0.0f, 0.0f, 0.0f)",//5
	"float3(0.0f, 0.0f, 0.0f)",//6
	"float3(0.0f, 0.0f, 0.0f)",//7
	"   %s + ((%s.r >= %s.r + (0.25f/255.0f)) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_R8_GT 8
	"   %s + ((abs(%s.r - %s.r) < (0.5f/255.0f)) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_R8_EQ 9
	"   %s + (( dot(%s.rgb, comp16) >= (dot(%s.rgb, comp16) + (0.25f/255.0f))) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_GR16_GT 10
	"   %s + (abs(dot(%s.rgb, comp16) - dot(%s.rgb, comp16)) < (0.5f/255.0f) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_GR16_EQ 11
	"   %s + (( dot(%s.rgb, comp24) >= (dot(%s.rgb, comp24) + (0.25f/255.0f))) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_BGR24_GT 12
	"   %s + (abs(dot(%s.rgb, comp24) - dot(%s.rgb, comp24)) < (0.5f/255.0f) ? %s : float3(0.0f, 0.0f, 0.0f))",//#define TEVCMP_BGR24_EQ 13
	"   %s + (max(sign(%s.rgb - %s.rgb - (0.25f/255.0f)), float3(0.0f, 0.0f, 0.0f)) * %s)",//#define TEVCMP_RGB8_GT  14
	"   %s + ((float3(1.0f, 1.0f, 1.0f) - max(sign(abs(%s.rgb - %s.rgb) - (0.5f/255.0f)), float3(0.0f, 0.0f, 0.0f))) * %s)"//#define TEVCMP_RGB8_EQ  15
};

//table with the alpha compare operations
static const char *TEVCMPAlphaOPTable[16] =
{
	"0.0f",//0
	"0.0f",//1
	"0.0f",//2
	"0.0f",//3
	"0.0f",//4
	"0.0f",//5
	"0.0f",//6
	"0.0f",//7
	"   %s.a + ((%s.r >= (%s.r + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_R8_GT 8
	"   %s.a + (abs(%s.r - %s.r) < (0.5f/255.0f) ? %s.a : 0.0f)",//#define TEVCMP_R8_EQ 9
	"   %s.a + ((dot(%s.rgb, comp16) >= (dot(%s.rgb, comp16) + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_GR16_GT 10
	"   %s.a + (abs(dot(%s.rgb, comp16) - dot(%s.rgb, comp16)) < (0.5f/255.0f) ? %s.a : 0.0f)",//#define TEVCMP_GR16_EQ 11
	"   %s.a + ((dot(%s.rgb, comp24) >= (dot(%s.rgb, comp24) + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_BGR24_GT 12
	"   %s.a + (abs(dot(%s.rgb, comp24) - dot(%s.rgb, comp24)) < (0.5f/255.0f) ? %s.a : 0.0f)",//#define TEVCMP_BGR24_EQ 13
	"   %s.a + ((%s.a >= (%s.a + (0.25f/255.0f))) ? %s.a : 0.0f)",//#define TEVCMP_A8_GT 14
	"   %s.a + (abs(%s.a - %s.a) < (0.5f/255.0f) ? %s.a : 0.0f)"//#define TEVCMP_A8_EQ 15

};


template<class T>
static void WriteStage(T& out, pixel_shader_uid_data& uid_data, int n, API_TYPE ApiType)
{
	int texcoord = bpmem.tevorders[n/2].getTexCoord(n&1);
	bool bHasTexCoord = (u32)texcoord < bpmem.genMode.numtexgens;
	bool bHasIndStage = bpmem.tevind[n].IsActive() && bpmem.tevind[n].bt < bpmem.genMode.numindstages;

	// HACK to handle cases where the tex gen is not enabled
	if (!bHasTexCoord)
		texcoord = 0;

	out.Write("// TEV stage %d\n", n);

	uid_data.bHasIndStage |= bHasIndStage << n;
	if (n < 8) { uid_data.tevorders_n_texcoord1 |= texcoord << (3 * n); }
	else uid_data.tevorders_n_texcoord2 |= texcoord << (3 * n - 24);
	if (bHasIndStage)
	{
		uid_data.tevind_n.bs |= bpmem.tevind[n].bs << (2*n);
		uid_data.tevind_n.bt |= bpmem.tevind[n].bt << (2*n);
		uid_data.tevind_n.fmt |= bpmem.tevind[n].fmt << (2*n);

		out.Write("// indirect op\n");
		// perform the indirect op on the incoming regular coordinates using indtex%d as the offset coords
		if (bpmem.tevind[n].bs != ITBA_OFF)
		{
			out.Write("alphabump = indtex%d.%s %s;\n",
					bpmem.tevind[n].bt,
					tevIndAlphaSel[bpmem.tevind[n].bs],
					tevIndAlphaScale[bpmem.tevind[n].fmt]);
		}
		// format
		out.Write("float3 indtevcrd%d = indtex%d * %s;\n", n, bpmem.tevind[n].bt, tevIndFmtScale[bpmem.tevind[n].fmt]);

		// bias
		uid_data.tevind_n.Set_bias(n, bpmem.tevind[n].bias);
		if (bpmem.tevind[n].bias != ITB_NONE )
			out.Write("indtevcrd%d.%s += %s;\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt]);

		// multiply by offset matrix and scale
		uid_data.tevind_n.Set_mid(n, bpmem.tevind[n].mid);
		if (bpmem.tevind[n].mid != 0)
		{
			if (bpmem.tevind[n].mid <= 3)
			{
				int mtxidx = 2*(bpmem.tevind[n].mid-1);
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("float2 indtevtrans%d = float2(dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d), dot(" I_INDTEXMTX"[%d].xyz, indtevcrd%d));\n",
							n, mtxidx, n, mtxidx+1, n);
			}
			else if (bpmem.tevind[n].mid <= 7 && bHasTexCoord)
			{ // s matrix
				_assert_(bpmem.tevind[n].mid >= 5);
				int mtxidx = 2*(bpmem.tevind[n].mid-5);
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.xx;\n", n, mtxidx, texcoord, n);
			}
			else if (bpmem.tevind[n].mid <= 11 && bHasTexCoord)
			{ // t matrix
				_assert_(bpmem.tevind[n].mid >= 9);
				int mtxidx = 2*(bpmem.tevind[n].mid-9);
				out.SetConstantsUsed(C_INDTEXMTX+mtxidx, C_INDTEXMTX+mtxidx);
				out.Write("float2 indtevtrans%d = " I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.yy;\n", n, mtxidx, texcoord, n);
			}
			else
				out.Write("float2 indtevtrans%d = float2(0.0f, 0.0f);\n", n);
		}
		else
			out.Write("float2 indtevtrans%d = float2(0.0f, 0.0f);\n", n);

		// ---------
		// Wrapping
		// ---------
		uid_data.tevind_n.Set_sw(n, bpmem.tevind[n].sw);
		uid_data.tevind_n.Set_tw(n, bpmem.tevind[n].tw);
		uid_data.tevind_n.fb_addprev |= bpmem.tevind[n].fb_addprev << n;

		// wrap S
		if (bpmem.tevind[n].sw == ITW_OFF)
			out.Write("wrappedcoord.x = uv%d.x;\n", texcoord);
		else if (bpmem.tevind[n].sw == ITW_0)
			out.Write("wrappedcoord.x = 0.0f;\n");
		else
			out.Write("wrappedcoord.x = fmod( uv%d.x, %s );\n", texcoord, tevIndWrapStart[bpmem.tevind[n].sw]);

		// wrap T
		if (bpmem.tevind[n].tw == ITW_OFF)
			out.Write("wrappedcoord.y = uv%d.y;\n", texcoord);
		else if (bpmem.tevind[n].tw == ITW_0)
			out.Write("wrappedcoord.y = 0.0f;\n");
		else
			out.Write("wrappedcoord.y = fmod( uv%d.y, %s );\n", texcoord, tevIndWrapStart[bpmem.tevind[n].tw]);

		if (bpmem.tevind[n].fb_addprev) // add previous tevcoord
			out.Write("tevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
		else
			out.Write("tevcoord.xy = wrappedcoord + indtevtrans%d;\n", n);
	}

	TevStageCombiner::ColorCombiner &cc = bpmem.combiners[n].colorC;
	TevStageCombiner::AlphaCombiner &ac = bpmem.combiners[n].alphaC;

	uid_data.combiners[n].colorC.hex = cc.hex & 0xFFFFFF;
	uid_data.combiners[n].alphaC.hex = ac.hex & 0xFFFFFF;

	if(cc.a == TEVCOLORARG_RASA || cc.a == TEVCOLORARG_RASC
		|| cc.b == TEVCOLORARG_RASA || cc.b == TEVCOLORARG_RASC
		|| cc.c == TEVCOLORARG_RASA || cc.c == TEVCOLORARG_RASC
		|| cc.d == TEVCOLORARG_RASA || cc.d == TEVCOLORARG_RASC
		|| ac.a == TEVALPHAARG_RASA || ac.b == TEVALPHAARG_RASA
		|| ac.c == TEVALPHAARG_RASA || ac.d == TEVALPHAARG_RASA)
	{
		const int i = bpmem.combiners[n].alphaC.rswap;
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2  ].swap1 << (i*2);
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2+1].swap1 << (i*2 + 1);
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2  ].swap2 << (i*2 + 16);
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2+1].swap2 << (i*2 + 17);

		char *rasswap = swapModeTable[bpmem.combiners[n].alphaC.rswap];
		out.Write("rastemp = %s.%s;\n", tevRasTable[bpmem.tevorders[n / 2].getColorChan(n & 1)], rasswap);
		out.Write("crastemp = frac(rastemp * (255.0f/256.0f)) * (256.0f/255.0f);\n");
	}


	if (bpmem.tevorders[n/2].getEnable(n&1))
	{
		if (!bHasIndStage)
		{
			// calc tevcord
			if(bHasTexCoord)
				out.Write("tevcoord.xy = uv%d.xy;\n", texcoord);
			else
				out.Write("tevcoord.xy = float2(0.0f, 0.0f);\n");
		}

		const int i = bpmem.combiners[n].alphaC.tswap;
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2  ].swap1 << (i*2);
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2+1].swap1 << (i*2 + 1);
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2  ].swap2 << (i*2 + 16);
		uid_data.tevksel_n_swap |= bpmem.tevksel[i*2+1].swap2 << (i*2 + 17);

		char *texswap = swapModeTable[bpmem.combiners[n].alphaC.tswap];
		int texmap = bpmem.tevorders[n/2].getTexMap(n&1);
		SampleTexture<T>(out, "textemp", "tevcoord", texswap, texmap, ApiType);
	}
	else
		out.Write("textemp = float4(1.0f, 1.0f, 1.0f, 1.0f);\n");


	if (cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST || cc.c == TEVCOLORARG_KONST || cc.d == TEVCOLORARG_KONST
		|| ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST || ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST)
	{
		int kc = bpmem.tevksel[n / 2].getKC(n & 1);
		int ka = bpmem.tevksel[n / 2].getKA(n & 1);
		out.Write("konsttemp = float4(%s, %s);\n", tevKSelTableC[kc], tevKSelTableA[ka]);
		if(kc > 7 || ka > 7)
		{
			out.Write("ckonsttemp = frac(konsttemp * (255.0f/256.0f)) * (256.0f/255.0f);\n");
		}
		else
		{
			out.Write("ckonsttemp = konsttemp;\n");
		}
		if (kc > 7)
			out.SetConstantsUsed(C_KCOLORS+((kc-0xc)%4),C_KCOLORS+((kc-0xc)%4));
		if (ka > 7)
			out.SetConstantsUsed(C_KCOLORS+((ka-0xc)%4),C_KCOLORS+((ka-0xc)%4));
	}

	if(cc.a == TEVCOLORARG_CPREV || cc.a == TEVCOLORARG_APREV
		|| cc.b == TEVCOLORARG_CPREV || cc.b == TEVCOLORARG_APREV
		|| cc.c == TEVCOLORARG_CPREV || cc.c == TEVCOLORARG_APREV
		|| ac.a == TEVALPHAARG_APREV || ac.b == TEVALPHAARG_APREV || ac.c == TEVALPHAARG_APREV)
	{
		if(RegisterStates[0].AlphaNeedOverflowControl || RegisterStates[0].ColorNeedOverflowControl)
		{
			out.Write("cprev = frac(prev * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[0].AlphaNeedOverflowControl = false;
			RegisterStates[0].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cprev = prev;\n");
		}
		RegisterStates[0].AuxStored = true;
	}

	if(cc.a == TEVCOLORARG_C0 || cc.a == TEVCOLORARG_A0
	|| cc.b == TEVCOLORARG_C0 || cc.b == TEVCOLORARG_A0
	|| cc.c == TEVCOLORARG_C0 || cc.c == TEVCOLORARG_A0
	|| ac.a == TEVALPHAARG_A0 || ac.b == TEVALPHAARG_A0 || ac.c == TEVALPHAARG_A0)
	{
		out.SetConstantsUsed(C_COLORS+1,C_COLORS+1);
		if(RegisterStates[1].AlphaNeedOverflowControl || RegisterStates[1].ColorNeedOverflowControl)
		{
			out.Write("cc0 = frac(c0 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[1].AlphaNeedOverflowControl = false;
			RegisterStates[1].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cc0 = c0;\n");
		}
		RegisterStates[1].AuxStored = true;
	}

	if(cc.a == TEVCOLORARG_C1 || cc.a == TEVCOLORARG_A1
	|| cc.b == TEVCOLORARG_C1 || cc.b == TEVCOLORARG_A1
	|| cc.c == TEVCOLORARG_C1 || cc.c == TEVCOLORARG_A1
	|| ac.a == TEVALPHAARG_A1 || ac.b == TEVALPHAARG_A1 || ac.c == TEVALPHAARG_A1)
	{
		out.SetConstantsUsed(C_COLORS+2,C_COLORS+2);
		if(RegisterStates[2].AlphaNeedOverflowControl || RegisterStates[2].ColorNeedOverflowControl)
		{
			out.Write("cc1 = frac(c1 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[2].AlphaNeedOverflowControl = false;
			RegisterStates[2].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cc1 = c1;\n");
		}
		RegisterStates[2].AuxStored = true;
	}

	if(cc.a == TEVCOLORARG_C2 || cc.a == TEVCOLORARG_A2
	|| cc.b == TEVCOLORARG_C2 || cc.b == TEVCOLORARG_A2
	|| cc.c == TEVCOLORARG_C2 || cc.c == TEVCOLORARG_A2
	|| ac.a == TEVALPHAARG_A2 || ac.b == TEVALPHAARG_A2 || ac.c == TEVALPHAARG_A2)
	{
		out.SetConstantsUsed(C_COLORS+3,C_COLORS+3);
		if(RegisterStates[3].AlphaNeedOverflowControl || RegisterStates[3].ColorNeedOverflowControl)
		{
			out.Write("cc2 = frac(c2 * (255.0f/256.0f)) * (256.0f/255.0f);\n");
			RegisterStates[3].AlphaNeedOverflowControl = false;
			RegisterStates[3].ColorNeedOverflowControl = false;
		}
		else
		{
			out.Write("cc2 = c2;\n");
		}
		RegisterStates[3].AuxStored = true;
	}

	RegisterStates[cc.dest].ColorNeedOverflowControl = (cc.clamp == 0);
	RegisterStates[cc.dest].AuxStored = false;

	if (cc.d == TEVCOLORARG_C0 || cc.d == TEVCOLORARG_A0 || ac.d == TEVALPHAARG_A0)
		out.SetConstantsUsed(C_COLORS+1,C_COLORS+1);

	if (cc.d == TEVCOLORARG_C1 || cc.d == TEVCOLORARG_A1 || ac.d == TEVALPHAARG_A1)
		out.SetConstantsUsed(C_COLORS+2,C_COLORS+2);

	if (cc.d == TEVCOLORARG_C2 || cc.d == TEVCOLORARG_A2 || ac.d == TEVALPHAARG_A2)
		out.SetConstantsUsed(C_COLORS+3,C_COLORS+3);

	if (cc.dest >= GX_TEVREG0 && cc.dest <= GX_TEVREG2)
		out.SetConstantsUsed(C_COLORS+cc.dest, C_COLORS+cc.dest);

	if (ac.dest >= GX_TEVREG0 && ac.dest <= GX_TEVREG2)
		out.SetConstantsUsed(C_COLORS+ac.dest, C_COLORS+ac.dest);

	out.Write("// color combine\n");
	if (cc.clamp)
		out.Write("%s = saturate(", tevCOutputTable[cc.dest]);
	else
		out.Write("%s = ", tevCOutputTable[cc.dest]);

	// combine the color channel
	if (cc.bias != TevBias_COMPARE) // if not compare
	{
		//normal color combiner goes here
		if (cc.shift > TEVSCALE_1)
			out.Write("%s*(", tevScaleTable[cc.shift]);

		if(!(cc.d == TEVCOLORARG_ZERO && cc.op == TEVOP_ADD))
			out.Write("%s%s", tevCInputTable[cc.d], tevOpTable[cc.op]);

		if (cc.a == cc.b)
			out.Write("%s", tevCInputTable[cc.a + 16]);
		else if (cc.c == TEVCOLORARG_ZERO)
			out.Write("%s", tevCInputTable[cc.a + 16]);
		else if (cc.c == TEVCOLORARG_ONE)
			out.Write("%s", tevCInputTable[cc.b + 16]);
		else if (cc.a == TEVCOLORARG_ZERO)
			out.Write("%s*%s", tevCInputTable[cc.b + 16], tevCInputTable[cc.c + 16]);
		else if (cc.b == TEVCOLORARG_ZERO)
			out.Write("%s*(float3(1.0f, 1.0f, 1.0f)-%s)", tevCInputTable[cc.a + 16], tevCInputTable[cc.c + 16]);
		else
			out.Write("lerp(%s, %s, %s)", tevCInputTable[cc.a + 16], tevCInputTable[cc.b + 16], tevCInputTable[cc.c + 16]);

		out.Write("%s", tevBiasTable[cc.bias]);

		if (cc.shift > TEVSCALE_1)
			out.Write(")");
	}
	else
	{
		int cmp = (cc.shift<<1)|cc.op|8; // comparemode stored here
		out.Write(TEVCMPColorOPTable[cmp],//lookup the function from the op table
				tevCInputTable[cc.d],
				tevCInputTable[cc.a + 16],
				tevCInputTable[cc.b + 16],
				tevCInputTable[cc.c + 16]);
	}
	if (cc.clamp)
		out.Write(")");
	out.Write(";\n");

	RegisterStates[ac.dest].AlphaNeedOverflowControl = (ac.clamp == 0);
	RegisterStates[ac.dest].AuxStored = false;

	out.Write("// alpha combine\n");
	if (ac.clamp)
		out.Write("%s = saturate(", tevAOutputTable[ac.dest]);
	else
		out.Write("%s = ", tevAOutputTable[ac.dest]);

	if (ac.bias != TevBias_COMPARE) // if not compare
	{
		//normal alpha combiner goes here
		if (ac.shift > TEVSCALE_1)
			out.Write("%s*(", tevScaleTable[ac.shift]);

		if(!(ac.d == TEVALPHAARG_ZERO && ac.op == TEVOP_ADD))
			out.Write("%s.a%s", tevAInputTable[ac.d], tevOpTable[ac.op]);

		if (ac.a == ac.b)
			out.Write("%s.a", tevAInputTable[ac.a + 8]);
		else if (ac.c == TEVALPHAARG_ZERO)
			out.Write("%s.a", tevAInputTable[ac.a + 8]);
		else if (ac.a == TEVALPHAARG_ZERO)
			out.Write("%s.a*%s.a", tevAInputTable[ac.b + 8], tevAInputTable[ac.c + 8]);
		else if (ac.b == TEVALPHAARG_ZERO)
			out.Write("%s.a*(1.0f-%s.a)", tevAInputTable[ac.a + 8], tevAInputTable[ac.c + 8]);
		else
			out.Write("lerp(%s.a, %s.a, %s.a)", tevAInputTable[ac.a + 8], tevAInputTable[ac.b + 8], tevAInputTable[ac.c + 8]);

		out.Write("%s",tevBiasTable[ac.bias]);

		if (ac.shift>0)
			out.Write(")");

	}
	else
	{
		//compare alpha combiner goes here
		int cmp = (ac.shift<<1)|ac.op|8; // comparemode stored here
		out.Write(TEVCMPAlphaOPTable[cmp],
				tevAInputTable[ac.d],
				tevAInputTable[ac.a + 8],
				tevAInputTable[ac.b + 8],
				tevAInputTable[ac.c + 8]);
	}
	if (ac.clamp)
		out.Write(")");
	out.Write(";\n\n");
	out.Write("// TEV done\n");
}

template<class T>
void SampleTexture(T& out, const char *destination, const char *texcoords, const char *texswap, int texmap, API_TYPE ApiType)
{
	out.SetConstantsUsed(C_TEXDIMS+texmap,C_TEXDIMS+texmap);
	if (ApiType == API_D3D11)
		out.Write("%s=Tex%d.Sample(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", destination, texmap,texmap, texcoords, texmap, texswap);
	else
		out.Write("%s=%s(samp%d,%s.xy * " I_TEXDIMS"[%d].xy).%s;\n", destination, ApiType == API_OPENGL ? "texture" : "tex2D", texmap, texcoords, texmap, texswap);
}

static const char *tevAlphaFuncsTable[] =
{
	"(false)",									// NEVER
	"(prev.a <= %s - (0.25f/255.0f))",			// LESS
	"(abs( prev.a - %s ) < (0.5f/255.0f))",		// EQUAL
	"(prev.a < %s + (0.25f/255.0f))",			// LEQUAL
	"(prev.a >= %s + (0.25f/255.0f))",			// GREATER
	"(abs( prev.a - %s ) >= (0.5f/255.0f))",	// NEQUAL
	"(prev.a > %s - (0.25f/255.0f))",			// GEQUAL
	"(true)"									// ALWAYS
};

static const char *tevAlphaFunclogicTable[] =
{
	" && ", // and
	" || ", // or
	" != ", // xor
	" == "  // xnor
};

template<class T>
static void WriteAlphaTest(T& out, pixel_shader_uid_data& uid_data, API_TYPE ApiType, DSTALPHA_MODE dstAlphaMode, bool per_pixel_depth)
{
	static const char *alphaRef[2] =
	{
		I_ALPHA"[0].r",
		I_ALPHA"[0].g"
	};

	out.SetConstantsUsed(C_ALPHA, C_ALPHA);

	// using discard then return works the same in cg and dx9 but not in dx11
	out.Write("\tif(!( ");

	uid_data.alpha_test.comp0 = bpmem.alpha_test.comp0;
	uid_data.alpha_test.logic = bpmem.alpha_test.comp1;
	uid_data.alpha_test.logic = bpmem.alpha_test.logic;

	// Lookup the first component from the alpha function table
	int compindex = bpmem.alpha_test.comp0;
	out.Write(tevAlphaFuncsTable[compindex], alphaRef[0]);

	out.Write("%s", tevAlphaFunclogicTable[bpmem.alpha_test.logic]);//lookup the logic op

	// Lookup the second component from the alpha function table
	compindex = bpmem.alpha_test.comp1;
	out.Write(tevAlphaFuncsTable[compindex], alphaRef[1]);
	out.Write(")) {\n");

	out.Write("\t\tocol0 = float4(0.0f, 0.0f, 0.0f, 0.0f);\n");
	if (dstAlphaMode == DSTALPHA_DUAL_SOURCE_BLEND)
		out.Write("\t\tocol1 = float4(0.0f, 0.0f, 0.0f, 0.0f);\n");
	if(per_pixel_depth)
		out.Write("\t\tdepth = 1.f;\n");

	// HAXX: zcomploc (aka early_ztest) is a way to control whether depth test is done before
	// or after texturing and alpha test. PC GPUs have no way to support this
	// feature properly as of 2012: depth buffer and depth test are not
	// programmable and the depth test is always done after texturing.
	// Most importantly, PC GPUs do not allow writing to the z buffer without
	// writing a color value (unless color writing is disabled altogether).
	// We implement "depth test before texturing" by discarding the fragment
	// when the alpha test fail. This is not a correct implementation because
	// even if the depth test fails the fragment could be alpha blended, but
	// we don't have a choice.
	uid_data.alpha_test.use_zcomploc_hack = bpmem.zcontrol.early_ztest && bpmem.zmode.updateenable;
	if (!(bpmem.zcontrol.early_ztest && bpmem.zmode.updateenable))
	{
		out.Write("\t\tdiscard;\n");
		if (ApiType != API_D3D11)
			out.Write("\t\treturn;\n");
	}

	out.Write("}\n");
}

static const char *tevFogFuncsTable[] =
{
	"",																//No Fog
	"",																//?
	"",																//Linear
	"",																//?
	"\tfog = 1.0f - pow(2.0f, -8.0f * fog);\n",						//exp
	"\tfog = 1.0f - pow(2.0f, -8.0f * fog * fog);\n",					//exp2
	"\tfog = pow(2.0f, -8.0f * (1.0f - fog));\n",						//backward exp
	"\tfog = 1.0f - fog;\n   fog = pow(2.0f, -8.0f * fog * fog);\n"	//backward exp2
};

template<class T>
static void WriteFog(T& out, pixel_shader_uid_data& uid_data)
{
	uid_data.fog.fsel = bpmem.fog.c_proj_fsel.fsel;
	if(bpmem.fog.c_proj_fsel.fsel == 0)
		return; // no Fog

	uid_data.fog.proj = bpmem.fog.c_proj_fsel.proj;

	out.SetConstantsUsed(C_FOG, C_FOG+1);
	if (bpmem.fog.c_proj_fsel.proj == 0)
	{
		// perspective
		// ze = A/(B - (Zs >> B_SHF)
		out.Write("\tfloat ze = " I_FOG"[1].x / (" I_FOG"[1].y - (zCoord / " I_FOG"[1].w));\n");
	}
	else
	{
		// orthographic
		// ze = a*Zs	(here, no B_SHF)
		out.Write("\tfloat ze = " I_FOG"[1].x * zCoord;\n");
	}

	// x_adjust = sqrt((x-center)^2 + k^2)/k
	// ze *= x_adjust
	// this is completely theoretical as the real hardware seems to use a table intead of calculating the values.
	uid_data.fog.RangeBaseEnabled = bpmem.fogRange.Base.Enabled;
	if(bpmem.fogRange.Base.Enabled)
	{
		out.SetConstantsUsed(C_FOG+2, C_FOG+2);
		out.Write("\tfloat x_adjust = (2.0f * (clipPos.x / " I_FOG"[2].y)) - 1.0f - " I_FOG"[2].x;\n");
		out.Write("\tx_adjust = sqrt(x_adjust * x_adjust + " I_FOG"[2].z * " I_FOG"[2].z) / " I_FOG"[2].z;\n");
		out.Write("\tze *= x_adjust;\n");
	}

	out.Write("\tfloat fog = saturate(ze - " I_FOG"[1].z);\n");

	if (bpmem.fog.c_proj_fsel.fsel > 3)
	{
		out.Write("%s", tevFogFuncsTable[bpmem.fog.c_proj_fsel.fsel]);
	}
	else
	{
		if (bpmem.fog.c_proj_fsel.fsel != 2)
			WARN_LOG(VIDEO, "Unknown Fog Type! %08x", bpmem.fog.c_proj_fsel.fsel);
	}

	out.Write("\tprev.rgb = lerp(prev.rgb, " I_FOG"[0].rgb, fog);\n");
}

void GetPixelShaderUid(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderUid>(object, dstAlphaMode, ApiType, components);
}

void GeneratePixelShaderCode(PixelShaderCode& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderCode>(object, dstAlphaMode, ApiType, components);
}

void GetPixelShaderConstantProfile(PixelShaderConstantProfile& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components)
{
	GeneratePixelShader<PixelShaderConstantProfile>(object, dstAlphaMode, ApiType, components);
}

