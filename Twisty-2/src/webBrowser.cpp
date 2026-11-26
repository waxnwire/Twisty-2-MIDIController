#include "webBrowser.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "encodercontrol.h"
#include "menudata.h"
#include "menu/controller.h"
#include "wifisettings.h"

extern int16_t saveconfig(int16_t slot);
extern int16_t loadconfig(int16_t slot);
extern void notifyWebControlUpdated(int16_t targetPage, int16_t index, bool encoder,
                                    bool sendOutput);
extern void prepareWebConfigurationLoad(void);
extern void notifyWebConfigurationLoaded(void);
extern float getWebEncoderCurrentValue(int16_t targetPage, int16_t index);
extern uint16_t page;

// ========================================
// TWISTY WIFI FILE MANAGER
// ========================================


static File uploadFile;

static const char* UPLOAD_TEMP = "/upload.tmp";
static const char* UPLOAD_BACKUP = "/upload.bak";

static String uploadTarget;
static String uploadResult = "No upload received";
static bool uploadError = false;
static size_t uploadBytes = 0;
static int16_t uploadSlot = 0;

WebServer fileServer(80);

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Twisty 2 Controller</title>
<style>
:root{color-scheme:dark;--bg:#151a21;--panel:#222b36;--line:#354351;--accent:#70d6ff;--good:#50d890;--warn:#ffd166}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:#eef4fa;font:14px system-ui,Arial,sans-serif}
header{position:sticky;top:0;z-index:2;background:#151a21ee;backdrop-filter:blur(8px);padding:14px 20px;border-bottom:1px solid var(--line)}
h1{font-size:20px;margin:0 0 10px;color:var(--accent)}.bar,.tabs,.actions{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.tabs{margin-top:12px}.tab,button,.button{border:1px solid var(--line);border-radius:8px;background:var(--panel);color:#eef4fa;padding:9px 13px;font:inherit;cursor:pointer;text-decoration:none}
.tab.active,button.primary{background:#12445a;border-color:var(--accent)}button:hover,.button:hover{border-color:var(--accent)}
main{padding:16px;max-width:1500px;margin:auto}.status{margin-left:auto;color:var(--good)}.actions{margin:10px 0}
input,select{width:100%;min-width:0;background:#11171e;color:#eef4fa;border:1px solid #465565;border-radius:6px;padding:7px;font:inherit}
input:focus,select:focus{outline:2px solid #70d6ff88;border-color:var(--accent)}
.settings{display:flex;gap:8px;align-items:center;background:var(--panel);padding:9px 12px;border:1px solid var(--line);border-radius:8px}.settings input{width:88px}
.grid{display:grid;grid-template-columns:repeat(4,minmax(230px,1fr));gap:10px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px;min-width:0}
.card h2{font-size:15px;margin:0 0 7px}.live{color:var(--accent);font-variant-numeric:tabular-nums}
details{border-top:1px solid var(--line);padding-top:7px;margin-top:7px}summary{cursor:pointer;font-weight:600;padding:3px}
.fields{display:grid;grid-template-columns:1fr 1fr;gap:7px;margin-top:8px}.field{min-width:0}.field label{display:block;color:#aab8c5;font-size:11px;margin:0 0 3px}
.wide{grid-column:1/-1}.hint{color:#aab8c5;font-size:12px}.slot{width:90px}
@media(max-width:1050px){.grid{grid-template-columns:repeat(3,minmax(210px,1fr))}}
@media(max-width:720px){header{padding:12px}.grid{grid-template-columns:repeat(2,minmax(0,1fr))}main{padding:10px}.fields{gap:6px}}
@media(max-width:440px){.grid{grid-template-columns:1fr}.settings{width:100%}}
</style>
</head>
<body>
<header>
  <h1>TWISTY 2 · Controller</h1>
  <div class="bar">
    <div class="settings"><label for="walkSpeed">Random Walk speed (ms)</label><input id="walkSpeed" type="number" min="10" max="5000" step="10"></div>
    <span id="status" class="status">Connecting…</span>
  </div>
  <nav id="tabs" class="tabs"></nav>
</header>
<main>
  <section class="actions">
    <label>Slot <select id="slot" class="slot"></select></label>
    <button id="save" class="primary">Save to device</button>
    <button id="load">Load from device</button>
    <a id="download" class="button" href="#">Download JSON</a>
    <label class="button" for="uploadFile">Load JSON file</label>
    <input id="uploadFile" type="file" accept=".json,application/json" hidden>
  </section>
  <p class="hint">Changes are applied immediately. Device encoder and switch values refresh here automatically; save a slot or download JSON to keep a preset.</p>
  <section id="controls" class="grid"></section>
</main>
<script>
const $=s=>document.querySelector(s), statusEl=$('#status');
let currentPage=1, renderedPage=0, refreshBusy=false;
let labelCount=1;
let labelOptions=[];
for(let i=1;i<=16;i++)$('#slot').add(new Option('Slot '+i,i));
const tabs=$('#tabs');
for(let i=1;i<=4;i++){const b=document.createElement('button');b.className='tab';b.textContent='Page '+i;b.dataset.page=i;b.onclick=()=>selectPage(i);tabs.appendChild(b)}
function esc(v){return String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function field(kind,index,name,label,value,type='number',opts={}){
 const attrs=type==='number'?`type="number" min="${opts.min??0}" max="${opts.max??127}" step="${opts.step??1}"`:'';
 let control;
 if(type==='select'){control=`<select data-kind="${kind}" data-index="${index}" data-field="${name}">`+opts.options.map(([v,t])=>`<option value="${v}" ${Number(value)===v?'selected':''}>${esc(t)}</option>`).join('')+'</select>'}
 else control=`<input ${attrs} value="${esc(value)}" data-kind="${kind}" data-index="${index}" data-field="${name}">`;
 return `<div class="field ${opts.wide?'wide':''}"><label>${label}</label>${control}</div>`;
}
const outputOptions=[[0,'MIDI'],[1,'OSC']], oscTypes=[[0,'Float'],[1,'Integer'],[2,'String']];
const colorOptions=[[0,'Red'],[1,'Orange'],[2,'Green'],[3,'Aqua'],[4,'Blue'],[5,'Violet'],[6,'White'],[7,'Off']];
function encoderFields(e,i){
 let s='<div class="fields">';
 s+=field('encoder',i,'output','Output',e.output,'select',{options:outputOptions});
 s+=field('encoder',i,'type','MIDI type',e.type,'select',{options:[[0,'CC'],[1,'Delta']]});
 s+=field('encoder',i,'mode','Mode',e.mode,'select',{options:[[0,'Absolute'],[1,'Delta']]});
 s+=field('encoder',i,'channel','MIDI channel',e.channel,'number',{min:1,max:16});
 s+=field('encoder',i,'cc','CC number',e.cc,'number');
 s+=field('encoder',i,'min','MIDI minimum',e.min,'number');
 s+=field('encoder',i,'max','MIDI maximum',e.max,'number');
 s+=field('encoder',i,'value','MIDI value',e.value,'number');
 s+=field('encoder',i,'label','Label',e.label,'select',{options:labelOptions});
 s+=field('encoder',i,'color','LED color',e.color,'select',{options:colorOptions});
 s+=field('encoder',i,'oscRange','OSC range',e.oscRange,'select',{options:[[0,'0 to 1'],[1,'-1 to 1'],[2,'Delta']]});
 s+=field('encoder',i,'oscValueType','OSC value type',e.oscValueType,'select',{options:oscTypes});
 s+=field('encoder',i,'oscValue','OSC centre',e.oscValueMilli/1000,'number',{min:e.oscRange===0?0:-1,max:1,step:0.001});
 s+=field('encoder',i,'randomWalkWidth','Random Walk width',e.randomWalkWidth,'number',{min:0,max:e.output===1?(e.oscRange===0?1:2):127,step:e.output===1?0.01:1});
 s+=field('encoder',i,'oscAddress','OSC path',e.oscAddress,'text',{wide:true});
 return s+'</div>';
}
function switchFields(s,i){
 let h='<div class="fields">';
 h+=field('switch',i,'function','Action',s.function,'select',{options:[[0,'Jump'],[1,'Ramp'],[2,'Wobble']]});
 h+=field('switch',i,'mode','Mode',s.mode,'select',{options:[[0,'Momentary'],[1,'Toggle']]});
 h+=field('switch',i,'type','MIDI type',s.type,'select',{options:[[0,'CC'],[1,'Program Change'],[2,'Note']]});
 h+=field('switch',i,'channel','MIDI channel',s.channel,'number',{min:1,max:16});
 h+=field('switch',i,'cc','CC number',s.cc,'number');
 h+=field('switch',i,'min','MIDI minimum',s.min,'number');
 h+=field('switch',i,'max','MIDI maximum',s.max,'number');
 h+=field('switch',i,'value','Value',s.value,'number');
 h+=field('switch',i,'label','Label',s.label,'select',{options:labelOptions});
 h+=field('switch',i,'color','LED color',s.color,'select',{options:colorOptions});
 h+=field('switch',i,'oscValueType','OSC value type',s.oscValueType,'select',{options:oscTypes});
 h+=field('switch',i,'oscAddress','OSC path',s.oscAddress,'text',{wide:true});
 return h+'</div>';
}
function render(state){
 labelCount=state.labelCount||1;
 labelOptions=(state.labels||[]).map((text,index)=>[index,text]);
 if(renderedPage!==currentPage){
   $('#controls').innerHTML=state.encoders.map((e,i)=>`<article class="card"><h2>Encoder ${String(i+1).padStart(2,'0')} · <span class="live" id="ev${i}"></span></h2><details><summary>Encoder settings</summary>${encoderFields(e,i)}</details><details><summary>Switch · <span class="live" id="sv${i}"></span></summary>${switchFields(state.switches[i],i)}</details></article>`).join('');
   renderedPage=currentPage;
 }
 state.encoders.forEach((e,i)=>{
   const encValue=e.output===1?Number(e.current).toFixed(3):e.current;
   $('#ev'+i).textContent=encValue;
   $('#sv'+i).textContent=e.switchValue??state.switches[i].value;
   const s=state.switches[i];
   for(const [kind,obj] of [['encoder',e],['switch',s]]){
     for(const [name,value] of Object.entries(obj)){
       const input=document.querySelector(`[data-kind="${kind}"][data-index="${i}"][data-field="${name}"]`);
       if(input&&document.activeElement!==input&&name!=='index')input.value=value;
     }
   }
   const osc=document.querySelector(`[data-kind="encoder"][data-index="${i}"][data-field="oscValue"]`);
   if(osc&&document.activeElement!==osc)osc.value=(e.oscValueMilli/1000).toFixed(3);
 });
 const speed=$('#walkSpeed');if(document.activeElement!==speed)speed.value=state.randomWalkSpeedMs;
 document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('active',Number(t.dataset.page)===currentPage));
}
async function refresh(){
 if(refreshBusy)return;refreshBusy=true;
 try{const r=await fetch('/api/state?page='+currentPage,{cache:'no-store'});if(!r.ok)throw Error(await r.text());const state=await r.json();render(state);statusEl.textContent='Connected · device page '+state.activePage;statusEl.style.color='var(--good)'}
 catch(e){statusEl.textContent='Disconnected';statusEl.style.color='var(--warn)'}finally{refreshBusy=false}
}
async function post(url,data){
 const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
 const result=await r.json();if(!r.ok||!result.ok)throw Error(result.error||'Request failed');return result;
}
async function selectPage(p){currentPage=p;renderedPage=0;await refresh()}
document.addEventListener('change',async ev=>{
 const el=ev.target;
 if(el=== $('#walkSpeed')){try{await post('/api/control',{kind:'settings',field:'randomWalkSpeedMs',value:Number(el.value)});statusEl.textContent='Speed applied'}catch(e){statusEl.textContent=e.message}return}
 if(!el.dataset.field)return;
 let value=el.value;
 if(el.type==='number'||el.tagName==='SELECT')value=Number(value);
 try{await post('/api/control',{kind:el.dataset.kind,page:currentPage,index:Number(el.dataset.index)+1,field:el.dataset.field,value});statusEl.textContent='Applied live';await refresh()}
 catch(e){statusEl.textContent=e.message;await refresh()}
});
$('#save').onclick=async()=>{try{await post('/api/slot',{action:'save',slot:Number($('#slot').value)});statusEl.textContent='Saved to device'}catch(e){statusEl.textContent=e.message}};
$('#load').onclick=async()=>{if(!confirm('Load this slot and replace the live controller settings?'))return;try{await post('/api/slot',{action:'load',slot:Number($('#slot').value)});renderedPage=0;await refresh();statusEl.textContent='Loaded from device'}catch(e){statusEl.textContent=e.message}};
$('#download').onclick=e=>{e.preventDefault();location.href='/download?name=slot'+$('#slot').value+'.json'};
$('#slot').onchange=()=>$('#download').href='/download?name=slot'+$('#slot').value+'.json';
$('#uploadFile').onchange=async e=>{const f=e.target.files[0];if(!f)return;const data=new FormData();data.append('file',f,'slot'+$('#slot').value+'.json');try{const r=await fetch('/upload',{method:'POST',body:data});const text=await r.text();if(!r.ok)throw Error(text);renderedPage=0;await refresh();statusEl.textContent=text}catch(err){statusEl.textContent=err.message}e.target.value=''};
$('#slot').dispatchEvent(new Event('change'));
refresh();setInterval(refresh,1000);
</script>
</body>
</html>
)rawliteral";

void handleClinet() {
    fileServer.handleClient();
}
void startWifiFileManager() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi file manager: no connection");
        return;
    }

    static bool serverStarted = false;
    if (serverStarted) {
        return;
    }

    registerFileManagerRoutes();
    fileServer.begin();
    serverStarted = true;
    Serial.print("WiFi file manager IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("HTTP server started");
}
bool isSlotJsonFile(const String& name) {
    if (!name.startsWith("slot") || !name.endsWith(".json")) {
        return false;
    }

    String number = name.substring(4, name.length() - 5);
    if (number.length() == 0) return false;
    for (unsigned int i = 0; i < number.length(); i++) {
        if (!isDigit(number[i])) return false;
    }
    int slot = number.toInt();
    return slot >= 1 && slot <= 16 && String(slot) == number;
}

void sendApiResult(int status, bool ok, const char *message) {
    JsonDocument response;
    response["ok"] = ok;
    if (message != nullptr) {
        response[ok ? "message" : "error"] = message;
    }
    String body;
    serializeJson(response, body);
    fileServer.send(status, "application/json", body);
}

bool readBoundedInt(JsonVariantConst input, int minimum, int maximum,
                    int16_t &destination) {
    if (!input.is<int>()) {
        return false;
    }
    int value = input.as<int>();
    if (value < minimum || value > maximum) {
        return false;
    }
    destination = static_cast<int16_t>(value);
    return true;
}

void handleApiState() {
    int pageNumber = fileServer.arg("page").toInt();
    if (pageNumber < 1 || pageNumber > CONTROLLER_PAGES) {
        sendApiResult(400, false, "Page must be from 1 to 4");
        return;
    }

    int16_t pageIndex = static_cast<int16_t>(pageNumber - 1);
    JsonDocument state;
    state["page"] = pageNumber;
    state["activePage"] = page + 1;
    state["randomWalkSpeedMs"] = wifiSettings.randomWalkSpeedMs;
    JsonArray encoders = state["encoders"].to<JsonArray>();
    JsonArray switches = state["switches"].to<JsonArray>();

    for (int16_t i = 0; i < NUMENCODERS; ++i) {
        const controllerencoder &encoder = controls[pageIndex].encoder[i];
        JsonObject item = encoders.add<JsonObject>();
        item["index"] = i + 1;
        item["type"] = encoder.type;
        item["mode"] = encoder.mode;
        item["channel"] = encoder.channel;
        item["cc"] = encoder.ccnumber;
        item["min"] = encoder.minvalue;
        item["max"] = encoder.maxvalue;
        item["value"] = encoder.value;
        item["current"] = getWebEncoderCurrentValue(pageIndex, i);
        item["color"] = encoder.colorindex;
        item["label"] = encoder.labelindex;
        item["output"] = encoder.output;
        item["oscValueType"] = encoder.oscValueType;
        item["oscRange"] = encoder.oscRange;
        item["oscValueMilli"] = encoder.oscValueMilli;
        item["randomWalkWidth"] = encoder.randomWalkWidth;
        item["oscAddress"] = encoder.oscAddress;

        const controllerswitch &controlSwitch = controls[pageIndex].encswitch[i];
        JsonObject switchItem = switches.add<JsonObject>();
        switchItem["index"] = i + 1;
        switchItem["mode"] = controlSwitch.mode;
        switchItem["type"] = controlSwitch.type;
        switchItem["channel"] = controlSwitch.channel;
        switchItem["cc"] = controlSwitch.ccnumber;
        switchItem["min"] = controlSwitch.minvalue;
        switchItem["max"] = controlSwitch.maxvalue;
        switchItem["value"] = controlSwitch.value;
        switchItem["color"] = controlSwitch.colorindex;
        switchItem["label"] = controlSwitch.labelindex;
        switchItem["output"] = controls[pageIndex].encoder[i].output;
        switchItem["function"] = controlSwitch.function;
        switchItem["oscValueType"] = controlSwitch.oscValueType;
        switchItem["oscAddress"] = controlSwitch.oscAddress;
    }

    state["labelCount"] = LABEL_COUNT;
    JsonArray labelNames = state["labels"].to<JsonArray>();
    for (uint16_t i = 0; i < LABEL_COUNT; ++i) {
        labelNames.add(labels[i]);
    }
    String body;
    serializeJson(state, body);
    fileServer.send(200, "application/json", body);
}

bool applyEncoderField(controllerencoder &encoder, const char *field,
                       JsonVariantConst input) {
    if (strcmp(field, "type") == 0) {
        return readBoundedInt(input, CCTYPE, DELTATYPE, encoder.type);
    }
    if (strcmp(field, "mode") == 0) {
        return readBoundedInt(input, 0, 1, encoder.mode);
    }
    if (strcmp(field, "channel") == 0) {
        return readBoundedInt(input, 1, 16, encoder.channel);
    }
    if (strcmp(field, "cc") == 0) {
        return readBoundedInt(input, 0, 127, encoder.ccnumber);
    }
    if (strcmp(field, "min") == 0) {
        return readBoundedInt(input, 0, 127, encoder.minvalue);
    }
    if (strcmp(field, "max") == 0) {
        return readBoundedInt(input, 0, 127, encoder.maxvalue);
    }
    if (strcmp(field, "value") == 0) {
        return readBoundedInt(input, 0, 127, encoder.value);
    }
    if (strcmp(field, "color") == 0) {
        return readBoundedInt(input, 0, 7, encoder.colorindex);
    }
    if (strcmp(field, "label") == 0) {
        return readBoundedInt(input, 0, LABEL_COUNT - 1, encoder.labelindex);
    }
    if (strcmp(field, "output") == 0) {
        if (!readBoundedInt(input, MIDI_OUTPUT, OSC_OUTPUT, encoder.output)) {
            return false;
        }
        float maxWidth = encoder.output == MIDI_OUTPUT ? 127.0f
            : (encoder.oscRange == OSC_RANGE_0_TO_1 ? 1.0f : 2.0f);
        if (encoder.randomWalkWidth > maxWidth) {
            encoder.randomWalkWidth = maxWidth;
        }
        return true;
    }
    if (strcmp(field, "oscValueType") == 0) {
        return readBoundedInt(input, OSC_FLOAT, OSC_STRING, encoder.oscValueType);
    }
    if (strcmp(field, "oscRange") == 0) {
        if (!readBoundedInt(input, OSC_RANGE_0_TO_1, OSC_RANGE_DELTA,
                            encoder.oscRange)) {
            return false;
        }
        if (encoder.oscRange == OSC_RANGE_0_TO_1) {
            encoder.oscValueMilli = constrain(encoder.oscValueMilli, 0, 1000);
            if (encoder.randomWalkWidth > 1.0f) {
                encoder.randomWalkWidth = 1.0f;
            }
        } else {
            encoder.oscValueMilli = constrain(encoder.oscValueMilli, -1000, 1000);
            if (encoder.randomWalkWidth > 2.0f) {
                encoder.randomWalkWidth = 2.0f;
            }
        }
        return true;
    }
    if (strcmp(field, "oscValue") == 0) {
        if (!input.is<float>()) {
            return false;
        }
        float value = input.as<float>();
        float minimum = encoder.oscRange == OSC_RANGE_0_TO_1 ? 0.0f : -1.0f;
        if (!isfinite(value) || value < minimum || value > 1.0f) {
            return false;
        }
        encoder.oscValueMilli = static_cast<int16_t>(value * 1000.0f);
        return true;
    }
    if (strcmp(field, "randomWalkWidth") == 0) {
        if (!input.is<float>()) {
            return false;
        }
        float value = input.as<float>();
        float maximum = encoder.output == MIDI_OUTPUT ? 127.0f
            : (encoder.oscRange == OSC_RANGE_0_TO_1 ? 1.0f : 2.0f);
        if (!isfinite(value) || value < 0.0f || value > maximum) {
            return false;
        }
        encoder.randomWalkWidth = value;
        return true;
    }
    if (strcmp(field, "oscAddress") == 0) {
        if (!input.is<const char *>()) {
            return false;
        }
        const char *value = input.as<const char *>();
        if (strlen(value) >= sizeof(encoder.oscAddress)) {
            return false;
        }
        snprintf(encoder.oscAddress, sizeof(encoder.oscAddress), "%s", value);
        return true;
    }
    return false;
}

bool applySwitchField(controllerswitch &controlSwitch, const char *field,
                      JsonVariantConst input) {
    if (strcmp(field, "mode") == 0) {
        return readBoundedInt(input, MOMENTARY, TOGGLE, controlSwitch.mode);
    }
    if (strcmp(field, "function") == 0) {
        return readBoundedInt(input, SWITCH_JUMP, SWITCH_WOBBLE,
                              controlSwitch.function);
    }
    if (strcmp(field, "type") == 0) {
        return readBoundedInt(input, CCMESSAGE, NOTEMESSAGE, controlSwitch.type);
    }
    if (strcmp(field, "channel") == 0) {
        return readBoundedInt(input, 1, 16, controlSwitch.channel);
    }
    if (strcmp(field, "cc") == 0) {
        return readBoundedInt(input, 0, 127, controlSwitch.ccnumber);
    }
    if (strcmp(field, "min") == 0) {
        return readBoundedInt(input, 0, 127, controlSwitch.minvalue);
    }
    if (strcmp(field, "max") == 0) {
        return readBoundedInt(input, 0, 127, controlSwitch.maxvalue);
    }
    if (strcmp(field, "value") == 0) {
        return readBoundedInt(input, 0, 127, controlSwitch.value);
    }
    if (strcmp(field, "color") == 0) {
        return readBoundedInt(input, 0, 7, controlSwitch.colorindex);
    }
    if (strcmp(field, "label") == 0) {
        return readBoundedInt(input, 0, LABEL_COUNT - 1, controlSwitch.labelindex);
    }
    if (strcmp(field, "oscValueType") == 0) {
        return readBoundedInt(input, OSC_FLOAT, OSC_STRING,
                              controlSwitch.oscValueType);
    }
    if (strcmp(field, "oscAddress") == 0) {
        if (!input.is<const char *>()) {
            return false;
        }
        const char *value = input.as<const char *>();
        if (strlen(value) >= sizeof(controlSwitch.oscAddress)) {
            return false;
        }
        snprintf(controlSwitch.oscAddress, sizeof(controlSwitch.oscAddress), "%s",
                 value);
        return true;
    }
    return false;
}

void handleApiControl() {
    JsonDocument request;
    DeserializationError error = deserializeJson(request, fileServer.arg("plain"));
    if (error) {
        sendApiResult(400, false, "Invalid JSON");
        return;
    }

    const char *kind = request["kind"] | "";
    const char *field = request["field"] | "";
    JsonVariantConst value = request["value"];
    if (strcmp(kind, "settings") == 0 &&
        strcmp(field, "randomWalkSpeedMs") == 0) {
        int16_t speed;
        if (!readBoundedInt(value, 10, 5000, speed)) {
            sendApiResult(400, false, "Random Walk speed must be 10-5000 ms");
            return;
        }
        uint16_t previous = wifiSettings.randomWalkSpeedMs;
        wifiSettings.randomWalkSpeedMs = static_cast<uint16_t>(speed);
        if (!saveWifiSettings()) {
            wifiSettings.randomWalkSpeedMs = previous;
            sendApiResult(500, false, "Could not save global settings");
            return;
        }
        sendApiResult(200, true, "Updated");
        return;
    }

    int pageNumber = request["page"] | 0;
    int controlNumber = request["index"] | 0;
    if (pageNumber < 1 || pageNumber > CONTROLLER_PAGES ||
        controlNumber < 1 || controlNumber > NUMENCODERS) {
        sendApiResult(400, false, "Invalid page or encoder index");
        return;
    }
    int16_t pageIndex = static_cast<int16_t>(pageNumber - 1);
    int16_t controlIndex = static_cast<int16_t>(controlNumber - 1);
    bool updated = false;
    bool isEncoder = strcmp(kind, "encoder") == 0;
    if (isEncoder) {
        updated = applyEncoderField(controls[pageIndex].encoder[controlIndex],
                                    field, value);
    } else if (strcmp(kind, "switch") == 0) {
        updated = applySwitchField(controls[pageIndex].encswitch[controlIndex],
                                   field, value);
    }
    if (!updated) {
        sendApiResult(400, false, "Unsupported field or value out of range");
        return;
    }
    controls[pageIndex].encswitch[controlIndex].output =
        controls[pageIndex].encoder[controlIndex].output;
    bool sendOutput = false;
    if (isEncoder) {
        const controllerencoder &encoder = controls[pageIndex].encoder[controlIndex];
        sendOutput = (strcmp(field, "value") == 0 &&
                      encoder.output == MIDI_OUTPUT) ||
                     (strcmp(field, "oscValue") == 0 &&
                      encoder.output == OSC_OUTPUT &&
                      encoder.oscRange != OSC_RANGE_DELTA);
    } else {
        sendOutput = strcmp(field, "value") == 0;
    }
    notifyWebControlUpdated(pageIndex, controlIndex, isEncoder, sendOutput);
    sendApiResult(200, true, "Updated");
}

void handleApiSlot() {
    JsonDocument request;
    DeserializationError error = deserializeJson(request, fileServer.arg("plain"));
    if (error) {
        sendApiResult(400, false, "Invalid JSON");
        return;
    }
    int slot = request["slot"] | 0;
    const char *action = request["action"] | "";
    if (slot < 1 || slot > 16) {
        sendApiResult(400, false, "Slot must be from 1 to 16");
        return;
    }
    if (strcmp(action, "save") == 0) {
        if (!saveconfig(static_cast<int16_t>(slot))) {
            sendApiResult(500, false, "Could not save slot");
            return;
        }
        sendApiResult(200, true, "Saved");
    } else if (strcmp(action, "load") == 0) {
        prepareWebConfigurationLoad();
        if (!loadconfig(static_cast<int16_t>(slot))) {
            sendApiResult(500, false, "Could not load slot");
            return;
        }
        notifyWebConfigurationLoaded();
        sendApiResult(200, true, "Loaded");
    } else {
        sendApiResult(400, false, "Action must be save or load");
    }
}

// Browser homepage
void handleFileManagerHome() {
    fileServer.send(200, "text/html", webpage);
}


// Download an individual JSON file
void handleFileDownload() {

    String filename = fileServer.arg("name");

    if (!isSlotJsonFile(filename)) {
        fileServer.send(400, "text/plain", "Invalid filename");
        return;
    }

    String path = "/" + filename;

    if (!LittleFS.exists(path)) {
        fileServer.send(404, "text/plain", "File not found");
        return;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        fileServer.send(500, "text/plain", "Could not open file");
        return;
    }

    fileServer.sendHeader(
        "Content-Disposition",
        "attachment; filename=\"" + filename + "\""
    );

    fileServer.streamFile(file, "application/json");

    file.close();
}


// Register browser routes
void registerFileManagerRoutes() {

    fileServer.on("/", HTTP_GET, handleFileManagerHome);

    fileServer.on("/api/state", HTTP_GET, handleApiState);
    fileServer.on("/api/control", HTTP_POST, handleApiControl);
    fileServer.on("/api/slot", HTTP_POST, handleApiSlot);
    fileServer.on("/download", HTTP_GET, handleFileDownload);

    fileServer.on(
    "/upload",
    HTTP_POST,

    // Called when the upload finishes
    []() {

        if (uploadError) {
            fileServer.send(400, "text/plain", uploadResult);
        }
        else if (uploadResult == "Upload in progress" ||
                 uploadResult == "No upload received") {
            fileServer.send(400, "text/plain", "Upload did not complete");
        }
        else {
            fileServer.send(200, "text/plain", uploadResult);
        }

        uploadResult = "No upload received";
    },

    // Receives the actual file data
    handleFileUpload
);
}

bool isValidConfigJson(JsonDocument &document) {
    if (!document["Version"].is<int>() ||
        !document["page"].is<JsonArray>()) {
        return false;
    }
    JsonArrayConst pages = document["page"].as<JsonArrayConst>();
    if (pages.size() < CONTROLLER_PAGES) {
        return false;
    }
    for (int16_t page = 0; page < CONTROLLER_PAGES; ++page) {
        if (!pages[page]["control"].is<JsonArray>() ||
            pages[page]["control"].as<JsonArrayConst>().size() < NUMENCODERS) {
            return false;
        }
    }
    return true;
}

void handleFileUpload() {

    HTTPUpload& upload = fileServer.upload();

    if (upload.status == UPLOAD_FILE_START) {

        uploadError = false;
        uploadBytes = 0;
        uploadTarget = "";
        uploadSlot = 0;
        uploadResult = "Upload in progress";

        // Only accept slotN.json filenames
        if (!isSlotJsonFile(upload.filename)) {
            uploadError = true;
            uploadResult = "Invalid filename. Use slotN.json";
            return;
        }

        uploadTarget = "/" + upload.filename;
        uploadSlot = upload.filename.substring(4, upload.filename.length() - 5).toInt();

        // Remove any leftover temporary upload
        if (LittleFS.exists(UPLOAD_TEMP)) {
            LittleFS.remove(UPLOAD_TEMP);
        }

        uploadFile = LittleFS.open(UPLOAD_TEMP, "w");

        if (!uploadFile) {
            uploadError = true;
            uploadResult = "Could not create temporary file";
            return;
        }

        Serial.print("Receiving: ");
        Serial.println(upload.filename);
    }

    else if (upload.status == UPLOAD_FILE_WRITE) {

        if (!uploadError && uploadFile) {
            if (uploadBytes + upload.currentSize > 128 * 1024) {
                uploadError = true;
                uploadResult = "JSON file is too large";
                uploadFile.close();
                LittleFS.remove(UPLOAD_TEMP);
                return;
            }

            size_t written = uploadFile.write(
                upload.buf,
                upload.currentSize
            );

            if (written != upload.currentSize) {
                uploadError = true;
                uploadResult = "Filesystem write error";
            }
            else {
                uploadBytes += written;
            }
        }
    }

    else if (upload.status == UPLOAD_FILE_ABORTED) {

        uploadError = true;
        uploadResult = "Upload aborted";

        if (uploadFile) uploadFile.close();

        LittleFS.remove(UPLOAD_TEMP);
    }

    else if (upload.status == UPLOAD_FILE_END) {

        if (uploadFile) uploadFile.close();

        if (uploadError) {
            LittleFS.remove(UPLOAD_TEMP);
            return;
        }

        if (uploadBytes == 0) {
            uploadError = true;
            uploadResult = "Empty file";
            LittleFS.remove(UPLOAD_TEMP);
            return;
        }

        // Validate uploaded JSON before replacing anything
        File checkFile = LittleFS.open(UPLOAD_TEMP, "r");

        if (!checkFile) {
            uploadError = true;
            uploadResult = "Could not reopen uploaded file";
            return;
        }

        JsonDocument doc;

        DeserializationError error =
            deserializeJson(doc, checkFile);

        checkFile.close();

        if (error || !isValidConfigJson(doc)) {

            uploadError = true;
            uploadResult = "Invalid Twisty JSON file";
            LittleFS.remove(UPLOAD_TEMP);
            return;
        }

        // Preserve the existing file until the new file is ready
        bool hadOriginal = LittleFS.exists(uploadTarget);

        if (LittleFS.exists(UPLOAD_BACKUP)) {
            LittleFS.remove(UPLOAD_BACKUP);
        }

        if (hadOriginal) {
            if (!LittleFS.rename(uploadTarget, UPLOAD_BACKUP)) {
                uploadError = true;
                uploadResult = "Could not back up existing preset";
                LittleFS.remove(UPLOAD_TEMP);
                return;
            }
        }

        // Move the validated upload into its final location
        if (!LittleFS.rename(UPLOAD_TEMP, uploadTarget)) {

            if (hadOriginal) {
                LittleFS.rename(UPLOAD_BACKUP, uploadTarget);
            }

            uploadError = true;
            uploadResult = "Could not install uploaded file";
            return;
        }

        prepareWebConfigurationLoad();
        if (!loadconfig(uploadSlot)) {
            LittleFS.remove(uploadTarget);
            if (hadOriginal) {
                LittleFS.rename(UPLOAD_BACKUP, uploadTarget);
                loadconfig(uploadSlot);
            }
            notifyWebConfigurationLoaded();
            uploadError = true;
            uploadResult = "JSON installed but could not be loaded";
            return;
        }
        notifyWebConfigurationLoaded();

        if (hadOriginal) LittleFS.remove(UPLOAD_BACKUP);

        uploadResult = "Imported and loaded " +
                       uploadTarget.substring(1);

        Serial.println(uploadResult);
    }
}