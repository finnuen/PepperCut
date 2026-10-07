import React, { useEffect, useRef, useState } from 'react';
import {
  Check,
  Copy,
  Download,
  Edit3,
  FileCode,
  FileText,
  Folder,
  Lock,
  Play,
  RefreshCw,
  Trash2,
  Unlock,
} from 'lucide-react';
import { REAL_PEPPERCUT_EXE_METADATA } from './peBuilder';
import { renderIconPixelsRGBA } from './icoBuilder';
import { PEPPERCUT_CPP_SOURCE } from './cppSource';

export function PepperCutIcon({ className = 'w-5 h-5' }: { className?: string }) {
  return (
    <svg
      viewBox="0 0 32 32"
      fill="none"
      xmlns="http://www.w3.org/2000/svg"
      className={`${className} shrink-0 select-none`}
      aria-label="PepperCut icon: white scissors on red background"
    >
      <rect width="32" height="32" rx="6" fill="#DC2626" />
      {/* White scissors blades */}
      <path
        d="M11.5 13.2L26.0 22.5"
        stroke="#FFFFFF"
        strokeWidth="2.6"
        strokeLinecap="round"
      />
      <path
        d="M11.5 18.8L26.0 9.5"
        stroke="#FFFFFF"
        strokeWidth="2.6"
        strokeLinecap="round"
      />
      {/* White scissors finger rings */}
      <circle cx="8.5" cy="11.0" r="3.6" stroke="#FFFFFF" strokeWidth="2.3" />
      <circle cx="8.5" cy="21.0" r="3.6" stroke="#FFFFFF" strokeWidth="2.3" />
      {/* Red pivot center */}
      <circle cx="16.2" cy="16.0" r="1.15" fill="#DC2626" />
    </svg>
  );
}

interface ExplorerItem {
  id: string;
  name: string;
  type: 'file' | 'folder';
  sizeText: string;
  modifiedText: string;
  lockedByProcess: string | null;
  lockedByPid: number | null;
  handleHex: string | null;
  bootDeleteScheduled: boolean;
  isImportantWindows?: boolean;
}

interface WinNotification {
  id: string;
  title: string;
  message: string;
  timestamp: string;
}

const INITIAL_ITEMS: ExplorerItem[] = [
  {
    id: 'item-1',
    name: 'Quarterly_Financial_Model_v4.xlsx',
    type: 'file',
    sizeText: '4,812 KB',
    modifiedText: '2026-10-06 19:14',
    lockedByProcess: 'EXCEL.EXE',
    lockedByPid: 4812,
    handleHex: '0x000004AC',
    bootDeleteScheduled: false,
  },
  {
    id: 'item-2',
    name: 'dist_build_cache',
    type: 'folder',
    sizeText: '142,900 KB',
    modifiedText: '2026-10-06 19:28',
    lockedByProcess: 'node.exe',
    lockedByPid: 9104,
    handleHex: '0x000008F0',
    bootDeleteScheduled: false,
  },
  {
    id: 'item-3',
    name: 'Client_Render_Master_4K.mp4',
    type: 'file',
    sizeText: '1,840,220 KB',
    modifiedText: '2026-10-06 18:55',
    lockedByProcess: 'AfterFX.exe',
    lockedByPid: 6620,
    handleHex: '0x00000C18',
    bootDeleteScheduled: true,
  },
  {
    id: 'item-4',
    name: 'C:\\Windows\\System32 (Important Windows Folder)',
    type: 'folder',
    sizeText: '4,920,110 KB',
    modifiedText: '2026-10-05 11:02',
    lockedByProcess: 'svchost.exe',
    lockedByPid: 1488,
    handleHex: '0x00000310',
    bootDeleteScheduled: false,
    isImportantWindows: true,
  },
  {
    id: 'item-5',
    name: 'Release_Notes_Draft.txt',
    type: 'file',
    sizeText: '18 KB',
    modifiedText: '2026-10-06 17:40',
    lockedByProcess: null,
    lockedByPid: null,
    handleHex: null,
    bootDeleteScheduled: false,
  },
  {
    id: 'item-6',
    name: 'Archived_Assets',
    type: 'folder',
    sizeText: '19,200 KB',
    modifiedText: '2026-10-04 09:15',
    lockedByProcess: null,
    lockedByPid: null,
    handleHex: null,
    bootDeleteScheduled: false,
  },
];

export default function App() {
  const inspection = REAL_PEPPERCUT_EXE_METADATA;

  const [activeTab, setActiveTab] = useState<'simulator' | 'binary' | 'source'>('simulator');
  const [downloadCount, setDownloadCount] = useState(0);
  const [copiedSource, setCopiedSource] = useState(false);
  const [hexDumpRows, setHexDumpRows] = useState<{ offset: string; hex: string; ascii: string }[]>([]);

  // Simulator state
  const [isTrayRunning, setIsTrayRunning] = useState(true);
  const [ignoreImportantWindows, setIgnoreImportantWindows] = useState(true);
  const [startOnBootReg, setStartOnBootReg] = useState(true);
  const [items, setItems] = useState<ExplorerItem[]>(INITIAL_ITEMS);
  const [selectedItemId, setSelectedItemId] = useState<string>('item-1');

  const [contextMenu, setContextMenu] = useState<{
    itemId: string;
    x: number;
    y: number;
  } | null>(null);

  const [trayMenuOpen, setTrayMenuOpen] = useState(false);
  const [renamingId, setRenamingId] = useState<string | null>(null);
  const [renameValue, setRenameValue] = useState('');
  const [explorerError, setExplorerError] = useState<string | null>(null);

  const [notifications, setNotifications] = useState<WinNotification[]>([
    {
      id: 'init-notif',
      title: 'PepperCut',
      message: 'PepperCut is running',
      timestamp: 'Just now',
    },
  ]);

  const canvasRef = useRef<HTMLCanvasElement | null>(null);

  // Render 64x64 preview of embedded RT_ICON onto canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const rgba = renderIconPixelsRGBA(64);
    const imgData = new ImageData(new Uint8ClampedArray(rgba), 64, 64);
    ctx.putImageData(imgData, 0, 0);
  }, []);

  // Load first 256 bytes of real compiled /PepperCut.exe for the hex inspector
  useEffect(() => {
    fetch('/PepperCut.exe?v=1.0')
      .then((res) => res.arrayBuffer())
      .then((buf) => {
        const slice = new Uint8Array(buf, 0, Math.min(256, buf.byteLength));
        const rows: { offset: string; hex: string; ascii: string }[] = [];
        for (let i = 0; i < slice.length; i += 16) {
          const chunk = slice.slice(i, i + 16);
          const hex = Array.from(chunk)
            .map((b) => b.toString(16).toUpperCase().padStart(2, '0'))
            .join(' ');
          const ascii = Array.from(chunk)
            .map((b) => (b >= 32 && b <= 126 ? String.fromCharCode(b) : '.'))
            .join('');
          rows.push({
            offset: i.toString(16).toUpperCase().padStart(4, '0'),
            hex,
            ascii,
          });
        }
        setHexDumpRows(rows);
      })
      .catch(() => {});
  }, []);

  const pushNotification = (message: string) => {
    const now = new Date();
    const timeStr = now.toLocaleTimeString([], {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });
    const newNotif: WinNotification = {
      id: `${Date.now()}-${Math.random()}`,
      title: 'PepperCut',
      message,
      timestamp: timeStr,
    };
    setNotifications((prev) => [newNotif, ...prev.slice(0, 3)]);
  };

  // Download the real MinGW-w64 compiled /PepperCut.exe
  const handleDownloadExe = () => {
    const a = document.createElement('a');
    a.href = `/PepperCut.exe?v=${Date.now()}`;
    a.download = 'PepperCut.exe';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setDownloadCount((c) => c + 1);
  };

  // Download the multi-resolution /PepperCut.ico
  const handleDownloadIco = () => {
    const a = document.createElement('a');
    a.href = '/PepperCut.ico';
    a.download = 'PepperCut.ico';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  };

  // Download PepperCut.cpp source
  const handleDownloadCpp = () => {
    const blob = new Blob([PEPPERCUT_CPP_SOURCE], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'PepperCut.cpp';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  const handleDisconnectItem = (item: ExplorerItem) => {
    setItems((prev) =>
      prev.map((it) =>
        it.id === item.id
          ? {
              ...it,
              lockedByProcess: null,
              lockedByPid: null,
              handleHex: null,
            }
          : it
      )
    );
    setContextMenu(null);
    setExplorerError(null);
    pushNotification(
      item.type === 'folder'
        ? 'Folder has been disconnected'
        : 'File has been disconnected'
    );
  };

  const handleToggleBootDelete = (item: ExplorerItem) => {
    const willBeScheduled = !item.bootDeleteScheduled;
    setItems((prev) =>
      prev.map((it) =>
        it.id === item.id
          ? {
              ...it,
              bootDeleteScheduled: willBeScheduled,
            }
          : it
      )
    );
    setContextMenu(null);
    if (willBeScheduled) {
      pushNotification('File will be deleted on next boot');
    } else {
      pushNotification('Canceled delete on next boot');
    }
  };

  const handleStartRename = (item: ExplorerItem) => {
    setContextMenu(null);
    if (item.lockedByProcess) {
      setExplorerError(
        `The action can't be completed because the ${item.type} is open in ${item.lockedByProcess} (PID ${item.lockedByPid}). Right-click and choose "Disconnect ${item.type}" first.`
      );
      return;
    }
    setExplorerError(null);
    setRenamingId(item.id);
    setRenameValue(item.name);
  };

  const handleCommitRename = (itemId: string) => {
    if (renameValue.trim()) {
      setItems((prev) =>
        prev.map((it) => (it.id === itemId ? { ...it, name: renameValue.trim() } : it))
      );
    }
    setRenamingId(null);
  };

  const handleDeleteNow = (item: ExplorerItem) => {
    setContextMenu(null);
    if (item.lockedByProcess) {
      setExplorerError(
        `The action can't be completed because the ${item.type} is open in ${item.lockedByProcess} (PID ${item.lockedByPid}). Use PepperCut "Disconnect ${item.type}" or "Delete on next boot".`
      );
      return;
    }
    setExplorerError(null);
    setItems((prev) => prev.filter((it) => it.id !== item.id));
  };

  const handleSimulateLockToggle = (item: ExplorerItem) => {
    if (item.lockedByProcess) {
      setItems((prev) =>
        prev.map((it) =>
          it.id === item.id
            ? { ...it, lockedByProcess: null, lockedByPid: null, handleHex: null }
            : it
        )
      );
    } else {
      const sampleProcs = [
        { name: 'chrome.exe', pid: 7420, handle: '0x00000514' },
        { name: 'devenv.exe', pid: 8812, handle: '0x000009A0' },
        { name: 'WINWORD.EXE', pid: 3108, handle: '0x000002C4' },
      ];
      const pick = sampleProcs[Math.floor(Math.random() * sampleProcs.length)];
      setItems((prev) =>
        prev.map((it) =>
          it.id === item.id
            ? {
                ...it,
                lockedByProcess: pick.name,
                lockedByPid: pick.pid,
                handleHex: pick.handle,
              }
            : it
        )
      );
    }
  };

  const handleLaunchPepperCut = () => {
    setIsTrayRunning(true);
    setStartOnBootReg(true);
    setTrayMenuOpen(false);
    pushNotification('PepperCut is running');
  };

  const handleExitPepperCut = () => {
    setIsTrayRunning(false);
    setTrayMenuOpen(false);
    setContextMenu(null);
  };

  const activeContextItem = items.find((i) => i.id === contextMenu?.itemId) || null;

  return (
    <div
      className="min-h-screen bg-[#0F172A] text-[#F8FAFC] flex flex-col"
      onClick={() => {
        if (contextMenu) setContextMenu(null);
        if (trayMenuOpen) setTrayMenuOpen(false);
      }}
    >
      {/* Top Bar Contract: 3 Zones (Single-element Brand | Nav Links | Primary Action) */}
      <header className="flex items-center justify-between px-6 py-4 border-b border-slate-800 bg-[#0F172A]/95 sticky top-0 z-30">
        <a
          href="#top"
          className="text-lg font-bold tracking-tight text-white whitespace-nowrap"
        >
          PepperCut
        </a>

        <nav className="hidden md:flex items-center gap-6 text-sm font-medium text-slate-300">
          <button
            type="button"
            onClick={() => setActiveTab('simulator')}
            className={`hover:text-white transition-colors whitespace-nowrap cursor-pointer ${
              activeTab === 'simulator'
                ? 'text-white underline underline-offset-8 decoration-[#DC2626]'
                : ''
            }`}
          >
            Windows 10 Simulator
          </button>
          <button
            type="button"
            onClick={() => setActiveTab('binary')}
            className={`hover:text-white transition-colors whitespace-nowrap cursor-pointer ${
              activeTab === 'binary'
                ? 'text-white underline underline-offset-8 decoration-[#DC2626]'
                : ''
            }`}
          >
            PE32+ Executable Inspector
          </button>
          <button
            type="button"
            onClick={() => setActiveTab('source')}
            className={`hover:text-white transition-colors whitespace-nowrap cursor-pointer ${
              activeTab === 'source'
                ? 'text-white underline underline-offset-8 decoration-[#DC2626]'
                : ''
            }`}
          >
            Win32 C++ Source
          </button>
        </nav>

        <div className="flex items-center gap-3">
          <button
            type="button"
            onClick={handleDownloadExe}
            className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-[#DC2626] rounded-lg hover:bg-red-700 transition-colors whitespace-nowrap cursor-pointer"
          >
            <Download className="w-4 h-4" />
            <span>Download PepperCut.exe</span>
          </button>
        </div>
      </header>

      {/* Main Content Container */}
      <main id="top" className="flex-1 max-w-[1360px] w-full mx-auto px-6 py-8 space-y-8">
        {/* Hero & Standalone Binary Download Bar */}
        <section className="border border-slate-800 bg-[#1E293B]/60 rounded-xl p-6 md:p-8 flex flex-col lg:flex-row items-start lg:items-center justify-between gap-6">
          <div className="flex items-start gap-5 max-w-2xl">
            <div className="p-1 bg-slate-900 border border-slate-700 rounded-xl shrink-0">
              <canvas
                ref={canvasRef}
                width={64}
                height={64}
                className="w-16 h-16 rounded-lg block"
                title="Embedded RT_ICON #101 (64x64 preview)"
              />
            </div>
            <div className="space-y-2">
              <h1
                className="text-2xl md:text-3xl font-bold tracking-tight text-white"
                style={{ textWrap: 'balance' }}
              >
                PepperCut v1.0 — Standalone Windows 10 Executable
              </h1>
              <p className="text-sm text-slate-300 leading-relaxed">
                Compiled with <code className="text-slate-100">x86_64-w64-mingw32-g++ -static -mwindows -municode</code> into
                a ready-to-use 64-bit Windows 10 executable (<code className="text-slate-100">PepperCut.exe</code>).
                Starts on boot in the system tray with <strong className="text-white">Ignore important windows file and folder</strong> enabled by default,
                saves settings and boot-delete queues to <code className="text-slate-100">%appdata%\PepperCut</code>, and shows
                right-click options only when a file or folder is locked by another app.
              </p>
              {/* Unboxed Metadata with · separators */}
              <div className="flex flex-wrap items-center gap-2 text-xs text-slate-400 font-mono tabular-nums pt-1">
                <span>Version: 1.0 (x64 PE32+)</span>
                <span aria-hidden="true">·</span>
                <span>Size: {inspection.fileSizeBytes.toLocaleString()} bytes</span>
                <span aria-hidden="true">·</span>
                <span>Config: %appdata%\PepperCut</span>
                <span aria-hidden="true">·</span>
                <span>External DLLs: None</span>
              </div>
            </div>
          </div>

          <div className="flex flex-col sm:flex-row lg:flex-col xl:flex-row items-stretch sm:items-center gap-3 w-full lg:w-auto shrink-0">
            <button
              type="button"
              onClick={handleDownloadExe}
              className="flex items-center justify-center gap-2.5 px-6 py-3.5 text-sm font-semibold text-white bg-[#DC2626] rounded-lg hover:bg-red-700 transition-colors whitespace-nowrap cursor-pointer shadow-sm"
            >
              <Download className="w-4 h-4" />
              <span>Download PepperCut.exe (v1.0)</span>
            </button>
            <button
              type="button"
              onClick={handleDownloadIco}
              className="flex items-center justify-center gap-2 px-4 py-3.5 text-xs font-semibold text-slate-200 bg-slate-800 border border-slate-700 rounded-lg hover:bg-slate-700 transition-colors whitespace-nowrap cursor-pointer"
            >
              <Download className="w-3.5 h-3.5" />
              <span>PepperCut.ico</span>
            </button>
          </div>
        </section>

        {/* Segmented View Switcher */}
        <div className="flex flex-wrap items-center justify-between gap-4 border-b border-slate-800 pb-4">
          <div className="flex items-center gap-1 p-1 bg-slate-900 border border-slate-800 rounded-lg">
            <button
              type="button"
              onClick={() => setActiveTab('simulator')}
              className={`px-4 py-2 text-xs font-semibold rounded-md transition-colors whitespace-nowrap cursor-pointer ${
                activeTab === 'simulator'
                  ? 'bg-[#1E293B] text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              01. Interactive Windows 10 Explorer & Tray Testbed
            </button>
            <button
              type="button"
              onClick={() => setActiveTab('binary')}
              className={`px-4 py-2 text-xs font-semibold rounded-md transition-colors whitespace-nowrap cursor-pointer ${
                activeTab === 'binary'
                  ? 'bg-[#1E293B] text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              02. Standalone PE32+ Binary Structure ({inspection.fileSizeBytes.toLocaleString()} B)
            </button>
            <button
              type="button"
              onClick={() => setActiveTab('source')}
              className={`px-4 py-2 text-xs font-semibold rounded-md transition-colors whitespace-nowrap cursor-pointer ${
                activeTab === 'source'
                  ? 'bg-[#1E293B] text-white'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              03. Pure C++ & Win32 Source (PepperCut.cpp)
            </button>
          </div>

          <div className="flex items-center gap-3 text-xs text-slate-400 font-mono tabular-nums">
            <span>
              Version: 1.0
            </span>
            <span aria-hidden="true">·</span>
            <span>
              Storage: %appdata%\PepperCut
            </span>
            <span aria-hidden="true">·</span>
            <span>
              Ignore Windows Files: {ignoreImportantWindows ? 'ON (Default)' : 'OFF'}
            </span>
            {downloadCount > 0 && (
              <>
                <span aria-hidden="true">·</span>
                <span className="text-emerald-400">
                  Downloaded PepperCut.exe ({downloadCount}x)
                </span>
              </>
            )}
          </div>
        </div>

        {/* ================================================================= */}
        {/* TAB 1: INTERACTIVE WINDOWS 10 EXPLORER & SYSTEM TRAY TESTBED      */}
        {/* ================================================================= */}
        {activeTab === 'simulator' && (
          <section className="space-y-4">
            <div className="flex flex-wrap items-center justify-between gap-4">
              <div>
                <h2 className="text-base font-semibold text-white">
                  Interactive Windows 10 Right-Click & System Tray Verification
                </h2>
                <p className="text-xs text-slate-400">
                  Right-click any file or folder below (or click its Right-Click Menu button).{' '}
                  <strong className="text-slate-200">Disconnect file / Disconnect folder</strong> and{' '}
                  <strong className="text-slate-200">Delete on next boot</strong> appear{' '}
                  <em>only</em> when the item is locked by another application.
                </p>
              </div>
              <div className="flex items-center gap-2">
                {!isTrayRunning && (
                  <button
                    type="button"
                    onClick={handleLaunchPepperCut}
                    className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-semibold text-white bg-[#DC2626] rounded-lg hover:bg-red-700 transition-colors whitespace-nowrap cursor-pointer"
                  >
                    <Play className="w-3.5 h-3.5" />
                    <span>Launch PepperCut.exe</span>
                  </button>
                )}
                <button
                  type="button"
                  onClick={() => {
                    setItems(INITIAL_ITEMS);
                    setExplorerError(null);
                    setIsTrayRunning(true);
                    pushNotification('PepperCut is running');
                  }}
                  className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-slate-300 bg-slate-800 border border-slate-700 rounded-lg hover:bg-slate-700 transition-colors whitespace-nowrap cursor-pointer"
                >
                  <RefreshCw className="w-3.5 h-3.5" />
                  <span>Reset Locked Files</span>
                </button>
              </div>
            </div>

            {/* Simulated Windows 10 Desktop Frame */}
            <div className="relative border border-slate-700 rounded-xl overflow-hidden bg-[#0B1120] select-none">
              {/* Windows 10 File Explorer Window */}
              <div className="m-4 md:m-6 border border-slate-700 rounded-lg bg-[#191919] overflow-hidden">
                {/* Explorer Window Title Bar */}
                <div className="flex items-center justify-between px-4 py-2.5 bg-[#202020] border-b border-neutral-800 text-xs">
                  <div className="flex items-center gap-2.5 text-neutral-200 font-medium">
                    <Folder className="w-4 h-4 text-amber-400" />
                    <span>File Explorer — C:\Users\Admin\Workspace</span>
                  </div>
                  <div className="flex items-center gap-4 text-neutral-400 font-mono text-[11px]">
                    <span>Win10 Build 19045.5011</span>
                  </div>
                </div>

                {/* Explorer Address & Action Toolbar */}
                <div className="flex flex-wrap items-center justify-between gap-3 px-4 py-2.5 bg-[#252526] border-b border-neutral-800 text-xs">
                  <div className="flex items-center gap-2 px-3 py-1.5 bg-[#191919] border border-neutral-700 rounded text-neutral-300 font-mono text-xs flex-1 min-w-[240px]">
                    <span>This PC &gt; Local Disk (C:) &gt; Users &gt; Admin &gt; Workspace</span>
                  </div>
                  <div className="flex items-center gap-2 text-neutral-400">
                    <span>Right-click any row to open Windows 10 Shell Context Menu</span>
                  </div>
                </div>

                {/* Error Banner when trying to Rename/Delete a Locked File without PepperCut */}
                {explorerError && (
                  <div className="px-4 py-2.5 bg-red-950/80 border-b border-red-800/80 flex items-center justify-between gap-4 text-xs text-red-200">
                    <div className="flex items-center gap-2">
                      <Lock className="w-4 h-4 text-red-400 shrink-0" />
                      <span>{explorerError}</span>
                    </div>
                    <button
                      type="button"
                      onClick={() => setExplorerError(null)}
                      className="text-red-300 hover:text-white font-medium whitespace-nowrap cursor-pointer"
                    >
                      Dismiss
                    </button>
                  </div>
                )}

                {/* File Explorer High-Density Data Grid */}
                <div className="overflow-x-auto">
                  <table className="w-full text-left border-collapse text-xs">
                    <thead>
                      <tr className="border-b border-neutral-800 bg-[#1E1E1E] text-neutral-400 font-medium">
                        <th className="py-2.5 px-4">Name</th>
                        <th className="py-2.5 px-4">Type</th>
                        <th className="py-2.5 px-4">Locking Process Status</th>
                        <th className="py-2.5 px-4">Boot Deletion State</th>
                        <th className="py-2.5 px-4 text-right">Size</th>
                        <th className="py-2.5 px-4 text-right">Shell Menu / Test Lock</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-neutral-800/70">
                      {items.map((item) => {
                        const isLocked = Boolean(item.lockedByProcess);
                        const isSelected = selectedItemId === item.id;
                        return (
                          <tr
                            key={item.id}
                            onClick={() => setSelectedItemId(item.id)}
                            onContextMenu={(e) => {
                              e.preventDefault();
                              e.stopPropagation();
                              setSelectedItemId(item.id);
                              setTrayMenuOpen(false);
                              setContextMenu({
                                itemId: item.id,
                                x: Math.min(e.clientX, window.innerWidth - 290),
                                y: Math.min(e.clientY, window.innerHeight - 260),
                              });
                            }}
                            className={`transition-colors cursor-pointer ${
                              isSelected ? 'bg-[#2A2D2E]' : 'hover:bg-[#222324]'
                            }`}
                          >
                            <td className="py-2.5 px-4 font-medium text-neutral-100">
                              <div className="flex items-center gap-2.5">
                                {item.type === 'folder' ? (
                                  <Folder className="w-4 h-4 text-amber-400 shrink-0" />
                                ) : (
                                  <FileText className="w-4 h-4 text-sky-400 shrink-0" />
                                )}
                                {renamingId === item.id ? (
                                  <form
                                    onSubmit={(e) => {
                                      e.preventDefault();
                                      handleCommitRename(item.id);
                                    }}
                                    className="flex items-center gap-1.5"
                                    onClick={(e) => e.stopPropagation()}
                                  >
                                    <input
                                      type="text"
                                      value={renameValue}
                                      onChange={(e) => setRenameValue(e.target.value)}
                                      autoFocus
                                      className="px-2 py-0.5 bg-neutral-900 border border-sky-500 rounded text-xs text-white focus:outline-none"
                                    />
                                    <button
                                      type="submit"
                                      className="px-2 py-0.5 bg-sky-600 text-white rounded text-[11px] font-medium"
                                    >
                                      Save
                                    </button>
                                  </form>
                                ) : (
                                  <span className="truncate max-w-[260px]">{item.name}</span>
                                )}
                              </div>
                            </td>

                            <td className="py-2.5 px-4 text-neutral-400">
                              {item.type === 'folder' ? 'File folder' : 'File'}
                            </td>

                            <td className="py-2.5 px-4 font-mono tabular-nums">
                              {isLocked ? (
                                <span className="inline-flex items-center gap-1.5 text-red-400">
                                  <Lock className="w-3.5 h-3.5 shrink-0" />
                                  <span>
                                    In use by {item.lockedByProcess} (PID {item.lockedByPid})
                                  </span>
                                </span>
                              ) : (
                                <span className="inline-flex items-center gap-1.5 text-emerald-400">
                                  <Unlock className="w-3.5 h-3.5 shrink-0" />
                                  <span>Unlocked (Free to rename/delete)</span>
                                </span>
                              )}
                            </td>

                            <td className="py-2.5 px-4 font-mono tabular-nums">
                              {item.bootDeleteScheduled ? (
                                <span className="text-amber-400">
                                  Scheduled for delete on next boot
                                </span>
                              ) : (
                                <span className="text-neutral-500">None</span>
                              )}
                            </td>

                            <td className="py-2.5 px-4 text-right font-mono tabular-nums text-neutral-300">
                              {item.sizeText}
                            </td>

                            <td
                              className="py-2.5 px-4 text-right"
                              onClick={(e) => e.stopPropagation()}
                            >
                              <div className="inline-flex items-center justify-end gap-2">
                                <button
                                  type="button"
                                  onClick={(e) => {
                                    const rect = e.currentTarget.getBoundingClientRect();
                                    setSelectedItemId(item.id);
                                    setTrayMenuOpen(false);
                                    setContextMenu({
                                      itemId: item.id,
                                      x: Math.min(rect.left, window.innerWidth - 290),
                                      y: Math.min(rect.bottom + 6, window.innerHeight - 260),
                                    });
                                  }}
                                  className="px-2.5 py-1 text-[11px] font-medium text-neutral-200 bg-neutral-800 border border-neutral-700 rounded hover:bg-neutral-700 transition-colors whitespace-nowrap cursor-pointer"
                                >
                                  Right-Click Menu
                                </button>
                                <button
                                  type="button"
                                  onClick={() => handleSimulateLockToggle(item)}
                                  className="px-2.5 py-1 text-[11px] font-medium text-neutral-400 hover:text-neutral-200 border border-neutral-800 rounded hover:border-neutral-700 transition-colors whitespace-nowrap cursor-pointer"
                                  title="Simulate locking or unlocking this item by another process"
                                >
                                  {isLocked ? 'Release App Lock' : 'Lock with App'}
                                </button>
                              </div>
                            </td>
                          </tr>
                        );
                      })}
                    </tbody>
                  </table>
                </div>
              </div>

              {/* Windows 10 Notification Stack (Bottom Right above System Tray) */}
              <div className="px-6 pb-4 flex flex-col md:flex-row items-start md:items-end justify-between gap-4">
                <div className="text-xs text-slate-400 space-y-1 max-w-xl">
                  <p className="font-medium text-slate-300">
                    Active Behavior Rules in PepperCut.exe:
                  </p>
                  <p>
                    1. Right-clicking an unlocked item hides PepperCut options completely.{' '}
                    2. Right-clicking a locked item displays{' '}
                    <span className="text-white font-medium">Disconnect file</span> (or{' '}
                    <span className="text-white font-medium">Disconnect folder</span>) and{' '}
                    <span className="text-white font-medium">Delete on next boot</span> (which changes to{' '}
                    <span className="text-amber-300 font-medium">Cancel delete on next boot</span> once set).
                  </p>
                </div>

                <div className="w-full md:w-80 space-y-2">
                  {notifications.map((notif) => (
                    <div
                      key={notif.id}
                      className="bg-[#1F1F1F] border border-neutral-700 rounded-lg p-3 shadow-lg flex items-start gap-3"
                    >
                      <PepperCutIcon className="w-7 h-7 mt-0.5" />
                      <div className="flex-1 min-w-0">
                        <div className="flex items-center justify-between gap-2">
                          <span className="text-xs font-semibold text-white">{notif.title}</span>
                          <span className="text-[10px] font-mono tabular-nums text-neutral-400">
                            {notif.timestamp}
                          </span>
                        </div>
                        <p className="text-xs text-neutral-200 mt-0.5">{notif.message}</p>
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* Windows 10 Bottom Taskbar & System Tray */}
              <div className="h-11 bg-[#101010] border-t border-neutral-800 px-4 flex items-center justify-between text-xs">
                <div className="flex items-center gap-4">
                  <span className="font-semibold text-neutral-300">Windows 10 Taskbar</span>
                  <span className="text-neutral-500 hidden sm:inline">·</span>
                  <span className="text-neutral-400 hidden sm:inline">
                    Data Directory: %appdata%\PepperCut (config.ini &amp; boot_delete_queue.txt)
                  </span>
                </div>

                {/* System Tray Area */}
                <div className="relative flex items-center gap-3">
                  {isTrayRunning ? (
                    <button
                      type="button"
                      onClick={(e) => {
                        e.stopPropagation();
                        setContextMenu(null);
                        setTrayMenuOpen((o) => !o);
                      }}
                      onContextMenu={(e) => {
                        e.preventDefault();
                        e.stopPropagation();
                        setContextMenu(null);
                        setTrayMenuOpen(true);
                      }}
                      className="flex items-center gap-2 px-2.5 py-1 rounded bg-neutral-800/90 hover:bg-neutral-700 border border-neutral-700 transition-colors cursor-pointer"
                      title="PepperCut is running — Right-click tray icon (only option is Exit)"
                    >
                      <PepperCutIcon className="w-4 h-4" />
                      <span className="text-[11px] font-medium text-neutral-200">
                        PepperCut Tray Icon (Right-Click)
                      </span>
                    </button>
                  ) : (
                    <span className="text-[11px] text-neutral-500">
                      PepperCut exited from tray
                    </span>
                  )}

                  <div className="font-mono tabular-nums text-[11px] text-neutral-300 pl-2 border-l border-neutral-800">
                    20:02
                  </div>

                  {/* Tray Right-Click Popup Menu: 'Ignore important windows file and folder' toggle + 'Exit' */}
                  {trayMenuOpen && isTrayRunning && (
                    <div
                      onClick={(e) => e.stopPropagation()}
                      className="absolute bottom-10 right-12 w-72 bg-[#2B2B2B] border border-neutral-600 rounded shadow-xl py-1 z-50"
                    >
                      <button
                        type="button"
                        onClick={() => {
                          setIgnoreImportantWindows((v) => !v);
                          setTrayMenuOpen(false);
                        }}
                        className="w-full px-3 py-1.5 text-left text-xs text-white hover:bg-neutral-700 transition-colors flex items-center gap-2.5 cursor-pointer"
                      >
                        <span className="w-4 h-4 flex items-center justify-center text-emerald-400 shrink-0">
                          {ignoreImportantWindows ? <Check className="w-3.5 h-3.5" /> : null}
                        </span>
                        <span className="whitespace-nowrap">
                          Ignore important windows file and folder
                        </span>
                      </button>
                      <div className="my-1 border-t border-neutral-700" />
                      <button
                        type="button"
                        onClick={handleExitPepperCut}
                        className="w-full px-3 py-1.5 text-left text-xs text-white hover:bg-[#DC2626] transition-colors flex items-center gap-2.5 cursor-pointer"
                      >
                        <span className="w-4 h-4 shrink-0" />
                        <span>Exit</span>
                      </button>
                    </div>
                  )}
                </div>
              </div>
            </div>

            {/* Floating Windows 10 Explorer Right-Click Context Menu */}
            {contextMenu && activeContextItem && (
              <div
                style={{ left: contextMenu.x, top: contextMenu.y }}
                onClick={(e) => e.stopPropagation()}
                className="fixed w-68 bg-[#2B2B2B] border border-neutral-600 rounded-md shadow-2xl py-1.5 text-xs text-neutral-100 z-50"
              >
                <div className="px-3 py-1 text-[11px] text-neutral-400 border-b border-neutral-700/80 truncate">
                  {activeContextItem.name}
                </div>

                <button
                  type="button"
                  onClick={() => handleStartRename(activeContextItem)}
                  className="w-full px-3 py-1.5 text-left hover:bg-neutral-700 flex items-center gap-2.5 cursor-pointer"
                >
                  <Edit3 className="w-3.5 h-3.5 text-neutral-400" />
                  <span>Rename</span>
                </button>
                <button
                  type="button"
                  onClick={() => handleDeleteNow(activeContextItem)}
                  className="w-full px-3 py-1.5 text-left hover:bg-neutral-700 flex items-center gap-2.5 cursor-pointer"
                >
                  <Trash2 className="w-3.5 h-3.5 text-neutral-400" />
                  <span>Delete</span>
                </button>

                {isTrayRunning &&
                activeContextItem.lockedByProcess &&
                !(ignoreImportantWindows && activeContextItem.isImportantWindows) ? (
                  <>
                    <div className="my-1 border-t border-neutral-700" />
                    <button
                      type="button"
                      onClick={() => handleDisconnectItem(activeContextItem)}
                      className="w-full px-3 py-1.5 text-left hover:bg-neutral-700 flex items-center gap-2.5 font-medium text-white cursor-pointer"
                    >
                      <PepperCutIcon className="w-4 h-4" />
                      <span>
                        {activeContextItem.type === 'folder'
                          ? 'Disconnect folder'
                          : 'Disconnect file'}
                      </span>
                    </button>
                    <button
                      type="button"
                      onClick={() => handleToggleBootDelete(activeContextItem)}
                      className="w-full px-3 py-1.5 text-left hover:bg-neutral-700 flex items-center gap-2.5 font-medium text-white cursor-pointer"
                    >
                      <PepperCutIcon className="w-4 h-4" />
                      <span>
                        {activeContextItem.bootDeleteScheduled
                          ? 'Cancel delete on next boot'
                          : 'Delete on next boot'}
                      </span>
                    </button>
                  </>
                ) : (
                  <div className="mt-1 px-3 py-1.5 border-t border-neutral-700/80 text-[11px] text-neutral-400">
                    {!isTrayRunning
                      ? 'PepperCut is not running in tray.'
                      : ignoreImportantWindows && activeContextItem.isImportantWindows
                      ? 'Ignored: Important Windows file/folder protection is enabled in tray.'
                      : 'Not used by any other app — PepperCut menu options hidden.'}
                  </div>
                )}
              </div>
            )}
          </section>
        )}

        {/* ================================================================= */}
        {/* TAB 2: PE32+ STANDALONE BINARY INSPECTOR                          */}
        {/* ================================================================= */}
        {activeTab === 'binary' && (
          <section className="space-y-6">
            <div className="border border-slate-800 bg-[#1E293B]/40 rounded-xl p-6 space-y-6">
              <div className="flex flex-wrap items-center justify-between gap-4 border-b border-slate-800 pb-4">
                <div>
                  <h2 className="text-base font-semibold text-white">
                    PE32+ Executable Verification (PepperCut.exe)
                  </h2>
                  <p className="text-xs text-slate-400">
                    Compiled with {inspection.compiler}. Statically linked with full ASLR relocation table (`.reloc`) and x64 unwind tables (`.pdata` / `.xdata`).
                  </p>
                </div>
                <button
                  type="button"
                  onClick={handleDownloadExe}
                  className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-[#DC2626] rounded-lg hover:bg-red-700 transition-colors cursor-pointer"
                >
                  <Download className="w-3.5 h-3.5" />
                  <span>Download PepperCut.exe (282.5 KB)</span>
                </button>
              </div>

              <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 text-xs font-mono tabular-nums">
                <div className="p-4 bg-slate-900/90 border border-slate-800 rounded-lg space-y-1">
                  <div className="text-slate-400">Architecture & Format</div>
                  <div className="text-white font-semibold">{inspection.architecture}</div>
                  <div className="text-slate-500">pei-x86-64 · Magic: 0x020B</div>
                </div>
                <div className="p-4 bg-slate-900/90 border border-slate-800 rounded-lg space-y-1">
                  <div className="text-slate-400">Windows Subsystem</div>
                  <div className="text-white font-semibold">WINDOWS_GUI (2)</div>
                  <div className="text-slate-500">Unicode wWinMain Entry</div>
                </div>
                <div className="p-4 bg-slate-900/90 border border-slate-800 rounded-lg space-y-1">
                  <div className="text-slate-400">ImageBase & EntryPoint</div>
                  <div className="text-white font-semibold">{inspection.imageBase}</div>
                  <div className="text-slate-500">{inspection.entryPointRva}</div>
                </div>
                <div className="p-4 bg-slate-900/90 border border-slate-800 rounded-lg space-y-1">
                  <div className="text-slate-400">Embedded Resource (.rsrc)</div>
                  <div className="text-white font-semibold">RT_ICON #101 (16/32/48/64px)</div>
                  <div className="text-slate-500">33,080 bytes (.rsrc section)</div>
                </div>
              </div>

              <div className="space-y-2">
                <h3 className="text-xs font-semibold text-slate-300">
                  PE32+ Section Headers (objdump -h PepperCut.exe)
                </h3>
                <div className="overflow-x-auto border border-slate-800 rounded-lg">
                  <table className="w-full text-left border-collapse text-xs font-mono tabular-nums">
                    <thead>
                      <tr className="bg-slate-900 border-b border-slate-800 text-slate-400">
                        <th className="py-2.5 px-4">Section</th>
                        <th className="py-2.5 px-4">Virtual Address (RVA)</th>
                        <th className="py-2.5 px-4 text-right">Virtual Size</th>
                        <th className="py-2.5 px-4">Raw Offset</th>
                        <th className="py-2.5 px-4 text-right">Raw Size</th>
                        <th className="py-2.5 px-4">Characteristics</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-800">
                      {inspection.sections.map((sec) => (
                        <tr key={sec.name} className="hover:bg-slate-900/50">
                          <td className="py-2.5 px-4 font-semibold text-white">{sec.name}</td>
                          <td className="py-2.5 px-4 text-slate-300">{sec.virtualAddress}</td>
                          <td className="py-2.5 px-4 text-right text-slate-300">
                            {sec.virtualSize.toLocaleString()} B
                          </td>
                          <td className="py-2.5 px-4 text-slate-300">{sec.rawOffset}</td>
                          <td className="py-2.5 px-4 text-right text-slate-300">
                            {sec.rawSize.toLocaleString()} B
                          </td>
                          <td className="py-2.5 px-4 text-slate-400">{sec.characteristics}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>

              <div className="space-y-2">
                <h3 className="text-xs font-semibold text-slate-300">
                  Imported Win32 System Libraries (Zero Third-Party DLLs)
                </h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  {inspection.imports.map((imp) => (
                    <div
                      key={imp.dll}
                      className="p-4 bg-slate-900/90 border border-slate-800 rounded-lg space-y-2 font-mono text-xs"
                    >
                      <div className="font-semibold text-white border-b border-slate-800 pb-1.5">
                        {imp.dll}
                      </div>
                      <div className="text-slate-400 leading-relaxed">
                        {imp.functions.join(' · ')}
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {hexDumpRows.length > 0 && (
                <div className="space-y-2">
                  <h3 className="text-xs font-semibold text-slate-300">
                    Binary Header Hex Dump (First 256 Bytes of Compiled PepperCut.exe)
                  </h3>
                  <div className="p-4 bg-slate-950 border border-slate-800 rounded-lg overflow-x-auto font-mono text-xs tabular-nums leading-relaxed">
                    {hexDumpRows.map((row) => (
                      <div key={row.offset} className="flex items-center gap-6 whitespace-pre">
                        <span className="text-slate-500">{row.offset}</span>
                        <span className="text-slate-200">{row.hex}</span>
                        <span className="text-amber-400">{row.ascii}</span>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          </section>
        )}

        {/* ================================================================= */}
        {/* TAB 3: PURE C++ & WIN32 SOURCE CODE (PepperCut.cpp)               */}
        {/* ================================================================= */}
        {activeTab === 'source' && (
          <section className="border border-slate-800 bg-[#1E293B]/40 rounded-xl p-6 space-y-4">
            <div className="flex flex-wrap items-center justify-between gap-4 border-b border-slate-800 pb-4">
              <div>
                <h2 className="text-base font-semibold text-white">
                  PepperCut.cpp — Pure 100% C++ & Win32 Implementation
                </h2>
                <p className="text-xs text-slate-400">
                  Exact C++ source code compiled into <code className="text-slate-200">PepperCut.exe</code> via MinGW-w64 GCC 12.2.
                </p>
              </div>
              <div className="flex items-center gap-2">
                <button
                  type="button"
                  onClick={() => {
                    navigator.clipboard.writeText(PEPPERCUT_CPP_SOURCE);
                    setCopiedSource(true);
                    setTimeout(() => setCopiedSource(false), 2000);
                  }}
                  className="flex items-center gap-1.5 px-3 py-2 text-xs font-medium text-slate-200 bg-slate-800 border border-slate-700 rounded-lg hover:bg-slate-700 transition-colors cursor-pointer"
                >
                  {copiedSource ? (
                    <>
                      <Check className="w-3.5 h-3.5 text-emerald-400" />
                      <span>Copied C++ Source</span>
                    </>
                  ) : (
                    <>
                      <Copy className="w-3.5 h-3.5" />
                      <span>Copy PepperCut.cpp</span>
                    </>
                  )}
                </button>
                <button
                  type="button"
                  onClick={handleDownloadCpp}
                  className="flex items-center gap-1.5 px-3 py-2 text-xs font-medium text-slate-200 bg-slate-800 border border-slate-700 rounded-lg hover:bg-slate-700 transition-colors cursor-pointer"
                >
                  <FileCode className="w-3.5 h-3.5" />
                  <span>Download PepperCut.cpp</span>
                </button>
              </div>
            </div>

            <pre className="p-4 bg-slate-950 border border-slate-800 rounded-lg overflow-x-auto text-xs font-mono text-slate-200 leading-relaxed max-h-[640px]">
              <code>{PEPPERCUT_CPP_SOURCE}</code>
            </pre>
          </section>
        )}
      </main>

      {/* Quiet Footer */}
      <footer className="border-t border-slate-800/80 py-5 px-6 text-xs text-slate-500">
        <div className="max-w-[1360px] mx-auto flex flex-col sm:flex-row items-center justify-between gap-3">
          <span>PepperCut — Standalone Windows 10 C++ Win32 File & Folder Unlocker</span>
          <div className="flex items-center gap-4">
            <button
              type="button"
              onClick={handleDownloadExe}
              className="text-slate-300 hover:text-white transition-colors cursor-pointer"
            >
              Download PepperCut.exe
            </button>
            <span aria-hidden="true">·</span>
            <button
              type="button"
              onClick={handleDownloadIco}
              className="text-slate-300 hover:text-white transition-colors cursor-pointer"
            >
              Download PepperCut.ico
            </button>
          </div>
        </div>
      </footer>
    </div>
  );
}
