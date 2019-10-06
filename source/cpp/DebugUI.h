#pragma once
#ifdef DEBUG_UI
#define DEBUGUI(x) x
#define DEBUGUI_PROCESSINPUT(x) x
#else
#define DEBUGUI(x)
#define DEBUGUI_PROCESSINPUT(x) false
#endif
