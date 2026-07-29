/* Embedded web configuration and status UI.
 * Licensed under the Apache License, Version 2.0.
 */

#include <WebServer.h>
#include <WiFi.h>
#include <esp_arduino_version.h>
#include <math.h>
#include <time.h>

#include "firmware_config.h"
#include "ltc_output.h"
#include "network_config.h"
#include "ntp_clock.h"
#include "partition_migration.h"
#include "secrets.h"
#include "web_config.h"

static WebServer server(80);

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SMPTE Clock</title>
<style>
:root{color-scheme:dark;--bg:#08111f;--panel:#111f33;--line:#263a55;--text:#eef6ff;--muted:#8ea4bd;--cyan:#31d7e8;--green:#5ee59b;--amber:#ffcc66;--red:#ff6b7a}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 15% 0,#17314d 0,transparent 34%),linear-gradient(145deg,#07101d,#0b1728 55%,#07111f);font:15px/1.5 Inter,system-ui,sans-serif;color:var(--text);min-height:100vh}
.wrap{width:min(1080px,calc(100% - 28px));margin:auto;padding:28px 0 48px}
header{display:flex;justify-content:space-between;align-items:center;gap:16px;margin-bottom:20px}.brand{display:flex;align-items:center;gap:12px}.mark{width:42px;height:42px;border-radius:12px;background:linear-gradient(135deg,var(--cyan),#5874ff);box-shadow:0 0 30px #31d7e844;display:grid;place-items:center;font-weight:900;color:#04111b}.brand h1{font-size:19px;margin:0}.brand p{margin:2px 0 0;color:var(--muted);font-size:12px}
.badge{display:inline-flex;align-items:center;gap:8px;border:1px solid var(--line);background:#0d1a2a;padding:8px 12px;border-radius:999px;color:var(--muted);font-size:12px}.dot{width:8px;height:8px;border-radius:50%;background:var(--amber);box-shadow:0 0 10px currentColor}.online .dot{background:var(--green)}.offline .dot{background:var(--red)}
.hero{position:relative;overflow:hidden;padding:30px;border:1px solid #2b4563;background:linear-gradient(145deg,#13263eeb,#0d1c30e8);border-radius:22px;box-shadow:0 24px 70px #0008}.hero:after{content:"";position:absolute;width:250px;height:250px;border-radius:50%;background:#31d7e819;filter:blur(10px);right:-100px;top:-120px}.label{text-transform:uppercase;letter-spacing:.16em;font-size:11px;color:var(--cyan);font-weight:700}.clock{display:flex;align-items:center;height:1em;font-size:clamp(44px,10vw,92px);line-height:1;margin:16px 0 12px;color:#ff3b50;filter:drop-shadow(0 0 14px #ff18384d)}.seg-digit{position:relative;display:inline-block;flex:0 0 .57em;width:.57em;height:1em;margin:0 .018em}.seg-digit.frame-digit{font-size:.46em;align-self:flex-end;margin-bottom:.035em}.seg{position:absolute;display:block;background:#ff26401a;opacity:.2}.seg.on{opacity:1;background:linear-gradient(90deg,#ff1f3d,#ff9aaa 45%,#ff1f3d);box-shadow:0 0 .11em #ff1838,0 0 .22em #ff18386b}.seg-h{left:12%;width:76%;height:9%;clip-path:polygon(7% 0,93% 0,100% 50%,93% 100%,7% 100%,0 50%)}.seg-v{width:14%;height:42%;clip-path:polygon(50% 0,100% 8%,100% 92%,50% 100%,0 92%,0 8%)}.seg-a{top:0}.seg-g{top:45.5%}.seg-d{bottom:0}.seg-f{left:0;top:5%}.seg-b{right:0;top:5%}.seg-e{left:0;bottom:5%}.seg-c{right:0;bottom:5%}.seg-colon{position:relative;display:inline-block;flex:0 0 .18em;width:.18em;height:1em;margin:0 .04em}.seg-colon:before,.seg-colon:after{content:"";position:absolute;left:22%;width:56%;aspect-ratio:1;border-radius:50%;background:#ff4057;box-shadow:0 0 .12em #ff1838}.seg-colon:before{top:28%}.seg-colon:after{bottom:28%}.seg-colon.frame-colon{font-size:.46em;align-self:flex-end;margin-bottom:.035em}.subtime{display:flex;gap:22px;flex-wrap:wrap;color:var(--muted);font-size:13px}.subtime strong{color:var(--text);font-weight:600}
.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:16px;margin-top:16px}.card{grid-column:span 4;border:1px solid var(--line);background:#0e1b2cdd;border-radius:17px;padding:19px;box-shadow:0 12px 35px #0003}.card.wide{grid-column:span 6}.card h2{font-size:14px;margin:0 0 16px}.metric{display:flex;justify-content:space-between;gap:12px;padding:8px 0;border-bottom:1px solid #21334a}.metric:last-child{border:0}.metric span{color:var(--muted)}.metric strong{text-align:right;font-weight:600}
form{display:grid;gap:12px}label{font-size:12px;color:var(--muted)}input,select{width:100%;margin-top:5px;padding:11px 12px;border:1px solid var(--line);border-radius:9px;background:#091522;color:var(--text);outline:none}input:focus,select:focus{border-color:var(--cyan);box-shadow:0 0 0 3px #31d7e818}.hint{margin:-4px 0 2px;color:var(--muted);font-size:11px}.hidden{display:none}.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}button{border:0;border-radius:9px;padding:11px 14px;background:linear-gradient(135deg,#22bfd1,#4e6fff);color:white;font-weight:700;cursor:pointer}button.secondary{background:#1b2b41;border:1px solid #30455f}button.danger{background:#4b2430;border:1px solid #7c3446}button:disabled{opacity:.55;cursor:wait}.actions{display:flex;gap:9px;flex-wrap:wrap}.notice{min-height:22px;margin-top:10px;font-size:12px;color:var(--green)}footer{text-align:center;color:#607792;font-size:11px;margin-top:24px}
@media(max-width:760px){.card,.card.wide{grid-column:span 12}.hero{padding:23px}.clock{letter-spacing:-.08em}.row{grid-template-columns:1fr}header{align-items:flex-start;flex-direction:column}}
</style>
</head>
<body><div class="wrap">
<header><div class="brand"><div class="mark">TC</div><div><h1 id="device">SMPTE Timecode Generator</h1><p>ESP32 disciplined LTC reference</p></div></div><div id="statusBadge" class="badge"><span class="dot"></span><span>Starting</span></div></header>
<section class="hero"><div class="label">Linear timecode output</div><div id="ltc" class="clock">--:--:--<span class="frame">:--</span></div><div class="subtime"><span>Local <strong id="local">—</strong></span><span>UTC <strong id="utc">—</strong></span><span><strong id="fps">—</strong> fps</span></div></section>
<div class="grid">
<section class="card"><h2>Signal</h2><div class="metric"><span>Output</span><strong id="output">—</strong></div><div class="metric"><span>Phase error</span><strong id="phase">—</strong></div><div class="metric"><span>Discipline</span><strong id="ppm">—</strong></div><div class="metric"><span>Buffer underruns</span><strong id="underruns">—</strong></div></section>
<section class="card"><h2>Network</h2><div class="metric"><span>Wi-Fi</span><strong id="ssid">—</strong></div><div class="metric"><span>Address</span><strong id="ip">—</strong></div><div class="metric"><span>Signal</span><strong id="rssi">—</strong></div><div class="metric"><span>Provisioning</span><strong id="provisioning">—</strong></div></section>
<section class="card"><h2>Device</h2><div class="metric"><span>Version</span><strong id="version">—</strong></div><div class="metric"><span>Core</span><strong id="core">—</strong></div><div class="metric"><span>Uptime</span><strong id="uptime">—</strong></div><div class="metric"><span>Free heap</span><strong id="heap">—</strong></div></section>
<section class="card wide"><h2>Clock configuration</h2><form id="clockForm"><label>Timezone<select id="timezonePreset" aria-describedby="timezoneHelp"><option value="UTC0">UTC — no daylight saving</option><option value="GMT0BST,M3.5.0/1,M10.5.0/2">United Kingdom / Ireland</option><option value="WET0WEST,M3.5.0/1,M10.5.0/2">Western Europe — Portugal</option><option value="CET-1CEST,M3.5.0/2,M10.5.0/3">Central Europe</option><option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe</option><option value="EST5EDT,M3.2.0/2,M11.1.0/2">US / Canada — Eastern</option><option value="CST6CDT,M3.2.0/2,M11.1.0/2">US / Canada — Central</option><option value="MST7MDT,M3.2.0/2,M11.1.0/2">US / Canada — Mountain</option><option value="PST8PDT,M3.2.0/2,M11.1.0/2">US / Canada — Pacific</option><option value="MST7">Arizona — no daylight saving</option><option value="AKST9AKDT,M3.2.0/2,M11.1.0/2">Alaska</option><option value="HST10">Hawaii — no daylight saving</option><option value="IST-5:30">India</option><option value="CST-8">China</option><option value="JST-9">Japan</option><option value="AEST-10AEDT,M10.1.0/2,M4.1.0/3">Australia — Sydney / Melbourne</option><option value="ACST-9:30ACDT,M10.1.0/2,M4.1.0/3">Australia — Adelaide</option><option value="AEST-10">Australia — Brisbane</option><option value="AWST-8">Australia — Perth</option><option value="NZST-12NZDT,M9.5.0/2,M4.1.0/3">New Zealand</option><option value="SAST-2">South Africa</option><option value="custom">Custom POSIX timezone…</option></select></label><label id="timezoneCustom" class="hidden">Custom POSIX timezone<input id="timezone" name="timezone" maxlength="128" autocomplete="off" spellcheck="false" required></label><p id="timezoneHelp" class="hint">Daylight-saving changes are automatic for regional presets.</p><label>Output adjustment in seconds<input id="fiddle" name="fiddle" type="number" min="-3600" max="3600" step="0.001" required></label><button>Save clock settings</button></form><div id="clockNotice" class="notice"></div></section>
<section class="card wide"><h2>Wi-Fi provisioning</h2><form id="wifiForm"><label>Network name<input id="wifiSsid" name="ssid" maxlength="32" required></label><label>Password<input name="password" type="password" maxlength="63" placeholder="Leave blank to keep the current password"></label><button>Save and reconnect</button></form><div id="wifiNotice" class="notice"></div></section>
<section class="card wide"><h2>Controls</h2><div class="actions"><button id="toggle" class="secondary">Toggle LTC output</button><button id="reboot" class="danger">Reboot device</button></div><div id="controlNotice" class="notice"></div></section>
</div><footer>SMPTE/EBU LTC • Self-contained control interface</footer>
</div>
<script>
const $=id=>document.getElementById(id), fmt=n=>String(n).padStart(2,"0");
const segmentMap={0:"abcdef",1:"bc",2:"abdeg",3:"abcdg",4:"bcfg",5:"acdfg",6:"acdefg",7:"abc",8:"abcdefg",9:"abcdfg"};
let first=true,last={},ltcAnchorFrames=0,ltcAnchorAt=0,ltcFps=25,ltcDigits=[];
async function api(path,options){const r=await fetch(path,options);if(!r.ok)throw new Error(await r.text());return r.json()}
function duration(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+"d ":"")+h+"h "+m+"m"}
function setTimezone(value){const select=$("timezonePreset"),known=Array.from(select.options).some(option=>option.value===value);select.value=known?value:"custom";$("timezone").value=value;$("timezoneCustom").classList.toggle("hidden",known)}
function timezoneChanged(){const value=$("timezonePreset").value,custom=value==="custom";$("timezoneCustom").classList.toggle("hidden",!custom);if(custom){$("timezone").value="";$("timezone").focus()}else{$("timezone").value=value}}
function initLtcDisplay(){const display=$("ltc");display.textContent="";display.setAttribute("role","timer");for(let i=0;i<8;i++){if(i===2||i===4||i===6){const colon=document.createElement("span");colon.className="seg-colon"+(i===6?" frame-colon":"");display.appendChild(colon)}const digit=document.createElement("span");digit.className="seg-digit"+(i>=6?" frame-digit":"");for(const name of "abcdefg"){const segment=document.createElement("i");segment.className="seg seg-"+name+" "+("adg".includes(name)?"seg-h":"seg-v");digit.appendChild(segment)}ltcDigits.push(digit);display.appendChild(digit)}}
function setLtcDisplay(value){$("ltc").setAttribute("aria-label",value);const values=value.replaceAll(":","");for(let i=0;i<ltcDigits.length;i++){const active=segmentMap[values[i]]||"";const segments=ltcDigits[i].children;for(let j=0;j<segments.length;j++)segments[j].classList.toggle("on",active.includes("abcdefg"[j]))}}
function anchorLtc(value,fps,requestAt,responseAt){const p=value.split(":").map(Number);ltcFps=fps;ltcAnchorFrames=(((p[0]*60+p[1])*60+p[2])*fps+p[3]);ltcAnchorAt=(requestAt+responseAt)/2;renderLtc()}
function renderLtc(){if(!ltcAnchorAt)return;const day=86400*ltcFps,total=Math.floor(ltcAnchorFrames+(performance.now()-ltcAnchorAt)*ltcFps/1000)%day,seconds=Math.floor(total/ltcFps),frame=total%ltcFps,h=Math.floor(seconds/3600),m=Math.floor(seconds/60)%60,s=seconds%60;setLtcDisplay(`${fmt(h)}:${fmt(m)}:${fmt(s)}:${fmt(frame)}`)}
async function refresh(){try{const requestAt=performance.now(),s=await api("/api/status"),responseAt=performance.now();last=s;anchorLtc(s.ltc,s.fps,requestAt,responseAt);$("device").textContent=s.device;$("local").textContent=s.local;$("utc").textContent=s.utc;$("fps").textContent=s.fps;$("output").textContent=s.output?"Running":"Stopped";$("phase").textContent=(s.phase_error_ms>=0?"+":"")+s.phase_error_ms.toFixed(1)+" ms";$("ppm").textContent=(s.correction_ppm>=0?"+":"")+s.correction_ppm.toFixed(1)+" ppm";$("underruns").textContent=s.underruns;$("ssid").textContent=s.ssid||"Not connected";$("ip").textContent=s.ip;$("rssi").textContent=s.connected?s.rssi+" dBm":"—";$("provisioning").textContent=s.provisioning?"Active":"Inactive";$("version").textContent=s.version;$("core").textContent=s.core;$("uptime").textContent=duration(s.uptime);$("heap").textContent=Math.round(s.free_heap/1024)+" KiB";const b=$("statusBadge");b.className="badge "+(s.connected&&s.synced?"online":"offline");b.lastElementChild.textContent=s.connected?(s.synced?"NTP "+s.ntp_status:"Awaiting NTP"):"Offline";if(first){setTimezone(s.timezone);$("fiddle").value=s.fiddle;$("wifiSsid").value=s.configured_ssid;first=false}}catch(e){const b=$("statusBadge");b.className="badge offline";b.lastElementChild.textContent="Unavailable"}}
async function submit(form,path,notice){const button=form.querySelector("button");button.disabled=true;try{const body=new URLSearchParams(new FormData(form));const r=await api(path,{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});$(notice).textContent=r.message;await refresh()}catch(e){$(notice).textContent=e.message}finally{button.disabled=false}}
$("clockForm").addEventListener("submit",e=>{e.preventDefault();submit(e.target,"/api/clock","clockNotice")});
$("timezonePreset").addEventListener("change",timezoneChanged);
$("wifiForm").addEventListener("submit",e=>{e.preventDefault();submit(e.target,"/api/wifi","wifiNotice")});
$("toggle").onclick=async()=>{try{const r=await api("/api/output",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"enabled="+(!last.output)});$("controlNotice").textContent=r.message;refresh()}catch(e){$("controlNotice").textContent=e.message}};
$("reboot").onclick=async()=>{if(confirm("Reboot the timecode generator?")){await api("/api/reboot",{method:"POST"});$("controlNotice").textContent="Rebooting…"}};
initLtcDisplay();refresh();setInterval(refresh,1000);setInterval(renderLtc,40);
</script></body></html>
)HTML";

static bool authenticated() {
  if (strlen(WEB_PASSWORD) == 0) {
    return true;
  }
  if (server.authenticate(WEB_USERNAME, WEB_PASSWORD)) {
    return true;
  }
  server.requestAuthentication(BASIC_AUTH, "SMPTE timecode generator");
  return false;
}

static String jsonEscape(const String &value) {
  String result;
  result.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '"' || c == '\\') {
      result += '\\';
      result += c;
    } else if (c == '\n') {
      result += F("\\n");
    } else if (static_cast<uint8_t>(c) >= 0x20) {
      result += c;
    }
  }
  return result;
}

static String formatTime(const tm &value) {
  char buffer[24];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &value);
  return String(buffer);
}

static String formatLtc() {
  const double secondsOfDay = ntpCurrentOutputSeconds();
  const uint32_t totalFrames =
      static_cast<uint32_t>(floor(secondsOfDay * static_cast<double>(FPS))) %
      (24U * 60U * 60U * FPS);
  const uint32_t wholeSeconds = totalFrames / FPS;
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu:%02lu",
           static_cast<unsigned long>(wholeSeconds / 3600U),
           static_cast<unsigned long>((wholeSeconds / 60U) % 60U),
           static_cast<unsigned long>(wholeSeconds % 60U),
           static_cast<unsigned long>(totalFrames % FPS));
  return String(buffer);
}

static void sendJson(int status, const String &json) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Referrer-Policy", "no-referrer");
  server.send(status, "application/json", json);
}

static void handleStatus() {
  if (!authenticated()) return;

  const time_t now = time(nullptr);
  tm local{};
  tm utc{};
  localtime_r(&now, &local);
  gmtime_r(&now, &utc);
  const bool connected = WiFi.status() == WL_CONNECTED;

  String json;
  json.reserve(768);
  json += F("{\"device\":\"");
  json += jsonEscape(name);
  json += F("\",\"version\":\"");
  json += FIRMWARE_VERSION;
  json += F("\",\"core\":\"");
  json += ESP_ARDUINO_VERSION_STR;
  json += F("\",\"ltc\":\"");
  json += formatLtc();
  json += F("\",\"local\":\"");
  json += formatTime(local);
  json += F("\",\"utc\":\"");
  json += formatTime(utc);
  json += F("\",\"fps\":");
  json += FPS;
  json += F(",\"output\":");
  json += rmtOutputEnabled() ? F("true") : F("false");
  json += F(",\"synced\":");
  json += ntpHasSynchronized() ? F("true") : F("false");
  json += F(",\"connected\":");
  json += connected ? F("true") : F("false");
  json += F(",\"provisioning\":");
  json += wifiProvisioningActive() ? F("true") : F("false");
  json += F(",\"ssid\":\"");
  json += connected ? jsonEscape(WiFi.SSID()) : String();
  json += F("\",\"configured_ssid\":\"");
  json += jsonEscape(configuredWifiSsid());
  json += F("\",\"ip\":\"");
  json += connected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  json += F("\",\"rssi\":");
  json += connected ? WiFi.RSSI() : 0;
  json += F(",\"timezone\":\"");
  json += jsonEscape(tz);
  json += F("\",\"fiddle\":");
  json += String(fiddleSeconds, 3);
  json += F(",\"correction_ppm\":");
  json += String(rmtFrequencyCorrectionPpm(), 1);
  json += F(",\"phase_error_ms\":");
  json += String(ntpPhaseErrorSeconds() * 1000.0, 1);
  json += F(",\"ntp_status\":\"");
  json += ntpSyncStatusText();
  json += F("\",\"ntp_updates\":");
  json += ntpNetworkSyncCount();
  json += F(",\"underruns\":");
  json += rmtUnderrunCount();
  json += F(",\"last_sync\":\"");
  const time_t lastSync = ntpLastLtcSync();
  if (lastSync > 0) {
    tm syncTime{};
    localtime_r(&lastSync, &syncTime);
    json += formatTime(syncTime);
  } else {
    json += F("Never");
  }
  json += F("\",\"uptime\":");
  json += millis() / 1000UL;
  json += F(",\"free_heap\":");
  json += ESP.getFreeHeap();
  json += F(",\"partition_layout\":\"");
  json += partitionLayoutName();
  json += F("\",\"running_partition\":\"");
  json += partitionRunningLabel();
  json += F("\",\"app_partition_size\":");
  json += partitionRunningSize();
  json += F(",\"partition_migration_available\":");
  json += partitionMigrationAvailable() ? F("true") : F("false");
  json += '}';
  sendJson(200, json);
}

static void handleClockConfiguration() {
  if (!authenticated()) return;
  if (!server.hasArg("timezone") || !server.hasArg("fiddle")) {
    sendJson(400, F("{\"error\":\"Missing timezone or adjustment\"}"));
    return;
  }

  const String adjustmentText = server.arg("fiddle");
  char *end = nullptr;
  const float adjustment = strtof(adjustmentText.c_str(), &end);
  if (end == adjustmentText.c_str() || *end != '\0' ||
      !isfinite(adjustment) ||
      setAndWriteNtp(adjustment, server.arg("timezone")) != 0) {
    sendJson(400, F("{\"error\":\"Invalid clock configuration\"}"));
    return;
  }
  sendJson(200, F("{\"message\":\"Clock settings saved\"}"));
}

static void handleWifiConfiguration() {
  if (!authenticated()) return;
  if (!server.hasArg("ssid")) {
    sendJson(400, F("{\"error\":\"Network name is required\"}"));
    return;
  }
  const String ssid = server.arg("ssid");
  String password = server.arg("password");
  if (password.isEmpty() && ssid == configuredWifiSsid()) {
    password = configuredWifiPassword();
  }
  if (!saveWifiConfiguration(ssid, password)) {
    sendJson(400, F("{\"error\":\"Unable to save Wi-Fi settings\"}"));
    return;
  }
  sendJson(200, F("{\"message\":\"Wi-Fi saved; reconnecting\"}"));
}

static void handleOutput() {
  if (!authenticated()) return;
  if (!server.hasArg("enabled")) {
    sendJson(400, F("{\"error\":\"Missing output state\"}"));
    return;
  }
  const bool enabled = server.arg("enabled") == "true" ||
                       server.arg("enabled") == "1";
  rmtSetOutputEnabled(enabled);
  sendJson(200, enabled ? F("{\"message\":\"LTC output enabled\"}")
                        : F("{\"message\":\"LTC output disabled\"}"));
}

#ifdef ENABLE_PARTITION_MIGRATION
static void handlePartitionMigration() {
  if (strlen(WEB_PASSWORD) == 0) {
    sendJson(503, F("{\"error\":\"A web password is required for migration\"}"));
    return;
  }
  if (!authenticated()) return;
  if (!server.hasArg("confirm") ||
      server.arg("confirm") != "large-dual-ota-v1") {
    sendJson(400, F("{\"error\":\"Migration confirmation is required\"}"));
    return;
  }
  const char *preflight = partitionMigrationPreflight();
  if (preflight != nullptr) {
    String json = F("{\"error\":\"");
    json += jsonEscape(preflight);
    json += F("\"}");
    sendJson(409, json);
    return;
  }
  if (!schedulePartitionMigration()) {
    sendJson(409, F("{\"error\":\"Migration is already scheduled\"}"));
    return;
  }
  sendJson(202, F("{\"message\":\"Partition migration scheduled\"}"));
}
#endif

void web_setup() {
  if (strlen(WEB_PASSWORD) == 0) {
    Serial.println("Warning: web authentication is disabled");
  }
  server.on("/", HTTP_GET, []() {
    if (!authenticated()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("Referrer-Policy", "no-referrer");
    server.sendHeader(
        "Content-Security-Policy",
        "default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'");
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/clock", HTTP_POST, handleClockConfiguration);
  server.on("/api/wifi", HTTP_POST, handleWifiConfiguration);
  server.on("/api/output", HTTP_POST, handleOutput);
#ifdef ENABLE_PARTITION_MIGRATION
  server.on("/api/partition-migrate", HTTP_POST, handlePartitionMigration);
#endif
  server.on("/api/reboot", HTTP_POST, []() {
    if (!authenticated()) return;
    sendJson(200, F("{\"message\":\"Rebooting\"}"));
    delay(150);
    ESP.restart();
  });
  server.onNotFound([]() {
    if (!authenticated()) return;
    sendJson(404, F("{\"error\":\"Not found\"}"));
  });
  server.begin();
}

void web_loop() {
  server.handleClient();
}
