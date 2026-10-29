#ifndef WEB_PAGES_H
#define WEB_PAGES_H
#define APP_CSS_VERSION "20261029-3"
const char commonCss[] PROGMEM = R"CSS(
:root{--primary:#6366f1;--primary-dark:#4f46e5;--bg:#f8fafc;--card:#ffffff;--text:#334155;--text-light:#64748b;--border:#e2e8f0;--success:#22c55e;--danger:#ef4444;--warning:#eab308;--info:#3b82f6}
*{box-sizing:border-box;margin:0;padding:0;outline:none;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);line-height:1.5;padding:12px 12px 70px}
.c{max-width:900px;margin:0 auto;padding:0 20px}

/* 导航栏 */
.nav{position:fixed;bottom:0;left:50%;transform:translateX(-50%);width:calc(100% - 24px);max-width:900px;background:rgba(255,255,255,0.95);backdrop-filter:blur(10px);border:1px solid var(--border);display:flex;justify-content:space-around;padding:8px;z-index:999;border-radius:16px;margin-bottom:12px;box-shadow:0 -2px 10px rgba(0,0,0,0.05)}
.nav-item{flex:1;text-align:center;padding:6px;border-radius:12px;color:var(--text-light);font-size:0.75em;cursor:pointer;transition:all .2s;display:flex;flex-direction:column;align-items:center;gap:4px}
.nav-item.active{color:var(--primary);background:#e0e7ff33}
.nav-icon{width:24px;height:24px;background:currentcolor;mask-size:contain;mask-repeat:no-repeat;-webkit-mask-size:contain;-webkit-mask-repeat:no-repeat}

/* 页面 */
.page{display:none;animation:fadeIn .3s ease}.page.active{display:block}
@keyframes fadeIn{from{opacity:0;transform:translateY(5px)}to{opacity:1;transform:translateY(0)}}

/* 卡片 */
.card{background:var(--card);border-radius:16px;padding:16px;margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,0.05);border:1px solid var(--border)}
.card-t{font-size:1.1em;font-weight:700;margin-bottom:12px;color:#1e293b;display:flex;justify-content:space-between;align-items:center}
.card-sub{font-size:0.85em;color:var(--text-light);font-weight:400}

/* 数据看板 */
.grid-2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px}
.stat-box{background:#f8fafc;padding:12px;border-radius:12px;text-align:center;border:1px solid var(--border)}
.stat-num{font-size:1.4em;font-weight:800;color:var(--primary);line-height:1.2}
.stat-tag{font-size:0.8em;color:var(--text-light)}

/* 状态徽章 */
.badge{padding:3px 8px;border-radius:99px;font-size:0.75em;font-weight:600;display:inline-flex;align-items:center;gap:4px}
.b-ok{background:#dcfce7;color:#15803d}
.b-err{background:#fee2e2;color:#b91c1c}
.b-wait{background:#f1f5f9;color:#475569}
.b-warn{background:#fef9c3;color:#a16207}

/* 表单元素 */
.fg{margin-bottom:16px}
label{display:block;margin-bottom:6px;font-size:0.9em;font-weight:600;color:#475569}
input,select,textarea{width:100%;padding:12px;border:1px solid var(--border);border-radius:10px;font-size:1em;color:#1e293b;background:#fff;transition:border-color .2s}
input:focus,textarea:focus{border-color:var(--primary);box-shadow:0 0 0 3px rgba(99,102,241,0.1)}
.btn{width:100%;padding:14px;border:none;border-radius:12px;background:var(--primary);color:#fff;font-weight:600;font-size:1em;cursor:pointer;transition:transform .1s,opacity .2s;display:flex;align-items:center;justify-content:center;gap:8px}
.btn:active{transform:scale(0.98);opacity:0.9}
.btn-w{background:#fff;color:var(--text);border:1px solid var(--border)}
.btn-d{background:var(--danger);color:#fff}

/* 折叠面板 */
details{border:1px solid var(--border);border-radius:12px;margin-bottom:8px;overflow:hidden;background:#fff}
summary{padding:14px;background:#f8fafc;cursor:pointer;font-weight:600;list-style:none;display:flex;justify-content:space-between;align-items:center}
summary::-webkit-details-marker{display:none}
summary:after{content:'+';font-size:1.2em;color:var(--text-light)}
details[open] summary:after{content:'-'}
details[open] summary{border-bottom:1px solid var(--border)}
.det-body{padding:16px}

/* 模式切换按钮组 */
.mode-toggle{display:flex;gap:8px;margin-bottom:12px}
.mode-btn{flex:1;padding:12px 8px;border:2px solid var(--border);border-radius:10px;background:#fff;cursor:pointer;text-align:center;transition:all .2s;font-size:0.9em}
.mode-btn:hover{border-color:#cbd5e1;background:#f8fafc}
.mode-btn.active{border-color:var(--primary);background:#eef2ff}
.mode-btn .mode-icon{font-size:1.4em;margin-bottom:4px}
.mode-btn .mode-title{font-weight:600;color:#1e293b}
.mode-btn .mode-desc{font-size:0.8em;color:var(--text-light);margin-top:2px}
.mode-btn.active .mode-title{color:var(--primary)}

/* 开关行 */
.sw-row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;cursor:pointer}
.sw{width:44px;height:24px;background:#cbd5e1;border-radius:24px;position:relative;transition:0.3s}
.sw:after{content:'';width:20px;height:20px;background:#fff;border-radius:50%;position:absolute;top:2px;left:2px;transition:0.3s;box-shadow:0 1px 2px rgba(0,0,0,0.2)}
.sw.on{background:var(--primary)}
.sw.on:after{left:22px}

/* 历史记录 - 聊天界面风格 */
.sms-container{display:flex;height:calc(100vh - 190px);gap:0;overflow:hidden;border:1px solid var(--border);background:#fff}
.contact-list{width:250px;min-width:180px;border-right:1px solid var(--border);overflow-y:auto;background:#fafbfc;flex-shrink:0}
.contact-list::-webkit-scrollbar{width:4px}
.contact-list::-webkit-scrollbar-thumb{background:#cbd5e1;border-radius:2px}
.contact-item{padding:12px 14px;cursor:pointer;border-bottom:1px solid #f1f5f9;transition:all .2s;position:relative;display:flex;align-items:center;gap:12px}
.contact-item:hover{background:#f1f5f9}
.contact-item.active{background:#eff6ff}
.contact-avatar{width:40px;height:40px;border-radius:50%;background:#e2e8f0;color:#fff;display:flex;align-items:center;justify-content:center;font-weight:700;font-size:1em;flex-shrink:0;text-transform:uppercase}
.contact-info{flex:1;overflow:hidden;display:flex;flex-direction:column;justify-content:center}
.contact-name-row{display:flex;justify-content:space-between;align-items:center;margin-bottom:2px}
.contact-name{font-size:0.95em;font-weight:600;color:#334155;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.contact-time{font-size:0.75em;color:#94a3b8;flex-shrink:0;margin-left:8px}
.contact-preview-row{display:flex;justify-content:space-between;align-items:center}
.contact-preview{font-size:0.8em;color:#94a3b8;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1}
.contact-badge{background:#f1f5f9;color:#64748b;border-radius:99px;font-size:0.75em;padding:2px 8px;margin-left:8px;flex-shrink:0;font-weight:500}

.chat-area{flex:1;display:flex;flex-direction:column;min-width:0;background:#fff}
.chat-header{padding:12px 16px;border-bottom:1px solid var(--border);background:#fff;display:flex;align-items:center;justify-content:space-between;z-index:10}
.chat-title{font-weight:700;font-size:1em;color:#1e293b;display:flex;align-items:center;gap:8px}
.chat-msg-count{font-size:0.75em;color:#94a3b8;background:#f1f5f9;padding:2px 8px;border-radius:12px}
.chat-messages{flex:1;overflow-y:auto;padding:16px;background:#fff}
.chat-messages::-webkit-scrollbar{width:4px}
.chat-messages::-webkit-scrollbar-thumb{background:#cbd5e1;border-radius:2px}
.chat-empty{text-align:center;padding:60px 20px;color:#94a3b8;font-size:0.9em;display:flex;flex-direction:column;align-items:center;gap:12px}
.chat-day{text-align:center;margin:20px 0 16px;position:relative}
.chat-day::before{content:'';position:absolute;top:50%;left:0;right:0;height:1px;background:#f1f5f9;z-index:0}
.chat-day span{position:relative;background:#fff;padding:4px 12px;font-size:0.75em;color:#cbd5e1;border:1px solid #f1f5f9;border-radius:99px;z-index:1}
.msg-bubble{max-width:80%;padding:10px 14px;margin-bottom:12px;border-radius:18px;position:relative;font-size:0.95em;line-height:1.6;word-break:break-word;box-shadow:0 1px 2px rgba(0,0,0,0.02)}
.msg-in{background:#f1f5f9;color:#334155;border-bottom-left-radius:4px;margin-right:auto}
.msg-time{font-size:0.7em;color:rgba(0,0,0,0.3);margin-top:4px;display:block;text-align:right}

/* 消息提示 */
.toast{position:fixed;top:20px;left:50%;transform:translateX(-50%);background:rgba(0,0,0,0.8);color:#fff;padding:8px 16px;border-radius:99px;font-size:0.85em;z-index:9999;transition:all .3s cubic-bezier(0.18, 0.89, 0.32, 1.28);opacity:0;transform:translate(-50%, -20px);pointer-events:none}
.toast.show{opacity:1;transform:translate(-50%, 0)}

/* 信息表格 */
.info-table{width:100%;font-size:0.85em;border-collapse:collapse}
.info-table td{padding:8px 0;border-bottom:1px solid #f8fafc}
.info-table td:first-child{color:var(--text-light);width:35%}

/* 移动端适配 */
.chat-back{display:none}
@media(max-width:600px){
  .sms-container{position:relative}
  .contact-list{width:100%;border-right:none}
  .chat-area{position:absolute;top:0;left:100%;width:100%;height:100%;transition:left .3s ease;z-index:20}
  .chat-area.show{left:0}
  .chat-back{display:flex;align-items:center;justify-content:center;width:32px;height:32px;margin-right:4px;cursor:pointer;border-radius:50%}
  .chat-back:active{background:#f1f5f9}
  .contact-item.active{background:transparent} /* 移动端取消列表选中高亮 */
}

[hidden]{display:none!important}
button{font:inherit;cursor:pointer}button:focus-visible,[tabindex]:focus-visible,summary:focus-visible{outline:2px solid var(--primary);outline-offset:3px}
.contact-item{width:100%;text-align:left;border-top:0;border-left:0;border-right:0;background:transparent}
.chat-back{border:0;background:transparent;flex-shrink:0}
.grid-2>*,.grid-3>*,.det-body{min-width:0}input,select,textarea{min-width:0;max-width:100%}
.info-table{table-layout:fixed}.info-table td,.msg-bubble,#modNet,#modSig{overflow-wrap:anywhere}
pre{white-space:pre-wrap;overflow-wrap:anywhere;font:inherit;font-size:.85em;margin:12px 0}
.actions{display:flex;flex-wrap:wrap;gap:8px;margin:12px 0}.actions>.btn{flex:1 1 140px;width:auto}
@media(max-width:600px){.chat-area:not(.show){visibility:hidden}}
/* 每日飞行时段：仅在本卡片内生效 */
.air-card .air-heading{gap:8px;flex-wrap:wrap}
.air-card .air-status{margin-bottom:16px;font-size:.85em}
.air-card .air-summary{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:12px}
.air-card .air-current{font-size:.95em;padding:5px 12px}
.air-card .air-clock{display:flex;gap:8px;flex-wrap:wrap;color:var(--text-light);font-variant-numeric:tabular-nums}
.air-card .air-clock strong{color:var(--text);font-weight:600}
.air-card .air-group{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}
.air-card .air-tag{display:inline-flex;flex-wrap:wrap;align-items:center;gap:6px;padding:4px 8px;background:var(--bg);border:1px solid var(--border);border-radius:8px;color:var(--text-light)}
.air-card .air-tag strong{color:var(--text);font-weight:600}
.air-card .air-note{margin-top:8px;color:var(--text-light);overflow-wrap:anywhere}
.air-card .air-override{padding:8px 12px;background:#eef2ff;border-radius:8px;color:var(--primary-dark)}
.air-card .air-error{padding:8px 12px;background:#fee2e2;border-radius:8px;color:#b91c1c;white-space:pre-wrap}
.air-card .air-fields{display:grid;grid-template-columns:minmax(100px,1fr) repeat(2,minmax(0,1fr));gap:12px;align-items:end}
.air-card .air-fields label{min-width:0;margin-bottom:0}
.air-card .air-fields input,.air-card .air-fields select{margin-top:6px;min-height:46px}
.air-card .air-rule{font-size:.8em;color:var(--text-light);margin:8px 0 12px}
.air-card .air-actions{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin:12px 0}
.air-card .air-actions .btn{font-size:.8em;padding:12px 4px;min-width:0}
.air-card .air-help{line-height:1.7}
.air-card .air-actions .btn:disabled{opacity:.5;cursor:not-allowed}
.air-card :focus-visible{outline:2px solid var(--primary);outline-offset:3px}
@media(max-width:600px){.air-card .air-fields{grid-template-columns:repeat(2,minmax(0,1fr))}.air-card .air-schedule{grid-column:1/-1}.air-card .air-fields input{padding:10px 8px}}
@media(max-width:360px){.air-card .air-actions{grid-template-columns:1fr}.air-card .air-heading .card-sub{font-size:.75em}}
)CSS";
const char htmlPage[] PROGMEM = R"PAGE(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=0"><title>短信转发器</title>
<link rel="stylesheet" href="/assets/app.css?v=%CSS_VERSION%">
</head><body><div class="c">

<div class="nav" role="tablist" aria-label="管理页面">
  <div class="nav-item active" id="tab-overview" role="tab" aria-controls="overview" aria-selected="true" tabindex="0" onclick="swTab(0)">
    <div class="nav-icon" style="-webkit-mask-image:url('data:image/svg+xml;utf8,<svg viewBox=\'0 0 24 24\' xmlns=\'http://www.w3.org/2000/svg\'><path d=\'M3 13h8V3H3v10zm0 8h8v-6H3v6zm10 0h8V11h-8v10zm0-18v6h8V3h-8z\'/></svg>')"></div>
    <span>概览</span>
  </div>
  <div class="nav-item" id="tab-control" role="tab" aria-controls="control" aria-selected="false" tabindex="-1" onclick="swTab(1)">
    <div class="nav-icon" style="-webkit-mask-image:url('data:image/svg+xml;utf8,<svg viewBox=\'0 0 24 24\' xmlns=\'http://www.w3.org/2000/svg\'><path d=\'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-1 17.93c-3.95-.49-7-3.85-7-7.93 0-.62.08-1.21.21-1.79L9 15v1c0 1.1.9 2 2 2v1.93zm6.9-2.54c-.26-.81-1-1.39-1.9-1.39h-1v-3c0-.55-.45-1-1-1H8v-2h2c.55 0 1-.45 1-1V7h2c1.1 0 2-.9 2-2v-.41c2.93 1.19 5 4.06 5 7.41 0 2.08-.8 3.97-2.1 5.39z\'/></svg>')"></div>
    <span>控制</span>
  </div>
  <div class="nav-item" id="tab-history" role="tab" aria-controls="history-page" aria-selected="false" tabindex="-1" onclick="swTab(2)">
    <div class="nav-icon" style="-webkit-mask-image:url('data:image/svg+xml;utf8,<svg viewBox=\'0 0 24 24\' xmlns=\'http://www.w3.org/2000/svg\'><path d=\'M13 3c-4.97 0-9 4.03-9 9H1l3.89 3.89.07.14L9 12H6c0-3.87 3.13-7 7-7s7 3.13 7 7-3.13 7-7 7c-1.93 0-3.68-.79-4.94-2.06l-1.42 1.42C8.27 19.99 10.51 21 13 21c4.97 0 9-4.03 9-9s-4.03-9-9-9zm-1 5v5l4.28 2.54.72-1.21-3.5-2.08V8H12z\'/></svg>')"></div>
    <span>历史</span>
  </div>
  <div class="nav-item" id="tab-config" role="tab" aria-controls="config" aria-selected="false" tabindex="-1" onclick="swTab(3)">
    <div class="nav-icon" style="-webkit-mask-image:url('data:image/svg+xml;utf8,<svg viewBox=%270 0 24 24%27 xmlns=%27http://www.w3.org/2000/svg%27><path d=%27M12 15.5A3.5 3.5 0 0 1 8.5 12 3.5 3.5 0 0 1 12 8.5a3.5 3.5 0 0 1 3.5 3.5 3.5 3.5 0 0 1-3.5 3.5m7.43-2.53c.04-.32.07-.64.07-.97s-.03-.66-.07-1l2.11-1.63c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.31-.61-.22l-2.49 1c-.52-.39-1.06-.73-1.69-.98l-.37-2.65A.506.506 0 0 0 14 2h-4c-.25 0-.46.18-.5.42l-.37 2.65c-.63.25-1.17.59-1.69.98l-2.49-1c-.22-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64L4.57 11c-.04.34-.07.67-.07 1s.03.65.07.97l-2.11 1.66c-.19.15-.25.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1.01c.52.4 1.06.74 1.69.99l.37 2.65c.04.24.25.42.5.42h4c.25 0 .46-.18.5-.42l.37-2.65c.63-.26 1.17-.59 1.69-.99l2.49 1.01c.22.08.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.66z%27/></svg>');mask-image:url('data:image/svg+xml;utf8,<svg viewBox=%270 0 24 24%27 xmlns=%27http://www.w3.org/2000/svg%27><path d=%27M12 15.5A3.5 3.5 0 0 1 8.5 12 3.5 3.5 0 0 1 12 8.5a3.5 3.5 0 0 1 3.5 3.5 3.5 3.5 0 0 1-3.5 3.5m7.43-2.53c.04-.32.07-.64.07-.97s-.03-.66-.07-1l2.11-1.63c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.31-.61-.22l-2.49 1c-.52-.39-1.06-.73-1.69-.98l-.37-2.65A.506.506 0 0 0 14 2h-4c-.25 0-.46.18-.5.42l-.37 2.65c-.63.25-1.17.59-1.69.98l-2.49-1c-.22-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64L4.57 11c-.04.34-.07.67-.07 1s.03.65.07.97l-2.11 1.66c-.19.15-.25.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1.01c.52.4 1.06.74 1.69.99l.37 2.65c.04.24.25.42.5.42h4c.25 0 .46-.18.5-.42l.37-2.65c.63-.26 1.17-.59 1.69-.99l2.49 1.01c.22.08.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.66z%27/></svg>')"></div>
    <span>配置</span>
  </div>
</div>

<div id="toast" class="toast"></div>

<!-- ============ 概览页 ============ -->
<div class="page active" id="overview" role="tabpanel" aria-labelledby="tab-overview">
  <div class="card">
    <div class="card-t">统计数据 <span class="badge b-wait" id="upT">-</span></div>
    <div class="grid-3">
      <div class="stat-box"><div class="stat-num" id="ssRecv">-</div><div class="stat-tag">收信</div></div>
      <div class="stat-box"><div class="stat-num" id="ssSent">-</div><div class="stat-tag">发信</div></div>
      <div class="stat-box"><div class="stat-num" id="ssBoot">-</div><div class="stat-tag">重启</div></div>
    </div>
  </div>

  <div class="card">
    <div class="card-t">状态看板</div>
    <div class="grid-2" style="margin-bottom:12px">
      <div class="stat-box" style="text-align:left">
        <div class="stat-tag">WiFi 信号</div>
        <div style="font-weight:700" id="wifiS">- dBm</div>
        <div class="badge b-ok" id="ipStr">%IP%</div>
      </div>

    </div>
    
    <div class="stat-box" style="text-align:left;position:relative;margin-bottom:12px">
       <div class="stat-tag">模组网络</div>
       <div style="font-weight:700;font-size:1.1em;margin:4px 0" id="modNet">查询中...</div>
       <div style="font-size:0.85em;color:#64748b" id="modSig">信号强度: -</div>
       <div style="position:absolute;top:10px;right:10px" class="badge b-wait" id="modSim">SIM?</div>
    </div>

    <div class="sw-row">
      <span style="font-weight:600;color:#64748b">系统看门狗</span>
      <span class="badge b-ok">30秒自动复位</span>
    </div>
  </div>
</div>

<!-- ============ 控制页 ============ -->
<div class="page" id="control" role="tabpanel" aria-labelledby="tab-control" hidden>
<div class="card air-card">
  <div class="card-t air-heading">每日飞行时段 <span class="card-sub">北京时间 · UTC+8</span></div>
  <div id="airStatus" class="air-status" role="status" aria-live="polite">
    <div class="air-summary"><span id="airCurrent" class="badge b-wait air-current">读取中</span><div class="air-clock"><span id="airDate">—</span><strong id="airClock">等待设备时间</strong></div></div>
    <div class="air-group" aria-label="飞行状态"><span class="air-tag">目标 <strong id="airTarget">—</strong></span><span class="air-tag">实际 <strong id="airActual">—</strong></span></div>
    <div class="air-group" aria-label="蜂窝连接"><span class="air-tag">SIM <strong id="airSim">—</strong></span><span class="air-tag">网络 <strong id="airNetwork">—</strong></span><span class="air-tag">附着 <strong id="airAttach">—</strong></span></div>
    <p id="airProgress" class="air-note" hidden></p><p id="airOverride" class="air-note air-override" hidden></p><p id="airError" class="air-note air-error" hidden></p>
  </div>
  <form id="air" onsubmit="saveAir(event)">
    <div class="air-fields"><label class="air-schedule">每日调度<select name="enabled" id="airEnabled"><option value="false">关闭</option><option value="true">开启</option></select></label><label>开始时间<input type="time" name="start" id="airStart" required></label><label>结束时间<input type="time" name="end" id="airEnd" required></label></div>
    <p id="airRule" class="air-rule">每天按北京时间执行，支持跨午夜。</p><button class="btn">保存飞行规则</button>
  </form>
  <div class="air-actions"><button type="button" class="btn btn-w" onclick="airAction('on')">手动开启飞行</button><button type="button" class="btn btn-w" onclick="airAction('off')">手动关闭飞行</button><button type="button" id="airAuto" class="btn btn-w" onclick="airAction('auto')">恢复自动</button></div>
  <p class="card-sub air-help">开启调度时，手动覆盖持续至下一开始或结束边界；关闭调度时，手动状态持续保留。开启调度后重启，校时后恢复自动。操作结果以设备回读为准。</p>
</div>
  <div class="card">
    <div class="card-t">发送短信</div>
    <div class="fg"><label>目标号码</label><input id="sPh" placeholder="10086" type="tel"></div>
    <div class="fg"><label>短信内容</label><textarea id="sTx" placeholder="内容支持中文" rows="4"></textarea></div>
    <button class="btn" onclick="act('sms')">发送 (Send)</button>
  </div>

  <div class="card">
    <div class="card-t">流量保号</div>
    <div class="sw-row"><span id="pStat">Ping 8.8.8.8 (会产生少量流量费用)</span> <button class="btn btn-w" style="width:auto;padding:6px 16px" onclick="if(confirm('确定要消耗流量保号吗？\n此操作会产生少量流量/话费'))act('ping')">消耗流量</button></div>
    <div id="pLog" style="margin-top:8px;font-size:0.85em;color:#64748b;display:none"></div>
  </div>


  <div class="card">
    <div class="card-t" style="color:var(--danger)">危险操作</div>
    <button class="btn btn-d" onclick="if(confirm('确定重启?'))act('reboot')">重启设备 (Reboot)</button>
  </div>
</div>

<!-- ============ 历史页 ============ -->
<div class="page" id="history-page" role="tabpanel" aria-labelledby="tab-history" hidden>
  <div class="card" style="padding:0;overflow:hidden">
    <div style="padding:12px 16px;border-bottom:1px solid var(--border);display:flex;justify-content:space-between;align-items:center;background:#fafbfc">
      <span style="font-weight:700;font-size:1.1em">短信历史</span>
      <div style="display:flex;gap:8px">
        <button type="button" class="btn btn-w" style="width:auto;padding:8px 12px;font-size:0.85em" onclick="loadHist()">刷新</button>
        <button type="button" id="clearHistBtn" class="btn btn-d" style="width:auto;padding:8px 12px;font-size:0.85em" onclick="clearHist()">清空全部</button>
      </div>
    </div>
    <div id="storageWarning" style="padding:12px 16px" role="alert" hidden>
      <p>短信存储未挂载，无法保存历史。首次使用可初始化；挂载失败也可能是已有数据损坏，请先检查备份。</p>
      <button type="button" id="storageInitBtn" class="btn btn-d" onclick="initStorage()">初始化未挂载存储（清除分区数据）</button>
    </div>
    <div class="sms-container" id="smsContainer">
      <div class="contact-list" id="contactList">
        <div style="text-align:center;padding:20px;color:#94a3b8;font-size:0.85em">加载中...</div>
      </div>
      <div class="chat-area">
        <div class="chat-header" id="chatHeader" style="display:none">
          <div>
            <div class="chat-title" id="chatTitle">
               <button type="button" class="chat-back" aria-label="返回联系人" onclick="backToList()">
                 <svg viewBox="0 0 24 24" width="24" height="24"><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z" fill="currentColor"/></svg>
               </button>
               <span id="chatName">选择联系人</span>
            </div>
            <div class="chat-msg-count" id="chatCount" style="margin-left:36px"></div>
          </div>
        </div>
        <div class="chat-messages" id="chatMessages">
          <div class="chat-empty">选择左侧号码查看短信</div>
        </div>
      </div>
    </div>
  </div>
</div>

<!-- ============ 配置页 ============ -->
<div class="page" id="config" role="tabpanel" aria-labelledby="tab-config" hidden>
  <form id="cf" onsubmit="event.preventDefault();post('/save',new URLSearchParams(new FormData(this)))">
  
%CONFIG%<div style="height:60px"></div><div style="position:fixed;bottom:80px;left:0;right:0;padding:12px;pointer-events:none"><button class="c btn" style="pointer-events:auto;box-shadow:0 10px 20px rgba(99,102,241,0.3)">保存所有系统配置</button></div>
  </form>
</div>

<script>
const $=id=>document.getElementById(id);
let toastTimer;function notice(text){$('toast').textContent=text;$('toast').className='toast show';clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').className='toast',2000)}
async function post(url,body){try{const r=await fetch(url,{method:'POST',body});const d=await r.json();notice(d.message||'请求已处理');if(url.startsWith('/airplane'))await refreshAir(false);return d}catch(e){notice('请求失败：'+e.message)}}
let airLoaded=false,airEdited=false;
function airRule(){const start=$('airStart').value,end=$('airEnd').value;if(!start||!end)return;const minutes=t=>Number(t.slice(0,2))*60+Number(t.slice(3));const span=(minutes(end)-minutes(start)+1440)%1440;$('airRule').textContent=start===end?'开始与结束时间不能相同。':(end<start?'跨日 · 次日结束':'当日结束')+' · 飞行 '+Math.floor(span/60)+' 小时'+(span%60?' '+span%60+' 分钟':'')+($('airEnabled').value==='true'?' · 每日执行':' · 调度已关闭')}
$('air').addEventListener('input',()=>{airEdited=true;airRule()});
$('air').addEventListener('change',()=>{airEdited=true;airRule()});
function renderAir(d){
  const actual={1:'正常',4:'飞行'}[d.actual]||'未知';
  const current=d.switching?'切换中':d.restoring?'恢复中':actual==='未知'?'状态未知':actual==='飞行'?'飞行模式':'正常运行';
  $('airCurrent').textContent=current;$('airCurrent').className='badge air-current '+(d.switching||d.restoring?'b-warn':d.actual===1?'b-ok':'b-wait');
  const clock=String(d.beijing||'未校时').split(' ');$('airDate').textContent=d.synced===false?'设备时间':clock.length>1?clock[0]:'设备时间';$('airClock').textContent=d.synced===false?'未校时':clock.length>1?clock.slice(1).join(' '):clock[0];
  $('airTarget').textContent=d.targetKnown?(d.target?'飞行':'正常'):'待校时确认';$('airActual').textContent=actual;
  $('airSim').textContent=d.simReady?'已就绪':'未就绪';$('airNetwork').textContent=d.registered?'已注册':'未注册';$('airAttach').textContent=d.attached?'已附着':'未附着';
  const progress=[d.synced===false?'设备尚未校时，自动调度等待时间同步。':'',d.switching?'正在切换射频状态，请等待设备回读。':d.restoring?'正在恢复模组与短信接收，请等待网络就绪。':''].filter(Boolean).join(' ');
  $('airProgress').textContent=progress;$('airProgress').hidden=!progress;
  const until=d.overrideUntil?new Date(d.overrideUntil*1000).toLocaleString('zh-CN',{timeZone:'Asia/Shanghai',hour12:false}):'首次校时后确定';
  $('airOverride').textContent=d.override?'临时覆盖 · 手动'+(d.target?'开启':'关闭')+'飞行 · 截止 '+until+'（北京时间）':d.enabled?'自动调度 · 无临时覆盖':'调度已关闭 · 当前状态手动保持';$('airOverride').hidden=false;
  $('airError').textContent=d.error?'设备错误：'+d.error:'';$('airError').hidden=!d.error;$('airAuto').disabled=!d.enabled;$('airAuto').title=d.enabled?'清除临时覆盖，按每日规则执行':'请先开启并保存每日调度';
}
async function refreshAir(fill){try{const r=await fetch('/airplane');if(r.ok===false)throw new Error('HTTP '+r.status);const d=await r.json();if(fill&&!airLoaded){if(!airEdited){$('airEnabled').value=String(d.enabled);$('airStart').value=d.start;$('airEnd').value=d.end;airRule()}airLoaded=true}renderAir(d)}catch(e){$('airError').textContent='飞行状态读取失败，请等待刷新或检查设备连接。';$('airError').hidden=false}}
function saveAir(e){e.preventDefault();return post('/airplane/save',new URLSearchParams(new FormData(e.target)))}
function airAction(action){return post('/airplane/action',new URLSearchParams({action}))}
async function initStorage(){if(!confirm('此操作会格式化未挂载的短信存储分区，原有短信历史将无法恢复。仅在确认首次使用或已备份数据后继续，是否初始化？'))return;const b=$('storageInitBtn');b.disabled=true;try{const d=await post('/storage/init',new URLSearchParams({confirm:'ERASE_UNMOUNTED_STORAGE'}));if(d&&d.success){await refresh();await loadHist()}}finally{b.disabled=false}}
async function refresh(){try{const d=await(await fetch('/stats')).json();$('ssRecv').textContent=d.received;$('ssSent').textContent=d.sent;$('ssBoot').textContent=d.boots;$('wifiS').textContent=d.wifiRssi+' dBm';$('storageWarning').hidden=d.storageReady!==false;$('upT').textContent='运行 '+Math.floor(d.uptime/3600)+' 小时 / 内存 '+(d.freeHeap/1024).toFixed(0)+'K'}catch(e){notice('设备状态读取失败')}return refreshAir(true)}
async function autoLoad(){await refresh();await query('network');await query('signal');await query('siminfo')}
function act(t){if(t==='sms')return post('/sendsms',new URLSearchParams({phone:$('sPh').value,content:$('sTx').value}));if(t==='reboot')return post('/restart',new URLSearchParams());if(t==='ping')return post('/ping',new URLSearchParams()).then(d=>{$('pLog').style.display='block';$('pLog').textContent=d.message})}
async function query(type){try{const d=await(await fetch('/query?type='+type)).json();if(type==='siminfo'){$('modSim').textContent=d.success?'SIM OK':'SIM ERR';$('modSim').className='badge '+(d.success?'b-ok':'b-err')}else{const e=$(type==='network'?'modNet':'modSig');e.innerHTML=d.message;if(type==='network')e.style.color=d.success?'#15803d':'#b91c1c'}}catch(e){notice('查询失败')}}
const tabIds=['overview','control','history-page','config'];
function swTab(n,focus=false,updateHash=true){
  if(n<0||n>=tabIds.length)n=0;
  document.querySelectorAll('[role="tab"]').forEach((e,i)=>{e.classList.toggle('active',i===n);e.setAttribute('aria-selected',String(i===n));e.tabIndex=i===n?0:-1;if(focus&&i===n)e.focus()});
  tabIds.forEach((id,i)=>{$(id).hidden=i!==n;$(id).classList.toggle('active',i===n)});
  if(updateHash&&location.hash!=='#'+tabIds[n])window.history.replaceState(null,'','#'+tabIds[n]);
  if(n===2)loadHist();
}
function hashTab(){const n=tabIds.indexOf(location.hash.slice(1));swTab(n<0?0:n,false,false)}
document.querySelector('[role="tablist"]').addEventListener('keydown',e=>{
  const tabs=[...document.querySelectorAll('[role="tab"]')],i=tabs.indexOf(e.target);if(i<0)return;
  if(e.key==='Enter'||e.key===' '){e.preventDefault();swTab(i,true);return}
  const keys={ArrowRight:(i+1)%4,ArrowLeft:(i+3)%4,Home:0,End:3};
  if(Object.hasOwn(keys,e.key)){e.preventDefault();swTab(keys[e.key],true)}
});
window.addEventListener('hashchange',hashTab);
let smsData=[],curContact=null,histLoading=false,histClearing=false,histRequest=0;
function emptyHist(){
  smsData=[];curContact=null;
  $('contactList').replaceChildren(node('p','chat-empty','暂无联系人'));
  $('chatMessages').replaceChildren(node('p','chat-empty','暂无短信记录'));
  $('chatHeader').style.display='none';$('chatName').textContent='选择联系人';$('chatCount').textContent='';
  const avatar=$('chatTitle').querySelector('.contact-avatar');if(avatar)avatar.remove();
  backToList(false);
}
async function clearHist(){
  if(histClearing||!confirm('确定清空本机全部短信历史吗？将删除所有联系人的记录，无法恢复。配置和统计保持不变。'))return;
  histClearing=true;++histRequest;histLoading=false;
  const button=$('clearHistBtn');button.disabled=true;button.textContent='清理中...';
  try{
    const r=await fetch('/history/clear',{method:'POST'});const d=await r.json();
    if(!r.ok||d.success!==true)throw new Error(d.message||'短信历史清理失败');
    emptyHist();notice(d.message||'已清空本机全部短信历史');
  }catch(e){notice(e.message||'短信历史清理失败，请重试')}
  finally{histClearing=false;button.disabled=false;button.textContent='清空全部'}
}
function node(tag,cls,text){const e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e}
async function loadHist(){
  if(histLoading||histClearing)return;histLoading=true;const request=++histRequest;
  try{const r=await fetch('/history');if(r.ok===false)throw new Error('读取失败');const d=await r.json();if(request!==histRequest)return;smsData=d.history||[];$('contactList').replaceChildren();
    const groups=new Map();for(const m of smsData){if(!groups.has(m.s))groups.set(m.s,[]);groups.get(m.s).push(m)}
    const contacts=[...groups].sort((a,b)=>b[1].map(m=>m.t).sort().pop().localeCompare(a[1].map(m=>m.t).sort().pop()));
    for(const [name,msgs] of contacts){const latest=[...msgs].sort((a,b)=>b.t.localeCompare(a.t))[0];const b=node('button','contact-item');b.type='button';b.dataset.name=name;b.onclick=()=>selContact(name,true);
      const avatar=node('span','contact-avatar',name.length>2?name.slice(0,1):name);avatar.style.background=getAvatarColor(name);b.append(avatar);const info=node('span','contact-info'),nr=node('span','contact-name-row'),pr=node('span','contact-preview-row');nr.append(node('span','contact-name',name),node('span','contact-time',latest.t.slice(5,16)));pr.append(node('span','contact-preview',latest.m.slice(0,20)+(latest.m.length>20?'...':'')));if(msgs.length>1)pr.append(node('span','contact-badge',String(msgs.length)));info.append(nr,pr);b.append(info);$('contactList').append(b)}
    if(contacts.length){selContact(groups.has(curContact)?curContact:contacts[0][0],false)}else{emptyHist()}
  }catch(e){if(request===histRequest)notice('历史读取失败')}finally{if(request===histRequest)histLoading=false}
}
function selContact(name,open=true){curContact=name;document.querySelectorAll('.contact-item').forEach(e=>e.classList.toggle('active',e.dataset.name===name));const msgs=smsData.filter(m=>m.s===name).sort((a,b)=>a.t.localeCompare(b.t));$('chatHeader').style.display='flex';$('chatName').textContent=name;let av=$('chatTitle').querySelector('.contact-avatar');if(!av){av=node('span','contact-avatar');av.style.cssText='width:28px;height:28px;font-size:0.8em';$('chatTitle').insertBefore(av,$('chatName'))}av.textContent=name.length>2?name.slice(0,1):name;av.style.background=getAvatarColor(name);$('chatCount').textContent=msgs.length+'条短信';$('chatMessages').replaceChildren();let lastDay='';for(const m of msgs){const day=m.t.slice(0,10);if(day!==lastDay){const d=node('div','chat-day');d.append(node('span','',day));$('chatMessages').append(d);lastDay=day}const b=node('div','msg-bubble msg-in');b.append(node('div','',m.m),node('span','msg-time',m.t.slice(11,16)));$('chatMessages').append(b)}if(open){document.querySelector('.chat-area').classList.add('show');if(innerWidth<=600)document.querySelector('.chat-back').focus()}requestAnimationFrame(()=>$('chatMessages').scrollTop=$('chatMessages').scrollHeight)}
function backToList(focus=true){document.querySelector('.chat-area').classList.remove('show');if(focus){const b=[...document.querySelectorAll('.contact-item')].find(e=>e.dataset.name===curContact);if(b)b.focus()}}
function upd(i){
  var t=$('tp'+i).value;
  $('cf'+i).style.display=(t=='4')?'block':'none';
  var showK1=(t=='5'||t=='7'||t=='8');
  $('k1'+i).style.display=showK1?'block':'none';
  var lbl=$('k1l'+i);
  if(lbl){lbl.innerText=(t=='5')?'Chat ID':'加签密钥 (可选)';}
  var key=$('k1'+i).querySelector('input');
  if(key){key.placeholder=(t=='5')?'如 123456789':(t=='8'?'飞书机器人签名校验密钥（可选）':'SEC开头的密钥');}
}
for(let i=0;i<3;i++)upd(i);

const colors=['#3b82f6','#10b981','#f59e0b','#ef4444','#8b5cf6','#ec4899','#6366f1','#14b8a6'];
function getAvatarColor(name){let hash=0;for(let i=0;i<name.length;i++)hash=name.charCodeAt(i)+((hash<<5)-hash);return colors[Math.abs(hash)%colors.length]}
function xToggle(id){const e=$(id);e.value=String(e.value!=='true');$(id+'Sw').classList.toggle('on',e.value==='true');if(id==='smEn')$('smBox').style.display=e.value==='true'?'block':'none';if(id==='tmEn')updTmInfo()}
function wfTog(i){const e=$('wfe'+i);e.value=String(e.value!=='true');$('wfs'+i).classList.toggle('on',e.value==='true')}
function chTog(i){const e=$('che'+i);e.value=String(e.value!=='true');$('chs'+i).classList.toggle('on',e.value==='true')}
function fd(i){const b=$('chb'+i),open=b.style.display==='none';b.style.display=open?'block':'none';$('chi'+i).style.transform=open?'rotate(90deg)':'rotate(0)'}
function updTmInfo(){const en=$('tmEn').value==='true';$('tmBadge').className='badge '+(en?'b-ok':'b-wait');$('tmBadge').textContent=en?($('tmType').value==='0'?'Ping保活':'发送短信'):'未启用';$('tmInfo').textContent=en?'每 '+$('tmInt').value+' 天执行一次':'定时任务已禁用'}
updTmInfo();hashTab();autoLoad();setInterval(refresh,15000);
</script></body></html>)PAGE";
#endif
