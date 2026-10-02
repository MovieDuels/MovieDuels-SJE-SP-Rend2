# MovieDuels launcher (Windows only), from the team's design: HOME, OPTIONS, DISCORD, HELP and
# SINGLEPLAYER MODE / MULTIPLAYER MODE, starting MovieDuels-SP.x86_64.exe or MovieDuels-MP.x86_64.exe
# from its own folder. The backgrounds in art/ are built by MD-work/launcher2/LauncherArt.cs from
# the mockup (checkerboard removed, labels erased; the launcher draws the labels).
# The sounds in sound/ are the in-game menu's own (MD_Assets.pk3: sound/interface/button1.wav,
# menuroam.wav, esc.wav) and the main menu music (music/mainMenu/main/CTCMusic.mp3).

set(LauncherName "MovieDuels-Launcher")

set(LauncherFiles
	"${SPDir}/launcher/md_launcher.cpp"
	"${SPDir}/launcher/md_launcher.rc"
	"${SPDir}/launcher/resource.h"
	)
source_group("launcher" FILES ${LauncherFiles})

add_executable(${LauncherName} WIN32 ${LauncherFiles})
target_compile_definitions(${LauncherName} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(${LauncherName} gdiplus shlwapi winmm shell32)
set_target_properties(${LauncherName} PROPERTIES PROJECT_LABEL "Launcher")

install(TARGETS ${LauncherName}
	RUNTIME
	DESTINATION ${JKAInstallDir}
	COMPONENT ${JKASPClientComponent})
