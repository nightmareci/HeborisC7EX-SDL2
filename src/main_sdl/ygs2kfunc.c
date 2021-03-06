#include "main_sdl/include.h"
#include "SDL_kanji.h"
#include "ygs2kfunc.h"
#include "ygs2kprivate.h"
#include "delay.h"

#define		GAME_CAPTION		"HEBORIS C7-EX SDL2"

typedef struct
{
	bool	enable;
	int	x;
	int	y;
	int	r, g, b;
	int	size;
	char	string[256];
} STextLayer;

enum
{
	YGS_SOUNDTYPE_NONE,
	YGS_SOUNDTYPE_WAV,
	YGS_SOUNDTYPE_MUS,
};

static SDL_Window		*s_pScreenWindow = NULL;
static SDL_Renderer		*s_pScreenRenderer = NULL;
static SDL_Texture		*s_pScreenRenderTarget = NULL;
static float			s_fScreenSubpixelOffset = 0.0f;

static SDL_Texture		*s_pYGSTexture[YGS_TEXTURE_MAX];

static int			s_iYGSSoundType[YGS_SOUND_MAX];
static Mix_Chunk		*s_pYGSSound[YGS_SOUND_MAX];
static int			s_iYGSSoundVolume[YGS_SOUND_MAX];
static Mix_Music		*s_pYGSExMusic[YGS_SOUND_MAX];
static Mix_Music		*s_pYGSMusic;
static int			s_iYGSMusicVolume;

#define		YGS_KANJIFONTFILE_MAX	3
static Kanji_Font		*s_pKanjiFont[YGS_KANJIFONTFILE_MAX];

static int			s_iLogicalWidth;
static int			s_iLogicalHeight;

static int			s_bBltAlways;
static Uint64			s_uTimeCount;
static Uint64			s_uTimeAccumulatorCount;
static Uint64			s_uFPSCount;
static unsigned int		s_uNowFrame;
static unsigned int		s_uFPSCnting;
static unsigned int		s_uFPS;
static unsigned int		s_uNowFPS = 60;

static STextLayer		s_TextLayer[YGS_TEXTLAYER_MAX];
static int32_t			s_iScreenMode;
static int32_t			s_iScreenIndex;

static int			s_iNewOffsetX = 0, s_iNewOffsetY = 0;
static int			s_iOffsetX = 0, s_iOffsetY = 0;

static float GetScreenSubpixelOffset()
{
	// The returned subpixel offset nudges all draws to have pixel coordinates
	// that end up centered in the floating point coordinate space. Without
	// this offset, pixel coordinates are at the upper left of the intended
	// pixels, resulting in off-by-one drawing errors sometimes.
	//
	// The numerator of the return value is the fraction of a pixel to adjust
	// draw position rightwards and downwards, and the division by the current
	// scale converts that subpixel amount to the amount to adjust by within
	// the currently set logical resolution (320x240 or 640x480), so it adjusts
	// by less than 0.375 if the render resolution is above logical resolution,
	// or greater than 0.375 if below logical resolution. If the numerator is
	// exactly 0.5f, then system-dependent rounding errors can occur, because
	// the coordinate is located exactly in the pixel center, producing
	// sampling artifacts on some systems, but not on others. By using a
	// numerator less than 0.5f, the rounding of draw coordinates by the
	// graphics implementation should place drawn pixels in the center. I also
	// saw scaling artifacts happen with a numerator of 0.25f, so maybe the
	// numerator needs to be somewhere in the open range (0.25f, 0.5f) to
	// likely work everywhere; for now, the midpoint, 0.375f, is used, and
	// seems to work correctly on some systems I've tested.
	//
	// Getting the scale from SDL_RenderGetScale is always appropriate here,
	// being an integer value when integer scaling is in effect, or a
	// non-integer scale when fill-screen scaling is in effect. Even if the
	// scale value is below 1.0f, the formula is still correct.
	//
	// -Brandon McGriff <nightmareci@gmail.com>
	if ( s_pScreenRenderer )
	{
		float scale;
		SDL_RenderGetScale(s_pScreenRenderer, &scale, NULL);
		return 0.375f / scale;
	}
	else
	{
		return 0.0f;
	}
}

bool YGS2kInit()
{
	int		configChanged = 0;

	s_iNewOffsetX = 0;	s_iNewOffsetY = 0;
	s_iOffsetX = 0;		s_iOffsetY = 0;

	InputOpen();

	/* ????????????????????????????????? */
	memset(s_pYGSTexture, 0, sizeof(s_pYGSTexture));

	/* ???????????????????????? */
	for ( int i = 0 ; i < YGS_SOUND_MAX ; i ++ )
	{
		s_iYGSSoundType[i] = YGS_SOUNDTYPE_NONE;
		s_iYGSSoundVolume[i] = 0;
		s_pYGSSound[i] = NULL;
		s_pYGSExMusic[i] = NULL;
	}

	s_pYGSMusic = NULL;
	s_iYGSMusicVolume = 0;

	/* ???????????????????????????????????? */
	for ( int i = 0 ; i < YGS_TEXTLAYER_MAX ; i ++ )
	{
		memset(&s_TextLayer[i], 0, sizeof(STextLayer));
		s_TextLayer[i].r = s_TextLayer[i].g = s_TextLayer[i].b = 255;
		s_TextLayer[i].size = 16;
	}
	YGS2kKanjiFontInitialize();

	s_uTimeCount		= SDL_GetPerformanceCounter();
	s_uTimeAccumulatorCount	= 0;
	s_uFPSCount		= SDL_GetPerformanceCounter();
	s_uNowFrame		= 0;
	s_uFPSCnting		= 0;
	s_uFPS			= 0;
	s_bBltAlways		= false;

	srand((unsigned)time(NULL));

	return true;
}

void YGS2kExit()
{
	InputClose();

	/* ?????????????????????????????? */
	for ( int i = 0 ; i < YGS_TEXTURE_MAX ; i ++ )
	{
		if ( s_pYGSTexture[i] )
		{
			SDL_DestroyTexture(s_pYGSTexture[i]);
			s_pYGSTexture[i] = NULL;
		}
	}
	if ( s_pScreenRenderer ) {
		SDL_SetRenderTarget( s_pScreenRenderer, NULL );
	}
	if ( s_pScreenRenderTarget ) {
		SDL_DestroyTexture( s_pScreenRenderTarget );
		s_pScreenRenderTarget = NULL;
	}
	if ( s_pScreenRenderer ) {
		SDL_DestroyRenderer( s_pScreenRenderer );
		s_pScreenRenderer = NULL;
	}
	if ( s_pScreenWindow ) {
		SDL_DestroyWindow( s_pScreenWindow );
		s_pScreenWindow = NULL;
	}

	/* ????????????????????? */
	for ( int i = 0 ; i < YGS_SOUND_MAX ; i ++ )
	{
		s_iYGSSoundType[i] = YGS_SOUNDTYPE_NONE;

		if ( s_pYGSSound[i] )
		{
			Mix_FreeChunk(s_pYGSSound[i]);
			s_pYGSSound[i] = NULL;
		}
		if ( s_pYGSExMusic[i] )
		{
			Mix_FreeMusic(s_pYGSExMusic[i]);
			s_pYGSExMusic[i] = NULL;
		}
	}

	YGS2kKanjiFontFinalize();

	if ( s_pYGSMusic )
	{
		Mix_FreeMusic(s_pYGSMusic);
		s_pYGSMusic = NULL;
	}
}

static inline void FrameDelay() {
	/*
	 * Here, we insert a delay to produce accurate frame timing, using "hybrid
	 * wait" and "fixed timestep". The first stage is to loop over delays of 1
	 * millisecond, where it's assumed that delays are roughly 1 millisecond,
	 * generally a bit above. But, during that first stage of delays, the max
	 * delay duration is kept track of, and that stage of the delay loop is
	 * broken out of when the time remaining to delay is less than or equal to
	 * the max of the 1-millisecond delays. Then, the same delay loop is done
	 * with 0-millisecond delay requests; requesting a delay of no time just
	 * yields the game's running process, allowing the system to run other
	 * tasks, without eating a bunch of CPU time. Once that loop stage is done,
	 * a final, as-small-as-possible pure busyloop is run to finish out the
	 * delay; it's critical to delay as much as possible via Delay first, so as
	 * to minimize wasted CPU time. This scheme appears to be "optimal", in
	 * that it produces correct timing, but with a minimum of wasted CPU time.
	 * Mufunyo ( https://github.com/mufunyo ) showed me the "hybrid wait"
	 * algorithm, where most of the delay is done via actual delays, and
	 * completed with a busyloop, but that algorithm has been adapted to fit
	 * into this game's "fixed timestep" variant ( https://www.gafferongames.com/post/fix_your_timestep/ ).
	 * -Brandon McGriff <nightmareci@gmail.com>
	 */

	/*
	 * Changing the ">= 0" to ">= 1" removes the Delay(0) delay, and increases
	 * CPU usage on my Linux system from 2.8-3.0% in-game up to 3.5-3.8%. And
	 * having it ">= 0" doesn't worsen frame timing, it's basically a free
	 * optimization. The plain busyloop is still required to get very precise
	 * timing, but with this setup the amount of spinning in the busyloop is
	 * minimized. Though CPU usage doesn't reduce when using Delay(0) on
	 * Windows.
	 * -Brandon McGriff <nightmareci@gmail.com>
	 */
	const Uint64 frameTimeCount = SDL_GetPerformanceFrequency() / s_uNowFPS;
	for (int milliseconds = 1; milliseconds >= 0; milliseconds--) {
		Uint64 delayStartTimeCount;
		Uint64 maxDelayCount = 0u;
		while (
			maxDelayCount < frameTimeCount &&
			(delayStartTimeCount = SDL_GetPerformanceCounter()) - s_uTimeCount < frameTimeCount - maxDelayCount
		) {
			Delay(milliseconds);
			Uint64 lastDelay;
			if ((lastDelay = SDL_GetPerformanceCounter() - delayStartTimeCount) > maxDelayCount) {
				maxDelayCount = lastDelay;
			}
		}
	}
	while (SDL_GetPerformanceCounter() - s_uTimeCount < frameTimeCount);

	Uint64 nextTimeCount;
	s_uTimeAccumulatorCount += (nextTimeCount = SDL_GetPerformanceCounter()) - s_uTimeCount;
	s_uTimeCount = nextTimeCount;
}

bool YGS2kHalt()
{
	SDL_Event	ev;
	const Uint64	frameTimeCount = SDL_GetPerformanceFrequency() / s_uNowFPS;

	if ( s_pScreenRenderer )
	{
		SDL_RenderFlush( s_pScreenRenderer );

		if ( s_bBltAlways )
		{
			/* ???????????????????????????????????????????????? */
			if ( s_pScreenRenderTarget )
			{
				SDL_SetRenderTarget(s_pScreenRenderer, NULL);
				SDL_RenderClear( s_pScreenRenderer );
				const SDL_FRect dstrect = { s_fScreenSubpixelOffset, s_fScreenSubpixelOffset, 320 * (!!(s_iScreenMode & SCREENMODE_DETAILLEVEL) + 1), 240 * (!!(s_iScreenMode & SCREENMODE_DETAILLEVEL) + 1) };
				SDL_RenderCopyF(s_pScreenRenderer, s_pScreenRenderTarget, NULL, &dstrect);
				SDL_RenderPresent(s_pScreenRenderer);
				SDL_SetRenderTarget(s_pScreenRenderer, s_pScreenRenderTarget);
			}
			else {
				SDL_RenderPresent(s_pScreenRenderer);
			}

			/* ??????????????????????????? */
			FrameDelay();

			s_uTimeAccumulatorCount = 0;

			/* ????????????????????? */
			SDL_RenderClear( s_pScreenRenderer );
		}
		else
		{
			if ( s_uTimeAccumulatorCount < frameTimeCount )
			{
				/* ???????????????????????????????????????????????? */
				if ( s_pScreenRenderTarget )
				{
					SDL_SetRenderTarget(s_pScreenRenderer, NULL);
					SDL_RenderClear( s_pScreenRenderer );
					const SDL_FRect dstrect = { s_fScreenSubpixelOffset, s_fScreenSubpixelOffset, 320 * (!!(s_iScreenMode & SCREENMODE_DETAILLEVEL) + 1), 240 * (!!(s_iScreenMode & SCREENMODE_DETAILLEVEL) + 1) };
					SDL_RenderCopyF(s_pScreenRenderer, s_pScreenRenderTarget, NULL, &dstrect);
					SDL_RenderPresent(s_pScreenRenderer);
					SDL_SetRenderTarget(s_pScreenRenderer, s_pScreenRenderTarget);
				}
				else {
					SDL_RenderPresent(s_pScreenRenderer);
				}

				/* ??????????????????????????? */
				FrameDelay();

				/* ????????????????????? */
				SDL_RenderClear( s_pScreenRenderer );
			}
			s_uTimeAccumulatorCount -= frameTimeCount;
		}
	}

	/* ??????????????????????????? */
	s_uFPSCnting ++;
	s_uNowFrame ++;

	if ( s_uFPSCount + SDL_GetPerformanceFrequency() <= s_uTimeCount )
	{
		s_uFPS = s_uFPSCnting;
		s_uFPSCnting = 0;
		s_uFPSCount = s_uTimeCount;
	}

	/* ?????????????????? */
	SDL_PumpEvents();
	#ifdef ENABLE_JOYSTICK
	bool joyChanged = false;
	#endif
	#ifdef ENABLE_GAME_CONTROLLER
	bool conChanged = false;
	#endif
	while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, 0, SDL_LASTEVENT) == 1)
	{
		switch(ev.type){
			case SDL_QUIT:						// ?????????????????????????????????????????????????????
				return false;
				break;

			case SDL_WINDOWEVENT:
				if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					s_fScreenSubpixelOffset = SCREEN_SUBPIXEL_OFFSET;
				}
				break;

			#ifdef ENABLE_JOYSTICK
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
				joyChanged = true;
			#endif

			#ifdef ENABLE_GAME_CONTROLLER
			case SDL_CONTROLLERDEVICEADDED:
			case SDL_CONTROLLERDEVICEREMOVED:
				conChanged = true;
				break;
			#endif

			default:
				break;
		}
	}

	#ifdef ENABLE_JOYSTICK
	if (joyChanged)
	{
		JoyClose();
		JoyOpen();
	}
	#endif
		
	#ifdef ENABLE_GAME_CONTROLLER
	if (conChanged)
	{
		ConClose();
		ConOpen();
	}
	#endif

	/* ??????????????????????????? */
	s_iOffsetX = s_iNewOffsetX;
	s_iOffsetY = s_iNewOffsetY;

	return true;
}

void YGS2kTextOut(int x, int y, const char* text, int r, int g, int b, int size)
{
	YGS2kKanjiDraw(x, y, r, g, b, size, text);
}

int IsPlayMIDI()
{
	return Mix_PlayingMusic();
}

int SetScreen(int32_t *screenMode, int32_t *screenIndex)
{
	Uint32		windowFlags = SDL_WINDOW_HIDDEN;
	int		windowX, windowY;
	int		logicalWidth, logicalHeight;
	
	if ( s_pScreenWindow )
	{
		return 0;
	}

	/* ??????????????? */
	EScreenModeFlag windowType = *screenMode & SCREENMODE_WINDOWTYPE;
	int displayIndex = SCREENINDEX_DISPLAY_TOVALUE(*screenIndex);
	int modeIndex = SCREENINDEX_MODE_TOVALUE(*screenIndex);
	int numVideoDisplays = SDL_GetNumVideoDisplays();
	int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
	if (numVideoDisplays < 0 || numDisplayModes < 0) {
		s_pScreenRenderTarget = NULL;
		s_pScreenRenderer = NULL;
		s_pScreenWindow = NULL;
		return 0;
	}

	if (
			displayIndex >= numVideoDisplays ||
			(windowType == SCREENMODE_FULLSCREEN && modeIndex >= numDisplayModes)
	) {
		*screenMode = DEFAULT_SCREEN_MODE;
		*screenIndex = 0;
		windowFlags |= SDL_WINDOW_RESIZABLE;
		windowX = SDL_WINDOWPOS_CENTERED_DISPLAY(*screenIndex);
		windowY = SDL_WINDOWPOS_CENTERED_DISPLAY(*screenIndex);
	}
	else
	{
		windowX = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);
		windowY = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);
		switch ( windowType & SCREENMODE_WINDOWTYPE )
		{
		case SCREENMODE_WINDOW_MAXIMIZED:
			windowFlags |= SDL_WINDOW_MAXIMIZED;
		case SCREENMODE_WINDOW:
			windowFlags |= SDL_WINDOW_RESIZABLE;
			break;

		case SCREENMODE_FULLSCREEN_DESKTOP:
			windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
			break;

		case SCREENMODE_FULLSCREEN: {
			windowFlags |= SDL_WINDOW_FULLSCREEN;
			break;
		}
		}
	}

	if ( *screenMode & SCREENMODE_DETAILLEVEL )
	{
		logicalWidth  = 640;
		logicalHeight = 480;
	}
	else
	{
		logicalWidth  = 320;
		logicalHeight = 240;
	}

	s_iLogicalWidth  = logicalWidth;
	s_iLogicalHeight = logicalHeight;

	/* ???????????????????????? */
	if ( windowType == SCREENMODE_FULLSCREEN )
	{
		SDL_DisplayMode displayMode;
		if ( SDL_GetDisplayMode(displayIndex, modeIndex, &displayMode) < 0 ) {
			*screenMode &= ~SCREENMODE_WINDOWTYPE;
			*screenMode |= SCREENMODE_WINDOW;
			*screenIndex = 0;
			s_pScreenWindow = SDL_CreateWindow(GAME_CAPTION, SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex), SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex), logicalWidth, logicalHeight, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
			if ( !s_pScreenWindow )
			{
				s_pScreenRenderTarget = NULL;
				s_pScreenRenderer = NULL;
				s_pScreenWindow = NULL;
				return 0;
			}
		}
		else {
			s_pScreenWindow = SDL_CreateWindow(GAME_CAPTION, windowX, windowY, displayMode.w, displayMode.h, windowFlags);
			if ( !s_pScreenWindow )
			{
				s_pScreenRenderTarget = NULL;
				s_pScreenRenderer = NULL;
				s_pScreenWindow = NULL;
				return 0;
			}
			if ( SDL_SetWindowDisplayMode(s_pScreenWindow, &displayMode) < 0 )
			{
				SDL_DestroyWindow(s_pScreenWindow);
				s_pScreenRenderTarget = NULL;
				s_pScreenRenderer = NULL;
				s_pScreenWindow = NULL;
				return 0;
			}
		}
	}
	else if ( windowType == SCREENMODE_WINDOW )
	{
		SDL_DisplayMode displayMode;
		if ( SDL_GetDesktopDisplayMode(displayIndex, &displayMode) < 0 )
		{
			s_pScreenRenderTarget = NULL;
			s_pScreenRenderer = NULL;
			s_pScreenWindow = NULL;
			return 0;
		}

		int maxScale;
		if(displayMode.w <= logicalWidth || displayMode.h <= logicalHeight)
		{
			maxScale = 1;
		}
		else if(displayMode.w > displayMode.h)
		{
			maxScale = (displayMode.h / logicalHeight) - (displayMode.h % logicalHeight == 0);
		}
		else
		{
			maxScale = (displayMode.w / logicalWidth) - (displayMode.w % logicalWidth == 0);
		}
		int maxWidth = maxScale * logicalWidth;
		int maxHeight = maxScale * logicalHeight;
		int scale = modeIndex + 1;
		int windowW = scale * logicalWidth;
		int windowH = scale * logicalHeight;
		if ( scale > maxScale) {
			windowW = logicalWidth;
			windowH = logicalHeight;
			*screenIndex = SCREENINDEX_TOSETTING(displayIndex, 0);
		}
		s_pScreenWindow = SDL_CreateWindow(GAME_CAPTION, windowX, windowY, windowW, windowH, windowFlags);
		if ( !s_pScreenWindow )
		{
			s_pScreenRenderTarget = NULL;
			s_pScreenRenderer = NULL;
			s_pScreenWindow = NULL;
			return 0;
		}
	}
	else
	{
		s_pScreenWindow = SDL_CreateWindow(GAME_CAPTION, windowX, windowY, logicalWidth, logicalHeight, windowFlags);
		if ( !s_pScreenWindow )
		{
			s_pScreenRenderTarget = NULL;
			s_pScreenRenderer = NULL;
			s_pScreenWindow = NULL;
			return 0;
		}
	}

	// fix to allow rendering to the texture.
	s_pScreenRenderer = SDL_CreateRenderer(s_pScreenWindow, -1, *screenMode & SCREENMODE_VSYNC ? SDL_RENDERER_PRESENTVSYNC : 0);
	if ( !s_pScreenRenderer )
	{
		SDL_DestroyWindow(s_pScreenWindow);
		s_pScreenRenderTarget = NULL;
		s_pScreenRenderer = NULL;
		s_pScreenWindow = NULL;
		return 0;
	}
	if ( SDL_RenderSetLogicalSize(s_pScreenRenderer, logicalWidth, logicalHeight) < 0 )
	{
		SDL_DestroyRenderer(s_pScreenRenderer);
		SDL_DestroyWindow(s_pScreenWindow);
		s_pScreenRenderTarget = NULL;
		s_pScreenRenderer = NULL;
		s_pScreenWindow = NULL;
		return 0;
	}
	if ( *screenMode & SCREENMODE_SCALEMODE )
	{
		if ( SDL_RenderSetIntegerScale(s_pScreenRenderer, SDL_TRUE) < 0 )
		{
			SDL_DestroyRenderer(s_pScreenRenderer);
			SDL_DestroyWindow(s_pScreenWindow);
			s_pScreenRenderTarget = NULL;
			s_pScreenRenderer = NULL;
			s_pScreenWindow = NULL;
			return 0;
		}
		SDL_SetWindowMinimumSize(s_pScreenWindow, logicalWidth, logicalHeight);
	}
	s_fScreenSubpixelOffset = SCREEN_SUBPIXEL_OFFSET;
	if ( SDL_RenderTargetSupported(s_pScreenRenderer) )
	{
		if ( !(*screenMode & SCREENMODE_RENDERLEVEL) )
		{
			s_pScreenRenderTarget = SDL_CreateTexture(s_pScreenRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, logicalWidth, logicalHeight);
			if ( !s_pScreenRenderTarget )
			{
				SDL_DestroyRenderer(s_pScreenRenderer);
				SDL_DestroyWindow(s_pScreenWindow);
				s_pScreenRenderTarget = NULL;
				s_pScreenRenderer = NULL;
				s_pScreenWindow = NULL;
				 return 0;
			}
			if (
				SDL_RenderClear(s_pScreenRenderer) < 0 ||
				SDL_SetRenderTarget(s_pScreenRenderer, s_pScreenRenderTarget) < 0 ||
				SDL_RenderClear(s_pScreenRenderer) < 0
			)
			{
				SDL_DestroyTexture(s_pScreenRenderTarget);
				SDL_DestroyRenderer(s_pScreenRenderer);
				SDL_DestroyWindow(s_pScreenWindow);
				s_pScreenRenderTarget = NULL;
				s_pScreenRenderer = NULL;
				s_pScreenWindow = NULL;
				return 0;
			}
		}
	}
	else {
		s_pScreenRenderTarget = NULL;
		*screenMode |= SCREENMODE_RENDERLEVEL;
	}
	SDL_ShowWindow(s_pScreenWindow);

	/* ??????????????????????????????????????? */
	if ( SDL_ShowCursor( !(windowFlags & SDL_WINDOW_FULLSCREEN) ) < 0)
	{
		if (s_pScreenRenderTarget)
		{
			SDL_DestroyTexture(s_pScreenRenderTarget);		
		}
		SDL_DestroyRenderer(s_pScreenRenderer);
		SDL_DestroyWindow(s_pScreenWindow);
		s_pScreenRenderTarget = NULL;
		s_pScreenRenderer = NULL;
		s_pScreenWindow = NULL;
		return 0;
	}
	
	s_iScreenMode = *screenMode;
	s_iScreenIndex = *screenIndex;
	return 1;
}

int GetMaxDisplayIndex()
{
	return SDL_GetNumVideoDisplays();
}

int GetMaxDisplayMode( int displayIndex )
{
	return SDL_GetNumDisplayModes(displayIndex);
}

int RenderLevelLowSupported()
{
	return s_pScreenRenderer ? SDL_RenderTargetSupported(s_pScreenRenderer) : 0;
}

void SetConstParam ( const char *param, int value )
{

}

int Rand ( int max )
{
	return rand() % max;
}

void PauseMIDI()
{
	Mix_PauseMusic();
}

void ReplayMIDI()
{
	Mix_ResumeMusic();
}

void PlayWave ( int no )
{
	switch ( s_iYGSSoundType[no] )
	{
	case YGS_SOUNDTYPE_WAV:
		Mix_PlayChannel(no, s_pYGSSound[no], 0);
		Mix_Volume(no, s_iYGSSoundVolume[no]);
		break;

	case YGS_SOUNDTYPE_MUS:
		if ((no == 56) || (no == 57)) {
			Mix_PlayMusic(s_pYGSExMusic[no], 0);
		} else {
			Mix_PlayMusic(s_pYGSExMusic[no], -1);
		}
		Mix_VolumeMusic(s_iYGSSoundVolume[no]);
		break;

	default:
		break;
	}
}

void ReplayWave ( int no )
{
	switch ( s_iYGSSoundType[no] )
	{
	case YGS_SOUNDTYPE_WAV:
		Mix_Resume(no);
		break;

	case YGS_SOUNDTYPE_MUS:
		Mix_RewindMusic();
		break;
	
	default:
		break;
	}
}

void StopWave ( int no )
{
	switch ( s_iYGSSoundType[no] )
	{
	case YGS_SOUNDTYPE_WAV:
		Mix_HaltChannel(no);
		break;

	case YGS_SOUNDTYPE_MUS:
		Mix_HaltMusic();
		break;
		
	default:
		break;
	}
}

void PauseWave ( int no )
{
	switch ( s_iYGSSoundType[no] )
	{
	case YGS_SOUNDTYPE_WAV:
		Mix_Pause(no);
		break;

	case YGS_SOUNDTYPE_MUS:
		Mix_PauseMusic();
		break;

	default:
		break;
	}
}

void SetVolumeWave( int no, int vol )
{
	int volume = (int)((vol / 100.0f) * YGS_VOLUME_MAX);
	if ( volume > YGS_VOLUME_MAX ) { volume = YGS_VOLUME_MAX; }
	if ( volume < 0 )   { volume = 0; }
	s_iYGSSoundVolume[no] = volume;

	switch ( s_iYGSSoundType[no] )
	{
	case YGS_SOUNDTYPE_WAV:
		Mix_Volume(no, volume);
		break;

	case YGS_SOUNDTYPE_MUS:
		Mix_VolumeMusic(volume);
		break;

	default:
		break;
	}
}

int IsPlayWave( int no )
{
	switch ( s_iYGSSoundType[no] )
	{
	case YGS_SOUNDTYPE_WAV:
		/* ????????????????????????????????????????????? */
		return Mix_Playing(no);

	case YGS_SOUNDTYPE_MUS:
		return Mix_PlayingMusic();
		
	default:
		return 0;
	}
}

void LoadWave( const char* filename, int no )
{
	int		len = strlen(filename);
	if ( len < 4 ) { return; }

	s_iYGSSoundType[no] = YGS_SOUNDTYPE_NONE;

	// ???????????????????????????(50????????????BGM)??????????????????????????????????????????
	SDL_RWops *src;
	src = PHYSFSRWOPS_openRead(filename);
	if ( !src ) return;
	if ( SDL_strcasecmp(&filename[len - 4], ".wav") || no >= 50 )
	{
		if ( s_pYGSExMusic[no] != NULL )
		{
			Mix_FreeMusic(s_pYGSExMusic[no]);
			s_pYGSExMusic[no] = NULL;
		}
		s_pYGSExMusic[no] = Mix_LoadMUS_RW(src, SDL_TRUE);
		s_iYGSSoundType[no] = YGS_SOUNDTYPE_MUS;
		s_iYGSSoundVolume[no] = YGS_VOLUME_MAX;
	}
	else
	{
		if ( s_pYGSSound[no] != NULL )
		{
			Mix_FreeChunk(s_pYGSSound[no]);
			s_pYGSSound[no] = NULL;
		}
		s_pYGSSound[no] = Mix_LoadWAV_RW(src, SDL_TRUE);
		s_iYGSSoundType[no] = YGS_SOUNDTYPE_WAV;
		s_iYGSSoundVolume[no] = YGS_VOLUME_MAX;
	}
}

void SetLoopModeWave( int no, int mode )
{
   // true=loop, false=no loop.  since this is only used on BGMs, and all BGMs already loop, this is a no-op.
}

void LoadMIDI( const char* filename )
{
	if ( s_pYGSMusic )
	{
		Mix_FreeMusic(s_pYGSMusic);
		s_pYGSMusic = NULL;
	}

	SDL_RWops* src;
	if (!(src = PHYSFSRWOPS_openRead(filename))) return;
	s_pYGSMusic = Mix_LoadMUS_RW(src, SDL_TRUE);
	s_iYGSMusicVolume = YGS_VOLUME_MAX;
}

void LoadBitmap( const char* filename, int plane, int val )
{
	if ( !s_pScreenRenderer )
	{
		return;
	}

	if ( s_pYGSTexture[plane] )
	{
		SDL_DestroyTexture(s_pYGSTexture[plane]);
		s_pYGSTexture[plane] = NULL;
	}

	SDL_RWops* src;
	if (!(src = PHYSFSRWOPS_openRead(filename))) return;
	if (!(s_pYGSTexture[plane] = IMG_LoadTexture_RW(s_pScreenRenderer, src, SDL_TRUE))) return;
	SDL_SetTextureBlendMode(s_pYGSTexture[plane], SDL_BLENDMODE_BLEND);
}

void PlayMIDI()
{
	if ( s_pYGSMusic )
	{
		Mix_PlayMusic(s_pYGSMusic, -1);
	}
}

void StopMIDI()
{
	Mix_HaltMusic();
}

void SetVolumeMIDI(int vol)
{
	int volume = (int)((vol / 100.0f) * YGS_VOLUME_MAX);
	if ( volume > YGS_VOLUME_MAX ) { volume = YGS_VOLUME_MAX; }
	if ( volume < 0 )   { volume = 0; }
	s_iYGSMusicVolume = volume;
	Mix_VolumeMusic(volume);
}

void SetColorKeyPos(int plane, int x, int y)
{
   // sets transparent color to the specified pixel.  Since we use actual alpha channel in our assets, this is a no-oop
}

void EnableBlendColorKey(int plane, int key)
{
   // alows for parial transparency.   again, because we use real transparency, it's a no-op.
}

void CreateSurface(int surf, int w, int h)
{
    // required for orignal YGS2K engine. not needed at all for SDL2 renderer.
}

void ClearSecondary()
{
    // used to write the listed color to all pixels of the rendering area.
	// with SDL2 renderer, we never need to do this, so it's a no-op
}

void SetFillColor(int col)
{
	// sets the color that ClearSecondary uses to fill the render target. since ClearSecondary is a no-op, so is this.
}

void LoadFile( const char* filename, void* buf, int size )
{
	SDL_RWops	*src = PHYSFSRWOPS_openRead(filename);

	if ( src )
	{
		SDL_RWread(src, buf, size, 1);
		SDL_RWclose(src);

		int32_t		i, *buf2;

		/* ???????????????????????? */
		buf2 = (int32_t*)buf;
		for ( i = 0 ; i < size / sizeof(int32_t) ; i ++ )
		{
			buf2[i] = SWAP32(buf2[i]);
		}
	}
}

void SaveFile( const char* filename, void* buf, int size )
{
	int32_t		i, *buf2;

	/* ???????????????????????? */
	buf2 = (int32_t*)buf;
	for ( i = 0 ; i < size / sizeof(int32_t) ; i ++ )
	{
		buf2[i] = SWAP32(buf2[i]);
	}

	SDL_RWops	*dst = PHYSFSRWOPS_openWrite(filename);

	if ( dst )
	{
		SDL_RWwrite(dst, buf, size, 1);
		SDL_RWclose(dst);
	}

	/* ????????? */
	for ( i = 0 ; i < size / sizeof(int32_t) ; i ++ )
	{
		buf2[i] = SWAP32(buf2[i]);
	}
}

void TextLayerOn ( int layer, int x, int y )
{
	s_TextLayer[layer].enable = true;
	s_TextLayer[layer].x = x;
	s_TextLayer[layer].y = y;
}

void TextMove ( int layer, int x, int y )
{
	s_TextLayer[layer].x = x;
	s_TextLayer[layer].y = y;
}

void TextColor ( int layer, int r, int g, int b )
{
	s_TextLayer[layer].r = r;
	s_TextLayer[layer].g = g;
	s_TextLayer[layer].b = b;
}

void TextBackColorDisable ( int layer )
{
	// turns off the shadow effect for text in the listed layer. since we don't even use said shadow effect to begin with, it's a no-op.
}

void TextSize ( int layer, int size )
{
	s_TextLayer[layer].size = size;
}

void TextHeight ( int layer, int height )
{
	// only used in flexdraw.c for ExTextHeight. But since ExTextHeight is unused, we don't need to bother implementing it. 
}

void TextOut ( int layer, const char* text )
{
	strcpy(s_TextLayer[layer].string, text);
}

void TextBlt ( int layer )
{
	if ( s_TextLayer[layer].enable )
	{
		YGS2kTextOut(s_TextLayer[layer].x + s_iOffsetX, s_TextLayer[layer].y + s_iOffsetY, s_TextLayer[layer].string, s_TextLayer[layer].r, s_TextLayer[layer].g, s_TextLayer[layer].b, s_TextLayer[layer].size);
	}
}

void TextLayerOff ( int layer )
{
	s_TextLayer[layer].enable = false;
}

void BltAlways(bool always)
{
	s_bBltAlways = always;
}

void Blt(int pno, int dx, int dy)
{
	if ( s_pYGSTexture[pno] == NULL ) { return; }
	int w, h;
	SDL_QueryTexture(s_pYGSTexture[pno], NULL, NULL, &w, &h);
	BltRect(pno, dx, dy, 0, 0, w, h);
}

void BltRect(int pno, int dx, int dy, int sx, int sy, int hx, int hy)
{
	if ( !s_pScreenRenderer )
	{
		return;
	}

	if ((pno > 99)&& s_pScreenRenderTarget) //  hack to use screen render target as source
	{
		SDL_Rect	src = { 0 };
		SDL_Rect	dst = { 0 };

		src.x = sx;			src.y = sy;
		src.w = hx;			src.h = hy;
		dst.x = dx + s_iOffsetX;	dst.y = dy + s_iOffsetY;
		dst.w = hx;			dst.h = hy;

		SDL_RenderCopy(s_pScreenRenderer, s_pScreenRenderTarget, &src, &dst);
		return;
	}
	if (pno > 99) return; // give up so check below isn't ran if we use the hack.
	if ( s_pYGSTexture[pno] == NULL ) return;

	if ( s_pScreenRenderTarget )
	{
		SDL_Rect	src = { 0 };
		SDL_Rect	dst = { 0 };

		src.x = sx;			src.y = sy;
		src.w = hx;			src.h = hy;
		dst.x = dx + s_iOffsetX;	dst.y = dy + s_iOffsetY;
		dst.w = hx;			dst.h = hy;

		SDL_RenderCopy(s_pScreenRenderer, s_pYGSTexture[pno], &src, &dst);
	}
	else
	{
		SDL_Rect	src = { 0 };
		SDL_FRect	dst = { s_fScreenSubpixelOffset, s_fScreenSubpixelOffset };

		src.x  = sx;			src.y  = sy;
		src.w  = hx;			src.h  = hy;
		dst.x += dx + s_iOffsetX;	dst.y += dy + s_iOffsetY;
		dst.w  = hx;			dst.h  = hy;

		SDL_RenderCopyF(s_pScreenRenderer, s_pYGSTexture[pno], &src, &dst);
	}
}

void BltFast(int pno, int dx, int dy)
{
	Blt(pno, dx, dy);
}

void BltFastRect(int pno, int dx, int dy, int sx, int sy, int hx, int hy)
{
	BltRect(pno, dx, dy, sx, sy, hx, hy);
}

void BlendBlt(int pno, int dx, int dy, int ar, int ag, int ab, int br, int bg, int bb)
{
	if ( s_pYGSTexture[pno] == NULL ) return;

	SDL_SetTextureAlphaMod(s_pYGSTexture[pno], ar);
	Blt(pno, dx, dy);
	SDL_SetTextureAlphaMod(s_pYGSTexture[pno], SDL_ALPHA_OPAQUE);
}

void BlendBltRect(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int ar, int ag, int ab, int br, int bg, int bb)
{
	if ( s_pYGSTexture[pno] == NULL ) return;

	SDL_SetTextureAlphaMod(s_pYGSTexture[pno], ar);
	BltRect(pno, dx, dy, sx, sy, hx, hy);
	SDL_SetTextureAlphaMod(s_pYGSTexture[pno], SDL_ALPHA_OPAQUE);
}

void BltR(int pno, int dx, int dy, int scx, int scy)
{
	BltRectR(pno, dx, dy, 0, 0, s_iLogicalWidth, s_iLogicalHeight, scx, scy);
}

void BltRectR(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int scx, int scy)
{
	if ( !s_pScreenRenderer )
	{
		return;
	}

	if ((pno > 99)&& s_pScreenRenderTarget) //  hack to use screen render target as source
	{
		SDL_Rect	src = { 0 };
		SDL_Rect	dst = { 0 };

		src.x = sx;					src.y = sy;
		src.w = hx;					src.h = hy;
		dst.x = dx + s_iOffsetX;	dst.y = dy + s_iOffsetY;
		dst.w = hx * (scx / 65536.0f);
		dst.h = hy * (scy / 65536.0f);

		if ( src.w == 0 || src.h == 0 || dst.w == 0 || dst.h == 0 ) { return; }

		SDL_RenderCopy(s_pScreenRenderer, s_pScreenRenderTarget, &src, &dst);
		return;
	}
	if (pno > 99) return; // give up so check below isn't ran if we use the hack.
	if ( s_pYGSTexture[pno] == NULL ) return;

	// ????????????????????????????????????
	if ( s_pScreenRenderTarget )
	{
		SDL_Rect	src = { 0 };
		SDL_Rect	dst = { 0 };

		src.x = sx;					src.y = sy;
		src.w = hx;					src.h = hy;
		dst.x = dx + s_iOffsetX;	dst.y = dy + s_iOffsetY;
		dst.w = hx * (scx / 65536.0f);
		dst.h = hy * (scy / 65536.0f);

		if ( src.w == 0 || src.h == 0 || dst.w == 0 || dst.h == 0 ) { return; }

		SDL_RenderCopy( s_pScreenRenderer, s_pYGSTexture[pno], &src, &dst );
	}
	else
	{
		SDL_Rect	src = { 0 };
		SDL_FRect	dst = { s_fScreenSubpixelOffset, s_fScreenSubpixelOffset };

		src.x  = sx;			src.y  = sy;
		src.w  = hx;			src.h  = hy;
		dst.x += dx + s_iOffsetX;	dst.y += dy + s_iOffsetY;
		dst.w  = (int)(hx * (scx / 65536.0f));
		dst.h  = (int)(hy * (scy / 65536.0f));

		if ( src.w == 0 || src.h == 0 || dst.w == 0 || dst.h == 0 ) { return; }

		SDL_RenderCopyF( s_pScreenRenderer, s_pYGSTexture[pno], &src, &dst );
	}
}

void BltFastR(int pno, int dx, int dy, int scx, int scy)
{
	BltR(pno, dx, dy, scx, scy);
}

void BltFastRectR(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int scx, int scy)
{
	BltRectR(pno, dx, dy, sx, sy, hx, hy, scx, scy);
}

void BltTrans(int pno, int dx, int dy)
{
	// completely unused.  so we don't need to care what it even does.
}

void BlendBltR(int pno, int dx, int dy, int ar, int ag, int ab, int br, int bg, int bb, int scx, int scy)
{
	BlendBltRectR(pno, dx, dy, 0, 0, s_iLogicalWidth, s_iLogicalHeight, ar, ag, ab, br, bg, bb, scx, scy);
}

void BlendBltRectR(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int ar, int ag, int ab, int br, int bg, int bb, int scx, int scy)
{
	if ( !s_pScreenRenderer )
	{
		return;
	}

	if ((pno > 99)&& s_pScreenRenderTarget) //  hack to use screen render target as source
	{
		SDL_Rect	src = { 0 };
		SDL_Rect	dst = { 0 };

		src.x = sx;					src.y = sy;
		src.w = hx;					src.h = hy;
		dst.x = dx + s_iOffsetX;	dst.y = dy + s_iOffsetY;
		dst.w = hx * (scx / 65536.0f);
		dst.h = hy * (scy / 65536.0f);

		if ( src.w == 0 || src.h == 0 || dst.w == 0 || dst.h == 0 ) { return; }

		SDL_SetTextureAlphaMod(s_pScreenRenderTarget, ar);
		SDL_RenderCopy(s_pScreenRenderer, s_pScreenRenderTarget, &src, &dst);
		SDL_SetTextureAlphaMod(s_pScreenRenderTarget, SDL_ALPHA_OPAQUE);		return;
	}
	if (pno > 99) return; // give up so check below isn't ran if we use the hack.
	if ( s_pYGSTexture[pno] == NULL ) return;

	// ????????????????????????????????????
	if ( s_pScreenRenderTarget )
	{
		SDL_Rect	src = { 0 };
		SDL_Rect	dst = { 0 };

		src.x = sx;					src.y = sy;
		src.w = hx;					src.h = hy;
		dst.x = dx + s_iOffsetX;	dst.y = dy + s_iOffsetY;
		dst.w = hx * (scx / 65536.0f);
		dst.h = hy * (scy / 65536.0f);

		if ( src.w == 0 || src.h == 0 || dst.w == 0 || dst.h == 0 ) { return; }

		SDL_SetTextureAlphaMod(s_pYGSTexture[pno], ar);
		SDL_RenderCopy( s_pScreenRenderer, s_pYGSTexture[pno], &src, &dst );
		SDL_SetTextureAlphaMod(s_pYGSTexture[pno], SDL_ALPHA_OPAQUE);
	}
	else
	{
		SDL_Rect	src = { 0 };
		SDL_FRect	dst = { s_fScreenSubpixelOffset, s_fScreenSubpixelOffset };

		src.x  = sx;			src.y  = sy;
		src.w  = hx;			src.h  = hy;
		dst.x += dx + s_iOffsetX;	dst.y += dy + s_iOffsetY;
		dst.w  = hx * (scx / 65536.0f);
		dst.h  = hy * (scy / 65536.0f);

		if ( src.w == 0 || src.h == 0 || dst.w == 0 || dst.h == 0 ) { return; }

		SDL_SetTextureAlphaMod(s_pYGSTexture[pno], ar);
		SDL_RenderCopyF( s_pScreenRenderer, s_pYGSTexture[pno], &src, &dst );
		SDL_SetTextureAlphaMod(s_pYGSTexture[pno], SDL_ALPHA_OPAQUE);
	}
}

void SetSecondaryOffset(int x, int y)
{
	s_iNewOffsetX = x;
	s_iNewOffsetY = y;
}

void SetColorKeyRGB(int pno, int r, int g, int b)
{
	//  again because we have actual transparency in our assets, this is a no-op.
}

void SwapToSecondary(int pno)
{
     // swaps the rendering target with a layer.  not implemented because no one can figuer out HOW in SDL. 
	 // Only required for EH-Final gimmick, which currently has a workaround.
	 // it's also used in the graphic loader, and the backgroud loader, but doesn't seem needed.
}

void SetFPS(int fps)
{
	s_uNowFPS = fps;
}

int GetFPS()
{
	return s_uNowFPS;
}

int GetRealFPS()
{
	return s_uFPS;
}

void StrCpy(char *dest, const char *src)
{
	strcpy(dest, src);
}

void StrCat(char *str1, const char *str2)
{
	strcat(str1, str2);
}

int StrLen(const char *stri)
{
	return strlen(stri);
}

void MidStr(const char *src, int start, int len, char *dest)
{
	int		i;
	for ( i = 0 ; i < len ; i ++ )
	{
		dest[i] = src[start - 1 + i];
	}
	dest[len] = '\0';
}

void LeftStr(const char *src, int len, char *dest)
{
	MidStr(src, 1, len, dest);
}

char CharAt(const char *stri, int pos)
{
	return stri[pos];
}

int ValLong(const char *stri)
{
	return atoi(stri);
}

void FillMemory(void* buf, int size, int val)
{
	memset(buf, val, size);
}

////////////////////////////////////////////////////

void YGS2kKanjiFontInitialize()
{
	SDL_RWops *src;

	/* 10px???????????????????????? */
	src = PHYSFSRWOPS_openRead("res/font/knj10.bdf");
	if ( src )
	{
		s_pKanjiFont[0] = Kanji_OpenFont(src, 10);
		SDL_RWclose(src);
	}
	else
	{
		s_pKanjiFont[0] = NULL;
	}
	if ( s_pKanjiFont[0] )
	{
		src = PHYSFSRWOPS_openRead("res/font/5x10a.bdf");
		if ( src ) {
			Kanji_AddFont(s_pKanjiFont[0], src);
			SDL_RWclose(src);
		}
	}
	else
	{
		/* ?????????????????????????????????????????? */
		src = PHYSFSRWOPS_openRead("res/font/knj12.bdf");
		if ( src )
		{
			s_pKanjiFont[0] = Kanji_OpenFont(src, 10);
			SDL_RWclose(src);
		}
		else {
			s_pKanjiFont[0] = NULL;
		}
		if ( s_pKanjiFont[0] )
		{
			src = PHYSFSRWOPS_openRead("res/font/6x12a.bdf");
			if ( src ) {
				Kanji_AddFont(s_pKanjiFont[0], src);
				SDL_RWclose(src);
			}
		}
	}

	if ( s_pKanjiFont[0] )
	{
		Kanji_SetCodingSystem(s_pKanjiFont[0], KANJI_SJIS);
	}

	/* 12px???????????????????????? */
	src = PHYSFSRWOPS_openRead("res/font/knj12.bdf");
	if ( src ) {
		s_pKanjiFont[1] = Kanji_OpenFont(src, 12);
		SDL_RWclose(src);
	}
	else
	{
		s_pKanjiFont[1] = NULL;
	}
	if ( s_pKanjiFont[1] )
	{
		src = PHYSFSRWOPS_openRead("res/font/6x12a.bdf");
		if ( src )
		{
			Kanji_AddFont(s_pKanjiFont[1], src);
			SDL_RWclose(src);
			Kanji_SetCodingSystem(s_pKanjiFont[1], KANJI_SJIS);
		}
	}

	/* 16px???????????????????????? */
	src = PHYSFSRWOPS_openRead("res/font/knj16.bdf");
	if ( src )
	{
		s_pKanjiFont[2] = Kanji_OpenFont(src, 16);
		SDL_RWclose(src);
	}
	else
	{
		s_pKanjiFont[2] = NULL;
	}
	if ( s_pKanjiFont[2] )
	{
		src = PHYSFSRWOPS_openRead("res/font/8x16a.bdf");
		if ( src )
		{
			Kanji_AddFont(s_pKanjiFont[2], src);
			SDL_RWclose(src);
			Kanji_SetCodingSystem(s_pKanjiFont[2], KANJI_SJIS);
		}
	}
}

void YGS2kKanjiFontFinalize()
{
	if ( s_pKanjiFont[0] ) { Kanji_CloseFont(s_pKanjiFont[0]); }
	if ( s_pKanjiFont[1] ) { Kanji_CloseFont(s_pKanjiFont[1]); }
	if ( s_pKanjiFont[2] ) { Kanji_CloseFont(s_pKanjiFont[2]); }
}

void YGS2kKanjiDraw(int x, int y, int r, int g, int b, int size, const char *str)
{
	if ( !s_pScreenRenderer )
	{
		return;
	}

	SDL_Color col = { 0 };
	int		font = 0;

	if ( size >= 12 )
	{
		font ++;
	}
	if ( size >= 16 )
	{
		font ++;
	}

	col.r = r;
	col.g = g;
	col.b = b;

	if ( s_pKanjiFont[font] )
	{
		if ( s_pScreenRenderTarget )
		{
			Kanji_PutTextRenderer(s_pKanjiFont[font], x, y, 0.0f, 0.0f, s_pScreenRenderer, str, col);
		}
		else {
			Kanji_PutTextRenderer(s_pKanjiFont[font], x, y, s_fScreenSubpixelOffset, s_fScreenSubpixelOffset, s_pScreenRenderer, str, col);
		}
	}
}
