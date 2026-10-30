# 低成本短信转发器

ESP32-C3 通过 UART 接收 ML307R 短信，通过 WiFi 转发至邮件或 Webhook。保留验证码提取、最长10段短信合并、三个推送通道、多WiFi配置、历史和定时保号。

本项目基于 [chenxuuu/sms_forwarding](https://github.com/chenxuuu/sms_forwarding/) 二次开发，保留 MIT 版权声明。界面及部分功能参考 [dushixiang/uart_sms_forwarder](https://github.com/dushixiang/uart_sms_forwarder)。

## 当前功能

- 邮件、POST JSON、Bark、GET、自定义模板及飞书群聊机器人；三个 Web 通道可同时配置。
- HTTP Basic 认证的管理页面、短信历史、模组查询、短信发送和Ping保号。
- 北京时间每日单个飞行时段，支持跨午夜、手动覆盖和恢复自动。
- 32条短信PDU队列和8条通知队列、8192字节UART接收缓冲、4096字节PDU解析容量。
- 通知队列单步处理HTTP通道；SMTP使用ReadyMail内部消息的异步状态机。

当前固件已删除 MQTT/Home Assistant、号码黑白名单和内容关键词过滤的源码、接口、页面及PubSubClient依赖。旧NVS键不再读写，未清空整个配置空间，WiFi、管理账号和推送配置继续保留。`homeassistant/`仅为legacy资料，当前固件不支持；外部Broker的retained实体不会自动删除。

Telegram Bot、企业微信机器人 Webhook、钉钉机器人 Webhook 已移除。通道类型编号保持 `0..4,8`，`5/6/7` 保留不用；加载这三个旧值或未知类型时，仅在内存中禁用对应槽位、设为 NONE 并清空 URL、key1/key2，不映射到其他通道、不主动写回配置 NVS。保存请求使用明确类型白名单，旧值或非法参数会整次拒绝，不部分应用。mDNS 保留，可继续通过 `sms.local` 访问。

## 每日飞行与手动规则

在首页保存调度开关、开始和结束时间。时间为北京时间 UTC+8，系统UTC时钟不被修改。普通时段为开始时刻包含、结束时刻不包含；例如23:00至07:00会跨午夜，开始与结束相同拒绝保存。

调度启用时，手动开启或关闭覆盖到下一次开始或结束边界；可以立即恢复自动。未校时允许手动操作，首次校时后确定覆盖截止时间。保存新规则清除覆盖。关闭调度保留当前有效目标为持久化手动目标。调度启用后重启清除临时覆盖，校时后恢复自动；调度关闭后重启重新回读并应用手动目标。已校时后断网继续使用本地时钟，时间跳变直接计算当前目标，不补做过期切换；失效覆盖不会因时间回拨重新出现。

启动先查询CFUN；调度启用但未校时时只查询，不主动开启射频、不激活PDP或等待附着。软件不能保证ML307R上电默认射频关闭，必须在目标设备上验证。当前切换使用`AT+CFUN=4`和`AT+CFUN=1`，不用复位命令代替；对应模组固件支持情况尚待硬件确认。

目标与实际状态独立展示。设置返回OK后必须回读CFUN匹配才确认状态。退出飞行后恢复`CMGF=0`、`CNMI=2,2,0,0,0`，再分别检查SIM、注册和附着。CFUN正常不代表蜂窝网络已就绪。未知、飞行、恢复或事务忙碌期间拒绝新蜂窝任务；已经开始的短信/Ping事务完整结束后再切换。保号到期最多保留一个待执行标记，恢复后执行一次，不补发多次或虚计成功。

API均使用现有Basic认证：

| 方法与路径 | 参数/用途 |
|---|---|
| GET `/airplane` | 调度、目标、实际CFUN、校时、覆盖截止、恢复、SIM/注册/附着及错误 |
| POST `/airplane/save` | 表单`enabled=true或false`、`start=HH:MM`、`end=HH:MM`，完整验证后保存 |
| POST `/airplane/action` | 表单`action=on/off/auto`，auto要求已开启调度 |

新增修改接口严格拒绝缺失、重复、额外参数及非法布尔/时间。202表示已接受、等待模组确认，页面继续读取实际状态，不将接受请求显示成切换成功。系统配置和飞行规则分别保存。

## 管理页面和本地样式缓存

恢复原版底部四个 Tab：概览、控制、历史、配置。概览显示计数和模组查询；控制包含飞行规则、手动覆盖、短信发送、流量保号和重启；历史按联系人展示聊天记录；配置使用折叠面板，保留三个推送通道。Tab 支持方向键、Home/End、Enter/空格和 `aria-selected`；默认概览，支持 `#overview`、`#control`、`#history-page`、`#config` 初始定位及 hash 切换。

以 git `c95518a` 的 `web_pages.h`、`web_handlers.ino` 为界面基准，保留原 SVG、字号、间距、概览自动查询、配置开关与通道展开、历史联系人左右栏和手机返回；对照 `assets/homepage.png`，移除 MQTT/HA、关键词过滤、号码名单及 Telegram/企业微信/钉钉通道选项和专属字段，并加入飞书与原卡片风格的每日飞行。原版 CSS 逐字保留，额外规则仅处理键盘焦点、长内容及语义按钮，不覆盖原版容器间距、字号或双列布局。样式保存在 Flash，由设备 GET `/assets/app.css?v=20261029-2` 分块发送，不依赖外网。匹配版本返回 `Cache-Control: public, max-age=31536000, immutable`；无版本或版本不符返回 `no-cache`，避免旧地址长期缓存新内容。修改 CSS 时同步更新 `APP_CSS_VERSION`；HTML 每次从设备认证读取，返回 `no-store`。样式资源公开只读且不包含配置、密码、Webhook 或密钥，JS 和全部 API 仍在本机。

可选外部托管只建议放置纯静态 CSS，以固定版本 URL 发布；启用前需评估断网可用性并保留本地 fallback，外部请求不应带配置或凭据。当前未启用外部 URL、未发布远程资源，也无需额外构建常量。

`python test_feishu.py` 会在 `build/ui-preview.html` 生成可双击的单文件离线预览，数据为宿主 mock 虚构数据，fetch 完全被本地桩替换，不读取设备也不会实际保存、发短信或通知；同时生成 `build/ui-desktop.png`、`build/ui-mobile.png`。浏览器回归使用独立 CSS 文件验证真实加载与计算样式，覆盖 320/390/600/768/1024/1440 视口，以及四 Tab、键盘/hash、历史安全文本/联系人切换/刷新/手机返回、WiFi/邮件/通道开关和全部推送类型显隐。测试直接比较指定提交的完整 CSS 与导航 SVG，并断言原版关键 DOM 和计算间距、字号，防止只保留相似配色。

## 一键清理本机短信历史

历史页右上角“清空全部”会先确认删除所有联系人的本机历史，确认后调用 Basic 认证的 `POST /history/clear`。其他 HTTP 方法返回 405，删除失败返回 500，成功返回 200 和 `success:true`。操作期间禁用按钮，完成/失败均提示；成功后直接重置联系人、聊天和选中状态，不刷新整页，并忽略清理前未完成的历史请求响应。

清理只依次删除 SPIFFS 的 `/sms.tmp`、`/sms.bak`、`/sms.txt`；主文件最后删除，任一步失败即停止，避免旧备份在下次启动时恢复已清空历史。失败可能已删掉临时文件或备份，可重试；不会修改 NVS、配置、累计统计或其他文件，不会发送 AT 命令删除 SIM 短信，也不会发送通知。清空后新收到的短信仍正常记录。

`test_storage.py` 使用实际存储和清理 handler，覆盖文件删除顺序、主/备/临时文件删除失败与恢复、仅备份存在、重复清理、认证/方法及其他文件与计数保持；`test_feishu.py` 的六种浏览器视口覆盖确认取消不请求、POST、操作禁用、失败提示、空态及旧响应防覆盖。验证不调用真实设备清理接口。

## 稳定性和内存

保留此前的日期/分段边界校验、共享逐行UART读取、URC交错处理、整行终结判断、有界响应、WiFi重连、看门狗及诊断字段。AT事务覆盖CMGS提示至最终响应、Ping激活至清理、CFUN设置回读恢复和模组查询。AT等待仅入队PDU，不递归执行主循环和网络通知。超时后排空并用有专属正文的CFUN查询重同步，短信成功还要求`+CMGS`，Ping清理增加CGACT回读。通用AT协议没有请求ID，任意延迟、无上限的迟到响应无法获得数学上的归属保证，仍需实机故障注入。

串口控制改为按行的只读查询白名单，避免原始透传破坏短信输入或绕过射频调度。支持AT、ATI、CFUN?、CPIN?、CEREG?、CGATT?；射频与短信操作使用认证页面。

首页Flash模板单次扫描，1024字节分块，WiFi和推送卡片逐项输出；配置内容按128字节分块HTML转义，不重新解释其中占位符。历史使用100个文件偏移和2048字节限长缓冲逐条输出，不构造完整数组和包装副本。历史仅保存短信约200字节UTF-8完整字符预览，不可视为完整备份。裁剪使用512字节复制缓冲，检查剩余空间、写入长度、重开文件大小和rename结果；临时文件完成后先将旧文件改为备份，再提升新文件，启动时主文件缺失则恢复备份。SPIFFS本身不是事务文件系统，不能代替硬件掉电验证。

通知槽、PDU队列和长短信槽使用`String=(const char*)nullptr`触发Core3.3.0的`invalidate()`，实际释放容量；空字符串和空String移动赋值会复用原容量。SMTP消息保持库内部所有权，停止后再清理；减少不使用JSON通道的转义副本。

HTTP连接/读取2秒、TLS握手3秒；TLS客户端生命周期覆盖HTTP客户端，包含连接失败析构路径。SMTP15秒为步骤间预算，无法抢占底层同步TCP/TLS调用；单次会话固定配置快照，配置变更取消旧任务，发送中断网记失败、不自动重发，避免重复投递。队列满时拒绝新通知并计数，5分钟过期释放，无断电持久化。飞书响应最多读取2048字节、2秒，严格检查顶层code/StatusCode为0；其他通道仍按HTTP2xx统计传输成功。HTTPS仍跳过证书验证。

Review修复补充：实际CFUN=1但未校时时允许CMGF/CNMI短信初始化；正常目标下启动APN重置成功一次，失败退避重试，退出飞行不重复重置；飞行状态每60秒回读。PDU队列32槽，溢出计入`pduDropped`，有限缓冲仍不能保证无限突发不丢。CEREG按行区分查询和异步通知。短信提交最多等待30秒，期间有界循环定期刷新看门狗；超时标记`smsResultUnknown`并暂停后续发送，需人工核实后重启，不能当作确定失败反复重发。同步发送期间管理页面可能短暂停顿。

损坏历史尾行只移除不完整记录，文件超50KB才执行容量裁剪。SPIFFS不自动格式化挂载失败的分区；历史页仅在`storageReady=false`时提供显式初始化按钮，确认后调用认证`POST /storage/init`，表单`confirm=ERASE_UNMOUNTED_STORAGE`。该操作会清除未挂载分区的数据，已有且可挂载的分区拒绝初始化；不在升级验证中调用。

系统配置采用完整验证后提交，缺失字段保留原值；无关配置保存不重置保号期限或到期待办。仅保存任务通过`/timer`发送五个任务字段，不保存其他编辑。`/timer`和`/sendsms`同时兼容原版扁平JSON及当前表单请求，重复、非法或额外字段拒绝。配置请求和副本使用有界堆分配，失败时拒绝提交，避免扩大主循环栈占用。

## 构建与离线验证

锁定Arduino CLI1.2.2、ESP32 Core3.3.0、ReadyMail0.4.2、pdulib0.5.11，见`toolchain-lock.json`。项目内工具位于`.tools/`和`.arduino/`。

```bash
python build.py setup
python build.py host
python test_modem.py
python test_notifications.py
python test_feishu.py
python test_airplane.py
python test_airplane_device.py
python test_storage.py
python test_sms_receive.py
python test_config_api.py
python build.py compile
```

已有工具链时无需执行setup。目标`esp32:esp32:esp32c3:PartitionScheme=huge_app`，产物`build/esp32c3/`，构建日志`build/compile.log`。测试提取实际固件函数，以宿主串口、时钟、网络、SMTP和文件系统mock运行，不烧录、不发送真实通知。

新增测试覆盖所有分钟的普通/跨午夜边界、未校时/首次校时、重启、手动覆盖、时间跳变、CFUN错误/回读不符/未知、CNMI恢复失败、事务互斥、认证/重复/非法参数、迟到OK重同步及历史裁剪写入/空间/提升失败恢复。原通知压力测试运行10万轮、20万任务，验证宿主分配净增长；该结果不等于ESP32真实堆测试。

内存历史参考基线为静态49448字节、freeHeap142888、minHeap65552、largestBlock114676。当前编译尺寸见当天项目日志。首页峰值降低25KiB、最低堆提升16KiB、队列填满排空100轮后净差不超过2KiB等目标需要相同设备、配置和负载实测，当前不能宣称达标。历史应用层缓冲为2048字节加400字节偏移，裁剪复制缓冲512字节，均与记录总量无关。

## 飞书

通道选择飞书（type=8），填写Webhook和可选签名密钥。签名使用秒级时间戳，以`timestamp + '\n' + secret`为HMAC-SHA256 key、空字符串为message，再Base64；timestamp/sign放POST JSON顶层。未校时立即跳过加签并计失败，不等待NTP、不自动重试。飞书路径不输出Webhook、密钥或签名请求体。机器人自身关键词校验仍须满足。

## 硬件和使用

ESP32-C3 Super Mini配ML307R-DC，GPIO3接ML307 RX、GPIO4接ML307 TX，GND共地，ML307 VCC/EN接合适的5V供电并安装天线。部分SIM/运营商兼容性需要自测。修改`code/wifi_config.h`默认WiFi；首次访问`sms.local`或串口显示的IP，默认账号`admin`、密码`admin123`，及时修改管理密码。网页可保存三个WiFi，变更后需重启。

2026-10-29的Review修复已通过八组回归、真实编译并烧录COM3，管理页面、存储挂载与SIM/注册/附着状态回读正常。未执行真实短信发送、存储清空/格式化或主动飞行切换。后续在设备上确认CFUN=4支持、上电射频默认行为、跨日/跨午夜、退出飞行后SIM注册/短信恢复，以及供电和Flash掉电恢复；再进行72小时故障注入及7天观察。断网/恢复WiFi、阻断NTP、延迟HTTP和AT响应、在Ping及短信发送期间接收短信，核对计数、队列、复位原因、恢复耗时和堆趋势。

每分钟输出`health`，认证后的`/stats`包含`minHeap`、`largestBlock`、`maxLoopMs`、队列深度、丢弃计数和复位原因。验收要求无意外复位、主循环和通知可恢复、预热后堆和最大连续块没有下降趋势。UART8192字节不保证任意突发流量绝不丢失。

## 文件

- `code/code.ino`：启动和主循环。
- `code/config.h/.ino`：配置、统计和历史存储。
- `code/airplane_mode.h/.ino`：每日调度、飞行状态机和认证接口。
- `code/web_handlers.h/.ino`、`web_pages.h`、`web_query.ino`：页面与AT事务。
- `code/sms_handler.h/.ino`：短信接收、发送与合并。
- `code/push_service.h/.ino`：HTTP/邮件/飞书及通知队列。
- `homeassistant/`：legacy归档。

## License

MIT
