(() => {
  'use strict';

  const legacyTranslations = {
    '输入格式': 'Input format', '导出记录': 'Export history', '关于此应用': 'About this app',
    '模型体素化': 'Voxelize model', '关于 VoxKit': 'About VoxKit', '文件设置': 'File setup',
    'OBJ 参数': 'OBJ parameters', 'GLB 参数': 'GLB parameters', 'STL 参数': 'STL parameters',
    '拖入或选择源模型': 'Drop or choose a source model', '选择…': 'Choose…', '选择文件': 'Choose file',
    '更改文件': 'Change file', '支持 OBJ 格式': 'OBJ format supported', '支持 GLB 格式': 'GLB format supported',
    '支持 STL 格式': 'STL format supported', '体素导出格式': 'Voxel export format',
    'STL 为默认格式；3MF 可保留颜色': 'STL is default; 3MF preserves colors',
    '输出文件保存位置': 'Output file location', '选择位置': 'Choose location', '更改位置': 'Change location',
    '选择 STL 文件保存位置': 'Choose an STL output location', '下一步': 'Next', '开始转换': 'Start conversion',
    'OBJ 体素参数': 'OBJ voxel parameters', 'GLB 转换参数': 'GLB conversion parameters', 'STL 转换参数': 'STL conversion parameters',
    '体素分辨率': 'Voxel resolution', '边界盒最长边': 'Longest bounding-box edge', '体素化方案': 'Voxelization method',
    '三角面方案使用 GPU': 'Triangle mode uses the GPU', '三角面方案需要启用 GPU 的构建（macOS Metal 或 Windows Direct3D 11）。': 'Triangle mode requires a GPU-enabled build (Metal on macOS or Direct3D 11 on Windows).', '像素化': 'Pixel', '三角面 GPU': 'Triangle GPU', '四边面': 'Quad',
    'Windows Direct3D 11 不可用': 'Windows Direct3D 11 unavailable', '请运行 Windows Update，并从 NVIDIA、AMD 或 Intel 官网安装最新显卡驱动。': 'Run Windows Update and install the latest graphics driver from NVIDIA, AMD, or Intel.', '打开 Microsoft Windows 更新说明': 'Open Microsoft Windows Update guidance',
    '填充模式': 'Fill mode', '输出体素内部': 'Fill voxel interior', '表面': 'Surface', '实体填充': 'Solid fill',
    '体素化质量': 'Voxelization quality', '速度与细节平衡': 'Balance speed and detail', '快速': 'Fast', '均衡': 'Balanced', '高质量': 'Quality',
    '模型预览': 'Model preview', '完成后生成': 'Generate after completion', '生成预览': 'Generate preview', '仅输出数据': 'Data only',
    '是否拆分零件': 'Split parts', '识别体素中相互独立的零件': 'Identify independent voxel parts', '是': 'Yes', '否': 'No',
    '正在处理': 'Processing', '取消转换': 'Cancel conversion', '返回': 'Back', '账号与联系': 'Account & contact',
    '欢迎关注': 'Stay connected', '更新日志': 'Changelog', '版本记录': 'Version history', '离线 3D 模型体素化工具': 'Offline 3D model voxelizer',
    '体素报告': 'Voxel report', '在 Finder 中显示': 'Reveal in Finder', '查看处理日志': 'View processing log',
    '转换完成': 'Conversion complete', '体素模型已就绪': 'Voxel model is ready', '开始新的转换': 'New conversion',
    '另存格式': 'Export as', '导出 OBJ': 'Export OBJ', '导出 GLB': 'Export GLB', '导出 STL': 'Export STL', '导出 3MF': 'Export 3MF',
    '通用网格模型': 'General mesh model', '二进制 glTF 模型': 'Binary glTF model', '单色 3D 打印网格': 'Monochrome 3D-print mesh',
    '保留体素颜色': 'Preserve voxel colors', '开始 3D 打印': 'Start 3D printing', '刷新': 'Refresh', '打开记录文件夹': 'Open history folder',
    '关闭': 'Close', '打印机': 'Printer', '确认发送打印': 'Send to printer', '取消': 'Cancel', '成型比例': 'Print scale',
    '原始比例': 'Original scale', '源格式': 'Source format', '分辨率': 'Resolution', '占用体素': 'Occupied voxels', '体素尺寸': 'Voxel size',
    '表面拓扑': 'Surface topology', '计算后端': 'Compute backend', '输出格式': 'Output format', '输入文件': 'Input file', '输出文件': 'Output file',
    '写出': 'Write', '通过': 'Passed', '状态': 'Status', '完成': 'Complete', '警告': 'Warning'
    , '已生成，存在质量警告': 'Generated with quality warnings', '转换已取消': 'Conversion cancelled', '转换失败': 'Conversion failed',
    '目标文件未被修改。': 'The target file was not modified.', '未生成输出文件': 'No output file generated', '无法进行 3D 打印': '3D printing unavailable',
    '准备导出': 'Preparing export', '正在导出': 'Exporting', '导出完成': 'Export complete', '导出失败': 'Export failed',
    '正在读取体素数据': 'Reading voxel data', '正在构建打印网格': 'Building print mesh', '正在准备模型': 'Preparing model',
    '发送失败': 'Send failed', '重试': 'Retry', '正在准备体素网格': 'Preparing voxel mesh', '正在准备体素模型': 'Preparing voxel model',
    '未命名体素模型': 'Unnamed voxel model', '由切片软件决定模型尺寸': 'Model size is determined by slicer', '体素化': 'voxelized',
    '该记录未保存处理日志。': 'No processing log was saved for this record.', '未知文件': 'Unknown file', '未知时间': 'Unknown time',
    '暂无导出记录': 'No export history', '无法读取导出记录': 'Unable to read export history', '用户文稿文件夹': 'Documents folder',
    '条记录': ' records', '扫码': 'Scan', '访问': 'Visit', '即将支持': 'Coming soon', '成型空间': 'build volume',
    '原始尺寸': 'Original size', '匹配': 'Fit', '等比缩放至成型空间的': 'Scale proportionally to', '保持模型原始尺寸直接发送到切片软件': 'Send at original size to slicer',
    '请先选择源模型与输出位置': 'Choose a source model and output location first', '请先完成体素化，再开始 3D 打印。': 'Complete voxelization before starting 3D printing.',
    '没有可用的打印机。': 'No available printers.', '模型导出失败：': 'Model export failed: ', '转换失败：': 'Conversion failed: ',
    '无法读取打印机列表：': 'Unable to read printer list: ', '已导出': 'Exported', '已发送到': 'Sent to', '打印文件': 'print file'
  };
  const extraTranslations = {
    '收起边栏': 'Collapse sidebar', '展开边栏': 'Expand sidebar', 'OBJ 模型': 'OBJ model', 'GLB 场景': 'GLB scene', 'STL 网格': 'STL mesh',
    'Hands3DLab-VoxKit · 模型体素化': 'Hands3DLab-VoxKit · Voxelize Model', '切换到 English': 'Switch to English', '0 条记录': '0 records',
    '查阅导出记录': 'View export history', '关于 Hands3DLab-VoxKit': 'About Hands3DLab-VoxKit', '切换语言': 'Switch language', '关于': 'About',
    '转换过程中不会修改源文件。': 'The source file will not be modified during conversion.', '3D 打印': '3D printing',
    '选择打印机': 'Choose a printer', '模型原始比例': 'Original model scale', '正在发送': 'Sending', '导出记录详情': 'Export details',
    '查看详情': 'View details', '打开文件': 'Open file', '从记录中移除': 'Remove from history', '鼠标移入查看二维码': 'Hover to view QR code',
    '打开主页': 'Open profile', '访问官网': 'Visit website', '官网': 'Website', '微信': 'WeChat', '视频号': 'WeChat Channels', '抖音': 'Douyin', '小红书': 'RED', 'B站': 'Bilibili',
    '准备就绪': 'Ready', '请选择源模型与输出位置': 'Choose a source model and output location', '进入参数设置': 'Continue to parameters', '请先选择源模型和输出位置': 'Choose a source model and output location first',
    '体素方案': 'Voxel method', '方块像素': 'Block voxels', '连续三角面': 'Continuous triangles', '连续四边面': 'Continuous quads', '失败': 'Failed',
    '转换未生成有效文件': 'No valid output was generated', '正在准备模型': 'Preparing model', '正在构建打印网格，请稍候': 'Building the print mesh. Please wait.',
    '暂无导出记录': 'No export history', '生成打印文件或将原始模型发送到打印机后，记录会显示在这里。': 'Records appear here after you export a print file or send a model to a printer.',
    '正在读取导出记录…': 'Loading export history…', '文件已被移动或删除。': 'The file was moved or deleted.', '正在取消转换…': 'Cancelling conversion…',
    '访问': 'Visit', '扫码': 'Scan', '打印机': 'Printer', '关闭': 'Close', '刷新': 'Refresh', '返回': 'Back',
    '正在处理': 'Processing', '目标文件未被修改': 'The target file was not modified', '正在准备体素数据': 'Preparing voxel data',
    '导出服务尚未加载，请完全退出并重新启动应用。': 'The export service is unavailable. Quit and restart the app.',
    '当前结果缺少体素缓存，请重新执行转换后再导出。': 'The voxel cache is missing. Run the conversion again before exporting.',
    '完成': 'Done', '转换未生成有效文件': 'No valid output was generated', '无法进行 3D 打印': '3D printing is unavailable',
    '将 OBJ 网格模型转换为可打印的体素模型。': 'Convert an OBJ mesh into a printable voxel model.',
    '选择打印机并调整成型比例，将原始模型直接发送到对应切片软件': 'Choose a printer and print scale, then send the original model directly to its slicer.'
  };

  const sourceMessages = { ...legacyTranslations, ...extraTranslations };
  const messageKeys = new Map(Object.keys(sourceMessages).map((zh, index) => [zh, `legacy.${index}`]));
  const messages = {
    'zh-CN': Object.fromEntries([...messageKeys].map(([zh, key]) => [key, zh])),
    en: Object.fromEntries([...messageKeys].map(([zh, key]) => [key, sourceMessages[zh]]))
  };
  Object.assign(messages['zh-CN'], {
    'app.title': 'Hands3DLab-VoxKit · 模型体素化',
    'language.switch': '切换到 English',
    'format.subtitle.obj': '将 OBJ 网格模型转换为所选格式的体素模型。',
    'format.subtitle.glb': '将 GLB 场景网格转换为所选格式的体素模型。',
    'format.subtitle.stl': '将 ASCII 或二进制 STL 网格转换为所选格式的体素模型。',
    'file.supported': '支持 {format} 格式', 'file.outputLocation': '选择 {format} 文件保存位置', 'file.chooseExtension': '请选择 .{format} 文件',
    'result.warning': '{format} 文件已生成，但引擎返回质量警告，建议检查输出数据。', 'result.success': '{format} 体素文件已成功生成。',
    'result.ready': '{format} 体素模型已就绪', 'export.running': '正在导出 {format} 文件', 'export.generated': '{format} 文件已生成',
    'export.log': '已导出 {format}：{path}', 'printer.volume': '{name} · {volume} mm 成型空间', 'printer.coming': '{name}（即将支持）',
    'printer.modelVoxelized': '{name}（体素化）', 'printer.buildVolume': '成型空间 {volume} mm', 'ratio.fit': '等比缩放至成型空间的 {percent}%',
    'print.reading': '正在读取 {format} 体素结果', 'print.sending': '正在发送到 {name}', 'print.scaling': '正在按 {percent}% 成型比例准备体素模型',
    'print.sentLog': '已将体素模型发送到 {printer}（{slicer}）：{path}', 'print.fit': '匹配 {size} mm（{percent}%）', 'print.sent': '已发送到 {name}',
    'history.count': '{count} 条记录', 'history.sent': '已发送到 {name}', 'history.exported': '已导出 {name}',
    'detail.sent': '该记录由「开始 3D 打印」发送到 {name}', 'detail.exported': '该记录由「导出」生成 {name}',
    'conversion.started': '开始转换：{name}', 'error.exportFormat': '不支持的导出格式：{format}', 'error.export': '模型导出失败：{message}',
    'error.conversion': '转换失败：{message}', 'error.printers': '无法读取打印机列表：{message}', 'error.reveal': '无法在 Finder 中显示文件：{message}',
    'error.about': '无法读取关于页配置：{message}', 'error.sendPrinter': '发送到打印机失败：{message}', 'qr.alt': '{name}二维码'
  });
  Object.assign(messages.en, {
    'app.title': 'Hands3DLab-VoxKit · Voxelize Model', 'language.switch': 'Switch to Simplified Chinese',
    'format.subtitle.obj': 'Convert an OBJ mesh into a voxel model in the selected format.', 'format.subtitle.glb': 'Convert a GLB scene mesh into a voxel model in the selected format.', 'format.subtitle.stl': 'Convert an ASCII or binary STL mesh into a voxel model in the selected format.',
    'file.supported': '{format} format supported', 'file.outputLocation': 'Choose a location for the {format} file', 'file.chooseExtension': 'Choose a .{format} file',
    'result.warning': 'The {format} file was generated, but the engine reported quality warnings. Please inspect the output.', 'result.success': 'The {format} voxel file was generated successfully.',
    'result.ready': '{format} voxel model is ready', 'export.running': 'Exporting {format} file', 'export.generated': '{format} file generated',
    'export.log': 'Exported {format}: {path}', 'printer.volume': '{name} · {volume} mm build volume', 'printer.coming': '{name} (coming soon)',
    'printer.modelVoxelized': '{name} (voxelized)', 'printer.buildVolume': 'Build volume {volume} mm', 'ratio.fit': 'Scale proportionally to {percent}% of the build volume',
    'print.reading': 'Reading {format} voxel result', 'print.sending': 'Sending to {name}', 'print.scaling': 'Preparing the voxel model at {percent}% print scale',
    'print.sentLog': 'Sent voxel model to {printer} ({slicer}): {path}', 'print.fit': 'Fit to {size} mm ({percent}%)', 'print.sent': 'Sent to {name}',
    'history.count': '{count} records', 'history.sent': 'Sent to {name}', 'history.exported': 'Exported {name}',
    'detail.sent': 'Sent from “Start 3D Printing” to {name}', 'detail.exported': 'Generated by “Export” as {name}',
    'conversion.started': 'Starting conversion: {name}', 'error.exportFormat': 'Unsupported export format: {format}', 'error.export': 'Model export failed: {message}',
    'error.conversion': 'Conversion failed: {message}', 'error.printers': 'Unable to load printers: {message}', 'error.reveal': 'Unable to reveal the file in Finder: {message}',
    'error.about': 'Unable to load About configuration: {message}', 'error.sendPrinter': 'Failed to send to printer: {message}', 'qr.alt': '{name} QR code'
  });
  const languageKey = 'voxkit-language';
  let language = localStorage.getItem(languageKey) === 'en' ? 'en' : 'zh-CN';
  const interpolate = (template, params) => String(template).replace(/\{(\w+)\}/g, (_match, name) => params[name] == null ? '' : String(params[name]));
  const t = (key, params = {}) => interpolate(messages[language][key] ?? messages['zh-CN'][key] ?? key, params);
  const textKey = (value) => messageKeys.get(String(value).trim());
  const originalText = new WeakMap();
  const originalAttributes = new WeakMap();
  const translateStaticText = () => {
    document.documentElement.lang = language;
    document.title = t('app.title');
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
    let node;
    while ((node = walker.nextNode())) {
      if (!node.nodeValue.trim()) continue;
      if (!originalText.has(node)) originalText.set(node, node.nodeValue);
      const source = originalText.get(node);
      const key = textKey(source);
      if (key) node.nodeValue = source.replace(source.trim(), t(key));
    }
    $$('[title],[aria-label]').forEach((element) => {
      if (!originalAttributes.has(element)) originalAttributes.set(element, {});
      const originals = originalAttributes.get(element);
      ['title', 'aria-label'].forEach((attribute) => {
        if (!(attribute in originals)) originals[attribute] = element.getAttribute(attribute);
        const key = textKey(originals[attribute]);
        if (key) element.setAttribute(attribute, t(key));
      });
    });
    $('languageToggle').textContent = language === 'en' ? '中' : 'EN';
    $('languageToggle').title = t('language.switch');
    $('languageToggle').setAttribute('aria-label', t('language.switch'));
  };

  const formats = {
    obj: { title: 'OBJ', subtitleKey: 'format.subtitle.obj' },
    glb: { title: 'GLB', subtitleKey: 'format.subtitle.glb' },
    stl: { title: 'STL', subtitleKey: 'format.subtitle.stl' }
  };

  const state = {
    format: 'obj', outputFormat: 'stl', step: 1, inputPath: '', outputPath: '', outputPathAuto: true, resolution: 128, voxelMode: 'pixel', fill: 'surface', quality: 'balanced', preview: 'yes', splitParts: 'no',
    converting: false, progress: 0, logs: [], result: null, exportingPrint: false, printOutputPath: '',
    conversion: null, printers: [], historyRecords: [], pendingPrinterId: '', directPrintState: 'idle', triangleVoxelization: false
  };

  const $ = (id) => document.getElementById(id);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
  const inputExtension = () => [state.format];
  const basename = (filePath) => filePath.split(/[\\/]/).pop();
  const withoutExtension = (filePath) => filePath.replace(/\.[^./\\]+$/, '');
  const pathExtension = (filePath) => String(filePath || '').split('.').pop().toLowerCase();
  const defaultOutputPath = () => state.inputPath ? `${withoutExtension(state.inputPath)}-voxel.${state.outputFormat}` : '';
  const escapeHtml = (value) => String(value).replace(/[&<>"']/g, (character) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[character]);
  const formatBytes = (bytes) => {
    if (!Number.isFinite(bytes) || bytes < 0) return '—';
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
  };

  function localizeText(value) {
    const text = String(value ?? '');
    const key = textKey(text);
    return key ? t(key) : text;
  }
  function setText(id, value) { $(id).textContent = localizeText(value); }
  function appendLog(message) {
    if (!message) return;
    const time = new Date().toLocaleTimeString(language === 'en' ? 'en-US' : 'zh-CN', { hour12: false });
    state.logs.push(`[${time}] ${message}`);
    if (state.logs.length > 500) state.logs.splice(0, state.logs.length - 500);
    setText('conversionLog', state.logs.join('\n'));
    setText('resultLog', state.logs.join('\n'));
    const log = $('conversionLog');
    log.scrollTop = log.scrollHeight;
  }

  function syncFormatUI() {
    $$('.format-item').forEach((button) => button.classList.toggle('active', button.dataset.format === state.format));
    const format = formats[state.format];
    setText('pageSubtitle', t(format.subtitleKey));
    setText('parameterStepLabel', `${format.title} 参数`);
    setText('parameterHeading', `${format.title} 转换参数`);
    setText('inputPath', state.inputPath ? state.inputPath : t('file.supported', { format: format.title }));
    setText('inputName', state.inputPath ? basename(state.inputPath) : '拖入或选择源模型');
    setText('inputAction', state.inputPath ? '更改文件' : '选择文件');
    setSegmentedValue($('outputFormat'), state.outputFormat);
    setText('outputPath', state.outputPath || t('file.outputLocation', { format: state.outputFormat.toUpperCase() }));
    setText('outputAction', state.outputPath ? '更改位置' : '选择位置');
    $('inputPicker').classList.toggle('has-file', Boolean(state.inputPath));
    updateReadiness();
  }

  function updateReadiness() {
    const ready = Boolean(state.inputPath && state.outputPath);
    setText('readinessTitle', ready ? '准备就绪' : '请选择源模型与输出位置');
    const action = $('primaryAction');
    action.disabled = state.converting;
    if (state.step === 1) {
      action.innerHTML = `${escapeHtml(localizeText('下一步'))} <i class="ph-thin ph-arrow-right" aria-hidden="true"></i>`;
      action.title = localizeText(ready ? '进入参数设置' : '请先选择源模型和输出位置');
    } else {
      action.innerHTML = `${escapeHtml(localizeText('开始转换'))} <i class="ph-thin ph-play" aria-hidden="true"></i>`;
      action.title = localizeText(ready ? '开始转换' : '请先选择源模型和输出位置');
    }
  }

  function setStep(step) {
    if (state.converting) return;
    if (step === 2 && (!state.inputPath || !state.outputPath)) {
      appendLog('请先选择源模型与输出位置');
      return;
    }
    state.step = step;
    $('fileStep').classList.toggle('hidden', step !== 1);
    $('parameterStep').classList.toggle('hidden', step !== 2);
    $$('.step-button').forEach((button) => {
      const active = Number(button.dataset.step) === step;
      button.classList.toggle('active', active);
      button.disabled = Number(button.dataset.step) === 2 && (!state.inputPath || !state.outputPath);
    });
    updateReadiness();
  }

  async function chooseInput() {
    if (state.converting) return;
    const selected = await window.voxkit.pickInput(inputExtension(), language);
    if (selected) setInput(selected);
  }

  function setInput(filePath) {
    if (!filePath || filePath.toLowerCase().split('.').pop() !== state.format) {
      appendLog(t('file.chooseExtension', { format: state.format }));
      return;
    }
    state.inputPath = filePath;
    state.outputPath = defaultOutputPath();
    state.outputPathAuto = true;
    state.result = null;
    syncFormatUI();
  }

  async function chooseOutput() {
    if (state.converting) return;
    const selected = await window.voxkit.pickOutput({ format: state.outputFormat, defaultPath: state.outputPath || defaultOutputPath() || undefined, locale: language });
    if (selected) { state.outputPath = selected; state.outputPathAuto = false; state.result = null; syncFormatUI(); }
  }

  function setSegmentedValue(group, value) {
    $$('button', group).forEach((button) => button.classList.toggle('selected', button.dataset.value === value));
  }

  function applyVoxelCapabilities(capabilities = {}) {
    state.triangleVoxelization = capabilities.triangleVoxelization === true;
    const triangleButton = $('parameterStep').querySelector('[data-setting="voxelMode"] button[data-value="triangle"]');
    if (!triangleButton) return;
    const guide = $('d3dGuide') || (() => {
      const element = document.createElement('div');
      element.id = 'd3dGuide'; element.className = 'd3d-guide'; element.setAttribute('role', 'status');
      element.innerHTML = '<strong></strong><p></p><a href="https://support.microsoft.com/windows/update-windows" target="_blank" rel="noopener noreferrer"></a>';
      $('parameterStep').prepend(element); return element;
    })();
    triangleButton.disabled = !state.triangleVoxelization || state.converting;
    if (!state.triangleVoxelization) {
      triangleButton.title = localizeText('三角面方案需要启用 GPU 的构建（macOS Metal 或 Windows Direct3D 11）。');
      triangleButton.setAttribute('aria-label', `${triangleButton.textContent} — ${triangleButton.title}`);
      guide.classList.toggle('hidden', !capabilities.gpuError && capabilities.platform !== 'win32');
      if (capabilities.platform === 'win32') {
        guide.querySelector('strong').textContent = localizeText('Windows Direct3D 11 不可用');
        guide.querySelector('p').textContent = localizeText('请运行 Windows Update，并从 NVIDIA、AMD 或 Intel 官网安装最新显卡驱动。');
        guide.querySelector('a').textContent = localizeText('打开 Microsoft Windows 更新说明');
      }
      if (state.voxelMode === 'triangle') {
        state.voxelMode = 'pixel';
        setSegmentedValue(triangleButton.parentElement, state.voxelMode);
      }
    } else {
      guide.classList.add('hidden');
      triangleButton.removeAttribute('title');
      triangleButton.removeAttribute('aria-label');
    }
  }

  function collectSettings() {
    return { inputPath: state.inputPath, outputPath: state.outputPath, outputFormat: state.outputFormat, resolution: state.resolution, voxelMode: state.voxelMode, splitParts: state.splitParts === 'yes' };
  }

  function setConverting(converting) {
    state.converting = converting;
    $('progressPanel').classList.toggle('hidden', !converting);
    $('cancelConversion').classList.toggle('hidden', !converting);
    $('primaryAction').classList.toggle('hidden', converting);
    $('inputPicker').disabled = converting;
    $('outputPicker').disabled = converting;
    $$('button', $('outputFormat')).forEach((button) => { button.disabled = converting; });
    $$('.format-item').forEach((button) => { button.disabled = converting; });
    applyVoxelCapabilities({ triangleVoxelization: state.triangleVoxelization });
    $$('.step-button').forEach((button) => { button.disabled = converting || (Number(button.dataset.step) === 2 && (!state.inputPath || !state.outputPath)); });
    updateReadiness();
  }

  function showProgress(payload) {
    if (typeof payload.progress === 'number') {
      state.progress = Math.max(state.progress, Math.min(1, payload.progress));
      $('progressBar').style.width = `${state.progress * 100}%`;
      setText('progressPercent', `${Math.round(state.progress * 100)}%`);
      setText('progressStage', payload.message || localizeText('正在处理'));
    }
    appendLog(payload.message);
  }

  function reportCell(label, value) {
    return `<div class="report-cell"><small>${escapeHtml(localizeText(label))}</small><strong>${escapeHtml(localizeText(value || '—'))}</strong></div>`;
  }

  function showResult(result) {
    state.result = result;
    $('workflowView').classList.add('hidden');
    $('resultView').classList.remove('hidden');
    $('actionbar').classList.add('hidden');
    const cancelled = Boolean(result.cancelled);
    const warning = !cancelled && !result.validationPassed;
    $('resultHeading').classList.remove('error');
    $('resultHeading').classList.toggle('warning', warning);
    const resultIcon = cancelled ? 'ph-stop' : warning ? 'ph-warning' : 'ph-check';
    $('resultSymbol').innerHTML = `<i class="ph-thin ${resultIcon}" aria-hidden="true"></i>`;
    setText('resultTitle', cancelled ? '转换已取消' : warning ? '已生成，存在质量警告' : '转换完成');
    const outputFormat = String(result.outputFormat || state.outputFormat).toUpperCase();
    setText('resultMessage', cancelled ? t(textKey('目标文件未被修改。')) : warning ? t('result.warning', { format: outputFormat }) : t('result.success', { format: outputFormat }));
    setText('resultFileName', cancelled ? '未生成输出文件' : basename(result.outputPath));
    setText('resultFilePath', cancelled ? localizeText('目标文件未被修改') : result.outputPath);
    const report = result.report || {};
    $('reportGrid').innerHTML = [
      reportCell('源格式', (report.source_format || state.format).toUpperCase()),
      reportCell('分辨率', report.resolution || state.resolution),
      reportCell('占用体素', report.occupied),
      reportCell('体素尺寸', report.voxelSize),
      reportCell('体素方案', report.voxelMode || state.voxelMode),
      reportCell('表面拓扑', ({ blocks: '方块像素', triangles: '连续三角面', quads: '连续四边面' })[report.topology] || report.topology),
      reportCell('计算后端', report.backend),
      reportCell('输出格式', outputFormat),
      reportCell('输入文件', basename(result.sourcePath || state.inputPath)),
      reportCell('输出文件', basename(result.outputPath)),
      reportCell('写出', result.validationPassed ? '通过' : '警告'),
      reportCell('状态', result.error ? '失败' : '完成')
    ].join('');
    setText('resultLog', state.logs.join('\n'));
    const printAvailable = !cancelled && !result.error && result.validationPassed && Boolean(result.outputPath);
    state.conversion = printAvailable ? {
      sourcePath: result.sourcePath || state.inputPath,
      outputPath: result.outputPath,
      voxelDataPath: result.voxelDataPath,
      outputFormat: result.outputFormat || state.outputFormat,
      format: report.source_format || state.format,
      resolution: report.resolution || state.resolution,
      occupied: report.occupied,
      voxelSize: report.voxelSize,
      voxelMode: report.voxelMode || state.voxelMode,
      topology: report.topology,
      backend: report.backend,
      validationPassed: Boolean(result.validationPassed),
      log: state.logs.join('\n')
    } : null;
    $('resultDock').classList.toggle('hidden', !printAvailable);
    setText('dockTitle', printAvailable ? '转换完成' : '转换未生成有效文件');
    setText('dockHint', printAvailable ? t('result.ready', { format: outputFormat }) : '无法进行 3D 打印');
    $('printStatus').classList.add('hidden');
    $('printStatus').classList.remove('error');
    hidePrintProgress();
    $('openInSlicer').classList.add('hidden');
    state.printOutputPath = '';
  }

  function hidePrintProgress() {
    $('printProgress').classList.add('hidden');
    $('printProgressBar').style.width = '0%';
    setText('printProgressPercent', '0%');
    setText('printProgressLabel', '准备导出');
  }

  function showPrintProgress(payload) {
    if (typeof payload.progress !== 'number') return;
    const progress = Math.max(0, Math.min(1, payload.progress));
    const labels = { 'loading-binvox': '正在读取体素数据', 'writing-stl': '正在构建打印网格', done: '导出完成' };
    setText('printProgressLabel', labels[payload.message] || payload.message || '正在导出');
    setText('printProgressPercent', `${Math.round(progress * 100)}%`);
    $('printProgressBar').style.width = `${progress * 100}%`;
  }

  function setDockBusy(busy) {
    $('dockStartPrint').disabled = busy || state.converting;
    $('dockExport').disabled = busy || state.exportingPrint;
    $('dockNewConversion').disabled = busy;
  }

  async function exportModel(format) {
    if (!state.result || state.exportingPrint) return;
    const labels = { obj: 'OBJ', glb: 'GLB', stl: 'STL', '3mf': '3MF', binvox: 'BINVOX' };
    const label = labels[format];
    let busy = false;
    try {
      if (!label) throw new Error(t('error.exportFormat', { format }));
      if (typeof window.voxkit?.pickModelOutput !== 'function' || typeof window.voxkit?.exportModel !== 'function') throw new Error('导出服务尚未加载，请完全退出并重新启动应用。');
      const inputPath = state.result.voxelDataPath;
      if (!inputPath) throw new Error('当前结果缺少体素缓存，请重新执行转换后再导出。');
      const suggestedPath = `${withoutExtension(state.result.outputPath || inputPath)}.${format}`;
      const outputPath = await window.voxkit.pickModelOutput({ format, defaultPath: suggestedPath, locale: language });
      if (!outputPath) return;
      state.exportingPrint = true;
      busy = true;
      setDockBusy(true);
      $('printStatus').classList.remove('hidden', 'error');
      setText('printStatusTitle', t('export.running', { format: label }));
      setText('printStatusDetail', '正在构建打印网格，请稍候');
      $('printProgress').classList.remove('hidden');
      $('openInSlicer').classList.add('hidden');
      showPrintProgress({ progress: 0, message: '正在准备模型' });
      const exported = await window.voxkit.exportModel({ format, inputPath, originalSourcePath: state.result.sourcePath || state.inputPath, outputPath, conversion: state.conversion, locale: language });
      state.printOutputPath = exported.outputPath;
      setText('printStatusTitle', t('export.generated', { format: label }));
      setText('printStatusDetail', `${basename(exported.outputPath)} · ${formatBytes(exported.bytes)}`);
      $('openInSlicer').classList.remove('hidden');
      hidePrintProgress();
      appendLog(t('export.log', { format: label, path: exported.outputPath }));
      if ($('recordsDialog').open) await loadExportHistory();
    } catch (error) {
      const message = error?.message || String(error);
      setText('printStatusTitle', '导出失败');
      setText('printStatusDetail', message);
      $('printStatus').classList.add('error');
      $('printProgress').classList.add('hidden');
      appendLog(t('error.export', { message }));
    } finally {
      hidePrintProgress();
      if (busy) {
        state.exportingPrint = false;
        setDockBusy(false);
      }
    }
  }

  const RATIO_OPTIONS = [
    { value: 0.25, label: '25%' },
    { value: 0.5, label: '50%' },
    { value: 0.75, label: '75%' },
    { value: 1, label: '100%' },
    { value: null, label: '模型原始比例' }
  ];

  function selectedPrinter() {
    const id = $('printerSelect').value || state.pendingPrinterId;
    return state.printers.find((printer) => printer.id === id && printer.enabled) || null;
  }

  function renderPrinterSelect() {
    const select = $('printerSelect');
    select.innerHTML = state.printers.map((printer) => {
      const volume = printer.buildVolume ? printer.buildVolume.join(' × ') : '';
      const label = printer.enabled ? t('printer.volume', { name: printer.name, volume }) : t('printer.coming', { name: printer.name });
      return `<option value="${escapeHtml(printer.id)}"${printer.enabled ? '' : ' disabled'}>${escapeHtml(label)}</option>`;
    }).join('');
    const current = state.printers.find((printer) => printer.id === state.pendingPrinterId && printer.enabled);
    select.value = current ? current.id : state.printers.find((printer) => printer.enabled)?.id || '';
    syncRatioPanel();
  }

  function syncRatioPanel() {
    const printer = selectedPrinter();
    if (!printer) {
      $('ratioPanel').classList.add('hidden');
      $('confirmPrintDialog').disabled = true;
      return;
    }
    state.pendingPrinterId = printer.id;
    $('ratioPanel').classList.remove('hidden');
    $('confirmPrintDialog').disabled = state.converting;
    setText('ratioModelName', state.result?.outputPath ? t('printer.modelVoxelized', { name: basename(state.result.outputPath) }) : '未命名体素模型');
    setText('ratioModelSize', printer.buildVolume ? t('printer.buildVolume', { volume: printer.buildVolume.join(' × ') }) : '由切片软件决定模型尺寸');
    $('ratioSlider').value = '3';
    updateRatioPreview();
  }

  function openPrintDialog() {
    if (!state.result?.outputPath) {
      appendLog('请先完成体素化，再开始 3D 打印。');
      return;
    }
    if (!state.printers.some((printer) => printer.enabled)) {
      appendLog('没有可用的打印机。');
      return;
    }
    if (!state.pendingPrinterId) state.pendingPrinterId = state.printers.find((printer) => printer.enabled)?.id || '';
    state.directPrintState = 'idle';
    $('printSettings').classList.remove('hidden');
    $('printDialogFeedback').className = 'print-dialog-feedback hidden';
    $('cancelPrintDialog').classList.remove('hidden');
    $('cancelPrintDialog').textContent = localizeText('取消');
    $('confirmPrintDialog').classList.remove('hidden');
    $('confirmPrintDialog').disabled = false;
    $('confirmPrintDialog').querySelector('span').textContent = localizeText('确认发送打印');
    renderPrinterSelect();
    $('printDialog').showModal();
  }

  function showPrintDialogProgress(progress, title, detail) {
    const value = Math.max(0, Math.min(1, progress));
    $('printDialogFeedback').className = 'print-dialog-feedback';
    $('printFeedbackIcon').innerHTML = '<i class="ph-thin ph-spinner-gap spin" aria-hidden="true"></i>';
    setText('printFeedbackTitle', title);
    setText('printFeedbackDetail', detail);
    $('printFeedbackProgress').classList.remove('hidden');
    $('printFeedbackBar').style.width = `${value * 100}%`;
    setText('printFeedbackPercent', `${Math.round(value * 100)}%`);
  }

  function updateRatioPreview() {
    const slider = $('ratioSlider');
    const step = Number(slider.value);
    const option = RATIO_OPTIONS[step] || RATIO_OPTIONS[3];
    const percent = option.value == null ? '原始比例' : option.label;
    setText('ratioPreviewValue', percent);
    const scale = Math.max(0.26, option.value == null ? 1 : option.value);
    $('ratioPreviewCube').style.width = `${Math.round(scale * 76)}px`;
    $('ratioPreviewCube').style.height = `${Math.round(scale * 76)}px`;
    const fill = Math.round((step / 4) * 100);
    slider.parentElement.style.setProperty('--ratio-fill', `${fill}%`);
    $$('#ratioStops span').forEach((stop) => stop.classList.toggle('active', Number(stop.dataset.ratioStep) === step));
    setText('ratioPreviewHint', option.value == null
      ? '保持模型原始尺寸直接发送到切片软件'
      : t('ratio.fit', { percent: Math.round(option.value * 100) }));
  }

  async function sendToPrinter() {
    if (state.directPrintState === 'sending') return;
    if (state.directPrintState === 'success') {
      $('printDialog').close();
      return;
    }
    const printer = state.printers.find((item) => item.id === state.pendingPrinterId) || state.printers.find((item) => item.enabled);
    if (!printer || !printer.enabled) return;
    const voxelPath = state.result?.outputPath;
    if (!voxelPath) return;
    const step = Number($('ratioSlider').value);
    const option = RATIO_OPTIONS[step] || RATIO_OPTIONS[3];
    const ratio = option.value == null ? null : option.value;
    state.directPrintState = 'sending';
    setDockBusy(true);
    $('printSettings').classList.add('hidden');
    $('cancelPrintDialog').classList.add('hidden');
    $('confirmPrintDialog').disabled = true;
    showPrintDialogProgress(0.04, '正在准备体素网格', t('print.reading', { format: String(state.result.outputFormat || state.outputFormat).toUpperCase() }));
    try {
      showPrintDialogProgress(0.08, t('print.sending', { name: printer.name }), ratio == null ? localizeText('正在准备体素模型') : t('print.scaling', { percent: Math.round(ratio * 100) }));
      const record = await window.voxkit.sendToPrinter({ printerId: printer.id, sourcePath: voxelPath, ratio, originalSourcePath: state.result.sourcePath || state.inputPath, conversion: state.conversion, locale: language });
      state.printOutputPath = record.outputPath;
      appendLog(t('print.sentLog', { printer: record.printerName, slicer: record.slicerName, path: record.outputPath }));
      const fitted = record.fittedDimensions?.map((dimension) => Number(dimension).toFixed(1)).join(' × ');
      const scaleText = record.scale === 1 ? localizeText('原始尺寸') : t('print.fit', { size: fitted, percent: (record.scale * 100).toFixed(1) });
      $('printDialogFeedback').classList.add('success');
      state.directPrintState = 'success';
      $('printFeedbackIcon').innerHTML = '<i class="ph-thin ph-check" aria-hidden="true"></i>';
      setText('printFeedbackTitle', t('print.sent', { name: record.slicerName }));
      setText('printFeedbackDetail', `${basename(record.outputPath)} · ${scaleText}`);
      $('printFeedbackBar').style.width = '100%';
      setText('printFeedbackPercent', '100%');
      $('confirmPrintDialog').disabled = false;
      $('confirmPrintDialog').querySelector('span').textContent = localizeText('完成');
      if ($('recordsDialog').open) await loadExportHistory();
    } catch (error) {
      state.directPrintState = 'error';
      const message = error?.message || String(error);
      $('printDialogFeedback').classList.add('error');
      $('printFeedbackIcon').innerHTML = '<i class="ph-thin ph-warning" aria-hidden="true"></i>';
      setText('printFeedbackTitle', '发送失败');
      setText('printFeedbackDetail', message);
      $('printFeedbackProgress').classList.add('hidden');
      setText('printFeedbackPercent', '');
      $('cancelPrintDialog').classList.remove('hidden');
      $('cancelPrintDialog').textContent = localizeText('关闭');
      $('confirmPrintDialog').disabled = false;
      $('confirmPrintDialog').querySelector('span').textContent = localizeText('重试');
      appendLog(t('error.sendPrinter', { message }));
    } finally {
      setDockBusy(false);
    }
  }

  async function loadExportHistory() {
    const list = $('recordsList');
    list.innerHTML = `<div class="records-empty"><i class="ph-thin ph-spinner-gap spin" aria-hidden="true"></i><p>${escapeHtml(localizeText('正在读取导出记录…'))}</p></div>`;
    try {
      const history = await window.voxkit.listExportHistory(language);
      const records = history.records || [];
      state.historyRecords = records;
      setText('recordsDirectory', history.directory || '用户文稿文件夹');
      setText('recordsCount', t('history.count', { count: records.length }));
      if (!records.length) {
        list.innerHTML = `<div class="records-empty"><i class="ph-thin ph-clock-counter-clockwise" aria-hidden="true"></i><strong>${escapeHtml(localizeText('暂无导出记录'))}</strong><p>${escapeHtml(localizeText('生成打印文件或将原始模型发送到打印机后，记录会显示在这里。'))}</p></div>`;
        return;
      }
      list.innerHTML = records.map((record) => {
        const date = new Date(record.exportedAt);
        const dateText = Number.isNaN(date.getTime()) ? localizeText('未知时间') : date.toLocaleString(language === 'en' ? 'en-US' : 'zh-CN', { hour12: false });
        const failed = record.status === 'failed';
        const filePath = record.artifactPath || record.outputPath || '';
        const displayPath = filePath || record.sourcePath || localizeText('未知文件');
        const operation = failed ? (record.error || localizeText('导出失败')) : record.operation === 'direct-send' ? t('history.sent', { name: record.printerName || record.slicerName || localizeText('打印机') }) : t('history.exported', { name: record.printer || localizeText('打印文件') });
        const hasDetail = Boolean(record.conversion && (record.conversion.outputPath || record.conversion.sourcePath));
        const fileActionsDisabled = failed || !filePath;
        return `<article class="record-item${failed ? ' failed' : ''}" data-record-id="${escapeHtml(record.id)}" data-output-path="${escapeHtml(filePath)}">
          <span class="record-icon"><i class="ph-thin ${failed ? 'ph-warning' : 'ph-cube'}" aria-hidden="true"></i></span>
          <div class="record-main"><strong>${escapeHtml(basename(displayPath))}</strong><span>${escapeHtml(operation)} · ${escapeHtml(record.format || '—')}${record.bytes ? ` · ${escapeHtml(formatBytes(record.bytes))}` : ''}</span><small title="${escapeHtml(displayPath)}">${escapeHtml(displayPath)}</small></div>
          <time datetime="${escapeHtml(record.exportedAt || '')}">${escapeHtml(dateText)}</time>
          <div class="record-actions">${hasDetail ? `<button data-action="detail" title="${escapeHtml(localizeText('查看详情'))}"><i class="ph-thin ph-eye" aria-hidden="true"></i></button>` : ''}<button data-action="open" ${fileActionsDisabled ? 'disabled' : ''} title="${escapeHtml(localizeText('打开文件'))}"><i class="ph-thin ph-arrow-square-out" aria-hidden="true"></i></button><button data-action="reveal" ${fileActionsDisabled ? 'disabled' : ''} title="${escapeHtml(localizeText('在 Finder 中显示'))}"><i class="ph-thin ph-folder-open" aria-hidden="true"></i></button><button data-action="remove" class="record-remove" title="${escapeHtml(localizeText('从记录中移除'))}"><i class="ph-thin ph-trash" aria-hidden="true"></i></button></div>
        </article>`;
      }).join('');
    } catch (error) {
      list.innerHTML = `<div class="records-empty error"><i class="ph-thin ph-warning" aria-hidden="true"></i><strong>${escapeHtml(localizeText('无法读取导出记录'))}</strong><p>${escapeHtml(error?.message || String(error))}</p></div>`;
    }
  }

  function renderConversionDetail(record) {
    const conversion = record.conversion || {};
    const sourcePath = conversion.sourcePath || record.sourcePath || '';
    const outputPath = conversion.outputPath || record.artifactPath || record.outputPath || record.sourcePath || '';
    const warning = conversion.validationPassed === false;
    const icon = warning ? 'ph-warning' : 'ph-check';
    const title = warning ? '已生成，存在质量警告' : '转换完成';
    $('detailSymbol').innerHTML = `<i class="ph-thin ${icon}" aria-hidden="true"></i>`;
    setText('detailTitle', title);
    const detailFormat = String(conversion.outputFormat || pathExtension(outputPath) || record.format || 'BINVOX').toUpperCase();
    setText('detailMessage', warning ? t('result.warning', { format: detailFormat }) : t('result.success', { format: detailFormat }));
    setText('detailFileName', outputPath ? basename(outputPath) : '未知文件');
    setText('detailFilePath', outputPath || '—');
    setText('detailHeading', title);
    setText('detailSub', record.operation === 'direct-send'
      ? t('detail.sent', { name: record.printerName || record.slicerName || localizeText('打印机') })
      : t('detail.exported', { name: record.printer || localizeText('打印文件') }));
    const format = conversion.format || record.format || 'BINVOX';
    const outputFormat = conversion.outputFormat || pathExtension(outputPath) || record.format || 'BINVOX';
    $('detailGrid').innerHTML = [
      reportCell('源格式', String(format).toUpperCase()),
      reportCell('分辨率', conversion.resolution || '—'),
      reportCell('占用体素', conversion.occupied || '—'),
      reportCell('体素尺寸', conversion.voxelSize || '—'),
      reportCell('输出格式', String(outputFormat).toUpperCase()),
      reportCell('输入文件', sourcePath ? basename(sourcePath) : '—'),
      reportCell('输出文件', outputPath ? basename(outputPath) : '—'),
      reportCell('状态', warning ? '警告' : '完成')
    ].join('');
    const logText = conversion.log || '';
    $('detailLogWrap').classList.toggle('hidden', !logText);
    setText('detailLog', logText || '该记录未保存处理日志。');
  }

  function openRecordDetail(recordId) {
    const record = state.historyRecords.find((item) => item.id === recordId);
    if (!record) return;
    renderConversionDetail(record);
    $('recordsDialog').close();
    $('detailDialog').showModal();
  }

  async function openExportHistory() {
    $('recordsDialog').showModal();
    await loadExportHistory();
  }

  async function startConversion() {
    if (!state.inputPath || !state.outputPath) { appendLog('请先选择源模型与输出位置'); return; }
    setConverting(true);
    state.progress = 0;
    state.logs = [];
    setText('conversionLog', '');
    showProgress({ progress: 0, message: t('conversion.started', { name: basename(state.inputPath) }) });
    try {
      const result = await window.voxkit.voxelize({ ...collectSettings(), locale: language });
      showResult(result);
    } catch (error) {
      const message = error?.message || String(error);
      appendLog(t('error.conversion', { message }));
      showResult({ outputPath: state.outputPath, report: {}, validationPassed: false, error: message });
      setText('resultTitle', '转换失败');
      setText('resultMessage', message);
      $('resultSymbol').innerHTML = '<i class="ph-thin ph-x" aria-hidden="true"></i>';
      $('resultHeading').classList.add('error');
    } finally {
      setConverting(false);
      setDockBusy(false);
    }
  }

  function resetForNewConversion() {
    state.result = null;
    state.step = 1;
    state.inputPath = '';
    state.outputPath = '';
    state.outputPathAuto = true;
    state.logs = [];
    state.exportingPrint = false;
    state.printOutputPath = '';
    state.conversion = null;
    $('workflowView').classList.remove('hidden');
    $('resultView').classList.add('hidden');
    $('actionbar').classList.remove('hidden');
    $('resultDock').classList.add('hidden');
    ['printDialog', 'detailDialog'].forEach((id) => { if ($(id).open) $(id).close(); });
    setStep(1);
    syncFormatUI();
  }

  $$('.format-item').forEach((button) => button.addEventListener('click', () => {
    if (button.disabled) return;
    state.format = button.dataset.format;
    resetForNewConversion();
    hideAboutPage();
  }));
  $$('button', $('outputFormat')).forEach((button) => button.addEventListener('click', () => {
    if (button.disabled || state.outputFormat === button.dataset.value) return;
    state.outputFormat = button.dataset.value;
    state.outputPath = state.outputPathAuto ? defaultOutputPath() : '';
    state.result = null;
    syncFormatUI();
  }));
  $$('.step-button').forEach((button) => button.addEventListener('click', () => setStep(Number(button.dataset.step))));
  $('inputPicker').addEventListener('click', chooseInput);
  $('outputPicker').addEventListener('click', chooseOutput);
  $('primaryAction').addEventListener('click', () => state.step === 1 ? setStep(2) : startConversion());
  $('cancelConversion').addEventListener('click', async () => { await window.voxkit.cancel(); appendLog('正在取消转换…'); });
  $('dockNewConversion').addEventListener('click', resetForNewConversion);
  $('dockExport').addEventListener('click', () => {
    const menu = $('exportMenu');
    const open = menu.classList.toggle('hidden');
    $('dockExport').setAttribute('aria-expanded', String(!open));
  });
  $$('[data-export-format]').forEach((item) => item.addEventListener('click', () => {
    $('exportMenu').classList.add('hidden');
    $('dockExport').setAttribute('aria-expanded', 'false');
    exportModel(item.dataset.exportFormat);
  }));
  document.addEventListener('click', (event) => {
    if (!event.target.closest('.export-menu-wrap')) {
      $('exportMenu').classList.add('hidden');
      $('dockExport').setAttribute('aria-expanded', 'false');
    }
  });
  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') $('exportMenu').classList.add('hidden');
  });
  $('dockStartPrint').addEventListener('click', openPrintDialog);
  $('revealOutput').addEventListener('click', () => window.voxkit.revealOutput(state.result?.outputPath));
  $('closePrintDialog').addEventListener('click', () => { if (state.directPrintState !== 'sending') $('printDialog').close(); });
  $('cancelPrintDialog').addEventListener('click', () => { if (state.directPrintState !== 'sending') $('printDialog').close(); });
  $('confirmPrintDialog').addEventListener('click', sendToPrinter);
  $('printerSelect').addEventListener('change', () => {
    const printer = selectedPrinter();
    if (!printer) return;
    state.pendingPrinterId = printer.id;
    syncRatioPanel();
  });
  $('ratioSlider').addEventListener('input', updateRatioPreview);
  $('ratioStops').addEventListener('click', (event) => {
    const stop = event.target.closest('[data-ratio-step]');
    if (!stop) return;
    $('ratioSlider').value = stop.dataset.ratioStep;
    updateRatioPreview();
  });
  $('closeDetailDialog').addEventListener('click', () => $('detailDialog').close());
  $('detailDialog').addEventListener('click', (event) => { if (event.target === $('detailDialog')) $('detailDialog').close(); });
  $('printDialog').addEventListener('click', (event) => { if (event.target === $('printDialog') && state.directPrintState !== 'sending') $('printDialog').close(); });
  $('printDialog').addEventListener('cancel', (event) => { if (state.directPrintState === 'sending') event.preventDefault(); });
  $('openInSlicer').addEventListener('click', async () => {
    if (!state.printOutputPath) return;
    try { await window.voxkit.revealOutput(state.printOutputPath); }
    catch (error) { appendLog(t('error.reveal', { message: error?.message || String(error) })); }
  });
  $('recordsTrigger').addEventListener('click', openExportHistory);
  $('closeRecords').addEventListener('click', () => $('recordsDialog').close());
  $('refreshRecords').addEventListener('click', loadExportHistory);
  $('openRecordsDirectory').addEventListener('click', () => window.voxkit.openExportDirectory());
  $('recordsList').addEventListener('click', async (event) => {
    const button = event.target.closest('button[data-action]');
    const item = event.target.closest('.record-item');
    if (!button || !item) return;
    const outputPath = item.dataset.outputPath;
    try {
      if (button.dataset.action === 'detail') {
        openRecordDetail(item.dataset.recordId);
      } else if (button.dataset.action === 'open') {
        const opened = await window.voxkit.openExportFile(outputPath);
        if (!opened) throw new Error('文件已被移动或删除。');
      } else if (button.dataset.action === 'reveal') {
        const revealed = await window.voxkit.revealOutput(outputPath);
        if (!revealed) throw new Error('文件已被移动或删除。');
      } else if (button.dataset.action === 'remove') {
        await window.voxkit.removeExportRecord(item.dataset.recordId);
        await loadExportHistory();
      }
    } catch (error) {
      item.classList.add('missing');
      item.querySelector('.record-main span').textContent = error?.message || String(error);
    }
  });
  $$('[data-setting]').forEach((group) => group.addEventListener('click', (event) => {
    const button = event.target.closest('button[data-value]');
    if (!button) return;
    if (group.dataset.setting === 'voxelMode' && button.dataset.value === 'triangle' && !state.triangleVoxelization) return;
    setSegmentedValue(group, button.dataset.value);
    if (group.dataset.setting === 'resolution') state.resolution = Number(button.dataset.value);
    if (group.dataset.setting === 'voxelMode') state.voxelMode = button.dataset.value;
    if (group.dataset.setting === 'fill') state.fill = button.dataset.value;
    if (group.dataset.setting === 'quality') state.quality = button.dataset.value;
    if (group.dataset.setting === 'preview') state.preview = button.dataset.value;
    if (group.dataset.setting === 'splitParts') state.splitParts = button.dataset.value;
  }));
  $('sidebarToggle').addEventListener('click', () => {
    const collapsed = $('appShell').classList.toggle('sidebar-collapsed');
    $('sidebarToggle').title = localizeText(collapsed ? '展开边栏' : '收起边栏');
    $('sidebarToggle').setAttribute('aria-label', localizeText(collapsed ? '展开边栏' : '收起边栏'));
  });
  $('floatingToggle').addEventListener('click', () => $('appShell').classList.remove('sidebar-collapsed'));

  const dropSurface = $('inputPicker');
  ['dragenter', 'dragover'].forEach((eventName) => dropSurface.addEventListener(eventName, (event) => { event.preventDefault(); dropSurface.classList.add('dragover'); }));
  ['dragleave', 'drop'].forEach((eventName) => dropSurface.addEventListener(eventName, (event) => { event.preventDefault(); dropSurface.classList.remove('dragover'); }));
  dropSurface.addEventListener('drop', (event) => {
    const file = event.dataTransfer.files[0];
    const filePath = file && window.voxkit.pathForFile(file);
    if (filePath) setInput(filePath);
  });

  const renderAboutConfig = (config = {}) => {
    const about = config.about || {};
    const social = config.social || {};
    const contact = config.contact || {};
    const changelog = Array.isArray(config.changelog) ? config.changelog : [];
    if (about.name) $('aboutPageTitle').textContent = about.name;
    if (about.description) $('aboutPageDescription').textContent = language === 'en' ? (about.descriptionEn || about.description) : about.description;
    if (about.version) $('aboutPageVersion').textContent = about.version;
    if (about.creatorUrl) $('creatorCredit').href = about.creatorUrl;
    if (about.creator) $('creatorCredit').textContent = about.creator;
    const workLabIcons = {
      bilibili: '<path fill="currentColor" d="M17.813 4.653h.854c1.51.054 2.769.578 3.773 1.574 1.004.995 1.524 2.249 1.56 3.76v7.36c-.036 1.51-.556 2.769-1.56 3.773s-2.262 1.524-3.773 1.56H5.333c-1.51-.036-2.769-.556-3.773-1.56S.036 18.858 0 17.347v-7.36c.036-1.511.556-2.765 1.56-3.76 1.004-.996 2.262-1.52 3.773-1.574h.774l-1.174-1.12a1.234 1.234 0 0 1-.373-.906c0-.356.124-.658.373-.907l.027-.027c.267-.249.573-.373.92-.373.347 0 .653.124.92.373L9.653 4.44c.071.071.134.142.187.213h4.267a.836.836 0 0 1 .16-.213l2.853-2.747c.267-.249.573-.373.92-.373.347 0 .662.151.929.4.267.249.391.551.391.907 0 .355-.124.657-.373.906zM5.333 7.24c-.746.018-1.373.276-1.88.773-.506.498-.769 1.13-.786 1.894v7.52c.017.764.28 1.395.786 1.893.507.498 1.134.756 1.88.773h13.334c.746-.017 1.373-.275 1.88-.773.506-.498.769-1.129.786-1.893v-7.52c-.017-.765-.28-1.396-.786-1.894-.507-.497-1.134-.755-1.88-.773zM8 11.107c.373 0 .684.124.933.373.25.249.383.569.4.96v1.173c-.017.391-.15.711-.4.96-.249.25-.56.374-.933.374s-.684-.125-.933-.374c-.25-.249-.383-.569-.4-.96V12.44c0-.373.129-.689.386-.947.258-.257.574-.386.947-.386zm8 0c.373 0 .684.124.933.373.25.249.383.569.4.96v1.173c-.017.391-.15.711-.4.96-.249.25-.56.374-.933.374s-.684-.125-.933-.374c-.25-.249-.383-.569-.4-.96V12.44c.017-.391.15-.711.4-.96.249-.249.56-.373.933-.373Z"/>',
      douyin: '<path fill="currentColor" d="M12 3v10.55c-.59-.34-1.27-.55-2-.55-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4V7h4V3h-6z"/><path fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" d="M4.6 6.4c1.5 1 3.7 1.3 5.3.6"/>',
      video: '<circle cx="12" cy="12" r="9.2" stroke="currentColor" stroke-width="2"/><path fill="currentColor" d="M10 8.4v7.2c0 .6.7 1 1.2.7l5.4-3.6c.5-.3.5-1.1 0-1.4l-5.4-3.6c-.5-.3-1.2.1-1.2.7z"/>',
      xiaohongshu: '<path fill="currentColor" d="M22.405 9.879c.002.016.01.02.07.019h.725a.797.797 0 0 0 .78-.972.794.794 0 0 0-.884-.618.795.795 0 0 0-.692.794c0 .101-.002.666.001.777zm-11.509 4.808c-.203.001-1.353.004-1.685.003a2.528 2.528 0 0 1-.766-.126.025.025 0 0 0-.03.014L7.7 16.127a.025.025 0 0 0 .01.032c.111.06.336.124.495.124.66.01 1.32.002 1.981 0 .01 0 .02-.006.023-.015l.712-1.545a.025.025 0 0 0-.024-.036zM.477 9.91c-.071 0-.076.002-.076.01a.834.834 0 0 0-.01.08c-.027.397-.038.495-.234 3.06-.012.24-.034.389-.135.607-.026.057-.033.042.003.112.046.092.681 1.523.787 1.74.008.015.011.02.017.02.008 0 .033-.026.047-.044.147-.187.268-.391.371-.606.306-.635.44-1.325.486-1.706.014-.11.021-.22.03-.33l.204-2.616.022-.293c.003-.029 0-.033-.03-.034zm7.203 3.757a1.427 1.427 0 0 1-.135-.607c-.004-.084-.031-.39-.235-3.06a.443.443 0 0 0-.01-.082c-.004-.011-.052-.008-.076-.008h-1.48c-.03.001-.034.005-.03.034l.021.293c.076.982.153 1.964.233 2.946.05.4.186 1.085.487 1.706.103.215.223.419.37.606.015.018.037.051.048.049.02-.003.742-1.642.804-1.765.036-.07.03-.055.003-.112zm3.861-.913h-.872a.126.126 0 0 1-.116-.178l1.178-2.625a.025.025 0 0 0-.023-.035l-1.318-.003a.148.148 0 0 1-.135-.21l.876-1.954a.025.025 0 0 0-.023-.035h-1.56c-.01 0-.02.006-.024.015l-.926 2.068c-.085.169-.314.634-.399.938a.534.534 0 0 0-.02.191.46.46 0 0 0 .23.378.981.981 0 0 0 .46.119h.59c.041 0-.688 1.482-.834 1.972a.53.53 0 0 0-.023.172.465.465 0 0 0 .23.398c.15.092.342.12.475.12l1.66-.001c.01 0 .02-.006.023-.015l.575-1.28a.025.025 0 0 0-.024-.035zm-6.93-4.937H3.1a.032.032 0 0 0-.034.033c0 1.048-.01 2.795-.01 6.829 0 .288-.269.262-.28.262h-.74c-.04.001-.044.004-.04.047.001.037.465 1.064.555 1.263.01.02.03.033.051.033.157.003.767.009.938-.014.153-.02.3-.06.438-.132.3-.156.49-.419.595-.765.052-.172.075-.353.075-.533.002-2.33 0-4.66-.007-6.991a.032.032 0 0 0-.032-.032zm11.784 6.896c0-.014-.01-.021-.024-.022h-1.465c-.048-.001-.049-.002-.05-.049v-4.66c0-.072-.005-.07.07-.07h.863c.08 0 .075.004.075-.074V8.393c0-.082.006-.076-.08-.076h-3.5c-.064 0-.075-.006-.075.073v1.445c0 .083-.006.077.08.077h.854c.075 0 .07-.004.07.07v4.624c0 .095.008.084-.085.084-.37 0-1.11-.002-1.304 0-.048.001-.06.03-.06.03l-.697 1.519s-.014.025-.008.036c.006.01.013.008.058.008 1.748.003 3.495.002 5.243.002.03-.001.034-.006.035-.033v-1.539zm4.177-3.43c0 .013-.007.023-.02.024-.346.006-.692.004-1.037.004-.014-.002-.022-.01-.022-.024-.005-.434-.007-.869-.01-1.303 0-.072-.006-.071.07-.07l.733-.003c.041 0 .081.002.12.015.093.025.16.107.165.204.006.431.002 1.153.001 1.153zm2.67.244a1.953 1.953 0 0 0-.883-.222h-.18c-.04-.001-.04-.003-.042-.04V10.21c0-.132-.007-.263-.025-.394a1.823 1.823 0 0 0-.153-.53 1.533 1.533 0 0 0-.677-.71 2.167 2.167 0 0 0-1-.258c-.153-.003-.567 0-.72 0-.07 0-.068.004-.068-.065V7.76c0-.031-.01-.041-.046-.039H17.93s-.016 0-.023.007c-.006.006-.008.012-.008.023v.546c-.008.036-.057.015-.082.022h-.95c-.022.002-.028.008-.03.032v1.481c0 .09-.004.082.082.082h.913c.082 0 .072.128.072.128V11.19s.003.117-.06.117h-1.482c-.068 0-.06.082-.06.082v1.445s-.01.068.064.068h1.457c.082 0 .076-.006.076.079v3.225c0 .088-.007.081.082.081h1.43c.09 0 .082.007.082-.08v-3.27c0-.029.006-.035.033-.035l2.323-.003c.098 0 .191.02.28.061a.46.46 0 0 1 .274.407c.008.395.003.79.003 1.185 0 .259-.107.367-.33.367h-1.218c-.023.002-.029.008-.028.033.184.437.374.871.57 1.303a.045.045 0 0 0 .04.026c.17.005.34.002.51.003.15-.002.517.004.666-.01a2.03 2.03 0 0 0 .408-.075c.59-.18.975-.698.976-1.313v-1.981c0-.128-.01-.254-.034-.38 0 .078-.029-.641-.724-.998z"/>',
      globe: '<circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="1.8"/><path fill="none" stroke="currentColor" stroke-width="1.8" d="M3 12h18M12 3c2.1 2.4 3.2 5.4 3.2 9s-1.1 6.6-3.2 9c-2.1-2.4-3.2-5.4-3.2-9S9.9 5.4 12 3Z"/>',
      chat: '<path fill="currentColor" d="M12 3.2a8.8 8.8 0 0 0-7.4 13.6L3.3 20.8l4.2-1.4A8.8 8.8 0 1 0 12 3.2Zm-4.1 7.9h8.2v1.5H7.9v-1.5Zm0-3h8.2v1.5H7.9V8.1Z"/>',
    };
    const makeCard = (item) => {
      const card = document.createElement('div');
      card.className = `social-card${item.qr ? ' has-qr' : ''}`;
      if (item.qr) card.title = localizeText('鼠标移入查看二维码');

      const copy = document.createElement('span');
      copy.className = 'social-copy';
      if (item.platform || item.iconKey) {
        const icon = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        icon.setAttribute('class', 'social-icon');
        icon.setAttribute('viewBox', '0 0 24 24');
        icon.setAttribute('aria-hidden', 'true');
        icon.innerHTML = workLabIcons[item.platform || item.iconKey] || '<circle cx="12" cy="12" r="8" fill="none" stroke="currentColor" stroke-width="1.7"/>';
        icon.setAttribute('aria-hidden', 'true');
        copy.append(icon);
      }
      const name = document.createElement('strong');
      name.className = 'social-name';
      name.textContent = language === 'en' ? (item.nameEn || item.name || '') : (item.name || '');
      copy.append(name);

      if (item.value) {
        const value = document.createElement('span');
        value.className = 'social-value';
        value.textContent = item.value;
        copy.append(value);
      }
      card.append(copy);

      const hint = document.createElement('span');
      hint.className = 'social-hint';
      if (item.url) {
        const link = document.createElement('a');
        link.href = item.url;
        link.target = '_blank';
        link.rel = 'noopener noreferrer';
        link.textContent = language === 'en' ? (item.linkTextEn || (item.linkText ? localizeText(item.linkText) : localizeText('访问'))) : (item.linkText || '访问');
        hint.append(link);
      } else if (item.qr) {
        hint.textContent = localizeText('扫码');
      }
      if (hint.childNodes.length || hint.textContent) card.append(hint);

      if (item.qr) {
        const image = document.createElement('img');
        image.src = item.qr;
        image.alt = t('qr.alt', { name: language === 'en' ? (item.nameEn || item.name || '') : (item.name || '') });
        image.className = 'social-qr';
        card.append(image);
        card.addEventListener('pointermove', (event) => {
          const bounds = card.getBoundingClientRect();
          card.style.setProperty('--qr-x', `${event.clientX - bounds.left}px`);
          card.style.setProperty('--qr-y', `${event.clientY - bounds.top}px`);
          card.style.setProperty('--qr-available-height', `${Math.max(120, window.innerHeight - event.clientY - 28)}px`);
        });
      }
      return card;
    };
    const socialLinks = $('socialLinks');
    socialLinks.replaceChildren(...Object.values(social).filter((item) => item && item.enabled !== false).map(makeCard), ...Object.values(contact).filter((item) => item && item.enabled !== false).map(makeCard));
    const changelogList = $('changelogList');
    changelogList.replaceChildren(...changelog.filter((item) => item && item.enabled !== false).map((item) => {
      const entry = document.createElement('article');
      entry.className = 'changelog-entry';
      const header = document.createElement('div');
      header.className = 'changelog-entry-head';
      const version = document.createElement('strong');
      version.textContent = item.version || '';
      const date = document.createElement('time');
      date.textContent = item.date || '';
      header.append(version, date);
      entry.append(header);
      const summary = document.createElement('p');
      summary.textContent = language === 'en' ? (item.summaryEn || item.summary || '') : (item.summary || '');
      entry.append(summary);
      if (Array.isArray(item.items) && item.items.length) {
        const list = document.createElement('ul');
        const itemTexts = language === 'en' && Array.isArray(item.itemsEn) ? item.itemsEn : item.items;
        itemTexts.forEach((text) => {
          const listItem = document.createElement('li');
          listItem.textContent = text;
          list.append(listItem);
        });
        entry.append(list);
      }
      return entry;
    }));
  };
  let aboutConfig = {};
  window.voxkit.getAboutConfig().then((config) => { aboutConfig = config; renderAboutConfig(config); }).catch((error) => appendLog(t('error.about', { message: error?.message || String(error) })));
  const showAboutPage = () => {
    $('workflowView').classList.add('hidden');
    $('resultView').classList.add('hidden');
    $('aboutPage').classList.remove('hidden');
    $('actionbar').classList.add('hidden');
    $('resultDock').classList.add('hidden');
    $('topAbout').classList.add('hidden');
    $('floatingToggle').classList.add('hidden');
    document.querySelector('.topbar-title').textContent = localizeText('关于 VoxKit');
  };
  const hideAboutPage = () => {
    $('aboutPage').classList.add('hidden');
    $('workflowView').classList.remove('hidden');
    $('actionbar').classList.remove('hidden');
    $('topAbout').classList.remove('hidden');
    $('floatingToggle').classList.remove('hidden');
    document.querySelector('.topbar-title').textContent = localizeText('模型体素化');
  };
  $('aboutTrigger').addEventListener('click', showAboutPage);
  $('topAbout').addEventListener('click', showAboutPage);
  $('aboutBack').addEventListener('click', hideAboutPage);
  $('languageToggle').addEventListener('click', () => {
    language = language === 'en' ? 'zh-CN' : 'en';
    localStorage.setItem(languageKey, language);
    syncFormatUI();
    if (state.result) showResult(state.result);
    translateStaticText();
    applyVoxelCapabilities({ triangleVoxelization: state.triangleVoxelization });
    syncFormatUI();
    renderAboutConfig(aboutConfig);
    if (state.result) showResult(state.result);
    if ($('printDialog').open) renderPrinterSelect();
    if ($('recordsDialog').open) loadExportHistory();
    if ($('detailDialog').open) {
      const record = state.historyRecords.find((item) => item.id === $('detailDialog').dataset.recordId);
      if (record) renderConversionDetail(record);
    }
  });
  $('recordsDialog').addEventListener('click', (event) => { if (event.target === $('recordsDialog')) $('recordsDialog').close(); });

  window.voxkit.onProgress(showProgress);
  window.voxkit.onPrintExportProgress(showPrintProgress);
  window.voxkit.onModelExportProgress(showPrintProgress);
  window.voxkit.getCapabilities().then(applyVoxelCapabilities).catch(() => applyVoxelCapabilities());
  window.voxkit.listPrinters(language).then((printers) => {
    state.printers = printers || [];
    state.pendingPrinterId = state.printers.find((printer) => printer.enabled)?.id || '';
    renderPrinterSelect();
  }).catch((error) => {
    appendLog(t('error.printers', { message: error?.message || String(error) }));
    state.printers = [];
    renderPrinterSelect();
  });
  syncFormatUI();
  translateStaticText();
})();
