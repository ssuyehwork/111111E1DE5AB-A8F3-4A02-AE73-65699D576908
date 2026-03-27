# ArcMeta 开发需求文档

---

## 应用命名说明

应用名称：**ArcMeta**

命名逻辑：
- Arc：归档（Archive）+ 速度感（电弧），呼应 MFT 极速索引的核心特性
- Meta：元数据（Metadata），呼应标签、星级、颜色标记等核心功能
- 合在一起：一个以极速索引为基础、以元数据管理为核心的文件管理器
- 内部文件命名前缀 .am_ 与应用名首字母 AM 保持一致，天然匹配

---

## 重要声明

- 本项目只在 Windows 系统环境下运行，不考虑任何跨平台兼容性
- 编译器只使用 MSVC 2022 64位，不使用 MinGW，不使用 GCC，不使用 Clang
- 所有代码均面向 MSVC 编译器编写，可自由使用 MSVC 专有特性
- 不需要任何跨平台宏、条件编译、平台抽象层

---

## 技术栈

- 语言：C++17
- IDE：Qt Creator 19.0.0 (Community)
- 编译器：MSVC 2022 64位（唯一编译器，通过 Qt Creator 工具链配置）
- Qt 版本：Qt 6.10.2 msvc2022_64
- Qt 安装路径：C:\Qt\6.10.2\msvc2022_64
- 构建系统：CMake（使用 CMakeLists.txt，Qt Creator 原生支持）
- UI 框架：Qt Widgets
- 数据库：SQLite（使用 SQLiteCpp 封装库）
- 并行：std::execution::par（C++17 并行算法，MSVC 下开箱即用）
- 平台：Windows only

## CMakeLists.txt 要求

- cmake_minimum_required(VERSION 3.20)
- set(CMAKE_CXX_STANDARD 17)
- 编译选项：/W4 /O2（MSVC 专用，不使用 -Wall 等 GCC 风格选项）
- 链接库（完整列表，缺一不可）：
  ntdll        （MFT 读取底层支持）
  ole32        （COM 初始化）
  bcrypt.lib   （AES-256 加密，Windows CNG API）
- 可执行文件需通过 .manifest 文件声明
  requestedExecutionLevel: requireAdministrator
  以确保 MFT 读取所需的管理员权限

---

## 代码组织结构

src/
├── mft/
│   ├── MftReader.h / .cpp         // MFT 读取，构建 FileIndex
│   ├── UsnWatcher.h / .cpp        // USN Journal 实时监听
│   └── PathBuilder.h / .cpp       // FRN → 完整路径重建
├── meta/
│   ├── AmMetaJson.h / .cpp        // .am_meta.json 读写（含安全写入）
│   └── SyncQueue.h / .cpp         // 懒更新队列
├── db/
│   ├── Database.h / .cpp          // SQLite 连接与 schema 初始化
│   ├── FolderRepo.h / .cpp        // folders 表的 CRUD
│   ├── ItemRepo.h / .cpp          // items 表的 CRUD
│   ├── CategoryRepo.h / .cpp      // categories / category_items 表的 CRUD
│   ├── FavoritesRepo.h / .cpp     // favorites 表的 CRUD
│   └── SyncEngine.h / .cpp        // 增量同步 + 全量扫描逻辑
├── crypto/
│   └── EncryptionManager.h / .cpp // AES-256 加密/解密，统一封装
├── ui/
│   ├── MainWindow.h / .cpp        // 主窗口，六栏布局
│   ├── CategoryPanel.h / .cpp     // 面板一：分类面板
│   ├── NavPanel.h / .cpp          // 面板二：导航面板
│   ├── FavoritesPanel.h / .cpp    // 面板三：收藏面板
│   ├── ContentPanel.h / .cpp      // 面板四：内容面板
│   ├── MetaPanel.h / .cpp         // 面板五：元数据面板
│   └── FilterPanel.h / .cpp       // 面板六：高级筛选面板
└── main.cpp

---

## 开发协作规范（Jules 必须在任何操作之前阅读并严格遵守）

### 黄金原则：先确认，后执行

任何情况下，收到需求或修改请求后，禁止直接动手修改代码。
必须严格按照以下四步流程执行：

第一步：理解阶段
- 收到需求后，用自己的语言复述对需求的理解
- 列出打算修改哪些文件、改动哪些内容
- 列出可能存在的疑问或歧义点

第二步：确认阶段
- 明确提问：「我的理解是否正确？是否可以开始执行？」
- 等待用户明确回复「确认」或「可以」之后才能动手
- 如果用户的回复有修正，重新复述修正后的理解，再次请求确认
- 直到双方达成共识后才进入第三步

第三步：执行阶段
- 仅修改确认范围内的内容，不擅自扩大修改范围
- 不得以「顺便优化」「顺便修复」为由修改未经确认的部分

第四步：汇报阶段
- 列出本次实际修改的文件和具体内容
- 说明是否完全符合确认时的方案
- 如有偏差主动说明原因

### 禁止行为

- 禁止收到消息后不经确认直接修改代码
- 禁止「我认为这样更好」式的自作主张改动
- 禁止在修复一个问题时顺带改动其他未提及的部分
- 禁止用「已完成」敷衍汇报，必须具体说明改了什么

---

## 模块一：MFT 文件索引

### 数据结构

struct FileEntry {
    DWORDLONG frn;         // File Reference Number
    DWORDLONG parentFrn;   // 父目录 FRN
    std::wstring name;     // 文件名（宽字符）
    DWORD attributes;      // 文件属性
    bool isDir() const { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};

using FileIndex = std::unordered_map<DWORDLONG, FileEntry>;

### MFT 读取（MftReader）

- 使用 CreateFileW 打开卷句柄，路径格式如 \\.\C:
- 需要管理员权限，无权限时执行以下降级方案：
  1. 索引层：切换为标准递归目录遍历（std::filesystem::recursive_directory_iterator）
  2. 监听层：切换为 QFileSystemWatcher 或 ReadDirectoryChangesW 监控活动目录
- 使用 DeviceIoControl + FSCTL_ENUM_USN_DATA 批量枚举 MFT 记录
- 缓冲区大小：64KB，循环读取直至枚举完毕
- index.reserve(1,000,000) 预分配，避免频繁 rehash
- 每条 USN_RECORD_V2 提取以下字段：
  FileReferenceNumber、ParentFileReferenceNumber、
  FileName、FileAttributes
- 所有字符串使用宽字符 std::wstring

### 路径重建（PathBuilder）

- 通过 parentFrn 递归向上查找，拼接完整路径
- 根目录判断条件：(parentFrn & 0x0000FFFFFFFFFFFF) == 5
- 最大递归深度 64 层，防止循环引用导致死循环

### 并行搜索

- 将 FileIndex 中所有 FileEntry 指针收集到 std::vector
- 使用 std::execution::par + std::for_each 并行过滤
- 支持大小写不敏感匹配
- 使用 std::mutex 保护搜索结果的收集过程

### USN Journal 实时监听（UsnWatcher，独立后台线程）

- 启动时使用 FSCTL_QUERY_USN_JOURNAL 获取当前 Journal 状态
- 使用 FSCTL_READ_USN_JOURNAL 持续监听变更事件
- 监听以下四种事件并执行对应操作：

  USN_REASON_FILE_CREATE
  → 插入新 FileEntry 到 FileIndex

  USN_REASON_FILE_DELETE
  → 从 FileIndex 删除对应 FRN
  → 从数据库 items 表删除对应 path 记录
  → 从数据库 folders 表删除对应 path 记录
  → 级联删除 items 表中 parent_path 等于该路径的所有记录
  → 从 category_items 表删除对应 item_path 记录

  USN_REASON_RENAME_OLD_NAME
  → 标记旧路径失效
  → 利用 FRN 在 FileIndex 中追踪新路径
  → 自动将旧路径关联的元数据（标签、星级等）迁移至新路径对应的 .am_meta.json
  → 路径是表现，FRN 是唯一身份标识

  USN_REASON_RENAME_NEW_NAME
  → 插入新 FileEntry 到 FileIndex
  → 触发元数据原子迁移逻辑，确保资产不丢失

- 轮询间隔：200ms
- 使用 std::mutex 保护 FileIndex 的读写

---

## 模块二：.am_meta.json 文件格式

每个文件夹下按需创建 .am_meta.json，记录该文件夹及其内部
文件和子文件夹的元数据。.am_meta.json 文件本身不计入 items 列表。

### 完整 JSON 结构

{
  "version": "1",
  "folder": {
    "sort_by": "name",
    "sort_order": "asc",
    "rating": 0,
    "color": "",
    "tags": [],
    "pinned": false,
    "note": ""
  },
  "items": {
    "文件名或子文件夹名": {
      "type": "file | folder",
      "rating": 0,
      "color": "",
      "tags": [],
      "pinned": false,
      "note": "",
      "encrypted": false,
      "encrypt_salt": "",
      "encrypt_verify_hash": "",
      "original_name": "",
      "frn": 0
    }
  }
}

### 字段说明

- folder：描述当前文件夹自身的元数据与视图配置
- folder.sort_by 可选值：
  name | size | ctime | mtime | type | rating | color | custom
- folder.sort_order 可选值：asc | desc
- color 字段：颜色标记字符串，只能使用以下固定值之一，
  空字符串表示无标记（此映射表为全局唯一标准，UI 层和存储层均以此为准）：

  ""        → 无色
  "red"     → #E24B4A
  "orange"  → #EF9F27
  "yellow"  → #FAC775
  "green"   → #639922
  "cyan"    → #1D9E75
  "blue"    → #378ADD
  "purple"  → #7F77DD
  "gray"    → #5F5E5A

- tags 字段：存储标签文字的字符串数组，直接存文字不存 ID，
  例如 ["工作", "待整理", "重要"]
- note 字段：备注文字，字符串，默认空字符串
- encrypted 字段：是否已加密，布尔值，默认 false
- encrypt_salt：加密 salt，仅 encrypted=true 时有值
- encrypt_verify_hash：加密校验哈希，仅 encrypted=true 时有值
- original_name：加密前的原始文件名，仅 encrypted=true 时有值
- items 只记录有过用户操作的条目，即至少满足以下之一：
  rating > 0，或 color 非空，或 tags 非空，或 pinned 为 true，
  或 note 非空，或 encrypted 为 true
  无任何操作的文件不写入 items

### 安全写入流程（AmMetaJson，必须严格遵守）

1. 将新内容序列化后写入 .am_meta.json.tmp 临时文件
2. 尝试重新解析 .tmp 文件，验证是合法完整的 JSON
3. 验证成功：
   调用 rename 将 .tmp 原子替换为 .am_meta.json
   替换完成后立即调用 SetFileAttributesW 将
   .am_meta.json 设置为隐藏属性（FILE_ATTRIBUTE_HIDDEN）
   目的是防止用户在文件管理器中误删该文件
4. 验证失败：
   删除 .tmp 文件
   记录错误日志
   保留原 .am_meta.json 不做任何修改
   向调用方返回写入失败的错误码

注意：.am_meta.json 首次创建时也必须在写入完成后
立即设置 FILE_ATTRIBUTE_HIDDEN 隐藏属性。
内容面板在显示目录内容时必须过滤掉 .am_meta.json 和
.am_meta.json.tmp，不将其作为普通文件展示给用户。

---

## 模块三：数据库 Schema（SQLite）

数据库仅作为全局搜索和聚合查询的索引。
.am_meta.json 是主数据源，数据库是从。
数据库损坏时可通过全量扫描从 JSON 完整重建。

### 建表语句

-- 文件夹元数据表
CREATE TABLE IF NOT EXISTS folders (
    path        TEXT PRIMARY KEY,
    rating      INTEGER DEFAULT 0,
    color       TEXT    DEFAULT '',
    tags        TEXT    DEFAULT '',    -- JSON 数组字符串
    pinned      INTEGER DEFAULT 0,
    note        TEXT    DEFAULT '',
    sort_by     TEXT    DEFAULT 'name',
    sort_order  TEXT    DEFAULT 'asc',
    last_sync   REAL                   -- 对应 .am_meta.json 的 mtime
);

-- 文件与子文件夹元数据表
CREATE TABLE IF NOT EXISTS items (
    path               TEXT,
    frn                INTEGER PRIMARY KEY,  -- FRN 作为主键实现跨路径追踪
    type               TEXT,                 -- 'file' | 'folder'
    rating             INTEGER DEFAULT 0,
    color              TEXT    DEFAULT '',
    tags               TEXT    DEFAULT '',  -- JSON 数组字符串
    pinned             INTEGER DEFAULT 0,
    note               TEXT    DEFAULT '',
    encrypted          INTEGER DEFAULT 0,
    encrypt_salt       TEXT    DEFAULT '',
    encrypt_verify_hash TEXT   DEFAULT '',
    original_name      TEXT    DEFAULT '',
    parent_path        TEXT
);

-- 标签聚合索引表
CREATE TABLE IF NOT EXISTS tags (
    tag         TEXT PRIMARY KEY,
    item_count  INTEGER DEFAULT 0
);

-- 收藏夹表
CREATE TABLE IF NOT EXISTS favorites (
    path        TEXT PRIMARY KEY,       -- 文件或文件夹的完整路径
    type        TEXT,                   -- 'file' | 'folder'
    name        TEXT,                   -- 显示名称
    sort_order  INTEGER DEFAULT 0,      -- 在收藏列表中的排序位置
    added_at    REAL                    -- 添加时间戳
);

-- 分类表
CREATE TABLE IF NOT EXISTS categories (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    parent_id           INTEGER DEFAULT 0,  -- 0 表示顶级分类
    name                TEXT NOT NULL,
    color               TEXT DEFAULT '',    -- 使用颜色字符串映射表中的值
    preset_tags         TEXT DEFAULT '',    -- JSON 数组字符串
    sort_order          INTEGER DEFAULT 0,
    pinned              INTEGER DEFAULT 0,
    encrypted           INTEGER DEFAULT 0,
    encrypt_salt        TEXT DEFAULT '',
    encrypt_verify_hash TEXT DEFAULT '',
    created_at          REAL
);

-- 分类条目关联表
CREATE TABLE IF NOT EXISTS category_items (
    category_id INTEGER,
    item_path   TEXT,                   -- 条目完整路径
    added_at    REAL,
    PRIMARY KEY (category_id, item_path)
);

-- 同步状态与全局配置表
CREATE TABLE IF NOT EXISTS sync_state (
    key         TEXT PRIMARY KEY,
    value       TEXT
);
-- 存储键值对，包括：
-- key = 'last_sync_time'   → 上次程序正常关闭时的时间戳
-- key = 'panel_widths'     → 六个面板宽度的 JSON 数组，下次启动时恢复
-- key = 'last_path'        → 上次退出时浏览的路径

### 索引

CREATE INDEX IF NOT EXISTS idx_items_path     ON items(path);
CREATE INDEX IF NOT EXISTS idx_items_parent   ON items(parent_path);
CREATE INDEX IF NOT EXISTS idx_items_rating   ON items(rating);
CREATE INDEX IF NOT EXISTS idx_items_color    ON items(color);
CREATE INDEX IF NOT EXISTS idx_items_tags     ON items(tags);
CREATE INDEX IF NOT EXISTS idx_items_pinned   ON items(pinned);
CREATE INDEX IF NOT EXISTS idx_cat_parent     ON categories(parent_id);
CREATE INDEX IF NOT EXISTS idx_cat_items_path ON category_items(item_path);

---

## 模块四：同步机制

### 懒更新队列（SyncQueue）

- 用户操作触发 JSON 安全写入成功后，将该文件夹路径加入队列
- 防抖合并：同一路径的多次变更合并为一条，只同步最终状态
- 后台线程空闲时批量从队列取出，解析 JSON 后写入数据库
- SQLite 所有写操作使用事务批量提交，不逐条提交
- 程序正常关闭前必须刷空队列，确保数据库与 JSON 一致

### 增量同步（SyncEngine，程序启动时自动执行）

- 从 sync_state 表读取 last_sync_time
- 遍历所有已知文件夹路径，获取 .am_meta.json 的 mtime
- 只对 mtime > last_sync_time 的文件执行解析和数据库更新
- 全部完成后将 sync_state 中 last_sync_time 更新为当前时间戳

### 全量扫描（SyncEngine，用户手动触发）

- 忽略 last_sync_time
- 递归遍历所有可访问路径，收集全部 .am_meta.json 文件
- 清空并重建 folders、items、tags 三张表
- categories、category_items、favorites 三张表不清空（用户手动管理的数据）
- 提供进度回调接口：std::function<void(int current, int total)>
  供 UI 层显示进度

### 标签聚合表维护

- tags 表不做实时维护
- 在每次增量同步完成后和全量扫描完成后重新聚合计数
- 聚合逻辑：从 items 表和 folders 表的 tags 字段解析所有
  标签字符串，统计每个标签出现的条目数，全量写入 tags 表

### 路径失效清理（由 UsnWatcher 触发）

收到 FILE_DELETE 或 RENAME_OLD_NAME 事件时执行：
1. 删除 folders 表中 path = 目标路径 的记录
2. 删除 items 表中 path = 目标路径 的记录
3. 删除 items 表中 parent_path = 目标路径 的所有子记录
4. 删除 category_items 表中 item_path = 目标路径 的记录
5. 触发一次标签聚合表重新计数

### 元数据迁移事务机制 (Metadata Transaction)
在进行批量重命名、移动或跨目录操作时，必须遵循“两阶段提交”原则：
1. **预变更阶段**：在内存中构建 `OldFRN -> NewData` 的映射，并预读所有受影响的 `.am_meta.json`。
2. **原子执行阶段**：先执行物理文件操作，若成功则立即批量更新 JSON 持久化层。
3. **回滚处理**：若物理操作中途失败，利用内存映射表撤回已完成的物理动作。

---

## 模块五：加密模块（EncryptionManager）

### 加密实现

- 使用 Windows CNG API 实现 AES-256-CBC
- 头文件：bcrypt.h
- 链接库：bcrypt.lib
- 不引入任何第三方加密库

### 密钥派生

- 用户设定密码后通过 PBKDF2 派生 AES 密钥
- 不明文存储密码
- 只存储 salt 和校验哈希用于验证身份

### 文件加密流程

1. 用户对某个文件执行「加密保护」
2. 弹出密码设定对话框（两次输入确认）
3. 生成随机 salt，通过 PBKDF2 派生密钥
4. 使用 AES-256-CBC 加密原文件内容
5. 加密结果写入 原文件名.amenc 文件
6. 验证 .amenc 文件可以被正确解密
7. 验证成功后删除原始文件
8. 将加密元数据写入该文件夹的 .am_meta.json：
   encrypted: true
   encrypt_salt: "..."
   encrypt_verify_hash: "..."
   original_name: "原始文件名"

### 文件解密访问流程

1. 用户双击已加密条目
2. 弹出密码输入对话框
3. 输入密码后验证哈希
4. 验证通过：临时解密到 GetTempPath()\amtemp\ 目录下
5. 用系统默认程序打开临时解密文件
6. 用户关闭文件后自动删除临时解密文件
7. 验证失败：提示「密码错误」，不展示任何文件内容

### 临时文件管理

- 临时目录：GetTempPath() 返回值 + \amtemp\
- 程序启动时：清理 \amtemp\ 下的全部残留文件
- 程序关闭时：再次清理 \amtemp\ 下的全部文件

### 分类密码保护

- 分类面板的密码保护与文件加密共用 EncryptionManager
- 启用密码保护的分类，点击时弹出密码验证对话框
- 验证通过后展示该分类内容，验证失败则拒绝访问

---

## UI 模块：自定义标题栏按钮设计规范

基于分析，MainWindow 的自定义标题栏按钮设计参数如下：

### 1. 基础视觉样式 (funcBtnStyle)
- **按钮尺寸**: 固定为 **24x24px**。
- **圆角半径**: **4px**（为了与 QuickWindow 风格对齐）。
- **图标尺寸**: 矢量图标统一使用 **18x18px**。
- **背景色状态**:
  - **默认**: `transparent`（透明）。
  - **悬停 (Hover)**: `rgba(255, 255, 255, 0.1)`（浅白透明）。
  - **按下 (Pressed)**: `rgba(255, 255, 255, 0.2)`。
- **内边距**: `0px`。

### 2. 物理排列顺序 (严格从右往左)
按照设计要求，标题栏右侧按钮组的物理显示顺序为：
1. **关闭按钮**: 悬停背景色使用专用红色 `#e81123`。
2. **最大化/还原按钮**。
3. **最小化按钮**。
4. **置顶按钮**: 具备切换状态。

### 3. 交互逻辑 (ToolTip 增强)
- **拦截机制**: 通过 `eventFilter` 物理级拦截原生 ToolTip。
- **显示标准**: 调用全局 `ToolTipOverlay` 显示自定义样式的 Tip。
- **持续时间**: 统一设定为 **2000ms**（2秒）。

---

## UI 模块：ToolTipOverlay 设计规范

### 1. 窗口底层属性
- **窗口标志 (WindowFlags)**: `Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput | Qt::NoDropShadowWindowHint | Qt::WindowDoesNotAcceptFocus`。
- **窗口属性 (Attributes)**: `Qt::WA_TranslucentBackground`, `Qt::WA_ShowWithoutActivating`。
- **特性**: 物理级 0 延时显隐（规避系统淡入淡出动画），支持鼠标穿透，不抢占焦点。

### 2. 视觉表现参数
- **背景色**: `#2B2B2B`（深灰）。
- **边框**: 宽度 `1px`，默认颜色 `#B0B0B0`（绘制时使用 `0.5px` 物理对齐校准）。
- **圆角半径**: 固定 `4px`。
- **阴影/投影**: 无（严格执行扁平化 Flat 风格）。
- **内边距 (Padding)**: 水平 `12px`，垂直 `8px`。
- **尺寸限制**:
  - **最小尺寸**: `40x24px`。
  - **最大宽度**: `450px`（超出则自动折行）。

### 3. 文本渲染属性
- **字体规范**: `9pt`，优先使用 `微软雅黑 (Microsoft YaHei)` 或 `Segoe UI`。
- **文字颜色**: 强制覆盖为 `#EEEEEE`（采用 `!important` 样式表确保全局一致）。
- **内容处理**: 支持标准 HTML 包装器，支持富文本渲染。

### 4. 交互逻辑参数
- **弹出位置**: 相对于当前光标坐标偏移 `QPoint(15, 15)`。
- **边缘保护**: 内置 `QScreen` 屏幕边界检测，防止提示框溢出可视区域。
- **持续时间 (Timeout)**:
  - **标题栏按钮专属**: `2000ms`（2秒）。
  - **通用默认值**: `700ms`。
  - **上限限制**: `60000ms`（60秒）。
- **刷新机制**: 采用计时器重置逻辑，确保连续交互时内容实时更新，不产生视觉残留。

---

## UI 模块：整体布局

### 主窗口尺寸

- 默认尺寸：1600 x 900（像素）
- 最小尺寸：1280 x 720，低于此尺寸不允许继续缩小

### 六栏布局（从左到右）

[ 分类面板 ] [ 导航面板 ] [ 收藏面板 ] [ 内容面板 ] [ 元数据面板 ] [ 筛选面板 ]

各面板之间使用 QSplitter 分隔。
QSplitter 分隔条宽度：3px（全局统一，不得使用其他值）
所有面板最小宽度：200px（全局统一，以确保在 1280px 最小宽度下逻辑自洽）

### 六个面板默认宽度分配（合计 1600px）

分类面板：230px
导航面板：200px
收藏面板：200px
内容面板：弹性伸缩，占据剩余空间
  （1600 - 230 - 200 - 200 - 240 - 230 - 5个分隔条×3px = 485px 起始值）
元数据面板：240px
筛选面板：230px

面板宽度在 sync_state 表的 panel_widths 键中持久化，
程序重启后恢复上次的宽度配置。

### 顶部工具栏

使用 QToolBar 实现，高度：40px，内边距左右：12px，控件间距：8px

控件从左到右排列：
- 后退按钮（QPushButton）：高 28px，最小宽 60px
- 前进按钮（QPushButton）：高 28px，最小宽 60px
- 上级按钮（QPushButton）：高 28px，最小宽 60px
- 当前路径框（QLineEdit）：弹性占满剩余空间，最小宽 200px，
  支持直接输入路径回车跳转
- 搜索框（QLineEdit）：固定宽度 200px，
  实时过滤当前目录内容，支持文件名模糊匹配
- 视图切换按钮：切换网格视图 / 列表视图
- 全量扫描按钮：触发全量扫描，显示进度对话框

---

## UI 面板一：分类面板

参考截图中展示的分类管理面板，结合 Adobe Bridge 的
Collections 概念与自定义分类系统。

### 面板结构

顶部统计区（固定高度，不随内容滚动）：
每行显示一个统计项，格式为：图标 + 名称 + 数量
- 全部数据（数据库中 items 总条目数）
- 今日数据（今日 ctime 或 mtime 的条目数）
- 昨日数据
- 最近访问（最近 7 天内 atime 有记录的条目数）
- 未分类（未出现在任何 category_items 中的条目数）
- 未标签（tags 为空的条目数）
- 收藏（pinned = true 的条目数）
- 回收站（软删除的条目数）

分类树区（可垂直滚动）：
- 显示 categories 表中所有分类，支持树状层级
- 每项显示：缩进层级 + 分类颜色圆点 + 分类名称 + 条目数量
- 置顶分类（pinned=1）显示图钉图标
- 加密分类（encrypted=1）显示锁图标
- 支持展开 / 折叠子分类
- 点击分类：内容面板显示该分类下所有条目
- 支持拖拽排序（更新 sort_order 字段）

底部工具栏（固定高度 36px）：
- 「+ 新建分类」按钮
- 搜索框：过滤分类列表

### 分类右键菜单（完整列表，按顺序）

- 新建数据
- 归类到此分类
- ────────────────
- 导入数据
- 导出
- ────────────────
- 设置颜色
- 随机颜色
- 设置预设标签
- ────────────────
- 新建分类（同级）
- 新建子分类
- ────────────────
- 置顶 / 取消置顶
- 重命名
- 删除（删除分类，条目不删除，变为未分类）
- ────────────────
- 排列（子菜单：按名称 / 按数量 / 按创建时间）
- 密码保护

### 拖拽行为

- 从内容面板拖拽条目到某个分类上，松开后该条目归属到该分类
- 预设标签追加到条目现有标签（不覆盖已有标签）
- 预设颜色：仅条目当前 color 为空时才赋予，已有颜色不覆盖
- 分类之间支持拖拽调整顺序

---

## UI 面板二：导航面板

参考 Adobe Bridge 的 Folders Panel。

- 使用 QTreeView + QFileSystemModel 实现
- 只显示文件夹，不显示文件
- 支持展开 / 折叠子目录
- 当前选中目录高亮显示
- 点击目录：内容面板同步更新
- 初始展开到 sync_state 中记录的 last_path，
  若无记录则展开到用户主目录

---

## UI 面板三：收藏面板

参考 Adobe Bridge 的 Favorites Panel。

- 使用 QListWidget 实现
- 每项显示：系统图标（16x16px）+ 名称
- 列表项高度：32px，左右内边距：10px
- 点击收藏的文件夹：内容面板跳转到该目录
- 点击收藏的文件：用系统默认程序打开
- 右键菜单：「从收藏夹移除」
- 拖拽放置区域：高度 60px，虚线边框，圆角 6px，
  提示文字「拖拽文件夹或文件到此处」
- 支持从内容面板拖拽（内部拖拽）
- 支持从 Windows 资源管理器拖拽（外部拖拽）
  接受 QDropEvent，处理 MIME 类型 text/uri-list
- 收藏数据持久化到数据库 favorites 表
- 分组标题字体：11px，颜色偏灰，上下内边距各 4px

---

## UI 面板四：内容面板

参考 Adobe Bridge 的 Content Panel。

### 网格视图（默认）

- 使用 QListView，ViewMode = IconMode
- 图标默认尺寸：64x64px，可调范围：32px 至 128px
- 网格单元格：图标区 + 上下内边距 8px + 文件名区 32px
- 网格列间距：8px，行间距：8px
- 文件名字体：12px，超出宽度截断显示省略号
- 星级显示：图标下方，使用 ★ 符号，字体 11px，颜色 #EF9F27
- 颜色圆点：图标右下角，尺寸 8x8px，偏移 right:2px bottom:2px
- 置顶图钉图标：图标左上角，16x16px
- 加密锁图标：图标右下角（与颜色圆点错开位置），16x16px

### 列表视图

- 使用 QTreeView
- 列定义及默认宽度：
  名称列：弹性伸缩（最小 180px）
  类型列：80px
  大小列：90px
  修改时间列：150px
  星级列：90px（显示 ★ 符号）
  颜色标记列：80px（显示颜色圆点）
  加密列：60px（显示锁图标）
  标签列：弹性伸缩（最小 120px）
- 支持点击列标题排序

### 排序规则

sort_by 对应 .am_meta.json 中 folder.sort_by 字段：
name | size | ctime | mtime | type | rating | color | custom

置顶规则：pinned=true 的条目始终排在最前，
两组内部各自按当前 sort_by 排序，不受置顶影响。

排序方式和方向持久化保存到当前目录的 .am_meta.json。

### 选中行为

- 单击：选中，元数据面板同步显示
- 双击文件夹：进入该目录
- 双击已加密文件：弹出密码验证对话框
- 双击普通文件：用系统默认程序打开
- 多选：Ctrl+单击，Shift+单击

### 内容面板右键菜单（完整列表，按顺序）

- 打开
- 用系统默认程序打开
- 在资源管理器中显示
- ────────────────
- 归类到...（子菜单显示所有分类）
- 添加到收藏夹
- ────────────────
- 置顶 / 取消置顶
- 加密保护 / 解除加密 / 修改密码
- ────────────────
- 批量重命名 (Ctrl+M)
- ────────────────
- 重命名
- 复制
- 剪切
- 粘贴
- 删除（移入回收站）
- ────────────────
- 复制路径
- 属性

---

## UI 面板五：元数据面板

参考 Adobe Bridge 的 Metadata Panel + Keywords Panel 合并版本。

### 面板内边距与间距

内边距：12px
各区块之间间距：12px
分隔线：1px，颜色使用 QPalette 中间色
标签文字（Label）字体：11px，颜色偏灰
值文字字体：13px

### 只读信息区

- 名称
- 类型（文件 / 文件夹）
- 文件大小
- 创建时间
- 修改时间
- 访问时间
- 完整路径
- 加密状态（已加密 / 未加密）

### 可编辑元数据区

置顶开关：
- QCheckBox 或 Toggle 样式
- 修改后立即触发 .am_meta.json 安全写入

星级（使用 ★ 符号，不使用数字）：
- 五颗星横向排列，每颗星尺寸：20x20px，星间距：4px
- 选中颜色：#EF9F27，未选中颜色：使用 QPalette 边框色
- 点击第 N 颗星设置星级为 N
- 点击已选中的同一颗星则清除星级（归零）
- 显示格式：★★★☆☆（不显示数字）
- 修改后立即触发 .am_meta.json 安全写入

颜色标记：
- 颜色圆点横向排列，每个圆点尺寸：18x18px，圆点间距：6px
- 选中状态：圆点外围显示 2px 边框，边框颜色使用 QPalette 文字色
- 颜色顺序（对应颜色字符串映射表）：
  无色(#888780) | red(#E24B4A) | orange(#EF9F27) | yellow(#FAC775) |
  green(#639922) | cyan(#1D9E75) | blue(#378ADD) | purple(#7F77DD) |
  gray(#5F5E5A)
- 点击选中，再次点击已选中颜色则清除
- 修改后立即触发 .am_meta.json 安全写入

标签 / 关键字：
- 已有标签以 Tag Pill（圆角标签）展示：
  高度：22px，左右内边距：8px，圆角：11px，字体：12px
  标签之间间距：4px，换行间距：4px
- 点击标签旁的 × 按钮删除该标签
- 输入框支持输入新标签后按 Enter 或逗号添加
- 支持从数据库 tags 表中自动补全
- 修改后立即触发 .am_meta.json 安全写入

备注：
- QPlainTextEdit，最小高度：80px，最大高度：160px
- 超出 160px 后内部滚动
- 失去焦点时触发 .am_meta.json 安全写入

加密操作区：
- 未加密：显示「加密保护」按钮
- 已加密：显示「解除加密」和「修改密码」按钮
  解除加密需再次输入密码验证身份

### 多选行为

- 只读信息区显示「已选中 N 个条目」
- 星级、颜色、标签支持批量修改
- 备注区显示为灰色禁用状态，不支持多选编辑
- 批量加密：右键菜单操作，不在元数据面板提供

---

## UI 面板六：高级筛选面板

参考 Adobe Bridge 的 Filter Panel。
所有筛选条件为 AND 关系（同时满足所有已勾选的条件）。

各筛选分组之间间距：12px
分组标题字体：11px，font-weight 500，颜色偏灰
复选框行高：24px，复选框与文字间距：6px
数量显示字体：11px，颜色偏灰，靠右对齐

### 筛选条件（每项显示符合条件的数量）

星级筛选（QCheckBox，可多选）：
- 无星级
- ★
- ★★
- ★★★
- ★★★★
- ★★★★★
- ★★★ 及以上（快捷选项，等同于同时勾选 ★★★/★★★★/★★★★★）
注意：星级选项必须使用 ★ 符号展示，禁止显示为「1星」等文字形式

颜色标记筛选（QCheckBox，可多选）：
- 每个颜色显示为颜色圆点 + 颜色名称 + 数量

标签筛选（QCheckBox，可多选）：
- 显示当前目录出现的所有标签

文件类型筛选（QCheckBox，可多选）：
- 文件夹
- 按扩展名分组

文件大小筛选：
- 双端滑块（RangeSlider）
- 单位自动换算（B / KB / MB / GB）

置顶筛选：
- 复选框：只显示 pinned=true 的条目

加密筛选：
- 复选框：只显示 encrypted=true 的条目

顶部「清除所有筛选」按钮：一键重置所有条件
筛选状态不持久化，关闭程序后清除

---

## 数据联动规则

- 内容面板选中变化 → 元数据面板刷新
- 元数据面板修改 → 立即写入 .am_meta.json（安全写入三步流程）
  → 加入懒更新队列 → 后台同步到数据库
  → 内容面板对应条目视觉状态同步刷新（星级/颜色圆点/图钉图标等）
- 筛选面板条件变化 → 内容面板实时过滤刷新
- 导航面板或收藏面板切换目录
  → 内容面板重新加载
  → 筛选面板重置
  → 元数据面板清空
- 分类面板点击某个分类
  → 内容面板切换为显示该分类下的条目
  → 筛选面板重置
  → 元数据面板清空
- 各面板之间通过 Qt Signal / Slot 通信，不直接互相引用实现文件

---

## 分阶段实现顺序

### 第一阶段（优先实现，核心数据层）

- .am_meta.json 的完整安全读写逻辑（含隐藏属性设置）
- 数据库 schema 完整初始化（全部建表语句）
- 懒更新队列（SyncQueue）
- 增量同步（启动时自动执行）
- 全量扫描（用户手动触发，带进度回调）
- 程序关闭时刷空懒更新队列

### 第二阶段（文件索引层）

- MFT 读取（IFileIndexer 接口先行，隔离实现细节）
- USN Journal 实时监听
- 路径失效级联清理

### 第三阶段（UI 层）

- 主窗口六栏布局（QSplitter）
- 各面板基础框架
- 内容面板网格视图 + 列表视图
- 元数据面板编辑功能
- 导航面板文件树
- 收藏面板拖拽功能

### 第四阶段（高级功能）

- 分类面板完整功能
- 加密保护（EncryptionManager）
- 高级筛选面板
- 置顶功能

---

## 模块六：批量重命名引擎 (BatchRenameEngine)

支持工业级的“组件化”重命名方案，通过管道模式依次处理文件名。

### 文件名构建组件 (Component types)
- **文本组件**：插入固定字符
- **序列数字**：支持起始序号、步长、位阶补零（如 001, 002）
- **日期组件**：支持多种格式定义（yyyyMMdd 等）
- **元数据变量**：支持注入 ArcMeta 标签、星级、或物理属性（如图片长宽）

### 操作逻辑
1. **预检阶段**：使用 QtConcurrent 异步计算所有新文件名。
2. **冲突预警**：检测到重名冲突时在 UI 高亮红色并锁定执行按钮。
3. **元数据迁移**：重命名成功后，必须同步更新对应的 `.am_meta.json` 键值并重算数据库。

---

## 模块七：跨目录暂存篮 (Scratchpad)

主界面底部或侧边提供可收起的“暂存篮”面板。

- **逻辑**：仅存储文件路径引用（FRN），不涉及物理移动。
- **批量动作**：支持对来自不同文件夹的“大杂烩”文件进行统一打标、批量加密或一键归类到某一分类。

---

## 模块八：专业缩略图与快速预览 (QuickLook)

### 缩略图增强
- 驱动：优先调用 Windows Shell (IShellItemImageFactory)。
- 格式：支持 PSD、AI、EPS 等专业素材缩略图。
- 性能：后台线程池异步生成，配合内存 LRU 缓存 (QCache)。

### 空格键快速预览
- **触发**：内容面板选中项按下 Space 键弹出。
- **渲染策略**：
  - 图片/PSD：QGraphicsView 硬件加速。
  - 文本/MD：内存映射 (QFile::map) 秒开，仅加载前 128KB。
- **交互**：预览状态下支持直接按 `1-5` 设置星级或标签，实现“一边确认一边筛选”。

## 快捷键矩阵 (Hotkeys)

### 文件操作
- **Enter**：打开文件 / 进入目录
- **F2**：快速重命名
- **Delete**：移至回收站 / **Shift+Delete**：彻底删除
- **Ctrl+C / X / V**：复制、剪切、粘贴
- **Ctrl+Shift+C**：复制选中项完整物理路径

### 快速打标
- **Space**：触发 QuickLook 预览
- **1 - 5**：设置星级 / **0**：清除星级
- **Alt + 1~9**：应用对应的颜色标记
- **Ctrl+T**：聚焦标签编辑框

### 导航与视图
- **Ctrl+L / Alt+D**：聚焦路径输入框
- **Ctrl+F**：聚焦搜索过滤框
- **Backspace**：返回上级目录
- **Ctrl+[ / ]**：路径历史后退 / 前进
- **Ctrl+\**：切换网格 / 列表视图
- **Alt+Q**：切换窗口置顶

---

## 全局注意事项

- 所有文件路径统一使用宽字符 std::wstring
- std::wstring 与 Qt 的 QString 之间通过
  QString::fromStdWString / toStdWString 转换
- 颜色标记在 .am_meta.json 和数据库中存储颜色字符串
  （"red"/"cyan" 等），UI 层根据本文档颜色映射表转换为 QColor，
  全局只有一套映射表，不得在多处各自定义
- 内容面板图标优先使用 QFileIconProvider 获取系统图标，
  无法获取时降级为内置默认图标
- .am_meta.json 和 .am_meta.json.tmp 必须从内容面板过滤掉，
  不展示给用户
- 加密功能只使用 Windows CNG API（bcrypt.h + bcrypt.lib），
  不引入任何第三方加密库
- SQLite 所有写操作使用事务批量提交，不逐条提交
- MFT 读取和 USN 监听依赖管理员权限，
  .manifest 必须声明 requireAdministrator
- 各模块之间通过接口解耦，不直接互相引用实现文件
- 禁止使用绝对定位（QWidget::move + resize），
  必须使用 QLayout 系列布局管理器
- 禁止所有控件使用默认尺寸不做任何调整
- 禁止忽略最小尺寸限制
