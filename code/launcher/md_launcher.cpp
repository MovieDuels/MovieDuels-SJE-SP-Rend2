/*
===========================================================================
MovieDuels launcher
===========================================================================

The launcher window from the team's design (1088x607): the MovieDuels logo between two bars
(HOME, OPTIONS | DISCORD, HELP), Vader on the left and Kylo Ren / Rey on the right, and a bottom
bar with SINGLEPLAYER MODE and MULTIPLAYER MODE either side of the emblem. Hovering a mode lights
up its side of the art and turns the other side grey (the backgrounds crossfade).

- SINGLEPLAYER MODE / MULTIPLAYER MODE start MovieDuels-SP.x86_64.exe / MovieDuels-MP.x86_64.exe
  from the launcher's folder, passing the OPTIONS as "+set" values, and close the launcher.
- OPTIONS opens a panel over the art: Renderer, Display, Animations (remembered in
  MovieDuels-Launcher.ini in Documents\My Games\MovieDuels, next to the game's own configs).
- HOME, DISCORD and HELP open the mod's ModDB page, the Discord invite and the ModDB tutorials
  in the browser.

The window is a layered window with per-pixel alpha, so the rounded corners are smooth. The art
(md_launcher.rc) is built by MD-work/launcher2/LauncherArt.cs from the mockup; the bars and the
emblem are part of the art, the labels are drawn here so they can light up.
*/

#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <mmsystem.h>

#include <memory>
#include <string>

#include "resource.h"

namespace
{
	constexpr int kWidth = 1088;
	constexpr int kHeight = 607;
	constexpr float kCornerRadius = 28.0f;

	// hit ids
	enum Hit
	{
		kHitNone = -1,
		kHitHome, kHitOptions, kHitDiscord, kHitHelp, // top bars
		kHitSingle, kHitMulti,                        // bottom bar
		kHitOpt0, kHitOpt1, kHitOpt2, kHitOptBack,    // options panel
		kHitMusic, kHitMinimise, kHitClose            // window buttons
	};

	const wchar_t* const kUrlHome = L"https://www.moddb.com/mods/movie-duels";
	const wchar_t* const kUrlDiscord = L"https://discord.com/invite/PhQPy7j";
	const wchar_t* const kUrlHelp = L"https://www.moddb.com/mods/movie-duels/tutorials";

	// background music: MCI plays the MP3 (from a temp file), PlaySound the click sounds alongside
	const wchar_t* const kMusicAlias = L"md_launcher_music";
	constexpr int kMusicVolume = 600; // 0..1000

	const wchar_t* const kSPExe = L"MovieDuels-SP.x86_64.exe";
	const wchar_t* const kMPExe = L"MovieDuels-MP.x86_64.exe";
	const wchar_t* const kIniName = L"MovieDuels-Launcher.ini";
	const wchar_t* const kHomeFolder = L"MovieDuels"; // the game's folder in My Games (HOMEPATH_NAME_WIN)
	const wchar_t* const kVersionText = L"Update 9";

	// colours from the art
	const Gdiplus::Color kGold(255, 253, 178, 42);      // the panel's border
	const Gdiplus::Color kGoldText(255, 255, 204, 102); // lit labels
	const Gdiplus::Color kWhiteText(255, 249, 249, 249);

	// background crossfade
	constexpr UINT_PTR kFadeTimer = 1;
	constexpr DWORD kFadeMs = 180;

	enum class Bg { Idle, Single, Multi, Count };

	struct Settings
	{
		bool rend2 = false;
		bool windowed = false;
		bool master_animations = false; // SP only: g_ActivateAnimationStyle
		bool music = true;              // launcher background music (speaker button)
	};

	struct Launcher
	{
		HINSTANCE instance = nullptr;
		HWND hwnd = nullptr;
		std::wstring folder; // where the launcher (and the game exes) are
		Settings settings;
		bool options_open = false;
		int hover = kHitNone;
		int pressed = kHitNone;
		bool tracking_mouse = false;

		std::wstring music_file; // the music written to %TEMP% for MCI
		bool music_open = false;

		std::unique_ptr<Gdiplus::Bitmap> bg[static_cast<int>(Bg::Count)];
		Bg bg_from = Bg::Idle, bg_to = Bg::Idle;
		DWORD fade_start = 0;
		bool fading = false;

		// the layered window's pixels (32 bpp premultiplied DIB)
		HDC mem_dc = nullptr;
		HBITMAP dib = nullptr;
		HGDIOBJ old_bitmap = nullptr;
		void* bits = nullptr;
	};

	Launcher g_launcher;

	// ------------------------------------------------------------------------------------------
	// Resources
	// ------------------------------------------------------------------------------------------

	// The bytes of an RCDATA resource of the exe (valid while the program runs).
	bool GetResourceBytes(const int id, const void** bytes, DWORD* size)
	{
		const HRSRC res = FindResourceW(g_launcher.instance, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10)); // RT_RCDATA
		const HGLOBAL data = res ? LoadResource(g_launcher.instance, res) : nullptr;
		*bytes = data ? LockResource(data) : nullptr;
		*size = res ? SizeofResource(g_launcher.instance, res) : 0;
		return *bytes && *size;
	}

	// ------------------------------------------------------------------------------------------
	// Sound
	// ------------------------------------------------------------------------------------------

	// Menu sounds: a newer one cuts off the one still playing, like in the game's menus.
	// wait: play it to the end first (for a click right before the launcher closes).
	void PlaySfx(const int id, const bool wait = false)
	{
		const void* bytes;
		DWORD size;
		if (GetResourceBytes(id, &bytes, &size))
		{
			PlaySoundW(static_cast<LPCWSTR>(bytes), nullptr, SND_MEMORY | SND_NODEFAULT | (wait ? SND_SYNC : SND_ASYNC));
		}
	}

	// MCI can only play music from a file: write it to %TEMP% once, then loop it. When a play
	// finishes, MCI sends MM_MCINOTIFY to the window, which starts it again (see WndProc).
	void PlayMusicFromStart(const HWND hwnd)
	{
		const std::wstring cmd = std::wstring(L"play ") + kMusicAlias + L" from 0 notify";
		mciSendStringW(cmd.c_str(), nullptr, 0, hwnd);
	}

	void StartMusic(const HWND hwnd)
	{
		if (g_launcher.music_open || !g_launcher.settings.music)
		{
			return;
		}
		if (g_launcher.music_file.empty())
		{
			const void* bytes;
			DWORD size;
			wchar_t temp[MAX_PATH] = {};
			if (!GetResourceBytes(IDR_MUSIC, &bytes, &size) || !GetTempPathW(MAX_PATH, temp))
			{
				return;
			}
			const std::wstring file = std::wstring(temp) + L"MovieDuels-Launcher-music.mp3";
			const HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
			if (h == INVALID_HANDLE_VALUE)
			{
				return;
			}
			DWORD written = 0;
			const bool ok = WriteFile(h, bytes, size, &written, nullptr) && written == size;
			CloseHandle(h);
			if (!ok)
			{
				DeleteFileW(file.c_str());
				return;
			}
			g_launcher.music_file = file;
		}
		const std::wstring open = L"open \"" + g_launcher.music_file + L"\" type mpegvideo alias " + kMusicAlias;
		if (mciSendStringW(open.c_str(), nullptr, 0, nullptr) != 0)
		{
			return; // no MP3 playback on this PC: just no music
		}
		g_launcher.music_open = true;
		const std::wstring volume = std::wstring(L"setaudio ") + kMusicAlias + L" volume to " + std::to_wstring(kMusicVolume);
		mciSendStringW(volume.c_str(), nullptr, 0, nullptr);
		PlayMusicFromStart(hwnd);
	}

	void StopMusic()
	{
		if (g_launcher.music_open)
		{
			const std::wstring close = std::wstring(L"close ") + kMusicAlias;
			mciSendStringW(close.c_str(), nullptr, 0, nullptr);
			g_launcher.music_open = false;
		}
	}

	void DeleteMusicFile()
	{
		if (!g_launcher.music_file.empty())
		{
			DeleteFileW(g_launcher.music_file.c_str());
			g_launcher.music_file.clear();
		}
	}

	// ------------------------------------------------------------------------------------------
	// Art
	// ------------------------------------------------------------------------------------------

	// Loads an image (JPEG/PNG) from the exe's RCDATA resources.
	std::unique_ptr<Gdiplus::Bitmap> LoadImageResource(const int id)
	{
		const void* bytes;
		DWORD size;
		if (!GetResourceBytes(id, &bytes, &size))
		{
			return nullptr;
		}
		IStream* stream = SHCreateMemStream(static_cast<const BYTE*>(bytes), size);
		if (!stream)
		{
			return nullptr;
		}
		// the bitmap reads from the stream while it lives: copy it so the stream can go
		std::unique_ptr<Gdiplus::Bitmap> from_stream(Gdiplus::Bitmap::FromStream(stream));
		std::unique_ptr<Gdiplus::Bitmap> copy;
		if (from_stream && from_stream->GetLastStatus() == Gdiplus::Ok)
		{
			copy.reset(from_stream->Clone(0, 0, from_stream->GetWidth(), from_stream->GetHeight(), PixelFormat32bppPARGB));
		}
		from_stream.reset();
		stream->Release();
		return copy;
	}

	void LoadArt()
	{
		g_launcher.bg[static_cast<int>(Bg::Idle)] = LoadImageResource(IDR_BG_IDLE);
		g_launcher.bg[static_cast<int>(Bg::Single)] = LoadImageResource(IDR_BG_SP);
		g_launcher.bg[static_cast<int>(Bg::Multi)] = LoadImageResource(IDR_BG_MP);
	}

	void FreeArt()
	{
		for (auto& bg : g_launcher.bg)
		{
			bg.reset();
		}
	}

	// ------------------------------------------------------------------------------------------
	// Settings (MovieDuels-Launcher.ini in Documents\My Games\MovieDuels, with the game's configs
	// and saves: a game installed under Program Files can't write next to its exe)
	// ------------------------------------------------------------------------------------------

	std::wstring IniPath()
	{
		static std::wstring path;
		if (path.empty())
		{
			wchar_t docs[MAX_PATH] = {};
			if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs)))
			{
				std::wstring dir = std::wstring(docs) + L"\\My Games";
				CreateDirectoryW(dir.c_str(), nullptr);
				dir += L"\\" + std::wstring(kHomeFolder);
				CreateDirectoryW(dir.c_str(), nullptr);
				path = dir + L"\\" + kIniName;

				// the first time, take over the options from an ini next to the launcher (older launchers)
				const std::wstring old_ini = g_launcher.folder + L"\\" + kIniName;
				if (!PathFileExistsW(path.c_str()) && PathFileExistsW(old_ini.c_str()))
				{
					CopyFileW(old_ini.c_str(), path.c_str(), TRUE);
				}
			}
			else
			{
				path = g_launcher.folder + L"\\" + kIniName; // no Documents folder: next to the launcher
			}
		}
		return path;
	}

	void LoadSettings()
	{
		const std::wstring ini = IniPath();
		g_launcher.settings.rend2 = GetPrivateProfileIntW(L"Options", L"Rend2", 0, ini.c_str()) != 0;
		g_launcher.settings.windowed = GetPrivateProfileIntW(L"Options", L"Windowed", 0, ini.c_str()) != 0;
		g_launcher.settings.master_animations = GetPrivateProfileIntW(L"Options", L"MasterAnimations", 0, ini.c_str()) != 0;
		g_launcher.settings.music = GetPrivateProfileIntW(L"Options", L"Music", 1, ini.c_str()) != 0;
	}

	void SaveSettings()
	{
		const std::wstring ini = IniPath();
		WritePrivateProfileStringW(L"Options", L"Rend2", g_launcher.settings.rend2 ? L"1" : L"0", ini.c_str());
		WritePrivateProfileStringW(L"Options", L"Windowed", g_launcher.settings.windowed ? L"1" : L"0", ini.c_str());
		WritePrivateProfileStringW(L"Options", L"MasterAnimations", g_launcher.settings.master_animations ? L"1" : L"0", ini.c_str());
		WritePrivateProfileStringW(L"Options", L"Music", g_launcher.settings.music ? L"1" : L"0", ini.c_str());
	}

	// ------------------------------------------------------------------------------------------
	// Layout (window pixels; the bars are part of the art, these are their label areas)
	// ------------------------------------------------------------------------------------------

	struct Label
	{
		RECT hit;       // clickable area
		float cx, cy;   // text centre
		float size;     // font size in pixels
		const wchar_t* text;
	};

	// where the mockup had its labels (HOME, OPTIONS, ABOUT -> DISCORD, HELP, the two modes)
	const Label kLabels[] = {
		{ { 40, 54, 176, 83 }, 115.0f, 68.0f, 21.0f, L"HOME" },
		{ { 176, 54, 322, 83 }, 248.5f, 68.0f, 21.0f, L"OPTIONS" },
		{ { 763, 54, 894, 83 }, 821.0f, 69.0f, 21.0f, L"DISCORD" },
		{ { 894, 54, 1052, 83 }, 967.0f, 69.0f, 21.0f, L"HELP" },
		{ { 140, 538, 505, 571 }, 343.5f, 555.0f, 20.0f, L"SINGLEPLAYER MODE" },
		{ { 582, 538, 950, 571 }, 746.5f, 555.0f, 20.0f, L"MULTIPLAYER MODE" },
	};

	// options panel over the middle of the art
	constexpr int kPanelW = 460;
	constexpr int kPanelH = 280;
	constexpr int kPanelX = (kWidth - kPanelW) / 2;
	constexpr int kPanelY = 150;
	constexpr int kOptButtonW = 360;
	constexpr int kOptButtonH = 38;
	constexpr int kOptButtonTop = kPanelY + 72;
	constexpr int kOptButtonGap = 10;

	RECT OptionRect(const int index)
	{
		const int x = kPanelX + (kPanelW - kOptButtonW) / 2;
		const int y = kOptButtonTop + index * (kOptButtonH + kOptButtonGap);
		return RECT{ x, y, x + kOptButtonW, y + kOptButtonH };
	}

	RECT MusicRect() { return RECT{ kWidth - 106, 12, kWidth - 84, 34 }; }
	RECT MinimiseRect() { return RECT{ kWidth - 78, 12, kWidth - 56, 34 }; }
	RECT CloseRect() { return RECT{ kWidth - 50, 12, kWidth - 28, 34 }; }

	int HitTest(const POINT pt)
	{
		const RECT music_rect = MusicRect();
		if (PtInRect(&music_rect, pt))
		{
			return kHitMusic;
		}
		const RECT min_rect = MinimiseRect();
		if (PtInRect(&min_rect, pt))
		{
			return kHitMinimise;
		}
		const RECT close_rect = CloseRect();
		if (PtInRect(&close_rect, pt))
		{
			return kHitClose;
		}
		if (g_launcher.options_open)
		{
			for (int i = 0; i < 4; i++)
			{
				const RECT r = OptionRect(i);
				if (PtInRect(&r, pt))
				{
					return kHitOpt0 + i;
				}
			}
			return kHitNone; // the rest of the window is covered by the panel
		}
		for (int i = 0; i < static_cast<int>(sizeof(kLabels) / sizeof(kLabels[0])); i++)
		{
			if (PtInRect(&kLabels[i].hit, pt))
			{
				return kHitHome + i;
			}
		}
		return kHitNone;
	}

	std::wstring OptionLabel(const int index)
	{
		const Settings& s = g_launcher.settings;
		switch (index)
		{
		case 0: return s.rend2 ? L"RENDERER: REND2" : L"RENDERER: VANILLA";
		case 1: return s.windowed ? L"DISPLAY: WINDOWED" : L"DISPLAY: FULLSCREEN";
		case 2: return s.master_animations ? L"ANIMATIONS: MASTER" : L"ANIMATIONS: CLASSIC";
		default: return L"BACK";
		}
	}

	// which background the current hover asks for
	Bg WantedBackground()
	{
		if (!g_launcher.options_open)
		{
			if (g_launcher.hover == kHitSingle)
			{
				return Bg::Single;
			}
			if (g_launcher.hover == kHitMulti)
			{
				return Bg::Multi;
			}
		}
		return Bg::Idle;
	}

	// ------------------------------------------------------------------------------------------
	// Drawing
	// ------------------------------------------------------------------------------------------

	void AddRoundRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, const Gdiplus::REAL radius)
	{
		const Gdiplus::REAL d = radius * 2;
		path.AddArc(r.X, r.Y, d, d, 180, 90);
		path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
		path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
		path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
		path.CloseFigure();
	}

	const Gdiplus::FontFamily* LabelFamily()
	{
		// a Trajan-like serif, close to the mockup's lettering; Palatino Linotype ships with Windows
		static Gdiplus::FontFamily palatino(L"Palatino Linotype");
		return palatino.IsAvailable() ? &palatino : Gdiplus::FontFamily::GenericSerif();
	}

	// A label centred on (cx, cy): a soft shadow, and when lit a gold glow around gold letters.
	void DrawLabel(Gdiplus::Graphics& g, const wchar_t* text, const float cx, const float cy, const float size,
		const bool lit, const bool down)
	{
		Gdiplus::StringFormat fmt;
		fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
		fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		const float shift = down ? 1.0f : 0.0f;
		const Gdiplus::RectF box(cx - 250.0f, cy - 30.0f + shift, 500.0f, 60.0f);

		Gdiplus::GraphicsPath path;
		path.AddString(text, -1, LabelFamily(), Gdiplus::FontStyleRegular, size, box, &fmt);

		if (lit)
		{
			for (const float width : { 9.0f, 5.5f, 3.0f })
			{
				Gdiplus::Pen glow(Gdiplus::Color(40, 255, 170, 40), width);
				glow.SetLineJoin(Gdiplus::LineJoinRound);
				g.DrawPath(&glow, &path);
			}
		}
		else
		{
			Gdiplus::Matrix offset;
			offset.Translate(1.0f, 1.5f);
			std::unique_ptr<Gdiplus::GraphicsPath> shadow(path.Clone());
			shadow->Transform(&offset);
			const Gdiplus::SolidBrush shadow_brush(Gdiplus::Color(170, 0, 0, 0));
			g.FillPath(&shadow_brush, shadow.get());
		}
		const Gdiplus::SolidBrush fill(lit ? kGoldText : kWhiteText);
		g.FillPath(&fill, &path);
	}

	// A bar like the art's (dark fill, thin light-grey edge), used for the options panel buttons.
	void DrawBarButton(Gdiplus::Graphics& g, const RECT& r, const std::wstring& label, const bool lit, const bool down)
	{
		const Gdiplus::RectF box(static_cast<Gdiplus::REAL>(r.left) + 0.5f, static_cast<Gdiplus::REAL>(r.top) + 0.5f,
			static_cast<Gdiplus::REAL>(r.right - r.left) - 1.0f, static_cast<Gdiplus::REAL>(r.bottom - r.top) - 1.0f);
		Gdiplus::GraphicsPath path;
		AddRoundRect(path, box, 9.0f);
		const Gdiplus::SolidBrush fill(lit ? Gdiplus::Color(235, 40, 32, 18) : Gdiplus::Color(235, 24, 24, 24));
		g.FillPath(&fill, &path);
		Gdiplus::Pen edge(lit ? Gdiplus::Color(255, 220, 160, 60) : Gdiplus::Color(255, 138, 138, 138), 1.5f);
		g.DrawPath(&edge, &path);
		DrawLabel(g, label.c_str(), box.X + box.Width / 2, box.Y + box.Height / 2 + 1.0f, 19.0f, lit, down);
	}

	void DrawOptionsPanel(Gdiplus::Graphics& g)
	{
		// dim the art behind it
		const Gdiplus::SolidBrush dim(Gdiplus::Color(110, 0, 0, 0));
		g.FillRectangle(&dim, 0, 0, kWidth, kHeight);

		Gdiplus::GraphicsPath panel;
		AddRoundRect(panel, Gdiplus::RectF(static_cast<Gdiplus::REAL>(kPanelX), static_cast<Gdiplus::REAL>(kPanelY),
			static_cast<Gdiplus::REAL>(kPanelW), static_cast<Gdiplus::REAL>(kPanelH)), 18.0f);
		const Gdiplus::SolidBrush fill(Gdiplus::Color(238, 14, 14, 14));
		g.FillPath(&fill, &panel);
		Gdiplus::Pen border(kGold, 2.0f);
		g.DrawPath(&border, &panel);

		DrawLabel(g, L"OPTIONS", kWidth / 2.0f, kPanelY + 38.0f, 26.0f, true, false);

		for (int i = 0; i < 4; i++)
		{
			const bool lit = g_launcher.hover == kHitOpt0 + i;
			DrawBarButton(g, OptionRect(i), OptionLabel(i), lit, lit && g_launcher.pressed == kHitOpt0 + i);
		}
	}

	void DrawWindowButtons(Gdiplus::Graphics& g)
	{
		const Gdiplus::Color gold(230, 253, 178, 42);
		const Gdiplus::Color bright(255, 255, 214, 120);
		const Gdiplus::SolidBrush back(Gdiplus::Color(150, 0, 0, 0));

		// speaker: waves when the music is on, a cross when it is off
		const RECT s = MusicRect();
		const Gdiplus::REAL sx = static_cast<Gdiplus::REAL>(s.left);
		const Gdiplus::REAL sy = static_cast<Gdiplus::REAL>(s.top);
		const Gdiplus::REAL sw = static_cast<Gdiplus::REAL>(s.right - s.left);
		Gdiplus::Pen music_pen(g_launcher.hover == kHitMusic ? bright : gold, 2);
		g.FillEllipse(&back, sx, sy, sw, sw);
		g.DrawEllipse(&music_pen, sx, sy, sw, sw);
		const Gdiplus::PointF speaker[] = {
			{ sx + 5.5f, sy + 9.0f }, { sx + 8.5f, sy + 9.0f }, { sx + 12.0f, sy + 6.0f },
			{ sx + 12.0f, sy + 16.0f }, { sx + 8.5f, sy + 13.0f }, { sx + 5.5f, sy + 13.0f } };
		Gdiplus::Pen thin_pen(g_launcher.hover == kHitMusic ? bright : gold, 1.5f);
		g.DrawPolygon(&thin_pen, speaker, 6);
		if (g_launcher.settings.music)
		{
			g.DrawArc(&thin_pen, sx + 10.0f, sy + 8.0f, 6.0f, 6.0f, -60.0f, 120.0f);
			g.DrawArc(&thin_pen, sx + 9.0f, sy + 5.5f, 9.5f, 11.0f, -60.0f, 120.0f);
		}
		else
		{
			g.DrawLine(&thin_pen, sx + 14.0f, sy + 9.0f, sx + 18.0f, sy + 13.0f);
			g.DrawLine(&thin_pen, sx + 18.0f, sy + 9.0f, sx + 14.0f, sy + 13.0f);
		}

		const RECT m = MinimiseRect();
		const Gdiplus::REAL mx = static_cast<Gdiplus::REAL>(m.left);
		const Gdiplus::REAL my = static_cast<Gdiplus::REAL>(m.top);
		Gdiplus::Pen min_pen(g_launcher.hover == kHitMinimise ? bright : gold, 2);
		g.FillEllipse(&back, mx, my, sw, sw);
		g.DrawEllipse(&min_pen, mx, my, sw, sw);
		g.DrawLine(&min_pen, mx + 6.0f, my + 11.0f, mx + 16.0f, my + 11.0f);

		const RECT c = CloseRect();
		const Gdiplus::REAL cx = static_cast<Gdiplus::REAL>(c.left);
		const Gdiplus::REAL cy = static_cast<Gdiplus::REAL>(c.top);
		Gdiplus::Pen close_pen(g_launcher.hover == kHitClose ? bright : gold, 2);
		g.FillEllipse(&back, cx, cy, sw, sw);
		g.DrawEllipse(&close_pen, cx, cy, sw, sw);
		g.DrawLine(&close_pen, cx + 6.5f, cy + 6.5f, cx + 15.5f, cy + 15.5f);
		g.DrawLine(&close_pen, cx + 15.5f, cy + 6.5f, cx + 6.5f, cy + 15.5f);
	}

	// Draws the background: the crossfade between the last and the wanted background.
	void DrawBackground(Gdiplus::Graphics& g)
	{
		Gdiplus::Bitmap* from = g_launcher.bg[static_cast<int>(g_launcher.bg_from)].get();
		Gdiplus::Bitmap* to = g_launcher.bg[static_cast<int>(g_launcher.bg_to)].get();
		if (!to)
		{
			g.Clear(Gdiplus::Color(255, 20, 20, 20));
			return;
		}
		float t = 1.0f;
		if (g_launcher.fading)
		{
			t = static_cast<float>(GetTickCount() - g_launcher.fade_start) / kFadeMs;
			if (t >= 1.0f)
			{
				t = 1.0f;
				g_launcher.fading = false;
				KillTimer(g_launcher.hwnd, kFadeTimer);
			}
		}
		if (from && t < 1.0f)
		{
			g.DrawImage(from, 0, 0, kWidth, kHeight);
			Gdiplus::ColorMatrix cm = {
				1, 0, 0, 0, 0,
				0, 1, 0, 0, 0,
				0, 0, 1, 0, 0,
				0, 0, 0, t, 0,
				0, 0, 0, 0, 1 };
			Gdiplus::ImageAttributes attributes;
			attributes.SetColorMatrix(&cm);
			g.DrawImage(to, Gdiplus::Rect(0, 0, kWidth, kHeight), 0, 0, kWidth, kHeight, Gdiplus::UnitPixel, &attributes);
		}
		else
		{
			g.DrawImage(to, 0, 0, kWidth, kHeight);
		}
	}

	// Renders the whole window into the DIB and hands it to Windows (layered window, per-pixel alpha).
	void Render()
	{
		if (!g_launcher.bits)
		{
			return;
		}
		{
			// the scene, square-cornered
			Gdiplus::Bitmap scene(kWidth, kHeight, PixelFormat32bppPARGB);
			{
				Gdiplus::Graphics g(&scene);
				g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
				g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
				g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
				g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

				DrawBackground(g);

				for (int i = 0; i < static_cast<int>(sizeof(kLabels) / sizeof(kLabels[0])); i++)
				{
					const bool lit = !g_launcher.options_open && g_launcher.hover == kHitHome + i;
					DrawLabel(g, kLabels[i].text, kLabels[i].cx, kLabels[i].cy, kLabels[i].size, lit,
						lit && g_launcher.pressed == kHitHome + i);
				}

				Gdiplus::FontFamily ui_family(L"Segoe UI");
				const Gdiplus::Font version_font(ui_family.IsAvailable() ? &ui_family : Gdiplus::FontFamily::GenericSansSerif(), 11, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
				const Gdiplus::SolidBrush grey(Gdiplus::Color(150, 200, 200, 200));
				g.DrawString(kVersionText, -1, &version_font, Gdiplus::PointF(26, kHeight - 30), &grey);

				if (g_launcher.options_open)
				{
					DrawOptionsPanel(g);
				}
				DrawWindowButtons(g);
			}

			// the window: the scene in a rounded shape (smooth edges), with the gold border on top
			Gdiplus::Bitmap canvas(kWidth, kHeight, kWidth * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(g_launcher.bits));
			Gdiplus::Graphics g(&canvas);
			g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
			g.Clear(Gdiplus::Color(0, 0, 0, 0));
			Gdiplus::GraphicsPath shape;
			AddRoundRect(shape, Gdiplus::RectF(0.5f, 0.5f, kWidth - 1.0f, kHeight - 1.0f), kCornerRadius);
			const Gdiplus::TextureBrush scene_brush(&scene);
			g.FillPath(&scene_brush, &shape);
			Gdiplus::GraphicsPath edge;
			AddRoundRect(edge, Gdiplus::RectF(1.25f, 1.25f, kWidth - 2.5f, kHeight - 2.5f), kCornerRadius - 0.75f);
			Gdiplus::Pen border(kGold, 2.5f);
			g.DrawPath(&border, &edge);
		}

		const HDC screen = GetDC(nullptr);
		POINT src = { 0, 0 };
		SIZE size = { kWidth, kHeight };
		BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
		UpdateLayeredWindow(g_launcher.hwnd, screen, nullptr, &size, g_launcher.mem_dc, &src, 0, &blend, ULW_ALPHA);
		ReleaseDC(nullptr, screen);
	}

	bool CreateSurface()
	{
		BITMAPINFO bi = {};
		bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth = kWidth;
		bi.bmiHeader.biHeight = -kHeight; // top-down
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		const HDC screen = GetDC(nullptr);
		g_launcher.mem_dc = CreateCompatibleDC(screen);
		g_launcher.dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_launcher.bits, nullptr, 0);
		ReleaseDC(nullptr, screen);
		if (!g_launcher.mem_dc || !g_launcher.dib)
		{
			return false;
		}
		g_launcher.old_bitmap = SelectObject(g_launcher.mem_dc, g_launcher.dib);
		return true;
	}

	void FreeSurface()
	{
		if (g_launcher.mem_dc)
		{
			SelectObject(g_launcher.mem_dc, g_launcher.old_bitmap);
			DeleteDC(g_launcher.mem_dc);
			g_launcher.mem_dc = nullptr;
		}
		if (g_launcher.dib)
		{
			DeleteObject(g_launcher.dib);
			g_launcher.dib = nullptr;
		}
		g_launcher.bits = nullptr;
	}

	// Starts a crossfade when the hover asks for another background.
	void UpdateBackground()
	{
		const Bg wanted = WantedBackground();
		if (wanted == g_launcher.bg_to)
		{
			return;
		}
		g_launcher.bg_from = g_launcher.bg_to;
		g_launcher.bg_to = wanted;
		g_launcher.fade_start = GetTickCount();
		g_launcher.fading = true;
		SetTimer(g_launcher.hwnd, kFadeTimer, 15, nullptr);
	}

	// ------------------------------------------------------------------------------------------
	// Actions
	// ------------------------------------------------------------------------------------------

	bool StartGame(const HWND hwnd, const bool multiplayer)
	{
		const Settings& s = g_launcher.settings;
		const std::wstring exe = g_launcher.folder + L"\\" + (multiplayer ? kMPExe : kSPExe);
		if (!PathFileExistsW(exe.c_str()))
		{
			const std::wstring msg = L"Could not find " + exe + L"\n\nThe launcher must be in the MovieDuels game folder.";
			MessageBoxW(hwnd, msg.c_str(), L"MovieDuels", MB_OK | MB_ICONERROR);
			return false;
		}

		// "+set" on the command line overrides the values in the configs
		std::wstring cmd = L"\"" + exe + L"\"";
		const wchar_t* renderer = multiplayer
			? (s.rend2 ? L"MovieDuels-rdmp-rend2" : L"MovieDuels-rdmp")
			: (s.rend2 ? L"MovieDuels-rdsp-rend2" : L"MovieDuels-rdsp");
		cmd += L" +set cl_renderer ";
		cmd += renderer;
		cmd += s.rend2 ? L" +set com_rend2 1" : L" +set com_rend2 0";
		cmd += s.windowed ? L" +set r_fullscreen 0" : L" +set r_fullscreen 1";
		if (!multiplayer)
		{
			cmd += s.master_animations ? L" +set g_ActivateAnimationStyle 1" : L" +set g_ActivateAnimationStyle 0";
		}

		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = {};
		std::wstring cmd_buffer = cmd; // CreateProcessW may write into it
		if (!CreateProcessW(exe.c_str(), cmd_buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, g_launcher.folder.c_str(), &si, &pi))
		{
			MessageBoxW(hwnd, (L"Could not start " + exe).c_str(), L"MovieDuels", MB_OK | MB_ICONERROR);
			return false;
		}
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return true;
	}

	void OpenLink(const HWND hwnd, const wchar_t* url)
	{
		PlaySfx(IDR_SND_CLICK);
		ShellExecuteW(hwnd, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
	}

	void SetOptionsOpen(const bool open)
	{
		g_launcher.options_open = open;
		g_launcher.hover = kHitNone;
		UpdateBackground();
	}

	void OnClick(const HWND hwnd, const int hit)
	{
		switch (hit)
		{
		case kHitSingle:
		case kHitMulti:
			PlaySfx(IDR_SND_CLICK, true); // the launcher closes right after
			if (StartGame(hwnd, hit == kHitMulti))
			{
				DestroyWindow(hwnd);
			}
			return;
		case kHitOptions:
			PlaySfx(IDR_SND_CLICK);
			SetOptionsOpen(true);
			break;
		case kHitHome:
			OpenLink(hwnd, kUrlHome);
			break;
		case kHitDiscord:
			OpenLink(hwnd, kUrlDiscord);
			break;
		case kHitHelp:
			OpenLink(hwnd, kUrlHelp);
			break;
		case kHitOpt0:
			PlaySfx(IDR_SND_CLICK);
			g_launcher.settings.rend2 = !g_launcher.settings.rend2;
			SaveSettings();
			break;
		case kHitOpt1:
			PlaySfx(IDR_SND_CLICK);
			g_launcher.settings.windowed = !g_launcher.settings.windowed;
			SaveSettings();
			break;
		case kHitOpt2:
			PlaySfx(IDR_SND_CLICK);
			g_launcher.settings.master_animations = !g_launcher.settings.master_animations;
			SaveSettings();
			break;
		case kHitOptBack:
			PlaySfx(IDR_SND_BACK);
			SetOptionsOpen(false);
			break;
		case kHitClose:
			PlaySfx(IDR_SND_BACK, true);
			DestroyWindow(hwnd);
			return;
		case kHitMinimise:
			PlaySfx(IDR_SND_CLICK);
			ShowWindow(hwnd, SW_MINIMIZE);
			break;
		case kHitMusic:
			PlaySfx(IDR_SND_CLICK);
			g_launcher.settings.music = !g_launcher.settings.music;
			SaveSettings();
			if (g_launcher.settings.music)
			{
				StartMusic(hwnd);
			}
			else
			{
				StopMusic();
			}
			break;
		default:
			return;
		}
		Render();
	}

	// ------------------------------------------------------------------------------------------
	// Window
	// ------------------------------------------------------------------------------------------

	bool IsMenuHit(const int hit)
	{
		return hit >= kHitHome && hit <= kHitOptBack;
	}

	void SetHover(const int hit)
	{
		if (hit != g_launcher.hover)
		{
			if (IsMenuHit(hit))
			{
				PlaySfx(IDR_SND_HOVER); // moved onto another menu button
			}
			g_launcher.hover = hit;
			UpdateBackground();
			Render();
		}
	}

	LRESULT CALLBACK WndProc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam)
	{
		switch (msg)
		{
		case WM_NCHITTEST:
		{
			// drag the window by anything that is not a button
			POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			ScreenToClient(hwnd, &pt);
			return HitTest(pt) == kHitNone ? HTCAPTION : HTCLIENT;
		}
		case WM_MOUSEMOVE:
			if (!g_launcher.tracking_mouse)
			{
				TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
				g_launcher.tracking_mouse = TrackMouseEvent(&tme) != FALSE;
			}
			SetHover(HitTest(POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }));
			return 0;
		case WM_MOUSELEAVE:
		case WM_NCMOUSEMOVE:
			g_launcher.tracking_mouse = false;
			g_launcher.pressed = kHitNone;
			SetHover(kHitNone);
			return 0;
		case WM_LBUTTONDOWN:
			g_launcher.pressed = HitTest(POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			SetCapture(hwnd);
			Render();
			return 0;
		case WM_LBUTTONUP:
		{
			ReleaseCapture();
			const int hit = HitTest(POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			const int pressed = g_launcher.pressed;
			g_launcher.pressed = kHitNone;
			if (hit != pressed || hit == kHitNone)
			{
				Render(); // released somewhere else
				return 0;
			}
			OnClick(hwnd, hit);
			return 0;
		}
		case WM_KEYDOWN:
			if (wparam == VK_ESCAPE)
			{
				if (g_launcher.options_open)
				{
					PlaySfx(IDR_SND_BACK);
					SetOptionsOpen(false);
					Render();
				}
				else
				{
					PlaySfx(IDR_SND_BACK, true);
					DestroyWindow(hwnd);
				}
			}
			return 0;
		case WM_TIMER:
			if (wparam == kFadeTimer)
			{
				Render();
			}
			return 0;
		case MM_MCINOTIFY:
			// the music reached its end: play it again (loop)
			if (wparam == MCI_NOTIFY_SUCCESSFUL && g_launcher.music_open)
			{
				PlayMusicFromStart(hwnd);
			}
			return 0;
		case WM_DESTROY:
			KillTimer(hwnd, kFadeTimer);
			StopMusic();
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProcW(hwnd, msg, wparam, lparam);
		}
	}
}

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, const int show)
{
	g_launcher.instance = instance;

	wchar_t path[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	PathRemoveFileSpecW(path);
	g_launcher.folder = path;
	LoadSettings();

	Gdiplus::GdiplusStartupInput gdiplus_input;
	ULONG_PTR gdiplus_token = 0;
	Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr);
	LoadArt();

	WNDCLASSEXW wc = { sizeof(wc) };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = instance;
	wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_LAUNCHER));
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.lpszClassName = L"MovieDuelsLauncher";
	RegisterClassExW(&wc);

	// centred on the work area, borderless and layered (the art has its own frame and window buttons)
	RECT work = {};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
	const int x = work.left + (work.right - work.left - kWidth) / 2;
	const int y = work.top + (work.bottom - work.top - kHeight) / 2;
	g_launcher.hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_APPWINDOW, wc.lpszClassName, L"MovieDuels",
		WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, x, y, kWidth, kHeight, nullptr, nullptr, instance, nullptr);
	if (!g_launcher.hwnd || !CreateSurface())
	{
		FreeSurface();
		FreeArt();
		Gdiplus::GdiplusShutdown(gdiplus_token);
		return 1;
	}
	Render();
	ShowWindow(g_launcher.hwnd, show);
	StartMusic(g_launcher.hwnd);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	DeleteMusicFile();
	FreeSurface();
	FreeArt(); // before GDI+ goes
	Gdiplus::GdiplusShutdown(gdiplus_token);
	return 0;
}