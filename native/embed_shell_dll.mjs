import fs from 'node:fs';

const dllBytes = fs.readFileSync('native/PepperCutShell.dll');
const hexArray = Array.from(dllBytes)
  .map((b) => `0x${b.toString(16).padStart(2, '0')}`)
  .join(',');

const headerContent = `#pragma once
// Auto-generated from PepperCutShell.dll (${dllBytes.length} bytes)
static const unsigned char kEmbeddedShellDll[${dllBytes.length}] = {
${hexArray}
};
static const unsigned int kEmbeddedShellDllSize = ${dllBytes.length};
`;

fs.writeFileSync('native/EmbeddedShellDll.h', headerContent);
console.log('Embedded PepperCutShell.dll:', dllBytes.length, 'bytes into native/EmbeddedShellDll.h');
