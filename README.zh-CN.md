# Hands3DLab-VoxKit

[English](README.md) | [简体中文](README.zh-CN.md)

<table>
    <tr>
        <td width="50%"><img src="electron/assets/screenshots/zh/s1.png" alt="Hands3DLab-VoxKit 中文界面截图 1"></td>
        <td width="50%"><img src="electron/assets/screenshots/zh/s2.png" alt="Hands3DLab-VoxKit 中文界面截图 2"></td>
    </tr>
    <tr>
        <td width="50%"><img src="electron/assets/screenshots/zh/s3.png" alt="Hands3DLab-VoxKit 中文界面截图 3"></td>
        <td width="50%"><img src="electron/assets/screenshots/zh/s4.png" alt="Hands3DLab-VoxKit 中文界面截图 4"></td>
    </tr>
</table>

Hands3DLab-VoxKit 是一款离线 Electron 桌面客户端，用于将 OBJ、GLB 和 STL 网格模型转换为体素模型。软件支持自定义体素分辨率、Binvox/OBJ/STL/3MF 导出、打印比例设置、切片软件调用及导出记录管理。所有转换均在桌面客户端内部本地完成。

Release 下载包只包含一个自包含的 Electron 客户端。体素化引擎已经内置在应用资源中，不需要也不会单独下载或分发 `voxkit`。

## 在 macOS 上打开未签名的 Release 版本

> **macOS Release 版本未签名。下载后，macOS 可能提示应用无法打开。确认 Release 来自本仓库且文件可信后，请在首次运行前手动移除隔离属性。**

将应用移动到 `/Applications`，打开“终端”并运行：

```bash
xattr -dr com.apple.quarantine "/Applications/Hands3DLab-VoxKit.app"
```

随后即可正常打开应用。如果应用位于其他位置，请将命令中的路径替换为实际 `.app` 路径。参数 `-r` 会对整个应用程序包递归执行操作。

移除 `com.apple.quarantine` 只是绕过 macOS 针对下载文件的隔离检查，并不会为应用签名或完成 Apple 公证。请仅对来源和完整性均可信的 Release 执行此命令，切勿对未知来源的应用随意使用。

## 功能


## 下载

请从 [GitHub Releases](https://github.com/Hands3DLab/Hands3DLab-VoxKit/releases) 下载最新的 macOS 版本：




```text
.
├── CMakeLists.txt
├── LICENSE
├── README.md
├── README.zh-CN.md
└── electron/             # 桌面客户端
    ├── assets/            # 图标和截图
    ├── renderer/          # 桌面界面
    ├── src/               # Electron 主进程与 preload 桥接
    └── package.json
```
## 在本地运行桌面客户端

```bash
npm --prefix electron install
npm --prefix electron start
```

从源码运行客户端需要 Node.js 与 npm。

## 许可

本项目依据 [Apache License 2.0](LICENSE) 授权。署名信息请参阅 [NOTICE](NOTICE)。
