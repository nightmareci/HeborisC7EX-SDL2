#pragma once

#include "ygs2kinput.h"
#include "SDL.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef		TextOut
#undef		TextOut
#endif
#ifdef		LoadBitmap
#undef		LoadBitmap
#endif

#define		YGS_DEADZONE_MAX	(32767 / 4)

bool YGS2kInit();
void YGS2kExit();
bool YGS2kHalt();

void YGS2kTextOut(int x, int y, const char* text, int r, int g, int b, int size);

int IsPlayMIDI();

typedef enum EScreenModeFlag {
	SCREENMODE_WINDOW,
	SCREENMODE_WINDOW_MAXIMIZED,
	SCREENMODE_FULLSCREEN_DESKTOP,
	SCREENMODE_FULLSCREEN,
	SCREENMODE_NUMWINDOWTYPES,
	SCREENMODE_WINDOWTYPE = 0x3,
	SCREENMODE_DETAILLEVEL = 0x4,
	SCREENMODE_VSYNC = 0x8,
	SCREENMODE_SCALEMODE = 0x10,
	SCREENMODE_RENDERLEVEL = 0x20
} EScreenModeFlag;

typedef enum EScreenIndexMask {
	SCREENINDEX_DISPLAY = 0x0000FFFF,
	SCREENINDEX_MODE = 0xFFFF0000
} EScreenIndexMask;

#define SCREENINDEX_DISPLAY_TOVALUE(setting) (((int)(setting)) & 0xFFFF)
#define SCREENINDEX_DISPLAY_TOSETTING(value) (((int32_t)(value)) & SCREENINDEX_DISPLAY)

#define SCREENINDEX_MODE_TOVALUE(setting) ((int)((((uint32_t)(setting)) & SCREENINDEX_MODE) >> 16))
#define SCREENINDEX_MODE_TOSETTING(setting) ((int32_t)((((uint32_t)(setting)) << 16) & SCREENINDEX_MODE))

#define SCREENINDEX_TOSETTING(display, mode) (((int32_t)((mode) & 0xFFFF) << 16) | (int32_t)((display) & 0xFFFF))

int SetScreen(int32_t *screenMode, int32_t *screenIndex);
int GetMaxDisplayIndex();
int GetMaxDisplayMode( int displayIndex );
int RenderLevelLowSupported();

void SetConstParam ( const char *param, int value );

int Rand ( int max );
void PauseMIDI();
void ReplayMIDI();
void PlayWave ( int no );
void ReplayWave ( int no );
void StopWave ( int no );
void PauseWave ( int no );
void SetVolumeWave( int no, int vol );
int IsPlayWave( int no );
void LoadWave( const char* filename, int no );
void SetLoopModeWave( int no, int mode );

void LoadMIDI( const char* filename );
void LoadBitmap( const char* filename, int plane, int value );
void PlayMIDI();
void StopMIDI();
void SetVolumeMIDI(int vol);
void SetColorKeyPos(int plane, int x, int y);
void EnableBlendColorKey(int plane, int key);
void CreateSurface(int surf, int w, int h);
void ClearSecondary();
void SetFillColor(int col);
void LoadFile( const char* filename, void* buf, int size );
void SaveFile( const char* filename, void* buf, int size );
void TextLayerOn ( int layer, int x, int y );
void TextMove ( int layer, int x, int y );
void TextColor ( int layer, int r, int g, int b );
void TextBackColorDisable ( int layer );
void TextSize ( int layer, int size );
void TextHeight ( int layer, int height );
void TextOut ( int layer, const char* text );
void TextBlt ( int layer );
void TextLayerOff ( int layer );
void BltAlways(bool always);
void Blt(int pno, int dx, int dy);
void BltRect(int pno, int dx, int dy, int sx, int sy, int hx, int hy);
void BltFast(int pno, int dx, int dy);
void BltFastRect(int pno, int dx, int dy, int sx, int sy, int hx, int hy);
void BlendBlt(int pno, int dx, int dy, int ar, int ag, int ab, int br, int bg, int bb);
void BlendBltRect(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int ar, int ag, int ab, int br, int bg, int bb);
void BltR(int pno, int dx, int dy, int scx, int scy);
void BltRectR(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int scx, int scy);
void BltFastR(int pno, int dx, int dy, int scx, int scy);
void BltFastRectR(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int scx, int scy);
void BltTrans(int pno, int dx, int dy);
void BlendBltR(int pno, int dx, int dy, int ar, int ag, int ab, int br, int bg, int bb, int scx, int scy);
void BlendBltRectR(int pno, int dx, int dy, int sx, int sy, int hx, int hy, int ar, int ag, int ab, int br, int bg, int bb, int scx, int scy);

void SetSecondaryOffset(int x, int y);
void SetColorKeyRGB(int pno, int r, int g, int b);
void SwapToSecondary(int pno);

void SetFPS(int fps);
int GetFPS();
int GetRealFPS();

void StrCpy(char *dest, const char *src);
void StrCat(char *str1, const char *str2);
int StrLen(const char *stri);
void MidStr(const char *src, int start, int len, char *dest);
void LeftStr(const char *src, int len, char *dest);
char CharAt(const char *stri, int pos);
int ValLong(const char *stri);
void FillMemory(void* buf, int size, int val);

void YGS2kKanjiFontInitialize();
void YGS2kKanjiFontFinalize();
void YGS2kKanjiDraw(int x, int y, int r, int g, int b, int size, const char *str);
