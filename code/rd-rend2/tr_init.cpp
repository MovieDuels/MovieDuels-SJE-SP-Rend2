/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
//#include "tr_stl.h"
//#include "ghoul2/g2_local.h"
#include "tr_cache.h"
#include "tr_allocator.h"
#include "tr_weather.h"
#include <algorithm>

#ifdef _G2_GORE
#include "G2_gore_r2.h"
#endif

static size_t STATIC_UNIFORM_BUFFER_SIZE = 1 * 1024 * 1024;
static size_t FRAME_UNIFORM_BUFFER_SIZE = 8 * 1024 * 1024;
static size_t FRAME_SCENE_UNIFORM_BUFFER_SIZE = 1 * 1024 * 1024;
static size_t FRAME_VERTEX_BUFFER_SIZE = 12 * 1024 * 1024;
static size_t FRAME_INDEX_BUFFER_SIZE = 4 * 1024 * 1024;

#if defined(_WIN32)
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

glconfig_t  glConfig;
glconfigExt_t glConfigExt;
glRefConfig_t glRefConfig;
glstate_t	glState;
window_t	window;

cvar_t* se_language;
#ifdef JK2_MODE
cvar_t* sp_language;			// JK2
#endif

cvar_t* r_verbose;
cvar_t* r_ignore;

cvar_t* r_detailTextures;

cvar_t* r_znear;
cvar_t* r_zproj;
cvar_t* r_stereoSeparation;

cvar_t* r_skipBackEnd;

cvar_t* r_stereo;
cvar_t* r_anaglyphMode;

cvar_t* r_greyscale;

cvar_t* r_measureOverdraw;

cvar_t* r_inGameVideo;
cvar_t* r_fastsky;
cvar_t* r_drawSun;
cvar_t* r_dynamiclight;

cvar_t* r_lodbias;
cvar_t* r_lodscale;
cvar_t* r_autolodscalevalue;

cvar_t* r_norefresh;
cvar_t* r_drawentities;
cvar_t* r_drawworld;
cvar_t* r_drawfog;
cvar_t* r_speeds;
cvar_t* r_fullbright;
cvar_t* r_novis;
cvar_t* r_nocull;
cvar_t* r_facePlaneCull;
cvar_t* r_showcluster;
cvar_t* r_nocurves;

cvar_t* r_volumetricFog;
cvar_t* r_volumetricFogDefaultScale;
cvar_t* r_volumetricFogSamples;
cvar_t* r_volumetricFogScale;

cvar_t* r_allowExtensions;

cvar_t* r_ext_compressed_textures;
cvar_t* r_ext_multitexture;
cvar_t* r_ext_compiled_vertex_array;
cvar_t* r_ext_texture_env_add;
cvar_t* r_ext_texture_filter_anisotropic;
cvar_t* r_ext_preferred_tc_method;

cvar_t* r_ext_draw_range_elements;
cvar_t* r_ext_multi_draw_arrays;
cvar_t* r_ext_texture_float;
cvar_t* r_arb_half_float_pixel;
cvar_t* r_ext_framebuffer_multisample;
cvar_t* r_arb_seamless_cube_map;
cvar_t* r_arb_vertex_type_2_10_10_10_rev;
cvar_t* r_arb_buffer_storage;

cvar_t* r_mergeMultidraws;
cvar_t* r_mergeLeafSurfaces;

cvar_t* r_smaa;
cvar_t* r_smaa_quality;

cvar_t* r_cameraExposure;

cvar_t* r_externalGLSL;

cvar_t* r_hdr;
cvar_t* r_floatLightmap;

cvar_t* r_toneMap;
cvar_t* r_forceToneMap;
cvar_t* r_forceToneMapMin;
cvar_t* r_forceToneMapAvg;
cvar_t* r_forceToneMapMax;

cvar_t* r_autoExposure;
cvar_t* r_forceAutoExposure;
cvar_t* r_forceAutoExposureMin;
cvar_t* r_forceAutoExposureMax;

cvar_t* r_depthPrepass;
cvar_t* r_ssao;

cvar_t* r_normalMapping;
cvar_t* r_specularMapping;
cvar_t* r_deluxeMapping;
cvar_t* r_deluxeSpecular;
cvar_t* r_parallaxMapping;
cvar_t* r_forceParallaxBias;
cvar_t* r_cubeMapping;
cvar_t* r_cubeMappingBounces;
cvar_t* r_baseNormalX;
cvar_t* r_baseNormalY;
cvar_t* r_baseParallax;
cvar_t* r_baseSpecular;
cvar_t* r_dlightMode;
cvar_t* r_pshadowDist;
cvar_t* r_imageUpsample;
cvar_t* r_imageUpsampleMaxSize;
cvar_t* r_imageUpsampleType;
cvar_t* r_genNormalMaps;
cvar_t* r_forceSun;
cvar_t* r_forceSunMapLightScale;
cvar_t* r_forceSunLightScale;
cvar_t* r_forceSunAmbientScale;
cvar_t* r_sunlightMode;
cvar_t* r_drawSunRays;
cvar_t* r_sunShadows;
cvar_t* r_shadowFilter;
cvar_t* r_shadowMapSize;
cvar_t* r_shadowCascadeZNear;
cvar_t* r_shadowCascadeZFar;
cvar_t* r_shadowCascadeZBias;
cvar_t* r_ignoreDstAlpha;

cvar_t* r_ignoreGLErrors;
cvar_t* r_logFile;

cvar_t* r_texturebits;

cvar_t* r_drawBuffer;
cvar_t* r_lightmap;
cvar_t* r_vertexLight;
cvar_t* r_uiFullScreen;
cvar_t* r_shadows;
cvar_t* r_flares;
cvar_t* r_nobind;
cvar_t* r_singleShader;
cvar_t* r_roundImagesDown;
cvar_t* r_colorMipLevels;
cvar_t* r_picmip;
cvar_t* r_showtris;
cvar_t* r_showsky;
cvar_t* r_shownormals;
cvar_t* r_finish;
cvar_t* r_clear;
cvar_t* r_markcount;
cvar_t* r_textureMode;
cvar_t* r_offsetFactor;
cvar_t* r_offsetUnits;
cvar_t* r_shadowOffsetFactor;
cvar_t* r_shadowOffsetUnits;
cvar_t* r_gamma;
cvar_t* r_intensity;
cvar_t* r_lockpvs;
cvar_t* r_noportals;
cvar_t* r_portalOnly;

cvar_t* r_subdivisions;
cvar_t* r_lodCurveError;

cvar_t* r_overBrightBits;
cvar_t* r_mapOverBrightBits;

cvar_t* r_debugSurface;
cvar_t* r_simpleMipMaps;

cvar_t* r_showImages;

cvar_t* r_ambientScale;
cvar_t* r_directedScale;
cvar_t* r_debugLight;
cvar_t* r_debugSort;
cvar_t* r_printShaders;
cvar_t* r_saveFontData;

#ifdef _DEBUG
cvar_t* r_noPrecacheGLA;
#endif

cvar_t* r_noServerGhoul2; // In SP renderer CVAR is actually r_noghoul2!
cvar_t* r_Ghoul2AnimSmooth = 0;
cvar_t* r_Ghoul2UnSqashAfterSmooth = 0;
//cvar_t	*r_Ghoul2UnSqash;
//cvar_t	*r_Ghoul2TimeBase=0; from single player
//cvar_t	*r_Ghoul2NoLerp;
//cvar_t	*r_Ghoul2NoBlend;
//cvar_t	*r_Ghoul2BlendMultiplier=0;

cvar_t* broadsword = 0;
cvar_t* broadsword_kickbones = 0;
cvar_t* broadsword_kickorigin = 0;
cvar_t* broadsword_playflop = 0;
cvar_t* broadsword_dontstopanim = 0;
cvar_t* broadsword_waitforshot = 0;
cvar_t* broadsword_smallbbox = 0;
cvar_t* broadsword_extra1 = 0;
cvar_t* broadsword_extra2 = 0;

cvar_t* broadsword_effcorr = 0;
cvar_t* broadsword_ragtobase = 0;
cvar_t* broadsword_dircap = 0;

cvar_t* r_marksOnTriangleMeshes;

cvar_t* r_aviMotionJpegQuality;
cvar_t* r_screenshotJpegQuality;
cvar_t* r_surfaceSprites;
cvar_t* r_AdvancedsurfaceSprites;

// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
#define	DEFAULT_MAX_POLYS		600
#define	DEFAULT_MAX_POLYVERTS	3000
cvar_t* r_maxpolys;
cvar_t* r_maxpolyverts;
int		max_polys;
int		max_polyverts;

cvar_t* r_dynamicGlow;
cvar_t* r_dynamicGlowPasses;
cvar_t* r_dynamicGlowDelta;
cvar_t* r_dynamicGlowIntensity;
cvar_t* r_dynamicGlowSoft;
cvar_t* r_dynamicGlowWidth;
cvar_t* r_dynamicGlowHeight;
cvar_t* r_dynamicGlowBloom;

cvar_t* r_debugContext;
cvar_t* r_debugWeather;

cvar_t* r_aspectCorrectFonts;

cvar_t* com_buildScript;
cvar_t* sv_mapname;
cvar_t* sv_mapChecksum;
cvar_t* r_modelpoolmegs;
cvar_t* r_environmentMapping;
cvar_t* r_ext_compressed_lightmaps;

cvar_t* r_com_rend2;
cvar_t* g_Weather;

cvar_t* r_patchStitching;
cvar_t* com_outcast;

#define ri_Cvar_Get_NoComm(varname, value, flag, comment) ri.Cvar_Get(varname, value, flag)

#ifdef REND2_SP
static qboolean gbAllowScreenDissolve = qtrue;
extern qboolean RE_InitDissolve(const qboolean bForceCircularExtroWipe);
extern qboolean RE_ProcessDissolve(void);
#endif // REND2_SP

// STUBS, REPLACEME

static void stub_R_InitWorldEffects(void) {}
static bool stub_R_SetTempGlobalFogColor(vec3_t color) { return qfalse; }

float tr_distortionAlpha = 1.0f; //opaque
float tr_distortionStretch = 0.0f; //no stretch override
qboolean tr_distortionPrePost = qfalse; //capture before postrender phase?
qboolean tr_distortionNegate = qfalse; //negative blend mode
static float* stub_get_tr_distortionAlpha(void) { return &tr_distortionAlpha; }
static float* stub_get_tr_distortionStretch(void) { return &tr_distortionStretch; }
static qboolean* stub_get_tr_distortionPrePost(void) { return &tr_distortionPrePost; }
static qboolean* stub_get_tr_distortionNegate(void) { return &tr_distortionNegate; }

extern void	RB_SetGL2D(void);
static void R_Splash()
{
	const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	GL_SetViewportAndScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglClearBufferfv(GL_COLOR, 0, black);
	qglClear(GL_DEPTH_BUFFER_BIT);

	GLSL_InitSplashScreenShader();

	GL_Cull(CT_TWO_SIDED);

	image_t* pImage = R_FindImageFile("menu/splash", IMGTYPE_COLORALPHA, IMGFLAG_NONE);
	if (pImage)
		GL_Bind(pImage);

	GL_State(GLS_DEPTHTEST_DISABLE);
	GLSL_BindProgram(&tr.splashScreenShader);
	RB_InstantTriangle();

	if (r_com_rend2->integer != 1)
	{
		ri.Cvar_Set("com_rend2", "1");
	}

	if (r_shadows->integer != 1)
	{
		ri.Cvar_Set("cg_shadows", "1");
	}

	if (com_outcast->integer != 0)
	{
		ri.Cvar_Set("com_outcast", "0");
	}

	ri.WIN_Present(&window);
}

/*
** GLW_CheckForExtension

  Cannot use strstr directly to differentiate between (for eg) reg_combiners and reg_combiners2
*/
static bool GL_CheckForExtension(const char* ext)
{
	const char* ptr = Q_stristr(glConfigExt.originalExtensionString, ext);
	if (ptr == NULL)
		return false;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify it's complete string.
}

void GLW_InitTextureCompression(void)
{
	bool newer_tc, old_tc;

	// Check for available tc methods.
	newer_tc = GL_CheckForExtension("ARB_texture_compression") && GL_CheckForExtension("EXT_texture_compression_s3tc");
	old_tc = GL_CheckForExtension("GL_S3_s3tc");

	if (old_tc)
	{
		Com_Printf("...GL_S3_s3tc available\n");
	}

	if (newer_tc)
	{
		Com_Printf("...GL_EXT_texture_compression_s3tc available\n");
	}

	if (!r_ext_compressed_textures->value)
	{
		// Compressed textures are off
		glConfig.textureCompression = TC_NONE;
		Com_Printf("...ignoring texture compression\n");
	}
	else if (!old_tc && !newer_tc)
	{
		// Requesting texture compression, but no method found
		glConfig.textureCompression = TC_NONE;
		Com_Printf("...no supported texture compression method found\n");
		Com_Printf(".....ignoring texture compression\n");
	}
	else
	{
		// some form of supported texture compression is avaiable, so see if the user has a preference
		if (r_ext_preferred_tc_method->integer == TC_NONE)
		{
			// No preference, so pick the best
			if (newer_tc)
			{
				Com_Printf("...no tc preference specified\n");
				Com_Printf(".....using GL_EXT_texture_compression_s3tc\n");
				glConfig.textureCompression = TC_S3TC_DXT;
			}
			else
			{
				Com_Printf("...no tc preference specified\n");
				Com_Printf(".....using GL_S3_s3tc\n");
				glConfig.textureCompression = TC_S3TC;
			}
		}
		else
		{
			// User has specified a preference, now see if this request can be honored
			if (old_tc && newer_tc)
			{
				// both are avaiable, so we can use the desired tc method
				if (r_ext_preferred_tc_method->integer == TC_S3TC)
				{
					Com_Printf("...using preferred tc method, GL_S3_s3tc\n");
					glConfig.textureCompression = TC_S3TC;
				}
				else
				{
					Com_Printf("...using preferred tc method, GL_EXT_texture_compression_s3tc\n");
					glConfig.textureCompression = TC_S3TC_DXT;
				}
			}
			else
			{
				// Both methods are not available, so this gets trickier
				if (r_ext_preferred_tc_method->integer == TC_S3TC)
				{
					// Preferring to user older compression
					if (old_tc)
					{
						Com_Printf("...using GL_S3_s3tc\n");
						glConfig.textureCompression = TC_S3TC;
					}
					else
					{
						// Drat, preference can't be honored
						Com_Printf("...preferred tc method, GL_S3_s3tc not available\n");
						Com_Printf(".....falling back to GL_EXT_texture_compression_s3tc\n");
						glConfig.textureCompression = TC_S3TC_DXT;
					}
				}
				else
				{
					// Preferring to user newer compression
					if (newer_tc)
					{
						Com_Printf("...using GL_EXT_texture_compression_s3tc\n");
						glConfig.textureCompression = TC_S3TC_DXT;
					}
					else
					{
						// Drat, preference can't be honored
						Com_Printf("...preferred tc method, GL_EXT_texture_compression_s3tc not available\n");
						Com_Printf(".....falling back to GL_S3_s3tc\n");
						glConfig.textureCompression = TC_S3TC;
					}
				}
			}
		}
	}
}

// Truncates the GL extensions string by only allowing up to 'maxExtensions' extensions in the string.
static const char* TruncateGLExtensionsString(const char* extensionsString, int maxExtensions)
{
	const char* p = extensionsString;
	const char* q;
	int numExtensions = 0;
	size_t extensionsLen = strlen(extensionsString);

	char* truncatedExtensions;

	while ((q = strchr(p, ' ')) != NULL && numExtensions < maxExtensions)
	{
		p = q + 1;
		numExtensions++;
	}

	if (q != NULL)
	{
		// We still have more extensions. We'll call this the end

		extensionsLen = p - extensionsString - 1;
	}

	truncatedExtensions = (char*)R_Malloc(extensionsLen + 1, TAG_GENERAL);
	Q_strncpyz(truncatedExtensions, extensionsString, extensionsLen + 1);

	return truncatedExtensions;
}

static const char* GetGLExtensionsString()
{
	GLint numExtensions;
	glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
	size_t extensionStringLen = 0;

	for (int i = 0; i < numExtensions; i++)
	{
		extensionStringLen += strlen((const char*)qglGetStringi(GL_EXTENSIONS, i)) + 1;
	}

	char* extensionString = (char*)R_Malloc(extensionStringLen + 1, TAG_GENERAL);
	char* p = extensionString;
	for (int i = 0; i < numExtensions; i++)
	{
		const char* extension = (const char*)qglGetStringi(GL_EXTENSIONS, i);
		while (*extension != '\0')
			*p++ = *extension++;

		*p++ = ' ';
	}

	*p = '\0';
	assert((p - extensionString) == extensionStringLen);

	return extensionString;
}

/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL(void)
{
	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma
	//

	if (glConfig.vidWidth == 0)
	{
		windowDesc_t windowDesc = {};
		memset(&glConfig, 0, sizeof(glConfig));

		windowDesc.api = GRAPHICS_API_OPENGL;
		windowDesc.gl.majorVersion = 3;
		windowDesc.gl.minorVersion = 2;
		windowDesc.gl.profile = GLPROFILE_CORE;
		if (r_debugContext->integer)
			windowDesc.gl.contextFlags = GLCONTEXT_DEBUG;

		window = ri.WIN_Init(&windowDesc, &glConfig);

		GLimp_InitCoreFunctions();

		Com_Printf("GL_RENDERER: %s\n", (char*)qglGetString(GL_RENDERER));

		// get our config strings
		glConfig.vendor_string = (const char*)qglGetString(GL_VENDOR);
		glConfig.renderer_string = (const char*)qglGetString(GL_RENDERER);
		glConfig.version_string = (const char*)qglGetString(GL_VERSION);
		glConfig.extensions_string = GetGLExtensionsString();

		glConfigExt.originalExtensionString = glConfig.extensions_string;
		glConfig.extensions_string = TruncateGLExtensionsString(glConfigExt.originalExtensionString, 128);

		// OpenGL driver constants
		qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize);

		// Determine GPU IHV
		if (Q_stristr(glConfig.vendor_string, "ATI Technologies Inc."))
		{
			glRefConfig.hardwareVendor = IHV_AMD;
		}
		else if (Q_stristr(glConfig.vendor_string, "NVIDIA"))
		{
			glRefConfig.hardwareVendor = IHV_NVIDIA;
		}
		else if (Q_stristr(glConfig.vendor_string, "INTEL"))
		{
			glRefConfig.hardwareVendor = IHV_INTEL;
		}
		else
		{
			glRefConfig.hardwareVendor = IHV_UNKNOWN;
		}

		// stubbed or broken drivers may have reported 0...
		glConfig.maxTextureSize = Q_max(0, glConfig.maxTextureSize);

		// initialize extensions
		GLimp_InitExtensions();

		// Create the default VAO
		GLuint vao;
		qglGenVertexArrays(1, &vao);
		qglBindVertexArray(vao);
#ifndef __APPLE__
		if (glRefConfig.annotateResources) qglObjectLabel(GL_VERTEX_ARRAY, vao, -1, "GlobalVAO");
#endif
		tr.globalVao = vao;

		// set default state
		GL_SetDefaultState();

		R_Splash();	//get something on screen asap
	}
	else
	{
		if (r_com_rend2->integer != 1)
		{
			ri.Cvar_Set("com_rend2", "1");
		}
		// set default state
		GL_SetDefaultState();
	}
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrs(const char* file, int line) {
#if defined(_DEBUG)
	GLenum	err;
	char	s[64];

	err = qglGetError();
	if (err == GL_NO_ERROR) {
		return;
	}
	if (r_ignoreGLErrors->integer) {
		return;
	}
	switch (err) {
	case GL_INVALID_ENUM:
		strcpy(s, "GL_INVALID_ENUM");
		break;
	case GL_INVALID_VALUE:
		strcpy(s, "GL_INVALID_VALUE");
		break;
	case GL_INVALID_OPERATION:
		strcpy(s, "GL_INVALID_OPERATION");
		break;
	case GL_OUT_OF_MEMORY:
		strcpy(s, "GL_OUT_OF_MEMORY");
		break;
	default:
		Com_sprintf(s, sizeof(s), "%i", err);
		break;
	}

	ri.Error(ERR_FATAL, "GL_CheckErrors: %s in %s at line %d", s, file, line);
#endif
}

/*
==============================================================================

						SCREEN SHOTS

NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
vanilla q3 .. baseq3/screenshots/ *.tga
team arena .. missionpack/screenshots/ *.tga

two commands: "screenshot" and "screenshotJPEG"
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with FS_FileExists / FS_FOpenFileWrite calls)
FIXME: the statics don't get a reinit between fs_game changes

==============================================================================
*/

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with R2_Hunk_FreeTempMemory()
==================
*/

byte* RB_ReadPixels(
	int x, int y, int width, int height, size_t* offset, int* padlen)
{
	byte* buffer, * bufstart;
	int padwidth, linelen;
	GLint packAlign;

	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = width * 3;
	padwidth = PAD(linelen, packAlign);

	// Allocate a few more bytes so that we can choose an alignment we like
	buffer = (byte*)Hunk_AllocateTempMemory(padwidth * height + *offset + packAlign - 1);

	bufstart = (byte*)(PADP((intptr_t)buffer + *offset, packAlign));
	qglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart);

	*offset = bufstart - buffer;
	*padlen = padwidth - linelen;

	return buffer;
}

static void ConvertRGBtoBGR(
	byte* dst, const byte* src, int stride, int width, int height)
{
	const byte* row = src;
	for (int y = 0; y < height; ++y)
	{
		const byte* pixelRGB = row;
		for (int x = 0; x < width; ++x)
		{
			// swap rgb to bgr
			const byte temp = pixelRGB[0];
			*dst++ = pixelRGB[2];
			*dst++ = pixelRGB[1];
			*dst++ = temp;

			pixelRGB += 3;
		}

		row += stride;
	}
}

static void R_SaveTGA(
	const char* filename,
	const byte* pixels,
	int width,
	int height,
	int stride)
{
	const size_t headerSize = 18;
	const size_t pixelBufferSize = stride * height;
	const size_t bufferSize = headerSize + pixelBufferSize;

	byte* buffer = (byte*)Hunk_AllocateTempMemory(bufferSize);

	// Write TGA header
	Com_Memset(buffer, 0, headerSize);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24; // pixel size

	ConvertRGBtoBGR(buffer + headerSize, pixels, stride, width, height);

	ri.FS_WriteFile(filename, buffer, bufferSize);
	Hunk_FreeTempMemory(buffer);
}

/*
==================
R_SaveScreenshotTGA
==================
*/
static void R_SaveScreenshotTGA(
	const screenshotReadback_t* screenshotReadback, byte* pixels)
{
	R_SaveTGA(
		screenshotReadback->filename,
		pixels,
		screenshotReadback->width,
		screenshotReadback->height,
		screenshotReadback->strideInBytes);
}

/*
==================
R_SaveScreenshotPNG
==================
*/
static void R_SaveScreenshotPNG(
	const screenshotReadback_t* screenshotReadback, byte* pixels)
{
	RE_SavePNG(
		screenshotReadback->filename,
		pixels,
		screenshotReadback->width,
		screenshotReadback->height,
		3);
}

/*
==================
R_SaveScreenshotJPG
==================
*/
static void R_SaveScreenshotJPG(
	const screenshotReadback_t* screenshotReadback, byte* pixels)
{
	RE_SaveJPG(
		screenshotReadback->filename,
		r_screenshotJpegQuality->integer,
		screenshotReadback->width,
		screenshotReadback->height,
		pixels,
		screenshotReadback->strideInBytes - screenshotReadback->rowInBytes);
}

void R_SaveScreenshot(screenshotReadback_t* screenshotReadback)
{
	qglBindBuffer(GL_PIXEL_PACK_BUFFER, screenshotReadback->pbo);

	byte* pixelBuffer = static_cast<byte*>(
		qglMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));

	if (pixelBuffer == nullptr)
	{
		ri.Printf(
			PRINT_ALL,
			S_COLOR_RED "Failed to read screenshot data from GPU\n");
	}
	else
	{
		const int height = screenshotReadback->height;
		const int stride = screenshotReadback->strideInBytes;
		const size_t pixelBufferSize = stride * height;

		byte* pixels = (byte*)Hunk_AllocateTempMemory(pixelBufferSize);
		Com_Memcpy(pixels, pixelBuffer, pixelBufferSize);
		qglUnmapBuffer(GL_PIXEL_PACK_BUFFER);

		if (glConfig.deviceSupportsGamma)
			R_GammaCorrect(pixels, pixelBufferSize);

		switch (screenshotReadback->format)
		{
		case SSF_JPEG:
			R_SaveScreenshotJPG(screenshotReadback, pixels);
			break;

		case SSF_TGA:
			R_SaveScreenshotTGA(screenshotReadback, pixels);
			break;

		case SSF_PNG:
			R_SaveScreenshotPNG(screenshotReadback, pixels);
			break;
		}

		Hunk_FreeTempMemory(pixels);
	}

	qglDeleteBuffers(1, &screenshotReadback->pbo);
	screenshotReadback->pbo = 0;
}

/*
==================
R_TakeScreenshotCmd
==================
*/
const void* RB_TakeScreenshotCmd(const void* data) {
	const screenshotCommand_t* cmd;

	cmd = (const screenshotCommand_t*)data;

	// finish any 2D drawing if needed
	if (tess.numIndexes)
		RB_EndSurface();

	const int frameNumber = backEndData->realFrameNumber;
	gpuFrame_t* thisFrame = &backEndData->frames[frameNumber % MAX_FRAMES];
	screenshotReadback_t* screenshot = &thisFrame->screenshotReadback;

	GLint packAlign;
	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	const int linelen = cmd->width * 3;
	const int strideInBytes = PAD(linelen, packAlign);

	qglGenBuffers(1, &screenshot->pbo);
	qglBindBuffer(GL_PIXEL_PACK_BUFFER, screenshot->pbo);
	if (glRefConfig.annotateResources) qglObjectLabel(GL_BUFFER, screenshot->pbo, -1, "Screenshot_PBO");
	qglBufferData(
		GL_PIXEL_PACK_BUFFER,
		strideInBytes * cmd->height,
		nullptr,
		GL_STATIC_COPY);
	qglReadPixels(
		cmd->x, cmd->y, cmd->width, cmd->height, GL_RGB, GL_UNSIGNED_BYTE, 0);

	screenshot->strideInBytes = strideInBytes;
	screenshot->rowInBytes = linelen;
	screenshot->width = cmd->width;
	screenshot->height = cmd->height;
	screenshot->format = cmd->format;
	Q_strncpyz(
		screenshot->filename, cmd->fileName, sizeof(screenshot->filename));

	return (const void*)(cmd + 1);
}

/*
==================
R_TakeScreenshot
==================
*/
static void R_TakeScreenshot(int x, int y, int width, int height, char* name, screenshotFormat_t format)
{
	static char	fileName[MAX_OSPATH]; // bad things if two screenshots per frame?
	screenshotCommand_t* cmd;

	cmd = (screenshotCommand_t*)R_GetCommandBuffer(sizeof(*cmd));
	if (!cmd) {
		return;
	}
	cmd->commandId = RC_SCREENSHOT;

	cmd->x = x;
	cmd->y = y;
	cmd->width = width;
	cmd->height = height;
	Q_strncpyz(fileName, name, sizeof(fileName));
	cmd->fileName = fileName;
	cmd->format = format;
}

/*
==================
R_ScreenshotFilename
==================
*/
static void R_ScreenshotFilename(char* buf, int bufSize, const char* ext)
{
	time_t rawtime;
	char timeStr[32] = { 0 }; // should really only reach ~19 chars

	time(&rawtime);
	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", localtime(&rawtime)); // or gmtime

	Com_sprintf(buf, bufSize, "screenshots/shot%s%s", timeStr, ext);
}

/*
====================
R_LevelShot

levelshots are specialized 256*256 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
constexpr auto LEVELSHOTSIZE = 256;
static void R_LevelShot(void) {
	char		checkname[MAX_OSPATH];
	byte* buffer;
	byte* source, * allsource;
	byte* src, * dst;
	size_t		offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	Com_sprintf(checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName);

	allsource = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
	source = allsource + offset;

	buffer = (byte*)Hunk_AllocateTempMemory(LEVELSHOTSIZE * LEVELSHOTSIZE * 3 + 18);
	Com_Memset(buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = LEVELSHOTSIZE & 255;
	buffer[13] = LEVELSHOTSIZE >> 8;
	buffer[14] = LEVELSHOTSIZE & 255;
	buffer[15] = LEVELSHOTSIZE >> 8;
	buffer[16] = 24;	// pixel size

	// resample from source
	xScale = glConfig.vidWidth / (4.0 * LEVELSHOTSIZE);
	yScale = glConfig.vidHeight / (3.0 * LEVELSHOTSIZE);
	for (y = 0; y < LEVELSHOTSIZE; y++) {
		for (x = 0; x < LEVELSHOTSIZE; x++) {
			r = g = b = 0;
			for (yy = 0; yy < 3; yy++) {
				for (xx = 0; xx < 4; xx++) {
					src = source + 3 * (glConfig.vidWidth * (int)((y * 3 + yy) * yScale) + (int)((x * 4 + xx) * xScale));
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * (y * LEVELSHOTSIZE + x);
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correct
	if ((tr.overbrightBits > 0) && glConfig.deviceSupportsGamma) {
		R_GammaCorrect(buffer + 18, LEVELSHOTSIZE * LEVELSHOTSIZE * 3);
	}

	ri.FS_WriteFile(checkname, buffer, LEVELSHOTSIZE * LEVELSHOTSIZE * 3 + 18);

	Hunk_FreeTempMemory(buffer);
	Hunk_FreeTempMemory(allsource);

	ri.Printf(PRINT_ALL, "Wrote %s\n", checkname);
}

/*
==================
R_ScreenShotTGA_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShotTGA_f(void) {
	char checkname[MAX_OSPATH] = { 0 };
	qboolean silent = qfalse;

	if (!strcmp(ri.Cmd_Argv(1), "levelshot")) {
		R_LevelShot();
		return;
	}

	if (!strcmp(ri.Cmd_Argv(1), "silent"))
		silent = qtrue;

	if (ri.Cmd_Argc() == 2 && !silent) {
		// explicit filename
		Com_sprintf(checkname, sizeof(checkname), "screenshots/%s.tga", ri.Cmd_Argv(1));
	}
	else {
		// timestamp the file
		R_ScreenshotFilename(checkname, sizeof(checkname), ".tga");

		if (ri.FS_FileExists(checkname)) {
			Com_Printf("ScreenShot: Couldn't create a file\n");
			return;
		}
	}

	R_TakeScreenshot(0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, SSF_TGA);

	if (!silent)
		ri.Printf(PRINT_ALL, "Wrote %s\n", checkname);
}

void R_ScreenShotPNG_f(void) {
	char checkname[MAX_OSPATH] = { 0 };
	qboolean silent = qfalse;

	if (!strcmp(ri.Cmd_Argv(1), "levelshot")) {
		R_LevelShot();
		return;
	}

	if (!strcmp(ri.Cmd_Argv(1), "silent"))
		silent = qtrue;

	if (ri.Cmd_Argc() == 2 && !silent) {
		// explicit filename
		Com_sprintf(checkname, sizeof(checkname), "screenshots/%s.png", ri.Cmd_Argv(1));
	}
	else {
		// timestamp the file
		R_ScreenshotFilename(checkname, sizeof(checkname), ".png");

		if (ri.FS_FileExists(checkname)) {
			Com_Printf("ScreenShot: Couldn't create a file\n");
			return;
		}
	}

	R_TakeScreenshot(0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, SSF_PNG);

	if (!silent)
		ri.Printf(PRINT_ALL, "Wrote %s\n", checkname);
}

void R_ScreenShotJPEG_f(void) {
	char checkname[MAX_OSPATH] = { 0 };
	qboolean silent = qfalse;

	if (!strcmp(ri.Cmd_Argv(1), "levelshot")) {
		R_LevelShot();
		return;
	}

	if (!strcmp(ri.Cmd_Argv(1), "silent"))
		silent = qtrue;

	if (ri.Cmd_Argc() == 2 && !silent) {
		// explicit filename
		Com_sprintf(checkname, sizeof(checkname), "screenshots/%s.jpg", ri.Cmd_Argv(1));
	}
	else {
		// timestamp the file
		R_ScreenshotFilename(checkname, sizeof(checkname), ".jpg");

		if (ri.FS_FileExists(checkname)) {
			Com_Printf("ScreenShot: Couldn't create a file\n");
			return;
		}
	}

	R_TakeScreenshot(0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, SSF_JPEG);

	if (!silent)
		ri.Printf(PRINT_ALL, "Wrote %s\n", checkname);
}

//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/

const void* RB_TakeVideoFrameCmd(const void* data)
{
	const videoFrameCommand_t* cmd;
	/*byte* cBuf;
	size_t				memcount, linelen;
	int				padwidth, avipadwidth, padlen, avipadlen;
	GLint packAlign;*/

	// finish any 2D drawing if needed
	if (tess.numIndexes)
		RB_EndSurface();

	cmd = (const videoFrameCommand_t*)data;
#ifndef REND2_SP
	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = cmd->width * 3;

	// Alignment stuff for glReadPixels
	padwidth = PAD(linelen, packAlign);
	padlen = padwidth - linelen;
	// AVI line padding
	avipadwidth = PAD(linelen, AVI_LINE_PADDING);
	avipadlen = avipadwidth - linelen;

	cBuf = (byte*)(PADP(cmd->captureBuffer, packAlign));

	qglReadPixels(0, 0, cmd->width, cmd->height, GL_RGB,
		GL_UNSIGNED_BYTE, cBuf);

	memcount = padwidth * cmd->height;

	// gamma correct
	if (glConfig.deviceSupportsGamma)
		R_GammaCorrect(cBuf, memcount);

	if (cmd->motionJpeg)
	{
		memcount = RE_SaveJPGToBuffer(cmd->encodeBuffer, linelen * cmd->height,
			r_aviMotionJpegQuality->integer,
			cmd->width, cmd->height, cBuf, padlen);
		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, memcount);
	}
	else
	{
		byte* lineend, * memend;
		byte* srcptr, * destptr;

		srcptr = cBuf;
		destptr = cmd->encodeBuffer;
		memend = srcptr + memcount;

		// swap R and B and remove line paddings
		while (srcptr < memend)
		{
			lineend = srcptr + linelen;
			while (srcptr < lineend)
			{
				*destptr++ = srcptr[2];
				*destptr++ = srcptr[1];
				*destptr++ = srcptr[0];
				srcptr += 3;
			}

			Com_Memset(destptr, '\0', avipadlen);
			destptr += avipadlen;

			srcptr += padlen;
		}

		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, avipadwidth * cmd->height);
	}
#endif
	return (const void*)(cmd + 1);
}

//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState(void)
{
	qglClearDepth(1.0f);

	qglCullFace(GL_FRONT);

	// initialize downstream texture unit if we're running
	// in a multitexture environment
	GL_SelectTexture(1);
	GL_TextureMode(r_textureMode->string);
	GL_SelectTexture(0);

	GL_TextureMode(r_textureMode->string);

	//qglShadeModel( GL_SMOOTH );
	qglDepthFunc(GL_LEQUAL);

	Com_Memset(&glState, 0, sizeof(glState));

	//
	// make sure our GL state vector is set correctly
	//
	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;
	glState.maxDepth = 1.0f;
	qglDepthRange(0.0f, 1.0f);

	qglUseProgram(0);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);
	qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	qglDepthMask(GL_TRUE);
	qglDisable(GL_DEPTH_TEST);
	qglEnable(GL_SCISSOR_TEST);
	qglEnable(GL_PROGRAM_POINT_SIZE);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_BLEND);
	glState.blend = false;

	qglEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	qglStencilFunc(GL_ALWAYS, 0, 0xff);
	qglStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
	qglStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

	// set default vertex color
	qglVertexAttrib4f(ATTR_INDEX_COLOR, 1.0f, 1.0f, 1.0f, 1.0f);

	// invalidate all vertex step rates
	// this ensures that attributes that have a set divisor will have a 
	// correct divisor set after GL_SetDefaultState is called
	for (int i = 0; i < ATTR_INDEX_MAX; i++)
	{
		glState.currentVaoAttribs[i].stepRate = -1;
	}
}

/*
================
R_PrintLongString

Workaround for ri.Printf's 1024 characters buffer limit.
================
*/
static void R_PrintLongString(const char* string) 
{
	char buffer[1024];
	const char* p;
	int size = strlen(string);

	p = string;
	while (size > 0)
	{
		Q_strncpyz(buffer, p, sizeof(buffer));
		ri.Printf(PRINT_ALL, "%s", buffer);
		p += 1023;
		size -= 1023;
	}
}

/*
================
GfxInfo_f
================
*/
static void GfxInfo_f(void)
{
	const char* enablestrings[] =
	{
		"disabled",
		"enabled"
	};
	const char* fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};
	const char* noborderstrings[] =
	{
		"",
		"noborder "
	};

	int fullscreen = ri.Cvar_VariableIntegerValue("r_fullscreen");
	int noborder = ri.Cvar_VariableIntegerValue("r_noborder");

	ri.Printf(PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string);
	ri.Printf(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
	ri.Printf(PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string);
	ri.Printf(PRINT_ALL, "GL_EXTENSIONS: ");
	R_PrintLongString(glConfigExt.originalExtensionString);
	ri.Printf(PRINT_ALL, "\n");
	ri.Printf(PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize);
	ri.Printf(PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);
	ri.Printf(PRINT_ALL, "MODE: %d, %d x %d %s%s hz:",
		ri.Cvar_VariableIntegerValue("r_mode"),
		glConfig.vidWidth, glConfig.vidHeight,
		fullscreen == 0 ? noborderstrings[noborder == 1] : noborderstrings[0],
		fsstrings[fullscreen == 1]);
	if (glConfig.displayFrequency)
	{
		ri.Printf(PRINT_ALL, "%d\n", glConfig.displayFrequency);
	}
	else
	{
		ri.Printf(PRINT_ALL, "N/A\n");
	}
	if (glConfig.deviceSupportsGamma)
	{
		ri.Printf(PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits);
	}
	else
	{
		ri.Printf(PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits);
	}

	ri.Printf(PRINT_ALL, "texturemode: %s\n", r_textureMode->string);
	ri.Printf(PRINT_ALL, "picmip: %d\n", r_picmip->integer);
	ri.Printf(PRINT_ALL, "texture bits: %d\n", r_texturebits->integer);

	if (r_vertexLight->integer)
	{
		ri.Printf(PRINT_ALL, "HACK: using vertex lightmap approximation\n");
	}
	int displayRefresh = ri.Cvar_VariableIntegerValue("r_displayRefresh");
	if (displayRefresh) {
		ri.Printf(PRINT_ALL, "Display refresh set to %d\n", displayRefresh);
	}

	if (r_finish->integer) {
		ri.Printf(PRINT_ALL, "Forcing glFinish\n");
	}

	ri.Printf(PRINT_ALL, "Dynamic Glow: %s\n", enablestrings[r_dynamicGlow->integer != 0]);
}

/*
================
GfxMemInfo_f
================
*/
static void GfxMemInfo_f(void)
{
	switch (glRefConfig.memInfo)
	{
	case MI_NONE:
	{
		ri.Printf(PRINT_ALL, "No extension found for GPU memory info.\n");
	}
	break;
	case MI_NVX:
	{
		int value;

		qglGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &value);
		ri.Printf(PRINT_ALL, "GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX: %ikb\n", value);

		qglGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &value);
		ri.Printf(PRINT_ALL, "GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX: %ikb\n", value);

		qglGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &value);
		ri.Printf(PRINT_ALL, "GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX: %ikb\n", value);

		qglGetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &value);
		ri.Printf(PRINT_ALL, "GPU_MEMORY_INFO_EVICTION_COUNT_NVX: %i\n", value);

		qglGetIntegerv(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &value);
		ri.Printf(PRINT_ALL, "GPU_MEMORY_INFO_EVICTED_MEMORY_NVX: %ikb\n", value);
	}
	break;
	case MI_ATI:
	{
		// GL_ATI_meminfo
		int value[4]{};

		qglGetIntegerv(GL_VBO_FREE_MEMORY_ATI, &value[0]);
		ri.Printf(PRINT_ALL, "VBO_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);

		qglGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &value[0]);
		ri.Printf(PRINT_ALL, "TEXTURE_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);

		qglGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, &value[0]);
		ri.Printf(PRINT_ALL, "RENDERBUFFER_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);
	}
	break;
	}
}

static void R_CaptureFrameData_f()
{
	int argc = ri.Cmd_Argc();
	if (argc <= 1)
	{
		ri.Printf(PRINT_ALL, "Usage: %s <multi|single>\n", ri.Cmd_Argv(0));
		return;
	}

	const char* cmd = ri.Cmd_Argv(1);
	if (Q_stricmp(cmd, "single") == 0)
		tr.numFramesToCapture = 1;
	else if (Q_stricmp(cmd, "multi") == 0)
		tr.numFramesToCapture = atoi(ri.Cmd_Argv(1));

	int len = ri.FS_FOpenFileByMode("rend2.log", &tr.debugFile, FS_APPEND);
	if (len == -1 || !tr.debugFile)
	{
		ri.Printf(PRINT_ERROR, "Failed to open rend2 log file\n");
		tr.numFramesToCapture = 0;
	}
}

typedef struct consoleCommand_s {
	const char* cmd;
	xcommand_t	func;
} consoleCommand_t;

static consoleCommand_t	commands[] = {
	{ "imagelist",			R_ImageList_f },
	{ "shaderlist",			R_ShaderList_f },
	{ "skinlist",			R_SkinList_f },
	{ "fontlist",			R_FontList_f },
	{ "screenshot",			R_ScreenShotJPEG_f },
	{ "screenshot_png",		R_ScreenShotPNG_f },
	{ "screenshot_tga",		R_ScreenShotTGA_f },
	{ "gfxinfo",			GfxInfo_f },
	{ "gfxmeminfo",			GfxMemInfo_f },
	{ "r_we",				R_WorldEffect_f },
	{ "modellist",			R_Modellist_f },
	{ "vbolist",			R_VBOList_f },
	{ "capframes",			R_CaptureFrameData_f },
	{ "r_weather",			R_WeatherEffect_f },
	{ "weather",			R_SetWeatherEffect_f },
};

static const size_t numCommands = ARRAY_LEN(commands);

/*
===============
R_Register
===============
*/
static void R_Register(void)
{
	//
	// latched and archived variables
	//
	r_allowExtensions = ri_Cvar_Get_NoComm("r_allowExtensions", "1", CVAR_ARCHIVE | CVAR_LATCH, "Allow GL extensions");
	r_ext_compressed_textures = ri_Cvar_Get_NoComm("r_ext_compress_textures", "0", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable texture compression");
	r_ext_compressed_lightmaps = ri.Cvar_Get("r_ext_compress_lightmaps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH);
	r_ext_multitexture = ri_Cvar_Get_NoComm("r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_LATCH, "Unused");
	r_ext_compiled_vertex_array = ri_Cvar_Get_NoComm("r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE | CVAR_LATCH, "Unused");
	r_ext_texture_env_add = ri_Cvar_Get_NoComm("r_ext_texture_env_add", "1", CVAR_ARCHIVE | CVAR_LATCH, "Unused");
	r_ext_preferred_tc_method = ri_Cvar_Get_NoComm("r_ext_preferred_tc_method", "0", CVAR_ARCHIVE | CVAR_LATCH, "Preferred texture compression method");

	r_ext_draw_range_elements = ri_Cvar_Get_NoComm("r_ext_draw_range_elements", "1", CVAR_ARCHIVE | CVAR_LATCH, "Unused");
	r_ext_multi_draw_arrays = ri_Cvar_Get_NoComm("r_ext_multi_draw_arrays", "1", CVAR_ARCHIVE | CVAR_LATCH, "Unused");
	r_ext_texture_float = ri_Cvar_Get_NoComm("r_ext_texture_float", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable floating-point textures");
	r_arb_half_float_pixel = ri_Cvar_Get_NoComm("r_arb_half_float_pixel", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable ARB_half_float GL extension");
	r_ext_framebuffer_multisample = ri_Cvar_Get_NoComm("r_ext_multisample", "8", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable framebuffer MSAA");
	// We do MSAA resolving manually in rend2, so don't bother with the default framebuffer
	ri.Cvar_Set("r_ext_multisample_default_fb", "0");
	r_arb_seamless_cube_map = ri_Cvar_Get_NoComm("r_arb_seamless_cube_map", "0", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable seamless cube map filtering GL extension");
	r_arb_vertex_type_2_10_10_10_rev = ri_Cvar_Get_NoComm("r_arb_vertex_type_2_10_10_10_rev", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable 1010102 UI data type");
	r_arb_buffer_storage = ri_Cvar_Get_NoComm("r_arb_buffer_storage", "0", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable buffer storage GL extension");
	r_ext_texture_filter_anisotropic = ri_Cvar_Get_NoComm("r_ext_texture_filter_anisotropic", "16", CVAR_ARCHIVE, "Disable/enable anisotropic texture filtering");

	r_dynamicGlow = ri_Cvar_Get_NoComm("r_dynamicGlow", "1", CVAR_ARCHIVE, "");
	r_dynamicGlowPasses = ri_Cvar_Get_NoComm("r_dynamicGlowPasses", "5", CVAR_ARCHIVE, "");
	r_dynamicGlowDelta = ri_Cvar_Get_NoComm("r_dynamicGlowDelta", "0.8f", CVAR_ARCHIVE, "");
	r_dynamicGlowIntensity = ri_Cvar_Get_NoComm("r_dynamicGlowIntensity", "1.13f", CVAR_ARCHIVE, "");
	r_dynamicGlowSoft = ri_Cvar_Get_NoComm("r_dynamicGlowSoft", "1", CVAR_ARCHIVE, "");
	r_dynamicGlowWidth = ri_Cvar_Get_NoComm("r_dynamicGlowWidth", "320", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_dynamicGlowHeight = ri_Cvar_Get_NoComm("r_dynamicGlowHeight", "240", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_dynamicGlowBloom = ri_Cvar_Get_NoComm("r_dynamicGlowBloom", "0.0", CVAR_ARCHIVE, "");
	ri.Cvar_CheckRange(r_dynamicGlowBloom, 0.f, 2.f, qfalse);

	r_debugContext = ri_Cvar_Get_NoComm("r_debugContext", "0", CVAR_LATCH, "");
	r_debugWeather = ri_Cvar_Get_NoComm("r_debugWeather", "0", CVAR_ARCHIVE, "");

	r_picmip = ri_Cvar_Get_NoComm("r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_picmip, 0, 16, qtrue);
	r_roundImagesDown = ri_Cvar_Get_NoComm("r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_colorMipLevels = ri_Cvar_Get_NoComm("r_colorMipLevels", "0", CVAR_LATCH, "");
	r_detailTextures = ri_Cvar_Get_NoComm("r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_texturebits = ri_Cvar_Get_NoComm("r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_overBrightBits = ri_Cvar_Get_NoComm("r_overBrightBits", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_simpleMipMaps = ri_Cvar_Get_NoComm("r_simpleMipMaps", "1", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_vertexLight = ri_Cvar_Get_NoComm("r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_uiFullScreen = ri_Cvar_Get_NoComm("r_uifullscreen", "0", 0, "");
	r_subdivisions = ri_Cvar_Get_NoComm("r_subdivisions", "4", CVAR_ARCHIVE | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_subdivisions, 4, 80, qfalse);
	r_stereo = ri_Cvar_Get_NoComm("r_stereo", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_greyscale = ri_Cvar_Get_NoComm("r_greyscale", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	ri.Cvar_CheckRange(r_greyscale, 0, 1, qfalse);

	r_externalGLSL = ri_Cvar_Get_NoComm("r_externalGLSL", "0", CVAR_LATCH, "");

	r_hdr = ri_Cvar_Get_NoComm("r_hdr", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable rendering in HDR");
	r_floatLightmap = ri_Cvar_Get_NoComm("r_floatLightmap", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable HDR lightmap support");

	r_toneMap = ri_Cvar_Get_NoComm("r_toneMap", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable tonemapping");
	r_forceToneMap = ri_Cvar_Get_NoComm("r_forceToneMap", "0", CVAR_CHEAT, "");
	r_forceToneMapMin = ri_Cvar_Get_NoComm("r_forceToneMapMin", "-8.0", CVAR_CHEAT, "");
	r_forceToneMapAvg = ri_Cvar_Get_NoComm("r_forceToneMapAvg", "-1.0", CVAR_CHEAT, "");
	r_forceToneMapMax = ri_Cvar_Get_NoComm("r_forceToneMapMax", "0.0", CVAR_CHEAT, "");

	r_autoExposure = ri_Cvar_Get_NoComm("r_autoExposure", "1", CVAR_ARCHIVE, "Disable/enable auto exposure");
	r_forceAutoExposure = ri_Cvar_Get_NoComm("r_forceAutoExposure", "0", CVAR_CHEAT, "");
	r_forceAutoExposureMin = ri_Cvar_Get_NoComm("r_forceAutoExposureMin", "-3", CVAR_CHEAT, "");
	r_forceAutoExposureMax = ri_Cvar_Get_NoComm("r_forceAutoExposureMax", "1.0", CVAR_CHEAT, "");

	r_smaa = ri_Cvar_Get_NoComm("r_smaa", "0", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable SMAA");
	r_smaa_quality = ri_Cvar_Get_NoComm("r_smaa_quality", "2", CVAR_ARCHIVE | CVAR_LATCH, "0: LOW | 1: MEDIUM | 2: HIGH | 3: ULTRA");

	r_cameraExposure = ri_Cvar_Get_NoComm("r_cameraExposure", "0", CVAR_ARCHIVE | CVAR_CHEAT, "");

	r_depthPrepass = ri_Cvar_Get_NoComm("r_depthPrepass", "1", CVAR_ARCHIVE, "");
	r_ssao = ri_Cvar_Get_NoComm("r_ssao", "1", CVAR_LATCH | CVAR_ARCHIVE, "");

	r_normalMapping = ri_Cvar_Get_NoComm("r_normalMapping", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable normal mapping");
	r_specularMapping = ri_Cvar_Get_NoComm("r_specularMapping", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable specular mapping");
	r_deluxeMapping = ri_Cvar_Get_NoComm("r_deluxeMapping", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable reading deluxemaps when compiled with q3map2");
	r_deluxeSpecular = ri_Cvar_Get_NoComm("r_deluxeSpecular", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable/scale the specular response from deluxemaps");
	r_cubeMapping = ri_Cvar_Get_NoComm("r_cubeMapping", "0", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable cubemapping");
	r_cubeMappingBounces = ri_Cvar_Get_NoComm("r_cubeMappingBounces", "1", CVAR_ARCHIVE | CVAR_LATCH, "Renders cubemaps multiple times to get reflections in reflections");
	ri.Cvar_CheckRange(r_cubeMappingBounces, 0, 2, qfalse);
	r_baseNormalX = ri_Cvar_Get_NoComm("r_baseNormalX", "1.0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_baseNormalY = ri_Cvar_Get_NoComm("r_baseNormalY", "1.0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_baseParallax = ri_Cvar_Get_NoComm("r_baseParallax", "0.05", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_baseSpecular = ri_Cvar_Get_NoComm("r_baseSpecular", "0.04", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_dlightMode = ri_Cvar_Get_NoComm("r_dlightMode", "1", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_pshadowDist = ri_Cvar_Get_NoComm("r_pshadowDist", "128", CVAR_ARCHIVE, "");
	r_imageUpsample = ri_Cvar_Get_NoComm("r_imageUpsample", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_imageUpsampleMaxSize = ri_Cvar_Get_NoComm("r_imageUpsampleMaxSize", "1024", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_imageUpsampleType = ri_Cvar_Get_NoComm("r_imageUpsampleType", "1", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_genNormalMaps = ri_Cvar_Get_NoComm("r_genNormalMaps", "1", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable generating normal maps from diffuse maps");

	r_forceSun = ri_Cvar_Get_NoComm("r_forceSun", "0", CVAR_CHEAT, "");
	r_forceSunMapLightScale = ri_Cvar_Get_NoComm("r_forceSunMapLightScale", "1.0", CVAR_CHEAT, "");
	r_forceSunLightScale = ri_Cvar_Get_NoComm("r_forceSunLightScale", "1.0", CVAR_CHEAT, "");
	r_forceSunAmbientScale = ri_Cvar_Get_NoComm("r_forceSunAmbientScale", "0.5", CVAR_CHEAT, "");
	r_drawSunRays = ri_Cvar_Get_NoComm("r_drawSunRays", "0", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_sunlightMode = ri_Cvar_Get_NoComm("r_sunlightMode", "1", CVAR_ARCHIVE | CVAR_LATCH, "");

	r_volumetricFog = ri_Cvar_Get_NoComm("r_volumetricFog", "0", CVAR_ARCHIVE | CVAR_LATCH, "Disable/enable lightgrid lighting on fog volumes");
	r_volumetricFogDefaultScale = ri_Cvar_Get_NoComm("r_volumetricFogDefaultScale", "1.0", CVAR_ARCHIVE | CVAR_LATCH, "Scales volumetric fog density unless scale has been explicitly defined");
	r_volumetricFogSamples = ri_Cvar_Get_NoComm("r_volumetricFogSamples", "48", CVAR_ARCHIVE | CVAR_LATCH, "How many ray samples to take");
	ri.Cvar_CheckRange(r_volumetricFogSamples, 16, 128, qfalse);
	r_volumetricFogScale = ri_Cvar_Get_NoComm("r_volumetricFogScale", "1.0", CVAR_TEMP, "Temporarily scales volumetric fog density");

	r_sunShadows = ri_Cvar_Get_NoComm("r_sunShadows", "1", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_shadowFilter = ri_Cvar_Get_NoComm("r_shadowFilter", "1", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_shadowMapSize = ri_Cvar_Get_NoComm("r_shadowMapSize", "1024", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_shadowCascadeZNear = ri_Cvar_Get_NoComm("r_shadowCascadeZNear", "4", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_shadowCascadeZFar = ri_Cvar_Get_NoComm("r_shadowCascadeZFar", "3072", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_shadowCascadeZBias = ri_Cvar_Get_NoComm("r_shadowCascadeZBias", "-320", CVAR_ARCHIVE | CVAR_LATCH, "");
	r_ignoreDstAlpha = ri_Cvar_Get_NoComm("r_ignoreDstAlpha", "1", CVAR_ARCHIVE | CVAR_LATCH, "");

	//
	// temporary latched variables that can only change over a restart
	//
	r_fullbright = ri_Cvar_Get_NoComm("r_fullbright", "0", CVAR_LATCH | CVAR_CHEAT, "");
	r_mapOverBrightBits = ri_Cvar_Get_NoComm("r_mapOverBrightBits", "0", CVAR_LATCH, "");
	r_intensity = ri_Cvar_Get_NoComm("r_intensity", "1", CVAR_LATCH, "");
	r_singleShader = ri_Cvar_Get_NoComm("r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH, "");

	//
	// archived variables that can change at any time
	//
	r_lodCurveError = ri_Cvar_Get_NoComm("r_lodCurveError", "250", CVAR_ARCHIVE | CVAR_CHEAT, "");
	r_lodbias = ri_Cvar_Get_NoComm("r_lodbias", "0", CVAR_ARCHIVE, "");
	r_flares = ri_Cvar_Get_NoComm("r_flares", "1", CVAR_ARCHIVE, "");
	r_znear = ri_Cvar_Get_NoComm("r_znear", "4", CVAR_CHEAT, "");
	ri.Cvar_CheckRange(r_znear, 0.001f, 200, qfalse);
	r_autolodscalevalue = ri_Cvar_Get_NoComm("r_autolodscalevalue", "0", CVAR_ROM, "");
	r_zproj = ri_Cvar_Get_NoComm("r_zproj", "64", CVAR_ARCHIVE, "");
	r_stereoSeparation = ri_Cvar_Get_NoComm("r_stereoSeparation", "64", CVAR_ARCHIVE, "");
	r_ignoreGLErrors = ri_Cvar_Get_NoComm("r_ignoreGLErrors", "1", CVAR_ARCHIVE, "");
	r_fastsky = ri_Cvar_Get_NoComm("r_fastsky", "0", CVAR_ARCHIVE, "");
	r_inGameVideo = ri_Cvar_Get_NoComm("r_inGameVideo", "1", CVAR_ARCHIVE, "");
	r_drawSun = ri_Cvar_Get_NoComm("r_drawSun", "0", CVAR_ARCHIVE, "");
	r_dynamiclight = ri_Cvar_Get_NoComm("r_dynamiclight", "1", CVAR_ARCHIVE, "");
	r_finish = ri_Cvar_Get_NoComm("r_finish", "0", CVAR_ARCHIVE, "");
	r_textureMode = ri_Cvar_Get_NoComm("r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, "");
	r_markcount = ri_Cvar_Get_NoComm("r_markcount", "100", CVAR_ARCHIVE, "");
	r_gamma = ri_Cvar_Get_NoComm("r_gamma", "1", CVAR_ARCHIVE, "");
	r_facePlaneCull = ri_Cvar_Get_NoComm("r_facePlaneCull", "1", CVAR_ARCHIVE, "");

	r_parallaxMapping = ri_Cvar_Get_NoComm("r_parallaxMapping", "0", CVAR_ARCHIVE, "Disable/enable parallax mapping");

	r_ambientScale = ri_Cvar_Get_NoComm("r_ambientScale", "0.6", CVAR_CHEAT, "");
	r_directedScale = ri_Cvar_Get_NoComm("r_directedScale", "1", CVAR_CHEAT, "");

	r_anaglyphMode = ri_Cvar_Get_NoComm("r_anaglyphMode", "0", CVAR_ARCHIVE, "");
	r_mergeMultidraws = ri_Cvar_Get_NoComm("r_mergeMultidraws", "1", CVAR_ARCHIVE, "");
	r_mergeLeafSurfaces = ri_Cvar_Get_NoComm("r_mergeLeafSurfaces", "1", CVAR_ARCHIVE, "");

	//
	// temporary variables that can change at any time
	//
	r_showImages = ri_Cvar_Get_NoComm("r_showImages", "0", CVAR_TEMP, "");

	r_debugLight = ri_Cvar_Get_NoComm("r_debuglight", "0", CVAR_TEMP, "");
	r_debugSort = ri_Cvar_Get_NoComm("r_debugSort", "0", CVAR_CHEAT, "");
	r_printShaders = ri_Cvar_Get_NoComm("r_printShaders", "0", 0, "");
	r_saveFontData = ri_Cvar_Get_NoComm("r_saveFontData", "0", 0, "");

	r_forceParallaxBias = ri_Cvar_Get_NoComm("r_forceParallaxBias", "0", CVAR_TEMP, "");
	ri.Cvar_CheckRange(r_forceParallaxBias, 0.0f, 1.0f, qfalse);

	r_nocurves = ri_Cvar_Get_NoComm("r_nocurves", "0", CVAR_CHEAT, "");
	r_drawworld = ri_Cvar_Get_NoComm("r_drawworld", "1", CVAR_CHEAT, "");
	r_drawfog = ri_Cvar_Get_NoComm("r_drawfog", "2", CVAR_CHEAT, "");
	r_lightmap = ri_Cvar_Get_NoComm("r_lightmap", "0", 0, "");
	r_portalOnly = ri_Cvar_Get_NoComm("r_portalOnly", "0", CVAR_CHEAT, "");

	r_skipBackEnd = ri_Cvar_Get_NoComm("r_skipBackEnd", "0", CVAR_CHEAT, "");

	r_measureOverdraw = ri_Cvar_Get_NoComm("r_measureOverdraw", "0", CVAR_CHEAT, "");
	r_lodscale = ri_Cvar_Get_NoComm("r_lodscale", "5", CVAR_CHEAT, "");
	r_norefresh = ri_Cvar_Get_NoComm("r_norefresh", "0", CVAR_CHEAT, "");
	r_drawentities = ri_Cvar_Get_NoComm("r_drawentities", "1", CVAR_CHEAT, "");
	r_ignore = ri_Cvar_Get_NoComm("r_ignore", "1", CVAR_CHEAT, "");
	r_nocull = ri_Cvar_Get_NoComm("r_nocull", "0", CVAR_CHEAT, "");
	r_novis = ri_Cvar_Get_NoComm("r_novis", "0", CVAR_CHEAT, "");
	r_showcluster = ri_Cvar_Get_NoComm("r_showcluster", "0", CVAR_CHEAT, "");
	r_speeds = ri_Cvar_Get_NoComm("r_speeds", "0", CVAR_CHEAT, "");
	r_verbose = ri_Cvar_Get_NoComm("r_verbose", "0", CVAR_CHEAT, "");
	r_logFile = ri_Cvar_Get_NoComm("r_logFile", "0", CVAR_CHEAT, "");
	r_debugSurface = ri_Cvar_Get_NoComm("r_debugSurface", "0", CVAR_CHEAT, "");
	r_nobind = ri_Cvar_Get_NoComm("r_nobind", "0", CVAR_CHEAT, "");
	r_showtris = ri_Cvar_Get_NoComm("r_showtris", "0", CVAR_CHEAT, "");
	r_showsky = ri_Cvar_Get_NoComm("r_showsky", "0", CVAR_CHEAT, "");
	r_shownormals = ri_Cvar_Get_NoComm("r_shownormals", "0", CVAR_CHEAT, "");
	r_clear = ri_Cvar_Get_NoComm("r_clear", "0", CVAR_CHEAT, "");
	r_offsetFactor = ri_Cvar_Get_NoComm("r_offsetfactor", "-1", CVAR_CHEAT, "");
	r_offsetUnits = ri_Cvar_Get_NoComm("r_offsetunits", "-2", CVAR_CHEAT, "");

	r_shadowOffsetFactor = ri_Cvar_Get_NoComm("r_shadowOffsetFactor", "1.0", CVAR_CHEAT, "");
	r_shadowOffsetUnits = ri_Cvar_Get_NoComm("r_shadowOffsetUnits", "1.0", CVAR_CHEAT, "");

	r_drawBuffer = ri_Cvar_Get_NoComm("r_drawBuffer", "GL_BACK", CVAR_CHEAT, "");
	r_lockpvs = ri_Cvar_Get_NoComm("r_lockpvs", "0", CVAR_CHEAT, "");
	r_noportals = ri_Cvar_Get_NoComm("r_noportals", "0", CVAR_CHEAT, "");
	r_shadows = ri_Cvar_Get_NoComm("cg_shadows", "3", 0, "");

	r_marksOnTriangleMeshes = ri_Cvar_Get_NoComm("r_marksOnTriangleMeshes", "0", CVAR_ARCHIVE, "");

	r_aviMotionJpegQuality = ri_Cvar_Get_NoComm("r_aviMotionJpegQuality", "90", CVAR_ARCHIVE, "");
	r_screenshotJpegQuality = ri_Cvar_Get_NoComm("r_screenshotJpegQuality", "90", CVAR_ARCHIVE, "");
	r_surfaceSprites = ri_Cvar_Get_NoComm("r_surfaceSprites", "1", CVAR_ARCHIVE, "");
	r_AdvancedsurfaceSprites = ri_Cvar_Get_NoComm("r_advancedlod", "1", CVAR_ARCHIVE, "");

	r_aspectCorrectFonts = ri_Cvar_Get_NoComm("r_aspectCorrectFonts", "0", CVAR_ARCHIVE, "");
	r_maxpolys = ri_Cvar_Get_NoComm("r_maxpolys", XSTRING(DEFAULT_MAX_POLYS), 0, "");
	r_maxpolyverts = ri_Cvar_Get_NoComm("r_maxpolyverts", XSTRING(DEFAULT_MAX_POLYVERTS), 0, "");

	/*
	Ghoul2 Insert Start
	*/
#ifdef _DEBUG
	r_noPrecacheGLA = ri_Cvar_Get_NoComm("r_noPrecacheGLA", "0", CVAR_CHEAT, "");
#endif
	r_noServerGhoul2 = ri_Cvar_Get_NoComm("r_noghoul2", "0", CVAR_CHEAT, "");
	r_Ghoul2AnimSmooth = ri_Cvar_Get_NoComm("r_ghoul2animsmooth", "0.3", CVAR_TEMP, "");
	r_Ghoul2UnSqashAfterSmooth = ri_Cvar_Get_NoComm("r_ghoul2unsqashaftersmooth", "1", CVAR_TEMP, "");
	broadsword = ri_Cvar_Get_NoComm("broadsword", "0", CVAR_ARCHIVE, "");
	broadsword_kickbones = ri_Cvar_Get_NoComm("broadsword_kickbones", "1", CVAR_TEMP, "");
	broadsword_kickorigin = ri_Cvar_Get_NoComm("broadsword_kickorigin", "1", CVAR_TEMP, "");
	broadsword_dontstopanim = ri_Cvar_Get_NoComm("broadsword_dontstopanim", "0", CVAR_TEMP, "");
	broadsword_waitforshot = ri_Cvar_Get_NoComm("broadsword_waitforshot", "0", CVAR_TEMP, "");
	broadsword_playflop = ri_Cvar_Get_NoComm("broadsword_playflop", "1", CVAR_TEMP, "");
	broadsword_smallbbox = ri_Cvar_Get_NoComm("broadsword_smallbbox", "0", CVAR_TEMP, "");
	broadsword_extra1 = ri_Cvar_Get_NoComm("broadsword_extra1", "0", CVAR_TEMP, "");
	broadsword_extra2 = ri_Cvar_Get_NoComm("broadsword_extra2", "0", CVAR_TEMP, "");
	broadsword_effcorr = ri_Cvar_Get_NoComm("broadsword_effcorr", "1", CVAR_TEMP, "");
	broadsword_ragtobase = ri_Cvar_Get_NoComm("broadsword_ragtobase", "2", CVAR_TEMP, "");
	broadsword_dircap = ri_Cvar_Get_NoComm("broadsword_dircap", "64", CVAR_TEMP, "");

	r_com_rend2 = ri.Cvar_Get("com_rend2", "0", CVAR_ARCHIVE | CVAR_SAVEGAME);

	com_outcast = ri.Cvar_Get("com_outcast", "0", CVAR_ARCHIVE | CVAR_SAVEGAME);
	/*
	Ghoul2 Insert End
	*/

	r_patchStitching = ri_Cvar_Get_NoComm("r_patchStitching", "1", CVAR_ARCHIVE, "Enable stitching of neighbouring patch surfaces");

	se_language = ri_Cvar_Get_NoComm("se_language", "english", CVAR_ARCHIVE | CVAR_NORESTART, "");
#ifdef JK2_MODE
	sp_language = ri.Cvar_Get("sp_language", va("%d", SP_LANGUAGE_ENGLISH), CVAR_ARCHIVE | CVAR_NORESTART);
#endif
	// added for SP
	// @TODO add all cvars from vanilla

	g_Weather = ri.Cvar_Get("r_weather", "0", CVAR_ARCHIVE);
	com_buildScript = ri.Cvar_Get("com_buildScript", "0", 0);
	sv_mapname = ri.Cvar_Get("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);
	sv_mapChecksum = ri.Cvar_Get("sv_mapChecksum", "", CVAR_ROM);
	r_modelpoolmegs = ri.Cvar_Get("r_modelpoolmegs", "20", CVAR_ARCHIVE);
	if (ri.LowPhysicalMemory())
	{
		ri.Cvar_Set("r_modelpoolmegs", "0");
	}
	r_environmentMapping = ri.Cvar_Get("r_environmentMapping", "1", CVAR_ARCHIVE_ND);

	for (size_t i = 0; i < numCommands; i++)
		ri.Cmd_AddCommand(commands[i].cmd, commands[i].func);
}

static void R_InitQueries(void)
{
	if (r_drawSunRays->integer)
		qglGenQueries(ARRAY_LEN(tr.sunFlareQuery), tr.sunFlareQuery);
}

static void R_ShutDownQueries(void)
{
	if (r_drawSunRays->integer)
		qglDeleteQueries(ARRAY_LEN(tr.sunFlareQuery), tr.sunFlareQuery);
}

void RE_SetLightStyle(int style, int color);

#ifdef _G2_GORE
static void R_InitGoreVertexData(gpuFrame_t* currentFrame)
{
	static int numGoreArrays = 0;
	currentFrame->goreVBO = R_CreateVBO(
		nullptr,
		sizeof(g2GoreVert_t) * (MAX_GORE_RECORDS + 1) * MAX_GORE_VERTS,
		VBO_USAGE_DYNAMIC, va("Gore_%i", numGoreArrays));

	currentFrame->goreVBO->offsets[ATTR_INDEX_POSITION] = offsetof(g2GoreVert_t, position);
	currentFrame->goreVBO->offsets[ATTR_INDEX_NORMAL] = offsetof(g2GoreVert_t, normal);
	currentFrame->goreVBO->offsets[ATTR_INDEX_TEXCOORD0] = offsetof(g2GoreVert_t, texCoords);
	currentFrame->goreVBO->offsets[ATTR_INDEX_BONE_INDEXES] = offsetof(g2GoreVert_t, bonerefs);
	currentFrame->goreVBO->offsets[ATTR_INDEX_BONE_WEIGHTS] = offsetof(g2GoreVert_t, weights);
	currentFrame->goreVBO->offsets[ATTR_INDEX_TANGENT] = offsetof(g2GoreVert_t, tangents);

	currentFrame->goreVBO->strides[ATTR_INDEX_POSITION] = sizeof(g2GoreVert_t);
	currentFrame->goreVBO->strides[ATTR_INDEX_NORMAL] = sizeof(g2GoreVert_t);
	currentFrame->goreVBO->strides[ATTR_INDEX_TEXCOORD0] = sizeof(g2GoreVert_t);
	currentFrame->goreVBO->strides[ATTR_INDEX_BONE_INDEXES] = sizeof(g2GoreVert_t);
	currentFrame->goreVBO->strides[ATTR_INDEX_BONE_WEIGHTS] = sizeof(g2GoreVert_t);
	currentFrame->goreVBO->strides[ATTR_INDEX_TANGENT] = sizeof(g2GoreVert_t);

	currentFrame->goreVBO->sizes[ATTR_INDEX_POSITION] = sizeof(vec3_t);
	currentFrame->goreVBO->sizes[ATTR_INDEX_NORMAL] = sizeof(uint32_t);
	currentFrame->goreVBO->sizes[ATTR_INDEX_TEXCOORD0] = sizeof(vec2_t);
	currentFrame->goreVBO->sizes[ATTR_INDEX_BONE_WEIGHTS] = sizeof(byte);
	currentFrame->goreVBO->sizes[ATTR_INDEX_BONE_INDEXES] = sizeof(byte);
	currentFrame->goreVBO->sizes[ATTR_INDEX_TANGENT] = sizeof(uint32_t);

	currentFrame->goreIBO = R_CreateIBO(
		nullptr,
		sizeof(glIndex_t) * (MAX_GORE_RECORDS + 1) * MAX_GORE_INDECIES,
		VBO_USAGE_DYNAMIC, va("Gore_%i", numGoreArrays));

	GL_CheckErrors();
}
#endif

static void R_InitBackEndFrameData()
{
	GLuint timerQueries[MAX_GPU_TIMERS * MAX_FRAMES];
	qglGenQueries(MAX_GPU_TIMERS * MAX_FRAMES, timerQueries);

	// For temporal data we need ubo buffers between frames for 
	// reading last frame data without fear of writing next frames data into them
	bool reserveTemporalUbo = (r_smaa->integer == 2
		// || r_smaa->integer == 4
		// || r_taa->integer
		// || r_ssr->integer
		// || r_motionBlur->integer
		);

	if (reserveTemporalUbo)
		backEndData->numFrameUbos = (MAX_FRAMES + 1) * MAX_SCENES;
	else
		backEndData->numFrameUbos = MAX_FRAMES * MAX_SCENES;

	backEndData->frameUbos = (uint32_t*)Z_Malloc(backEndData->numFrameUbos * sizeof(*backEndData->frameUbos), TAG_GENERAL);
	backEndData->cachePreviousFrameUbos = reserveTemporalUbo;

	qglGenBuffers(backEndData->numFrameUbos, backEndData->frameUbos);

	for (int i = 0; i < MAX_FRAMES; i++)
	{
		gpuFrame_t* frame = backEndData->frames + i;
		const GLbitfield mapBits = GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT;

		for (byte j = 0; j < MAX_SCENES; j++)
		{
			size_t BUFFER_SIZE = j == 0 ? FRAME_UNIFORM_BUFFER_SIZE : FRAME_SCENE_UNIFORM_BUFFER_SIZE;
			frame->ubo[j] = backEndData->frameUbos[i * MAX_SCENES + j];
			frame->uboWriteOffset[j] = 0;
			frame->uboSize[j] = BUFFER_SIZE;
			qglBindBuffer(GL_UNIFORM_BUFFER, frame->ubo[j]);
			glState.currentGlobalUBO = frame->ubo[j];

			if (glRefConfig.annotateResources) qglObjectLabel(GL_BUFFER, frame->ubo[j], -1, va("FrameUBO_%i_%i", i, j));

			// TODO: persistently mapped UBOs
			qglBufferData(GL_UNIFORM_BUFFER, BUFFER_SIZE,
				nullptr, GL_DYNAMIC_DRAW);
		}

		// TODO: persistently mapped UBOs
		qglBufferData(GL_UNIFORM_BUFFER, FRAME_UNIFORM_BUFFER_SIZE,
			nullptr, GL_DYNAMIC_DRAW);

		frame->dynamicVbo = R_CreateVBO(nullptr, FRAME_VERTEX_BUFFER_SIZE,
			VBO_USAGE_DYNAMIC, va("Frame_%i", i));
		frame->dynamicVboCommitOffset = 0;
		frame->dynamicVboWriteOffset = 0;

		frame->dynamicIbo = R_CreateIBO(nullptr, FRAME_INDEX_BUFFER_SIZE,
			VBO_USAGE_DYNAMIC, va("Frame_%i", i));
		frame->dynamicIboCommitOffset = 0;
		frame->dynamicIboWriteOffset = 0;

		if (glRefConfig.immutableBuffers)
		{
			R_BindVBO(frame->dynamicVbo);
			frame->dynamicVboMemory = qglMapBufferRange(GL_ARRAY_BUFFER, 0,
				frame->dynamicVbo->vertexesSize, mapBits);

			R_BindIBO(frame->dynamicIbo);
			frame->dynamicIboMemory = qglMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0,
				frame->dynamicIbo->indexesSize, mapBits);
		}
		else
		{
			frame->dynamicVboMemory = nullptr;
			frame->dynamicIboMemory = nullptr;
		}

		for (int j = 0; j < MAX_GPU_TIMERS; j++)
		{
			gpuTimer_t* timer = frame->timers + j;
			timer->queryName = timerQueries[i * MAX_GPU_TIMERS + j];
		}

#ifdef _G2_GORE
		R_InitGoreVertexData(frame);
#endif
	}

	if (reserveTemporalUbo)
	{
		// Allocate the spare ubo for last frame infos
		gpuFrame_t* frame = &backEndData->frames[0];
		for (byte j = 0; j < MAX_SCENES; j++)
		{
			size_t BUFFER_SIZE = j == 0 ? FRAME_UNIFORM_BUFFER_SIZE : FRAME_SCENE_UNIFORM_BUFFER_SIZE;
			frame->ubo[j] = backEndData->frameUbos[MAX_FRAMES * MAX_SCENES + j];
			frame->uboWriteOffset[j] = 0;
			frame->uboSize[j] = BUFFER_SIZE;
			qglBindBuffer(GL_UNIFORM_BUFFER, frame->ubo[j]);
			glState.currentGlobalUBO = frame->ubo[j];

			if (glRefConfig.annotateResources) qglObjectLabel(GL_BUFFER, frame->ubo[j], -1, va("FrameUBO_spare_%i", j));

			// TODO: persistently mapped UBOs
			qglBufferData(GL_UNIFORM_BUFFER, BUFFER_SIZE,
				nullptr, GL_DYNAMIC_DRAW);
		}
	}

	backEndData->currentFrame = backEndData->frames;
}

static void R_InitStaticConstants()
{
	const int alignment = glRefConfig.uniformBufferOffsetAlignment - 1;
	size_t alignedBlockSize = 0;

	qglBindBuffer(GL_UNIFORM_BUFFER, tr.staticUbo);
	if (glRefConfig.annotateResources) qglObjectLabel(GL_BUFFER, tr.staticUbo, -1, "StaticUBO");
	qglBufferData(
		GL_UNIFORM_BUFFER,
		STATIC_UNIFORM_BUFFER_SIZE,
		nullptr,
		GL_STATIC_DRAW);

	// Setup static 2d camera data
	EntityBlock entity2DBlock = {};
	entity2DBlock.fxVolumetricBase = -1.0f;
	Matrix16Identity(entity2DBlock.modelMatrix);
	tr.entity2DUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, 0, sizeof(entity2DBlock), &entity2DBlock);
	alignedBlockSize += (sizeof(EntityBlock) + alignment) & ~alignment;

	// Setup static 2d camera data
	CameraBlock a2DCameraBlock = {};
	Matrix16Ortho(
		0.0f,
		640.0f,
		480.0f,
		0.0f,
		0.0f,
		1.0f,
		a2DCameraBlock.viewProjectionMatrix);

	tr.camera2DUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, tr.camera2DUboOffset, sizeof(a2DCameraBlock), &a2DCameraBlock);
	alignedBlockSize += (sizeof(CameraBlock) + alignment) & ~alignment;

	// Setup static flare entity data
	EntityBlock entityFlareBlock = {};
	entityFlareBlock.fxVolumetricBase = -1.0f;
	Matrix16Identity(entityFlareBlock.modelMatrix);

	tr.entityFlareUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, tr.entityFlareUboOffset, sizeof(entityFlareBlock), &entityFlareBlock);
	alignedBlockSize += (sizeof(EntityBlock) + alignment) & ~alignment;

	// Setup static flare camera data
	CameraBlock flareCameraBlock = {};
	Matrix16Ortho(
		0.0f,
		glConfig.vidWidth,
		glConfig.vidHeight,
		0.0f,
		-99999.0f,
		99999.0f,
		flareCameraBlock.viewProjectionMatrix);

	tr.cameraFlareUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, tr.cameraFlareUboOffset, sizeof(flareCameraBlock), &flareCameraBlock);
	alignedBlockSize += (sizeof(CameraBlock) + alignment) & ~alignment;

	// Setup default light block
	LightsBlock lightsBlock = {};
	lightsBlock.numLights = 0;

	tr.defaultLightsUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, tr.defaultLightsUboOffset, sizeof(lightsBlock), &lightsBlock);
	alignedBlockSize += (sizeof(LightsBlock) + alignment) & ~alignment;

	// Setup default scene block
	SceneBlock sceneBlock = {};
	sceneBlock.globalFogIndex = -1;
	sceneBlock.current_time = 0.1f;
	sceneBlock.frameTime = 0.1f;

	tr.defaultSceneUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, tr.defaultSceneUboOffset, sizeof(sceneBlock), &sceneBlock);
	alignedBlockSize += (sizeof(SceneBlock) + alignment) & ~alignment;

	// Setup default fogs block
	FogsBlock fogsBlock = {};
	fogsBlock.numFogs = 0;
	tr.defaultFogsUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, tr.defaultFogsUboOffset, sizeof(fogsBlock), &fogsBlock);
	alignedBlockSize += (sizeof(FogsBlock) + alignment) & ~alignment;

	// Setup default shader instance block
	ShaderInstanceBlock shaderInstanceBlock = {};
	tr.defaultShaderInstanceUboOffset = alignedBlockSize;
	qglBufferSubData(
		GL_UNIFORM_BUFFER, tr.defaultShaderInstanceUboOffset, sizeof(shaderInstanceBlock), &shaderInstanceBlock);
	alignedBlockSize += (sizeof(ShaderInstanceBlock) + alignment) & ~alignment;

	qglBindBuffer(GL_UNIFORM_BUFFER, NULL);
	glState.currentGlobalUBO = -1;

	GL_CheckErrors();
}

static void R_ShutdownBackEndFrameData()
{
	if (!backEndData)
		return;

	qglDeleteBuffers(backEndData->numFrameUbos, backEndData->frameUbos);
	Z_Free(backEndData->frameUbos);

	for (int i = 0; i < MAX_FRAMES; i++)
	{
		gpuFrame_t* frame = backEndData->frames + i;

		if (frame->sync)
		{
			qglDeleteSync(frame->sync);
			frame->sync = NULL;
		}

		if (glRefConfig.immutableBuffers)
		{
			R_BindVBO(frame->dynamicVbo);
			R_BindIBO(frame->dynamicIbo);
			qglUnmapBuffer(GL_ARRAY_BUFFER);
			qglUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		}

		for (int j = 0; j < MAX_GPU_TIMERS; j++)
		{
			gpuTimer_t* timer = frame->timers + j;
			qglDeleteQueries(1, &timer->queryName);
		}
	}
}

// need to do this hackery so ghoul2 doesn't crash the game because of ITS hackery...
//
static void R_ClearStuffToStopGhoul2CrashingThings(void)
{
	memset(&tr, 0, sizeof(tr));
}

static bool r_inited = false;

/*
===============
R_Init
===============
*/
void R_Init()
{
	byte* ptr;
	int i;

	if (r_inited)
		return;

	ri.Printf(PRINT_ALL, "----- Loading Rend2 renderer -----\n");

	// clear all our internal state
	Com_Memset(&tr, 0, sizeof(tr));
	Com_Memset(&backEnd, 0, sizeof(backEnd));
	Com_Memset(&tess, 0, sizeof(tess));

	//
	// init function tables
	//
	for (i = 0; i < FUNCTABLE_SIZE; i++)
	{
		tr.sinTable[i] = sin(DEG2RAD(i * 360.0f / ((float)(FUNCTABLE_SIZE - 1))));
		tr.squareTable[i] = (i < FUNCTABLE_SIZE / 2) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if (i < FUNCTABLE_SIZE / 2)
		{
			if (i < FUNCTABLE_SIZE / 4)
			{
				tr.triangleTable[i] = (float)i / (FUNCTABLE_SIZE / 4);
			}
			else
			{
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
			}
		}
		else
		{
			tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
		}
	}

	R_InitFogTable();

	R_ImageLoader_Init();
	R_NoiseInit();
	R_Register();

	max_polys = Q_min(r_maxpolys->integer, DEFAULT_MAX_POLYS);
	max_polyverts = Q_min(r_maxpolyverts->integer, DEFAULT_MAX_POLYVERTS);

	ptr = (byte*)Hunk_Alloc(
		sizeof(*backEndData) +
		sizeof(srfPoly_t) * max_polys +
		sizeof(polyVert_t) * max_polyverts +
		sizeof(Allocator) +
		PER_FRAME_MEMORY_BYTES,
		h_low);

	backEndData = (backEndData_t*)ptr;
	ptr = (byte*)(backEndData + 1);

	backEndData->polys = (srfPoly_t*)ptr;
	ptr += sizeof(*backEndData->polys) * max_polys;

	backEndData->polyVerts = (polyVert_t*)ptr;
	ptr += sizeof(*backEndData->polyVerts) * max_polyverts;

	backEndData->perFrameMemory = new(ptr) Allocator(ptr + sizeof(*backEndData->perFrameMemory), PER_FRAME_MEMORY_BYTES);

	R_InitNextFrame();

	for (int i = 0; i < MAX_LIGHT_STYLES; i++)
	{
		RE_SetLightStyle(i, -1);
	}

	R_InitImagesPool();

	InitOpenGL();

	R_InitGPUBuffers();

	R_InitStaticConstants();
	R_InitBackEndFrameData();
	R_InitImages();

	FBO_Init();

	GLSL_LoadGPUShaders();

	R_InitShaders(qfalse);

	R_InitSkins();

	R_InitFonts();

	R_ModelInit();

	R_InitDecals();

	R_InitQueries();

	R_InitWeatherSystem();

#if defined(_DEBUG)
	GLenum err = qglGetError();
	if (err != GL_NO_ERROR)
		ri.Printf(PRINT_ALL, "glGetError() = 0x%x\n", err);
#endif

	RestoreGhoul2InfoArray();

	// print info
	GfxInfo_f();
	r_inited = true;

	if (r_com_rend2->integer != 1)
	{
		ri.Cvar_Set("com_rend2", "1");
	}

	ri.Printf(PRINT_ALL, "----- Rend2 renderer loaded -----\n");
}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown(qboolean destroyWindow, qboolean restarting)
{
	ri.Printf(PRINT_ALL, "RE_Shutdown( %i )\n", destroyWindow);

	for (size_t i = 0; i < numCommands; i++)
		ri.Cmd_RemoveCommand(commands[i].cmd);

	// Flush here to make sure all the fences are processed
	qglFlush();

	R_IssuePendingRenderCommands();

	R_ShutdownBackEndFrameData();

	R_ShutdownWeatherSystem();

	R_ShutdownFonts();

	if (r_inited)
	{
		R_ShutDownQueries();
		FBO_Shutdown();
		R_DeleteTextures();
		R_DestroyGPUBuffers();
		GLSL_ShutdownGPUShaders();
	}

	if (destroyWindow && restarting && tr.registered)
	{
		ri.Z_Free((void*)glConfig.extensions_string);
		ri.Z_Free((void*)glConfigExt.originalExtensionString);

		qglDeleteVertexArrays(1, &tr.globalVao);
		SaveGhoul2InfoArray();
	}

	// shut down platform specific OpenGL stuff
	if (destroyWindow) {
		ri.WIN_Shutdown();
	}

	tr.registered = qfalse;
	r_inited = false;
	backEndData = NULL;
}

/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
static void RE_EndRegistration(void)
{
	R_IssuePendingRenderCommands();
}

// HACK
extern qboolean gG2_GBMNoReconstruct;
extern qboolean gG2_GBMUseSPMethod;
static void G2API_BoltMatrixReconstruction(qboolean reconstruct) { gG2_GBMNoReconstruct = (qboolean)!reconstruct; }
static void G2API_BoltMatrixSPMethod(qboolean spMethod) { gG2_GBMUseSPMethod = spMethod; }

static float GetDistanceCull(void) { return tr.distanceCull; }

extern void R_SVModelInit(void); //tr_model.cpp

static void GetRealRes(int* w, int* h) {
	*w = glConfig.vidWidth;
	*h = glConfig.vidHeight;
}

// STUBS, REPLACEME
static qboolean stub_InitializeWireframeAutomap() { return qtrue; }

static void RE_GetLightStyle(int style, color4ub_t color)
{
	if (style >= MAX_LIGHT_STYLES)
	{
		Com_Error(ERR_FATAL, "RE_GetLightStyle: %d is out of range", style);
		return;
	}

	byteAlias_t* baDest = (byteAlias_t*)&color, * baSource = (byteAlias_t*)&styleColors[style];
	baDest->i = baSource->i;
}

void RE_SetLightStyle(int style, int color)
{
	if (style >= MAX_LIGHT_STYLES)
	{
		Com_Error(ERR_FATAL, "RE_SetLightStyle: %d is out of range", style);
		return;
	}

	byteAlias_t* ba = (byteAlias_t*)&styleColors[style];
	if (ba->i != color) {
		ba->i = color;
	}
}

void RE_GetBModelVerts(int bmodelIndex, vec3_t* verts, vec3_t normal);

static void stub_RE_AddWeatherZone(vec3_t mins, vec3_t maxs) {} // Intentionally left blank. Rend2 reads the zones manually on bsp load
static void RE_SetRefractionProperties(float distortionAlpha, float distortionStretch, qboolean distortionPrePost, qboolean distortionNegate) { }

static void C_LevelLoadBegin(const char* psMapName, ForceReload_e eForceReload, qboolean bAllowScreenDissolve)
{
	static char sPrevMapName[MAX_QPATH] = { 0 };
	bool bDeleteModels = eForceReload == eForceReload_MODELS || eForceReload == eForceReload_ALL;

	if (bDeleteModels)
		CModelCache->DeleteAll();
	else if (ri.Cvar_VariableIntegerValue("sv_pure"))
		CModelCache->DumpNonPure();

	tr.numBSPModels = 0;

	/* If we're switching to the same level, don't increment current level */
	if (Q_stricmp(psMapName, sPrevMapName))
	{
		Q_strncpyz(sPrevMapName, psMapName, sizeof(sPrevMapName));
		tr.currentLevel++;
	}
}

static int C_GetLevel(void)
{
	return tr.currentLevel;
}

static void C_LevelLoadEnd(void)
{
#ifndef REND2_SP
	CModelCache->LevelLoadEnd(qfalse);
	ri.SND_RegisterAudio_LevelLoadEnd(qfalse);
	ri.S_RestartMusic();
#else
	CModelCache->LevelLoadEnd(qfalse);

	ri.SND_RegisterAudio_LevelLoadEnd(qfalse);

	if (gbAllowScreenDissolve)
	{
		RE_InitDissolve(qfalse);
	}

	ri.S_RestartMusic();

	*(ri.gbAlreadyDoingLoad()) = qfalse;
#endif // REND2_SP
}

extern void RE_GetModelBounds(const refEntity_t* refEnt, vec3_t bounds1, vec3_t bounds2);
extern void G2API_AnimateG2ModelsRag(CGhoul2Info_v& ghoul2, const int acurrent_time, CRagDollUpdateParams* params);
extern qboolean G2API_GetRagBonePos(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos, vec3_t entAngles, vec3_t ent_pos, vec3_t entScale);
extern qboolean G2API_RagEffectorKick(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t velocity);
extern qboolean G2API_RagForceSolve(CGhoul2Info_v& ghoul2, const qboolean force);
extern qboolean G2API_SetBoneIKState(CGhoul2Info_v& ghoul2, int time, const char* boneName, int ikState, sharedSetBoneIKStateParams_t* params);
extern qboolean G2API_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params);
extern qboolean G2API_RagEffectorGoal(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos);
extern qboolean G2API_RagPCJGradientSpeed(CGhoul2Info_v& ghoul2, const char* boneName, const float speed);
extern qboolean G2API_RagPCJConstraint(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t min, vec3_t max);
extern void G2API_SetRagDoll(CGhoul2Info_v& ghoul2, CRagDollParams* parms);
#ifdef G2_PERFORMANCE_ANALYSIS
extern void G2Time_ResetTimers(void);
extern void G2Time_ReportTimers(void);
#endif

#ifdef JK2_MODE
static unsigned int AnyLanguage_ReadCharFromString_JK2(char** text, qboolean* pbIsTrailingPunctuation)
{
	return AnyLanguage_ReadCharFromString(text, pbIsTrailingPunctuation);
}
#endif

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
extern "C" Q_EXPORT refexport_t * QDECL GetRefAPI(const int api_version, const refimport_t * refimp)
{
	static refexport_t	re;

	ri = *refimp;

	memset(&re, 0, sizeof re);

	if (api_version != REF_API_VERSION)
	{
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, api_version);
		return nullptr;
	}

	// the RE_ functions are Renderer Entry points

	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.GetAnimationCFG = RE_GetAnimationCFG;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorldMap;
	re.R_LoadImage = R_LoadImage;

	re.RegisterMedia_LevelLoadBegin = C_LevelLoadBegin;
	re.RegisterMedia_LevelLoadEnd = C_LevelLoadEnd;
	re.RegisterMedia_GetLevel = C_GetLevel;
	re.RegisterImages_LevelLoadEnd = C_Images_LevelLoadEnd;
	re.RegisterModels_LevelLoadEnd = C_Models_LevelLoadEnd;

	re.SetWorldVisData = RE_SetWorldVisData;

	re.EndRegistration = RE_EndRegistration;

	re.ClearScene = RE_ClearScene;
	//re.ClearDecals = RE_ClearDecals;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	//re.AddMiniRefEntityToScene = RE_AddMiniRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.AddLightToScene = RE_AddLightToScene;
	//re.AddDecalToScene = RE_AddDecalToScene;
	//re.LightForPoint = R_LightForPoint;
	re.RenderScene = RE_RenderScene;
	//re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.GetLighting = RE_GetLighting;

	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawRotatePic = RE_RotatePic;
	re.DrawRotatePic2 = RE_RotatePic2;
	re.LAGoggles = RE_LAGoggles;
	re.Scissor = RE_Scissor;

	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	re.ProcessDissolve = RE_ProcessDissolve;
	re.InitDissolve = RE_InitDissolve;
	re.GetScreenShot = RE_GetScreenShot;

#ifdef JK2_MODE
	re.SaveJPGToBuffer = RE_SaveJPGToBuffer;
	re.LoadJPGFromBuffer = LoadJPGFromBuffer;
#endif
	re.TempRawImage_ReadFromFile = RE_TempRawImage_ReadFromFile;
	re.TempRawImage_CleanUp = RE_TempRawImage_CleanUp;

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;
	re.GetLightStyle = RE_GetLightStyle;
	re.SetLightStyle = RE_SetLightStyle;
	re.GetBModelVerts = RE_GetBModelVerts;
	re.WorldEffectCommand = RE_WorldEffectCommand;
	re.GetModelBounds = RE_GetModelBounds;

	re.SVModelInit = R_SVModelInit;

	re.RegisterFont = RE_RegisterFont;
	re.Font_HeightPixels = RE_Font_HeightPixels;
	re.Font_StrLenPixels = RE_Font_StrLenPixels;
	re.Font_DrawString = RE_Font_DrawString;
	re.Font_StrLenChars = RE_Font_StrLenChars;
	re.Language_IsAsian = Language_IsAsian;
	re.Language_UsesSpaces = Language_UsesSpaces;
	re.AnyLanguage_ReadCharFromString = AnyLanguage_ReadCharFromString;
#ifdef JK2_MODE
	re.AnyLanguage_ReadCharFromString2 = AnyLanguage_ReadCharFromString_JK2;
#endif

	re.R_InitWorldEffects = stub_R_InitWorldEffects;
	re.R_ClearStuffToStopGhoul2CrashingThings = R_ClearStuffToStopGhoul2CrashingThings;
	re.R_inPVS = R_inPVS;

	re.tr_distortionAlpha = stub_get_tr_distortionAlpha;
	re.tr_distortionStretch = stub_get_tr_distortionStretch;
	re.tr_distortionPrePost = stub_get_tr_distortionPrePost;
	re.tr_distortionNegate = stub_get_tr_distortionNegate;

	//re.InitializeWireframeAutomap = stub_InitializeWireframeAutomap; // MP
	re.GetWindVector = R_GetWindVector;
	re.GetWindGusting = R_GetWindGusting;
	re.IsOutside = R_IsOutside;
	re.IsOutsideCausingPain = R_IsOutsideCausingPain;
	re.GetChanceOfSaberFizz = R_GetChanceOfSaberFizz;
	re.IsShaking = R_IsShaking;
	re.AddWeatherZone = stub_RE_AddWeatherZone;
	re.SetTempGlobalFogColor = stub_R_SetTempGlobalFogColor;

	re.SetRangedFog = RE_SetRangedFog;
	//re.GetDistanceCull = GetDistanceCull; // MP
	//re.GetRealRes = GetRealRes; // MP

	re.TheGhoul2InfoArray = TheGhoul2InfoArray;
	//re.GetEntityToken = R_GetEntityToken;  //MP only, but need this for cubemaps...

	re.G2API_AddBolt = G2API_AddBolt;
	re.G2API_AddBoltSurfNum = G2API_AddBoltSurfNum;
	re.G2API_AddSurface = G2API_AddSurface;
	re.G2API_AnimateG2Models = G2API_AnimateG2ModelsRag;
	re.G2API_AttachEnt = G2API_AttachEnt;
	re.G2API_AttachG2Model = G2API_AttachG2Model;
	re.G2API_CollisionDetect = G2API_CollisionDetect;
	re.G2API_CleanGhoul2Models = G2API_CleanGhoul2Models;
	re.G2API_CopyGhoul2Instance = G2API_CopyGhoul2Instance;
	re.G2API_DetachEnt = G2API_DetachEnt;
	re.G2API_DetachG2Model = G2API_DetachG2Model;
	re.G2API_GetAnimFileName = G2API_GetAnimFileName;
	re.G2API_GetAnimFileNameIndex = G2API_GetAnimFileNameIndex;
	re.G2API_GetAnimFileInternalNameIndex = G2API_GetAnimFileInternalNameIndex;
	re.G2API_GetAnimIndex = G2API_GetAnimIndex;
	re.G2API_GetAnimRange = G2API_GetAnimRange;
	re.G2API_GetAnimRangeIndex = G2API_GetAnimRangeIndex;
	re.G2API_GetBoneAnim = G2API_GetBoneAnim;
	re.G2API_GetBoneAnimIndex = G2API_GetBoneAnimIndex;
	re.G2API_GetBoneIndex = G2API_GetBoneIndex;
	re.G2API_GetBoltMatrix = G2API_GetBoltMatrix;
	re.G2API_GetGhoul2ModelFlags = G2API_GetGhoul2ModelFlags;
	re.G2API_GetGLAName = G2API_GetGLAName;
	re.G2API_GetParentSurface = G2API_GetParentSurface;
	re.G2API_GetRagBonePos = G2API_GetRagBonePos;
	re.G2API_GetSurfaceIndex = G2API_GetSurfaceIndex;
	re.G2API_GetSurfaceName = G2API_GetSurfaceName;
	re.G2API_GetSurfaceRenderStatus = G2API_GetSurfaceRenderStatus;
	re.G2API_GetTime = G2API_GetTime;
	re.G2API_GiveMeVectorFromMatrix = G2API_GiveMeVectorFromMatrix;
	re.G2API_HaveWeGhoul2Models = G2API_HaveWeGhoul2Models;
	re.G2API_IKMove = G2API_IKMove;
	re.G2API_InitGhoul2Model = G2API_InitGhoul2Model;
	re.G2API_IsPaused = G2API_IsPaused;
	re.G2API_ListBones = G2API_ListBones;
	re.G2API_ListSurfaces = G2API_ListSurfaces;
	re.G2API_LoadGhoul2Models = G2API_LoadGhoul2Models;
	re.G2API_LoadSaveCodeDestructGhoul2Info = G2API_LoadSaveCodeDestructGhoul2Info;
	re.G2API_PauseBoneAnim = G2API_PauseBoneAnim;
	re.G2API_PauseBoneAnimIndex = G2API_PauseBoneAnimIndex;
	re.G2API_PrecacheGhoul2Model = G2API_PrecacheGhoul2Model;
	re.G2API_RagEffectorGoal = G2API_RagEffectorGoal;
	re.G2API_RagEffectorKick = G2API_RagEffectorKick;
	re.G2API_RagForceSolve = G2API_RagForceSolve;
	re.G2API_RagPCJConstraint = G2API_RagPCJConstraint;
	re.G2API_RagPCJGradientSpeed = G2API_RagPCJGradientSpeed;
	re.G2API_RemoveBolt = G2API_RemoveBolt;
	re.G2API_RemoveBone = G2API_RemoveBone;
	re.G2API_RemoveGhoul2Model = G2API_RemoveGhoul2Model;
	re.G2API_RemoveSurface = G2API_RemoveSurface;
	re.G2API_SaveGhoul2Models = G2API_SaveGhoul2Models;
	re.G2API_SetAnimIndex = G2API_SetAnimIndex;
	re.G2API_SetBoneAnim = G2API_SetBoneAnim;
	re.G2API_SetBoneAnimIndex = G2API_SetBoneAnimIndex;
	re.G2API_SetBoneAngles = G2API_SetBoneAngles;
	re.G2API_SetBoneAnglesIndex = G2API_SetBoneAnglesIndex;
	re.G2API_SetBoneAnglesMatrix = G2API_SetBoneAnglesMatrix;
	re.G2API_SetBoneAnglesMatrixIndex = G2API_SetBoneAnglesMatrixIndex;
	re.G2API_SetBoneIKState = G2API_SetBoneIKState;
	re.G2API_SetGhoul2ModelFlags = G2API_SetGhoul2ModelFlags;
	re.G2API_SetGhoul2ModelIndexes = G2API_SetGhoul2ModelIndexes;
	re.G2API_SetLodBias = G2API_SetLodBias;
	//G2EX(SetModelIndexes);
	re.G2API_SetNewOrigin = G2API_SetNewOrigin;
	re.G2API_SetRagDoll = G2API_SetRagDoll;
	re.G2API_SetRootSurface = G2API_SetRootSurface;
	re.G2API_SetShader = G2API_SetShader;
	re.G2API_SetSkin = G2API_SetSkin;
	re.G2API_SetSurfaceOnOff = G2API_SetSurfaceOnOff;
	re.G2API_SetTime = G2API_SetTime;
	re.G2API_StopBoneAnim = G2API_StopBoneAnim;
	re.G2API_StopBoneAnimIndex = G2API_StopBoneAnimIndex;
	re.G2API_StopBoneAngles = G2API_StopBoneAngles;
	re.G2API_StopBoneAnglesIndex = G2API_StopBoneAnglesIndex;
#ifdef _G2_GORE
	re.G2API_AddSkinGore = G2API_AddSkinGore;
	re.G2API_ClearSkinGore = G2API_ClearSkinGore;
	//re.G2API_GetNumGoreMarks = G2API_GetNumGoreMarks; // MP
#endif

#ifdef G2_PERFORMANCE_ANALYSIS
	re.G2Time_ReportTimers = G2Time_ReportTimers;
	re.G2Time_ResetTimers = G2Time_ResetTimers;
#endif

	//Swap_Init();

	return &re;
}