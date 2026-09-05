const { contextBridge, ipcRenderer, webUtils } = require('electron');

contextBridge.exposeInMainWorld('voxkit', {
  pickInput: (extensions, locale) => ipcRenderer.invoke('dialog:open-input', { extensions, locale }),
  pickOutput: (settings) => ipcRenderer.invoke('dialog:save-output', settings),
  pickPrintOutput: (defaultPath) => ipcRenderer.invoke('dialog:save-print-output', defaultPath),
  pickModelOutput: (settings) => ipcRenderer.invoke('dialog:save-model-output', settings),
  getAboutConfig: () => ipcRenderer.invoke('about:config'),
  voxelize: (settings) => ipcRenderer.invoke('voxelize:start', settings),
  exportForSnapmakerU1: (settings) => ipcRenderer.invoke('print-export:snapmaker-u1', settings),
  exportModel: (settings) => ipcRenderer.invoke('model:export', settings),
  openInSlicer: (filePath, locale) => ipcRenderer.invoke('print-export:open-slicer', filePath, locale),
  listPrinters: (locale) => ipcRenderer.invoke('printers:list', locale),
  inspectPrintModel: (settings) => ipcRenderer.invoke('print:inspect', settings),
  sendToPrinter: (settings) => ipcRenderer.invoke('print:send', settings),
  listExportHistory: (locale) => ipcRenderer.invoke('export-history:list', locale),
  removeExportRecord: (id) => ipcRenderer.invoke('export-history:remove', id),
  openExportDirectory: () => ipcRenderer.invoke('export-history:open-directory'),
  openExportFile: (filePath) => ipcRenderer.invoke('export-history:open-file', filePath),
  cancel: () => ipcRenderer.invoke('voxelize:cancel'),
  revealOutput: (filePath) => ipcRenderer.invoke('shell:show-item', filePath),
  pathForFile: (file) => webUtils.getPathForFile(file),
  onProgress: (listener) => {
    const handler = (_event, payload) => listener(payload);
    ipcRenderer.on('voxelize:progress', handler);
    return () => ipcRenderer.removeListener('voxelize:progress', handler);
  },
  onPrintExportProgress: (listener) => {
    const handler = (_event, payload) => listener(payload);
    ipcRenderer.on('print-export:progress', handler);
    return () => ipcRenderer.removeListener('print-export:progress', handler);
  },
  onModelExportProgress: (listener) => {
    const handler = (_event, payload) => listener(payload);
    ipcRenderer.on('model-export:progress', handler);
    return () => ipcRenderer.removeListener('model-export:progress', handler);
  }
});
