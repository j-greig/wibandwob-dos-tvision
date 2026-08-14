// Command IDs for menus, status line and handleEvent dispatch.
// Single source — extracted from wwdos_app.cpp (monolith split stage 0).
// Range blocks: cmFigletCatFontBase spans 200 ids; cmSkinBase spans 310..319.
#pragma once

#define Uses_TEvent
#include <tvision/tv.h>

const ushort cmNewWindow = 100;
const ushort cmNewGradientH = 102;
const ushort cmNewGradientV = 103;
const ushort cmNewGradientR = 104;
const ushort cmNewGradientD = 105;
const ushort cmNewDonut = 108;
const ushort cmOpenAnimation = 109;
const ushort cmSaveWorkspace = 110;
const ushort cmNewMechs = 111;
const ushort cmOpenWorkspace = 115;
const ushort cmSaveWorkspaceAs = 120;
const ushort cmManageWorkspaces = 280;
const ushort cmRecentWorkspace = 275;  // 275..279
// Future File commands
const ushort cmOpenAnsiArt = 112;
const ushort cmNewPaintCanvas = 113;
const ushort cmNewFigletText = 119;
const ushort cmOpenImageFile = 114;

// Window menu commands
const ushort cmOpenTransparentText = 116;
const ushort cmOpenMonodraw = 118;

// Edit menu commands
const ushort cmScreenshot = 101;
const ushort cmPatternContinuous = 106;
const ushort cmPatternTiled = 107;
// Edit menu commands
const ushort cmSettings = 117;

// View menu commands
const ushort cmZoomIn = 121;
const ushort cmZoomOut = 122;
const ushort cmActualSize = 123;
const ushort cmFullScreen = 124;
const ushort cmTextEditor = 130;
const ushort cmAsciiGridDemo = 132;
const ushort cmAnimatedBlocks = 134;
const ushort cmAnimatedGradient = 135;
const ushort cmAnimatedScore = 136;
const ushort cmScoreBgColor = 137;
const ushort cmWindowBgColor = 139;
const ushort cmVerseField = 138;
const ushort cmOrbitField = 150;
const ushort cmMyceliumField = 151;
const ushort cmTorusField = 152;
const ushort cmCubeField = 153;
const ushort cmMonsterPortal = 154;
const ushort cmMonsterVerse = 155;
const ushort cmMonsterCam   = 156;
const ushort cmASCIICam     = 157;

// Tools menu commands (future)
const ushort cmAnsiEditor = 125;
const ushort cmPaintTools = 126;
const ushort cmAnimationStudio = 127;
const ushort cmQuantumPrinter = 128;
const ushort cmWibWobChat = 131;
const ushort cmSendToBack = 133;
const ushort cmWibWobTestA = 148;  // Scrollbar test: standardScrollBar fix
const ushort cmWibWobTestB = 149;  // Scrollbar test: TScroller refactor
const ushort cmWibWobTestC = 160;  // Scrollbar test: Split view architecture
const ushort cmRepaint = 161;      // Force repaint
const ushort cmBrowser = 170;      // Browser window
const ushort cmApiKey = 171;       // API key entry dialog
// cmScrambleToggle (180) defined in scramble_view.h
const ushort cmScrambleCat = cmScrambleToggle;  // alias for menu/IPC

// Help menu commands
const ushort cmAbout = 129;
const ushort cmKeyboardShortcuts = 210;
const ushort cmDebugInfo = 211;
const ushort cmApiKeyHelp = 212;
const ushort cmLlmStatus = 230;
const ushort cmMicropolisAscii = 213;
const ushort cmQuadra = 215;
const ushort cmSnake = 216;
const ushort cmRogue = 217;
const ushort cmRogueHackTerminal = 218;
const ushort cmDeepSignal = 219;
const ushort cmDeepSignalTerminal = 220;
const ushort cmOpenTerminal = 214;
const ushort cmAppLauncher = 232;    // Applications folder browser
const ushort cmScrambleReply = 233;  // Async Scramble LLM response ready
const ushort cmAsciiGallery = 234;   // ASCII Art Gallery browser
const ushort cmDiskLibrary = 301;    // SYMBIENT SHAREWARE LIBRARY floppy launcher
const ushort cmTweetShader = 302;    // MONO.SHDR tsubuyaki-GLSL port
const ushort cmSkinBase = 310;       // 310..318: skins menu (order matches kMenuSkinNames)
const ushort cmBackroomsTv = 284;    // Backrooms TV live art window

// Glitch menu commands
const ushort cmToggleGlitchMode = 140;
const ushort cmGlitchScatter = 141;
const ushort cmGlitchColorBleed = 142;
const ushort cmGlitchRadialDistort = 143;
const ushort cmGlitchDiagonalScatter = 144;
const ushort cmCaptureGlitchedFrame = 145;
const ushort cmResetGlitchParams = 146;
const ushort cmGlitchSettings = 147;

// Right-click context menu commands
// cmApiKeyChanged (186) and cmNoOp (999) defined in room_chat_view.h
const ushort cmCtxToggleShadow = 250;
const ushort cmCtxClearTitle = 251;
const ushort cmCtxToggleFrame = 252;
const ushort cmCtxGalleryToggle = 253;

// Desktop right-click preset commands
const ushort cmDeskPresetBase = 260;  // 260..268 for up to 9 presets
const ushort cmDeskGallery = 270;
