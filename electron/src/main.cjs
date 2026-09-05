const { app, BrowserWindow, dialog, ipcMain, Menu, nativeImage, shell } = require('electron');
const path = require('node:path');
const fs = require('node:fs');
const crypto = require('node:crypto');
const { spawn, spawnSync } = require('node:child_process');
const readline = require('node:readline');

let mainWindow;
let activeProcess;
let cancelRequested = false;
const APP_NAME = 'Hands3DLab-VoxKit';
// Electron 的运行时窗口/Dock 图标使用 PNG，避免某些版本无法读取 ICNS
// 后回退到方形 JPG。ICNS 仍作为 macOS 打包图标保留。
const APP_ICON = path.join(__dirname, '..', 'assets', 'AppIcon.png');
const APP_ICON_FALLBACK = path.join(__dirname, '..', 'assets', 'Logo.jpg');
const EXPORT_HISTORY_VERSION = 1;
const PRINTERS = Object.freeze([
  {
    id: 'snapmaker-u1',
    name: 'Snapmaker U1',
    enabled: true,
    buildVolume: [270, 270, 270],
    slicer: {
      id: 'snapmaker-orca', name: 'Snapmaker Orca', bundleId: 'com.snapmaker.snapmaker-orca',
      executable: '/Applications/Snapmaker Orca.app/Contents/MacOS/Snapmaker_Orca', extensions: ['obj', 'stl', '3mf']
    }
  },
  {
    id: 'bambu-studio',
    name: 'Bambu Studio',
    enabled: false,
    buildVolume: [256, 256, 256],
    slicer: null
  }
]);
const isEnglish = (locale) => locale === 'en';
const localized = (locale, zh, en) => isEnglish(locale) ? en : zh;
const localizedError = (locale, zh, en) => new Error(localized(locale, zh, en));

function loadAppIcon() {
  for (const file of [APP_ICON, APP_ICON_FALLBACK]) {
    if (!fs.existsSync(file)) continue;
    const image = nativeImage.createFromPath(file);
    if (!image.isEmpty()) return image;
  }
  return undefined;
}

function electronRoot() { return path.resolve(__dirname, '..'); }
function projectRoot() { return app.isPackaged ? electronRoot() : path.resolve(__dirname, '..', '..'); }
function slicerExecutable(slicer) {
  if (process.platform === 'darwin') return slicer.executable;
  if (process.platform !== 'win32') return undefined;
  const roots = [process.env.LOCALAPPDATA, process.env.ProgramFiles, process.env['ProgramFiles(x86)']].filter(Boolean);
  const names = ['Snapmaker Orca', 'Snapmaker_Orca'];
  for (const root of roots) {
    for (const name of names) {
      const candidate = path.join(root, 'Snapmaker Orca', `${name}.exe`);
      if (fs.existsSync(candidate)) return candidate;
    }
  }
  return undefined;
}
function parseAboutToml(source) {
  const result = {};
  let section = result;
  for (const rawLine of source.split(/\r?\n/)) {
    const line = rawLine.replace(/#.*$/, '').trim();
    if (!line) continue;
    const arraySectionMatch = line.match(/^\[\[([^\]]+)\]\]$/);
    if (arraySectionMatch) {
      const parts = arraySectionMatch[1].split('.');
      section = result;
      parts.slice(0, -1).forEach((part) => { section = result[part] ||= {}; });
      const key = parts.at(-1);
      section[key] ||= [];
      section[key].push({});
      section = section[key].at(-1);
      continue;
    }
    const sectionMatch = line.match(/^\[([^\]]+)\]$/);
    if (sectionMatch) {
      section = result;
      for (const part of sectionMatch[1].split('.')) section = section[part] ||= {};
      continue;
    }
    const valueMatch = line.match(/^([A-Za-z0-9_-]+)\s*=\s*(.*)$/);
    if (!valueMatch) continue;
    const [, key, rawValue] = valueMatch;
    const value = rawValue.trim();
    section[key] = value.startsWith('[') && value.endsWith(']')
      ? value.slice(1, -1).split(',').map((item) => item.trim().replace(/^"|"$/g, '')).filter(Boolean)
      : value.startsWith('"') && value.endsWith('"')
      ? value.slice(1, -1).replace(/\\"/g, '"').replace(/\\n/g, '\n')
      : value === 'true' ? true : value === 'false' ? false : value;
  }
  return result;
}
function readAboutConfig() {
  if (app.isPackaged) {
    const protectedFile = path.join(process.resourcesPath, 'voxkit-config.enc.json');
    try {
      const envelope = JSON.parse(fs.readFileSync(protectedFile, 'utf8'));
      if (envelope.algorithm !== 'aes-256-gcm' || envelope.format !== 1) throw new Error('unsupported protected resource format');
      const version = app.getVersion();
      if (envelope.platform !== process.platform || envelope.version !== version) throw new Error('platform/version mismatch');
      const key = crypto.createHash('sha256')
        .update(`Hands3DLab-VoxKit/resource/${process.platform}/${version}`)
        .digest();
      const decipher = crypto.createDecipheriv('aes-256-gcm', key, Buffer.from(envelope.iv, 'base64'));
      decipher.setAuthTag(Buffer.from(envelope.tag, 'base64'));
      const source = Buffer.concat([
        decipher.update(Buffer.from(envelope.data, 'base64')),
        decipher.final()
      ]).toString('utf8');
      return parseAboutToml(source);
    } catch (error) {
      throw new Error(`Protected application resources are invalid or have been modified: ${error.message}`);
    }
  }
  const file = path.join(projectRoot(), 'config.toml');
  try { return parseAboutToml(fs.readFileSync(file, 'utf8')); }
  catch (error) { if (error.code === 'ENOENT') return {}; throw error; }
}
function exportRecordsDirectory() { return path.join(app.getPath('documents'), APP_NAME, '导出记录'); }
function exportRecordsPath() { return path.join(exportRecordsDirectory(), 'exports.json'); }
function ensureExportRecordsDirectory() { fs.mkdirSync(exportRecordsDirectory(), { recursive: true }); }
function readExportRecords() {
  ensureExportRecordsDirectory();
  try {
    const parsed = JSON.parse(fs.readFileSync(exportRecordsPath(), 'utf8'));
    return Array.isArray(parsed.records) ? parsed.records.filter((record) => (
      record && typeof record.id === 'string' && ['success', 'failed', 'cancelled'].includes(record.status || 'success')
    )) : [];
  } catch (error) {
    if (error.code !== 'ENOENT' && !(error instanceof SyntaxError)) throw error;
    return [];
  }
}
function writeExportRecords(records) {
  ensureExportRecordsDirectory();
  const target = exportRecordsPath();
  const temporary = `${target}.tmp`;
  fs.writeFileSync(temporary, JSON.stringify({ version: EXPORT_HISTORY_VERSION, records }, null, 2), 'utf8');
  fs.renameSync(temporary, target);
}
function addExportRecord(record) {
  const records = readExportRecords();
  records.unshift(record);
  writeExportRecords(records.slice(0, 500));
}
function failedExportRecord(operation, settings, error) {
  const outputPath = typeof settings?.outputPath === 'string' ? settings.outputPath : '';
  const inputPath = typeof settings?.inputPath === 'string' ? settings.inputPath : '';
  return {
    id: `${Date.now()}-${Math.random().toString(36).slice(2, 10)}`,
    status: 'failed',
    exportedAt: new Date().toISOString(),
    failedAt: new Date().toISOString(),
    operation,
    format: String(settings?.format || path.extname(outputPath).slice(1) || '').toUpperCase(),
    sourcePath: inputPath,
    originalSourcePath: settings?.originalSourcePath || inputPath,
    ...(outputPath ? { outputPath } : {}),
    error: error?.message || String(error),
    conversion: normalizeConversion(settings?.conversion)
  };
}
async function recordExportFailure(operation, settings, task) {
  try {
    return await task();
  } catch (error) {
    addExportRecord(failedExportRecord(operation, settings, error));
    throw error;
  }
}
function locateVoxkit() {
  const binaryName = process.platform === 'win32' ? 'voxkit.exe' : 'voxkit';
  const candidates = [
    process.env.VOXKIT_BIN,
    path.join(projectRoot(), 'build', binaryName),
    path.join(projectRoot(), 'build', 'Release', binaryName),
    path.join(process.resourcesPath, binaryName)
  ].filter(Boolean);
  return candidates.find((file) => { try { return fs.statSync(file).isFile(); } catch { return false; } });
}
function nativeWorkingDirectory(binary) {
  // In a packaged application, __dirname is inside app.asar. Windows cannot use
  // that virtual archive path as CreateProcess' working directory; doing so
  // makes spawn/spawnSync report ENOENT even when resources/voxkit.exe exists.
  return app.isPackaged ? path.dirname(binary) : projectRoot();
}
function nativeCapabilities() {
  const binary = locateVoxkit();
  if (!binary) return { triangleVoxelization: false, gpuBackend: null };
  const result = spawnSync(binary, ['--capabilities'], {
    cwd: nativeWorkingDirectory(binary), encoding: 'utf8', windowsHide: true
  });
  const line = String(result.stdout || '').split(/\r?\n/).find((value) => value.startsWith('VOXKIT_CAPABILITIES ')) || '';
  return {
    triangleVoxelization: /\btriangle=true\b/.test(line),
    gpuBackend: line.match(/\bbackend=([^\s]+)/)?.[1] || null
  };
}
function sendProgress(payload) { if (mainWindow && !mainWindow.isDestroyed()) mainWindow.webContents.send('voxelize:progress', payload); }
const progressMessages = {
  'loading-mesh': ['正在读取网格模型', 'Loading mesh model'],
  'loading-binvox': ['正在读取体素数据', 'Loading voxel data'],
  'writing-binvox': ['正在写入体素数据', 'Writing voxel data'],
  'writing-stl': ['正在写入 STL 网格', 'Writing STL mesh'],
  'writing-obj': ['正在写入 OBJ 网格', 'Writing OBJ mesh'],
  'writing-glb': ['正在写入 GLB 模型', 'Writing GLB model'],
  'writing-3mf': ['正在写入 3MF 模型', 'Writing 3MF model'],
  done: ['处理完成', 'Complete']
};
function localizeProgressMessage(message, locale) {
  const pair = progressMessages[message];
  return pair ? localized(locale, pair[0], pair[1]) : message;
}
function runVoxelizer(binary, args, progressChannel = 'voxelize:progress', mapProgress = (value) => value, locale = 'zh-CN') {
  return new Promise((resolve, reject) => {
    const child = spawn(binary, args, { cwd: nativeWorkingDirectory(binary), windowsHide: true });
    activeProcess = child;
    let stdout = ''; let stderr = '';
    const handle = (line, error = false) => {
      const text = line.trim(); if (!text) return;
      const parts = text.split(/\s+/);
      const payload = parts[0] === 'H3DL_PROGRESS'
        ? { progress: mapProgress(Number(parts[1]) / 100), message: localizeProgressMessage(parts.slice(2).join(' '), locale) }
        : { progress: null, message: text, source: error ? 'engine-error' : 'engine' };
      if (mainWindow && !mainWindow.isDestroyed()) mainWindow.webContents.send(progressChannel, payload);
    };
    readline.createInterface({ input: child.stdout }).on('line', (line) => { stdout += `${line}\n`; handle(line); });
    readline.createInterface({ input: child.stderr }).on('line', (line) => { stderr += `${line}\n`; handle(line, true); });
    child.on('error', (error) => { activeProcess = undefined; reject(error); });
    child.on('close', (code) => { activeProcess = undefined; resolve({ code: code ?? -1, stdout, stderr }); });
  });
}
async function voxelize(settings) {
  const locale = settings?.locale;
  if (activeProcess) throw localizedError(locale, '已有任务正在运行，请稍后再试。', 'Another task is already running. Please try again later.');
  const outputFormat = String(settings?.outputFormat || 'stl').toLowerCase();
  if (!settings || typeof settings.inputPath !== 'string' || typeof settings.outputPath !== 'string' || !['obj', 'stl', 'binvox', '3mf'].includes(outputFormat)) {
    throw localizedError(locale, '体素化参数或输出格式无效。', 'Invalid voxelization parameters or output format.');
  }
  if (settings.voxelMode === 'triangle' && !nativeCapabilities().triangleVoxelization) {
    throw localizedError(locale, '三角面方案需要启用 GPU 的构建（macOS Metal 或 Windows Direct3D 11）。', 'Triangle mode requires a GPU-enabled build (Metal on macOS or Direct3D 11 on Windows).');
  }
  if (!fs.existsSync(settings.inputPath)) throw localizedError(locale, '找不到源模型文件。', 'The source model file could not be found.');
  if (path.resolve(settings.inputPath) === path.resolve(settings.outputPath)) throw localizedError(locale, '输出文件不能覆盖源模型。', 'The output file cannot overwrite the source model.');
  const binary = locateVoxkit();
  if (!binary) throw localizedError(locale, '未找到体素化引擎，请检查应用安装是否完整。', 'Voxelization engine not found. Please check that the application is installed correctly.');
  cancelRequested = false;
  let voxelDataPath = settings.outputPath;
  if (outputFormat !== 'binvox') {
    const cacheDirectory = path.join(app.getPath('userData'), 'voxel-cache');
    fs.mkdirSync(cacheDirectory, { recursive: true });
    const cacheName = `${path.basename(settings.inputPath, path.extname(settings.inputPath))}-${Date.now()}-${Math.random().toString(36).slice(2, 10)}.binvox`;
    voxelDataPath = path.join(cacheDirectory, cacheName);
  }
  const args = [settings.inputPath, '-r', String(settings.resolution)];
  if (['pixel', 'triangle', 'quad'].includes(settings.voxelMode)) args.push('--voxel-mode', settings.voxelMode);
  if (settings.splitParts) args.push('--split-parts');
  args.push('-o', voxelDataPath);
  sendProgress({ progress: 0.01, message: localized(locale, '正在生成体素数据', 'Generating voxel data') });
  const voxelProgressScale = outputFormat === 'binvox' ? 1 : 0.78;
  const result = await runVoxelizer(binary, args, 'voxelize:progress', (value) => value * voxelProgressScale, locale);
  if (cancelRequested) return { cancelled: true, outputPath: settings.outputPath, voxelDataPath, outputFormat };
  if (result.code !== 0) throw new Error(result.stderr || localized(locale, `体素化失败（退出码 ${result.code}）`, `Voxelization failed (exit code ${result.code})`));
  const report = {};
  const line = result.stdout.split('\n').find((item) => item.startsWith('VOXKIT_RESULT ')) || '';
  for (const match of line.matchAll(/([a-zA-Z_]+)=("[^"]*"|[^ ]+)/g)) report[match[1]] = match[2].replace(/^"|"$/g, '');
  if (outputFormat !== 'binvox') {
    sendProgress({ progress: 0.8, message: localized(locale, `正在导出 ${outputFormat.toUpperCase()}`, `Exporting ${outputFormat.toUpperCase()}`) });
    const exported = await runVoxelizer(binary, [`--export-${outputFormat}`, voxelDataPath, '-o', settings.outputPath], 'voxelize:progress', (value) => 0.8 + value * 0.2, locale);
    if (cancelRequested) return { cancelled: true, outputPath: settings.outputPath, voxelDataPath, outputFormat };
    if (exported.code !== 0) throw new Error(exported.stderr || localized(locale, `${outputFormat.toUpperCase()} 导出失败（退出码 ${exported.code}）`, `${outputFormat.toUpperCase()} export failed (exit code ${exported.code})`));
  }
  const stats = fs.statSync(settings.outputPath);
  if (!stats.isFile() || stats.size < 4) throw localizedError(locale, `生成的 ${outputFormat.toUpperCase()} 文件无效。`, `The generated ${outputFormat.toUpperCase()} file is invalid.`);
  sendProgress({ progress: 1, message: localized(locale, `${outputFormat.toUpperCase()} 导出完成`, `${outputFormat.toUpperCase()} export complete`) });
  return { sourcePath: settings.inputPath, outputPath: settings.outputPath, voxelDataPath, outputFormat, report, validationPassed: true };
}
async function exportForSnapmakerU1(settings) {
  const locale = settings?.locale;
  if (activeProcess) throw localizedError(locale, '已有任务正在运行，请稍后再试。', 'Another task is already running. Please try again later.');
  if (!settings || typeof settings.inputPath !== 'string' || typeof settings.outputPath !== 'string') {
    throw localizedError(locale, '导出参数无效。', 'Invalid export parameters.');
  }
  if (path.extname(settings.inputPath).toLowerCase() !== '.binvox') throw localizedError(locale, '仅支持从 Binvox 结果导出。', 'Export is supported only from a Binvox result.');
  if (!['.3mf', '.stl'].includes(path.extname(settings.outputPath).toLowerCase())) throw localizedError(locale, 'Snapmaker U1 导出格式必须为 3MF 或 STL。', 'The Snapmaker U1 export format must be 3MF or STL.');
  if (!fs.existsSync(settings.inputPath)) throw localizedError(locale, '找不到 Binvox 输出文件。', 'The Binvox output file could not be found.');
  const binary = locateVoxkit();
  if (!binary) throw localizedError(locale, '未找到体素化引擎，请检查应用安装是否完整。', 'Voxelization engine not found. Please check that the application is installed correctly.');
  const format = path.extname(settings.outputPath).toLowerCase() === '.3mf' ? '3mf' : 'stl';
  const result = await runVoxelizer(binary, [`--export-${format}`, settings.inputPath, '-o', settings.outputPath], 'print-export:progress', (value) => value, locale);
  if (result.code !== 0) throw new Error(result.stderr || localized(locale, `3D 打印导出失败（退出码 ${result.code}）`, `3D-print export failed (exit code ${result.code})`));
  const stats = fs.statSync(settings.outputPath);
  if (!stats.isFile() || stats.size < (format === '3mf' ? 100 : 84)) throw localizedError(locale, `导出的 ${format.toUpperCase()} 文件无效。`, `The exported ${format.toUpperCase()} file is invalid.`);
  const exported = {
    id: `${Date.now()}-${Math.random().toString(36).slice(2, 10)}`,
    exportedAt: new Date().toISOString(),
    operation: 'print-export',
    printer: 'Snapmaker U1',
    printerId: 'snapmaker-u1',
    format: format.toUpperCase(),
    sourcePath: settings.inputPath,
    originalSourcePath: settings.originalSourcePath || settings.inputPath,
    outputPath: settings.outputPath,
    bytes: stats.size,
    conversion: normalizeConversion(settings.conversion)
  };
  addExportRecord(exported);
  return exported;
}
async function exportModel(settings) {
  const locale = settings?.locale;
  if (activeProcess) throw localizedError(locale, '已有任务正在运行，请稍后再试。', 'Another task is already running. Please try again later.');
  const format = String(settings?.format || '').toLowerCase();
  if (!['obj', 'glb', 'stl', '3mf', 'binvox'].includes(format) || typeof settings?.inputPath !== 'string' || typeof settings?.outputPath !== 'string') throw localizedError(locale, '导出参数无效。', 'Invalid export parameters.');
  if (path.extname(settings.inputPath).toLowerCase() !== '.binvox' || !fs.existsSync(settings.inputPath)) throw localizedError(locale, '仅支持从 Binvox 结果导出。', 'Export is supported only from a Binvox result.');
  const expectedExtension = `.${format}`;
  if (path.extname(settings.outputPath).toLowerCase() !== expectedExtension) {
    throw localizedError(locale, `输出文件扩展名必须为 ${expectedExtension}。`, `The output filename extension must be ${expectedExtension}.`);
  }
  if (path.resolve(settings.inputPath) === path.resolve(settings.outputPath)) throw localizedError(locale, '另存文件不能覆盖当前 Binvox 缓存。', 'The exported file cannot overwrite the current Binvox cache.');
  if (format === 'binvox') {
    fs.copyFileSync(settings.inputPath, settings.outputPath);
    const stats = fs.statSync(settings.outputPath);
    if (!stats.isFile() || stats.size < 4) throw localizedError(locale, `导出的 BINVOX 文件无效：${settings.outputPath}`, `The exported BINVOX file is invalid: ${settings.outputPath}`);
    const exported = { id: `${Date.now()}-${Math.random().toString(36).slice(2, 10)}`, exportedAt: new Date().toISOString(), operation: 'model-export', format: 'BINVOX', sourcePath: settings.inputPath, originalSourcePath: settings.originalSourcePath || settings.inputPath, outputPath: settings.outputPath, bytes: stats.size, conversion: normalizeConversion(settings.conversion) };
    addExportRecord(exported);
    return exported;
  }
  const binary = locateVoxkit();
  if (!binary) throw localizedError(locale, '未找到体素化引擎，请检查应用安装是否完整。', 'Voxelization engine not found. Please check that the application is installed correctly.');
  const result = await runVoxelizer(binary, [`--export-${format}`, settings.inputPath, '-o', settings.outputPath], 'model-export:progress', (value) => value, locale);
  if (result.code !== 0) throw new Error(result.stderr.trim() || localized(locale, `模型导出失败（退出码 ${result.code}）`, `Model export failed (exit code ${result.code})`));
  const stats = fs.statSync(settings.outputPath);
  if (!stats.isFile() || stats.size < 4) throw localizedError(locale, `导出的 ${format.toUpperCase()} 文件无效：${settings.outputPath}`, `The exported ${format.toUpperCase()} file is invalid: ${settings.outputPath}`);
  const exported = { id: `${Date.now()}-${Math.random().toString(36).slice(2, 10)}`, exportedAt: new Date().toISOString(), operation: 'model-export', format: format.toUpperCase(), sourcePath: settings.inputPath, originalSourcePath: settings.originalSourcePath || settings.inputPath, outputPath: settings.outputPath, bytes: stats.size, conversion: normalizeConversion(settings.conversion) };
  addExportRecord(exported);
  return exported;
}
function findPrinter(printerId) { return PRINTERS.find((printer) => printer.id === printerId); }
function normalizeConversion(conversion) {
  if (!conversion || typeof conversion !== 'object') return null;
  const allowed = ['sourcePath', 'outputPath', 'voxelDataPath', 'outputFormat', 'format', 'resolution', 'occupied', 'voxelSize', 'voxelMode', 'backend', 'topology', 'validationPassed', 'log'];
  const result = {};
  for (const key of allowed) {
    if (conversion[key] !== undefined && conversion[key] !== null) result[key] = conversion[key];
  }
  return Object.keys(result).length ? result : null;
}
function inspectModel(slicer, sourcePath, locale) {
  return new Promise((resolve, reject) => {
    const executable = slicerExecutable(slicer);
    if (!executable) return reject(localizedError(locale, `未找到 ${slicer.name}。`, `${slicer.name} could not be found.`));
    const child = spawn(executable, ['--info', sourcePath], { stdio: ['ignore', 'pipe', 'pipe'] });
    let output = '';
    child.stdout.on('data', (chunk) => { output += chunk; });
    child.stderr.on('data', (chunk) => { output += chunk; });
    child.on('error', reject);
    child.on('close', (code) => {
      const value = (axis) => Number(output.match(new RegExp(`size_${axis}\\s*=\\s*([0-9.eE+-]+)`))?.[1]);
      const dimensions = ['x', 'y', 'z'].map(value);
      if (code !== 0 || dimensions.some((dimension) => !Number.isFinite(dimension) || dimension <= 0)) {
        reject(localizedError(locale, '无法读取模型尺寸，不能安全执行自动匹配。', 'The model dimensions could not be read, so automatic fitting cannot be performed safely.'));
      } else resolve(dimensions);
    });
  });
}
function fitScale(dimensions, buildVolume, ratio) {
  const limits = buildVolume.map((size) => size * ratio);
  return Math.min(...limits.map((limit, index) => limit / dimensions[index]));
}
function createScaledMesh(sourcePath, scale, extension) {
  const directory = path.join(app.getPath('temp'), APP_NAME, 'scaled-models');
  fs.mkdirSync(directory, { recursive: true });
  const target = path.join(directory, `${path.basename(sourcePath, path.extname(sourcePath))}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}-scaled.${extension}`);
  const source = fs.readFileSync(sourcePath);
  if (extension === 'obj') {
    const text = source.toString('utf8').replace(/^(\s*v\s+)([-+0-9.eE]+)(\s+)([-+0-9.eE]+)(\s+)([-+0-9.eE]+)(.*)$/gm, (_m, p, x, gx, y, gy, z, suffix) => `${p}${Number(x) * scale}${gx}${Number(y) * scale}${gy}${Number(z) * scale}${suffix}`);
    fs.writeFileSync(target, text, 'utf8');
  } else if (source.length >= 84 && source.readUInt32LE(80) * 50 + 84 === source.length) {
    const output = Buffer.from(source);
    for (let triangle = 0; triangle < output.readUInt32LE(80); triangle += 1) {
      const offset = 84 + triangle * 50 + 12;
      for (let vertex = 0; vertex < 9; vertex += 1) output.writeFloatLE(output.readFloatLE(offset + vertex * 4) * scale, offset + vertex * 4);
    }
    fs.writeFileSync(target, output);
  } else {
    const text = source.toString('utf8').replace(/(vertex\s+)([-+0-9.eE]+)(\s+)([-+0-9.eE]+)(\s+)([-+0-9.eE]+)/gi, (_m, p, x, gx, y, gy, z) => `${p}${Number(x) * scale}${gx}${Number(y) * scale}${gy}${Number(z) * scale}`);
    fs.writeFileSync(target, text, 'utf8');
  }
  return target;
}
async function inspectPrintModel(settings) {
  const locale = settings?.locale;
  if (!settings || typeof settings.printerId !== 'string' || typeof settings.sourcePath !== 'string') {
    throw localizedError(locale, '模型检测参数无效。', 'Invalid model inspection parameters.');
  }
  const printer = findPrinter(settings.printerId);
  if (!printer || !printer.enabled || !printer.slicer) throw localizedError(locale, `${printer?.name || '该打印机'} 尚未开放支持。`, `${printer?.name || 'This printer'} is not supported yet.`);
  if (!fs.existsSync(settings.sourcePath)) throw localizedError(locale, '找不到源模型文件，可能已被移动或删除。', 'The source model could not be found. It may have been moved or deleted.');
  const dimensions = await inspectModel(printer.slicer, settings.sourcePath, locale);
  return { printerId: printer.id, printerName: printer.name, buildVolume: printer.buildVolume, dimensions };
}

async function sendToPrinter(settings) {
  const locale = settings?.locale;
  if (!settings || typeof settings.printerId !== 'string' || typeof settings.sourcePath !== 'string') {
    throw localizedError(locale, '打印参数无效。', 'Invalid print parameters.');
  }
  const printer = findPrinter(settings.printerId);
  if (!printer) throw localizedError(locale, '不支持的打印机。', 'Unsupported printer.');
  if (!printer.enabled || !printer.slicer) throw localizedError(locale, `${printer.name} 尚未开放支持。`, `${printer.name} is not supported yet.`);
  const slicer = printer.slicer;
  if (!path.isAbsolute(settings.sourcePath)) throw localizedError(locale, '源文件路径无效。', 'Invalid source file path.');
  let sourcePath = settings.sourcePath;
  let sourceStats;
  try {
    sourceStats = fs.statSync(sourcePath);
  } catch {
    throw localizedError(locale, '找不到源模型文件，可能已被移动或删除。', 'The source model could not be found. It may have been moved or deleted.');
  }
  let extension = path.extname(sourcePath).slice(1).toLowerCase();
  if (!sourceStats.isFile()) throw localizedError(locale, '源模型文件无效。', 'The source model file is invalid.');
  if (extension === 'binvox') {
    const binary = locateVoxkit();
    if (!binary) throw localizedError(locale, '未找到体素化引擎，请检查应用安装是否完整。', 'Voxelization engine not found. Please check that the application is installed correctly.');
    const printDirectory = path.join(app.getPath('temp'), 'hands3dlab-voxkit-print');
    fs.mkdirSync(printDirectory, { recursive: true });
    const voxel3mfPath = path.join(printDirectory, `${path.basename(sourcePath, '.binvox')}-${Date.now()}.3mf`);
    const exportResult = await runVoxelizer(binary, ['--export-3mf', sourcePath, '-o', voxel3mfPath], 'print-export:progress', (value) => value, locale);
    if (exportResult.code !== 0) throw new Error(exportResult.stderr || localized(locale, `体素 3MF 导出失败（退出码 ${exportResult.code}）`, `Voxel 3MF export failed (exit code ${exportResult.code})`));
    sourcePath = voxel3mfPath;
    sourceStats = fs.statSync(sourcePath);
    extension = '3mf';
  }
  if (!slicer.extensions.includes(extension)) {
    throw localizedError(locale, `${printer.name} 暂不支持直接导入 .${extension || '该'} 文件。`, `${printer.name} cannot directly import .${extension || 'this'} files yet.`);
  }
  const ratio = settings.ratio == null ? null : Number(settings.ratio);
  const isOriginal = ratio === null || !Number.isFinite(ratio) || ratio <= 0;
  let scale = 1;
  let dimensions;
  let openedPath = sourcePath;
  if (!isOriginal) {
    dimensions = await inspectModel(slicer, sourcePath, locale);
    scale = fitScale(dimensions, printer.buildVolume, ratio);
    if (!Number.isFinite(scale) || scale <= 0) throw localizedError(locale, '无法计算模型缩放比例。', 'The model scale could not be calculated.');
    if (extension === '3mf') throw localizedError(locale, '3MF 暂不支持自动缩放；请先选择原始尺寸或导出 STL 后缩放。', 'Automatic scaling is not yet supported for 3MF. Use the original size or export STL before scaling.');
    openedPath = createScaledMesh(sourcePath, scale, extension);
  }
  if (process.platform === 'darwin') {
    await new Promise((resolve, reject) => {
      const args = ['-b', slicer.bundleId, openedPath];
      const child = spawn('open', args, { detached: true, stdio: 'ignore' });
      child.on('error', reject);
      child.on('close', (code) => code === 0
        ? resolve()
        : reject(localizedError(locale, `未找到 ${slicer.name}，请先安装后再导入模型。`, `${slicer.name} was not found. Install it before importing the model.`)));
    });
  } else {
    const executable = slicerExecutable(slicer);
    const error = executable
      ? await new Promise((resolve) => {
          const child = spawn(executable, [openedPath], { detached: true, stdio: 'ignore' });
          child.once('error', (spawnError) => resolve(spawnError.message));
          child.once('spawn', () => { child.unref(); resolve(''); });
        })
      : await shell.openPath(openedPath);
    if (error) throw new Error(error);
  }
  const record = {
    id: `${Date.now()}-${Math.random().toString(36).slice(2, 10)}`,
    exportedAt: new Date().toISOString(),
    operation: 'direct-send',
    printerId: printer.id,
    printerName: printer.name,
    slicerName: slicer.name,
    format: extension.toUpperCase(),
    sourcePath: settings.sourcePath,
    originalSourcePath: settings.originalSourcePath || settings.sourcePath,
    outputPath: openedPath,
    bytes: sourceStats.size,
    sizeMode: isOriginal ? 'original' : 'ratio',
    ratio: isOriginal ? null : ratio,
    originalDimensions: dimensions,
    scale,
    fittedDimensions: dimensions?.map((dimension) => dimension * scale),
    buildVolume: printer.buildVolume,
    conversion: normalizeConversion(settings.conversion)
  };
  addExportRecord(record);
  return record;
}
function createWindow() {
  const appIcon = loadAppIcon();
  const macWindowChrome = process.platform === 'darwin'
    ? { titleBarStyle: 'hiddenInset', trafficLightPosition: { x: 18, y: 18 } }
    : {};
  mainWindow = new BrowserWindow({
    width: 1180,
    height: 760,
    minWidth: 900,
    minHeight: 640,
    title: APP_NAME,
    ...(appIcon ? { icon: appIcon } : {}),
    backgroundColor: '#f4f6f8',
    ...macWindowChrome,
    webPreferences: { preload: path.join(__dirname, 'preload.cjs'), contextIsolation: true, nodeIntegration: false, sandbox: false, webviewTag: false }
  });
  if (process.platform === 'win32') {
    mainWindow.setMenuBarVisibility(false);
    mainWindow.setAutoHideMenuBar(true);
  }
  const openInSystemBrowser = (url) => {
    if (/^https?:\/\//i.test(url)) shell.openExternal(url);
  };

  // VoxKit 是封闭的桌面工作区：不允许网页在 Electron 内跳转、打开标签页或创建浏览器窗口。
  // 外部网站统一交给系统默认浏览器，当前转换界面保持不变。
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    openInSystemBrowser(url);
    return { action: 'deny' };
  });
  mainWindow.webContents.on('will-navigate', (event, url) => {
    event.preventDefault();
    openInSystemBrowser(url);
  });
  const rendererPath = app.isPackaged
    ? path.join(electronRoot(), 'renderer', 'index.html')
    : path.join(projectRoot(), 'electron', 'renderer', 'index.html');
  mainWindow.loadFile(rendererPath);
}
app.whenReady().then(() => {
  app.setName(APP_NAME);
  if (process.platform === 'win32') Menu.setApplicationMenu(null);
  const appIcon = loadAppIcon();
  if (process.platform === 'darwin' && appIcon) app.dock.setIcon(appIcon);
  ipcMain.handle('dialog:open-input', async (_event, settings = {}) => {
    const extensions = Array.isArray(settings) ? settings : settings.extensions;
    const locale = Array.isArray(settings) ? 'zh-CN' : settings.locale;
    const result = await dialog.showOpenDialog(mainWindow, { properties: ['openFile'], filters: [{ name: localized(locale, '网格模型', 'Mesh models'), extensions }] });
    return result.canceled ? null : result.filePaths[0];
  });
  ipcMain.handle('about:config', () => readAboutConfig());
  ipcMain.handle('app:capabilities', () => ({ ...nativeCapabilities(), platform: process.platform }));
  ipcMain.handle('dialog:save-output', async (_event, settings = {}) => {
    const format = String(settings.format || 'stl').toLowerCase();
    if (!['obj', 'stl', 'binvox', '3mf'].includes(format)) throw localizedError(settings.locale, '不支持的体素输出格式。', 'Unsupported voxel output format.');
    const names = isEnglish(settings.locale)
      ? { obj: 'OBJ voxel mesh', stl: 'STL 3D-print mesh', binvox: 'Binvox voxel data', '3mf': '3MF color print model' }
      : { obj: 'OBJ 体素网格', stl: 'STL 3D 打印网格', binvox: 'Binvox 体素数据', '3mf': '3MF 彩色打印模型' };
    const result = await dialog.showSaveDialog(mainWindow, { defaultPath: settings.defaultPath, filters: [{ name: names[format], extensions: [format] }] });
    if (result.canceled || !result.filePath) return null;
    return path.extname(result.filePath).toLowerCase() === `.${format}` ? result.filePath : `${result.filePath}.${format}`;
  });
  ipcMain.handle('dialog:save-print-output', async (_event, defaultPath) => {
    ensureExportRecordsDirectory();
    const result = await dialog.showSaveDialog(mainWindow, {
      defaultPath: path.join(exportRecordsDirectory(), path.basename(defaultPath || 'Snapmaker-U1.stl')),
      filters: [{ name: 'Snapmaker Orca 网格文件', extensions: ['stl'] }]
    });
    if (result.canceled || !result.filePath) return null;
    return path.extname(result.filePath).toLowerCase() === '.stl' ? result.filePath : `${result.filePath}.stl`;
  });
  ipcMain.handle('dialog:save-model-output', async (_event, settings = {}) => {
    const format = String(settings.format || '').toLowerCase();
    const extensions = ['obj', 'glb', 'stl', '3mf', 'binvox'];
    if (!extensions.includes(format)) throw localizedError(settings.locale, '不支持的导出格式。', 'Unsupported export format.');
    ensureExportRecordsDirectory();
    const defaultPath = path.join(exportRecordsDirectory(), path.basename(settings.defaultPath || `VoxKit-export.${format}`));
    const filterName = localized(settings.locale, `${format.toUpperCase()} 网格文件`, `${format.toUpperCase()} mesh file`);
    const result = await dialog.showSaveDialog(mainWindow, { defaultPath, filters: [{ name: filterName, extensions: [format] }] });
    if (result.canceled || !result.filePath) return null;
    return path.extname(result.filePath).toLowerCase() === `.${format}` ? result.filePath : `${result.filePath}.${format}`;
  });
  ipcMain.handle('voxelize:start', (_event, settings) => voxelize(settings));
  ipcMain.handle('print-export:snapmaker-u1', (_event, settings) => recordExportFailure('print-export', settings, () => exportForSnapmakerU1(settings)));
  ipcMain.handle('model:export', (_event, settings) => recordExportFailure('model-export', settings, () => exportModel(settings)));
  ipcMain.handle('printers:list', () => PRINTERS.map(({ id, name, enabled, buildVolume }) => ({ id, name, enabled, buildVolume })));
  ipcMain.handle('print:inspect', (_event, settings) => inspectPrintModel(settings));
  ipcMain.handle('print:send', (_event, settings) => sendToPrinter(settings));
  ipcMain.handle('export-history:list', () => ({
    records: readExportRecords(),
    directory: exportRecordsDirectory()
  }));
  ipcMain.handle('export-history:remove', (_event, id) => {
    if (typeof id !== 'string') return false;
    const records = readExportRecords();
    const next = records.filter((record) => record.id !== id);
    if (next.length === records.length) return false;
    writeExportRecords(next);
    return true;
  });
  ipcMain.handle('export-history:open-directory', async () => {
    ensureExportRecordsDirectory();
    const error = await shell.openPath(exportRecordsDirectory());
    if (error) throw new Error(error);
    return true;
  });
  ipcMain.handle('export-history:open-file', async (_event, filePath) => {
    if (typeof filePath !== 'string' || !path.isAbsolute(filePath) || !fs.existsSync(filePath)) return false;
    const error = await shell.openPath(filePath);
    if (error) throw new Error(error);
    return true;
  });
  ipcMain.handle('print-export:open-slicer', async (_event, filePath, locale) => {
    if (!filePath || !['.stl', '.3mf'].includes(path.extname(filePath).toLowerCase()) || !fs.existsSync(filePath)) return false;
    if (process.platform === 'darwin') {
      return new Promise((resolve, reject) => {
        const child = spawn('open', ['-b', 'com.snapmaker.snapmaker-orca', filePath], { stdio: 'ignore' });
        child.on('error', reject);
        child.on('close', async (code) => {
          if (code === 0) resolve(true);
          else {
            const error = await shell.openPath(filePath);
            if (error) reject(localizedError(locale, '未找到 Snapmaker Orca，请先安装后再打开 STL。', 'Snapmaker Orca was not found. Install it before opening the STL file.'));
            else resolve(true);
          }
        });
      });
    }
    const error = await shell.openPath(filePath);
    if (error) throw new Error(error);
    return true;
  });
  ipcMain.handle('voxelize:cancel', () => { cancelRequested = true; if (activeProcess) activeProcess.kill('SIGTERM'); return true; });
  ipcMain.handle('shell:show-item', (_event, filePath) => {
    if (typeof filePath !== 'string' || !path.isAbsolute(filePath) || !fs.existsSync(filePath)) return false;
    shell.showItemInFolder(filePath);
    return true;
  });
  createWindow();
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
