import rawMainCpp from '../native/PepperCut.cpp?raw';
import rawShellCpp from '../native/PepperCutShell.cpp?raw';
import rawCommonH from '../native/PepperCutCommon.h?raw';

export const PEPPERCUT_CPP_SOURCE: string =
  `// ============================================================================\n` +
  `// [1/3] PepperCut.cpp (Main Standalone Executable + Tray Daemon)\n` +
  `// ============================================================================\n\n` +
  rawMainCpp +
  `\n\n// ============================================================================\n` +
  `// [2/3] PepperCutShell.cpp (Embedded COM IContextMenu & IShellExtInit Handler)\n` +
  `// ============================================================================\n\n` +
  rawShellCpp +
  `\n\n// ============================================================================\n` +
  `// [3/3] PepperCutCommon.h (Shared Lock Detection & Menu Bitmap Renderer)\n` +
  `// ============================================================================\n\n` +
  rawCommonH;
