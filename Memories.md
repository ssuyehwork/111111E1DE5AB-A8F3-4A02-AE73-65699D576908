迭代维护与升级：
1. 为了降低后期的维护成本，所以需要将曾经的错误记录到这里并注释原因，这样就不会发生重蹈覆辙。

2. 右键菜单的圆角设计为 8 像素

3. 悬浮面板 圆角设计为 6 像素

4. 悬停高亮 圆角设计为为 4 像素

---

### 🏛️ FramelessDialog 界面架构标准规范 (2026-03-xx 确立)

**1. 基础物理布局与样式 (Standard Layout & Styling):**
*   **外层布局 (`m_outerLayout`)**: `QVBoxLayout`，提供 `20px` 呼吸感外边距（用于承载阴影）。
*   **主容器 (`m_container`)**: `QFrame` (ID: `DialogContainer`)。
    *   **圆角 (Radius)**: 正常状态 `12px`，最大化 `0px`。
    *   **边框 (Border)**: `1px solid #333333`。
    *   **背景 (Background)**: `#1e1e1e`。
*   **主布局 (`m_mainLayout`)**: 内部 `QVBoxLayout`，`Margins` 和 `Spacing` 均为 `0`。
*   **内容区 (`m_contentArea`)**: `QWidget` (ID: `DialogContentArea`)。

**2. 标题栏与系统按钮 (TitleBar & System Buttons):**
*   **标题栏容器 (TitleBar)**: 固定高度 `32px`，背景 `#252526`，底边框 `1px solid #333333`。
*   **按钮组逻辑 (从右到左)**:
    1.  **关闭 (`close`)**: `28x28px`, `Radius: 4px`, 悬停背景 `#E81123`。
    2.  **最大化/还原 (`maximize`/`restore`)**: `28x28px`, `Radius: 4px`, 悬停背景 `rgba(255,255,255,0.1)`。
    3.  **最小化 (`minimize`)**: `28x28px`, `Radius: 4px`。
    4.  **置顶 (`pin`)**: 选中态图标橙色 (`#FF551C`)，背景高亮。

**3. 核心交互机制 (Core Interaction Mechanics):**
*   **WinAPI 深度集成**: 通过 `WM_NCHITTEST` 模拟原生边缘缩放 (Resize) 和标题栏拖拽 (HTCAPTION)。
*   **交互排除**: 拖拽逻辑必须跳过所有可交互子控件（按钮、搜索框等）。
*   **持久化机制**: 基于 `objectName` 自动保存 `StayOnTop` 偏好至 `QSettings`。
*   **UI 指示器**: 必须安装全局事件过滤器以重定向所有 ToolTip 至 `ToolTipOverlay`。
