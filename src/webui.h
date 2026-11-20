#pragma once
#include <Arduino.h>

static const char WEB_UI[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LightLink</title>
<style>
:root{--bg:#121417;--card:#1c1f24;--line:#2c3038;--fg:#e8e8e8;--mute:#9aa0a6;--acc:#ff9f1a;--red:#ff3b2f;--grn:#3ddc5a}
*{box-sizing:border-box}body{margin:0;font:15px system-ui,sans-serif;background:var(--bg);color:var(--fg)}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;border-bottom:1px solid var(--line)}
header h1{font-size:18px;margin:0;color:var(--acc)}#stat{color:var(--mute);font-size:13px;margin-left:auto;text-align:right}
nav{display:flex;gap:4px;padding:8px 12px;overflow-x:auto;border-bottom:1px solid var(--line)}
nav button{background:none;border:0;color:var(--mute);padding:8px 12px;border-radius:6px;font-size:15px;cursor:pointer}
nav button.on{background:var(--card);color:var(--fg)}
main{padding:12px;max-width:1100px;margin:auto}section{display:none}section.on{display:block}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:12px}
.card h2{font-size:15px;margin:0 0 10px;color:var(--mute);font-weight:600}
#pv{width:100%;image-rendering:pixelated;background:#000;border-radius:6px;display:block}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:end}.row>label{display:flex;flex-direction:column;gap:4px;font-size:12px;color:var(--mute)}
input,select,textarea{background:#0e1013;color:var(--fg);border:1px solid var(--line);border-radius:6px;padding:7px 8px;font-size:14px}
textarea{width:100%;min-height:54px}input[type=checkbox]{width:18px;height:18px}
.grow{flex:1;min-width:180px}.grow input{width:100%}
button.b{background:var(--acc);color:#111;border:0;border-radius:6px;padding:8px 14px;font-weight:600;cursor:pointer}
button.s{background:#2a2e35;color:var(--fg);border:0;border-radius:6px;padding:8px 12px;cursor:pointer}
button.d{background:#4a1f1f;color:#ffb3ad;border:0;border-radius:6px;padding:6px 10px;cursor:pointer}
.item{border:1px solid var(--line);border-radius:8px;padding:10px;margin-bottom:8px}
.item .hd{display:flex;gap:6px;align-items:center;margin-bottom:8px}.item .hd b{margin-right:auto;color:var(--acc)}
table{width:100%;border-collapse:collapse;font-size:14px}td,th{text-align:left;padding:6px;border-bottom:1px solid var(--line)}
.mute{color:var(--mute);font-size:13px}#toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#333;padding:10px 16px;border-radius:8px;display:none}
progress{width:100%}code{background:#0e1013;padding:1px 5px;border-radius:4px}
</style></head><body>
<header><h1>LightLink</h1><div id="stat">...</div></header>
<nav id="nav"></nav>
<main>
<section id="t-sign">
 <div class="card"><canvas id="pv" width="1000" height="80"></canvas>
  <div class="mute" id="now" style="margin-top:8px"></div></div>
 <div class="card"><h2>Quick message</h2>
  <textarea id="m-text" placeholder="Text. Tokens: {time} {time12} {date} {ip} {host}. Use | for a 2nd line with the small font."></textarea>
  <div class="row" style="margin-top:8px">
   <label>Font<select id="m-font" class="fonts"></select></label>
   <label>Color<select id="m-color"><option value="0">Red</option><option value="1">Green</option><option value="2" selected>Orange</option><option value="3">Cycle</option></select></label>
   <label>Effect<select id="m-effect"><option value="1">Scroll</option><option value="0">Static</option><option value="2">Blink</option></select></label>
   <label>Duration<input id="m-dur" type="number" value="2" min="0" style="width:80px"></label>
   <button class="b" onclick="sendMsg()">Show now</button>
   <button class="s" onclick="post('/api/clear')">Clear</button>
   <button class="s" onclick="post('/api/skip')">Skip item</button>
  </div><div class="mute" style="margin-top:6px">Duration is passes for scroll, seconds otherwise, 0 = until cleared.</div></div>
 <div class="card"><h2>Brightness</h2><input id="bri" type="range" min="5" max="100" style="width:100%"></div>
</section>

<section id="t-playlist">
 <div class="card"><h2>Playlist</h2><div id="pl"></div>
  <div class="row"><button class="s" onclick="addItem('text')">+ Text</button><button class="s" onclick="addItem('fseq')">+ Sequence</button>
  <span style="flex:1"></span><button class="b" onclick="savePl()">Save &amp; play</button></div></div>
</section>

<section id="t-files">
 <div class="card"><h2>Upload sequence (.fseq)</h2>
  <p class="mute">Export from xLights as <b>V2 Uncompressed</b> (or V1). Model: 200 x 16 horizontal matrix, start top-left. Red channel lights red, green lights green, both = orange, blue ignored.</p>
  <div class="row"><input type="file" id="up" accept=".fseq"><button class="b" onclick="upload()">Upload</button></div>
  <progress id="upp" value="0" max="100" style="display:none;margin-top:8px"></progress></div>
 <div class="card"><h2>Files <span id="fsu" class="mute"></span></h2><table id="files"></table></div>
</section>

<section id="t-setup">
 <div class="card"><h2>Test patterns</h2><div class="row" id="tests"></div>
  <p class="mute">Use <b>ident</b> to fix orientation: T1..T5 should read left to right across the top in green, B1..B5 across the bottom in red, orange bar on the far left.
  Mirrored text = toggle Reverse X. Upside-down text = toggle Flip rows. T and B swapped = toggle Swap bands.</p>
  <div class="row"><label>Reverse X<input type="checkbox" id="c-reverseX"></label><label>Flip rows<input type="checkbox" id="c-flipY"></label>
  <label>Swap bands<input type="checkbox" id="c-swapBands"></label><button class="b" onclick="saveMap()">Apply &amp; save</button></div></div>
</section>

<section id="t-settings">
 <div class="card"><h2>Network</h2><div class="row">
  <label class="grow">WiFi SSID<input id="c-ssid"></label><label class="grow">WiFi password<input id="c-pass" type="password"></label>
  <label>Hostname<input id="c-hostname"></label><label>Setup AP password<input id="c-apPass" type="password"></label></div></div>
 <div class="card"><h2>Web UI login</h2><div class="row">
  <label>User<input id="c-webUser"></label><label>Password (blank = open)<input id="c-webPass" type="password"></label></div>
  <p class="mute">Plain HTTP basic auth. Also protects OTA and /update. Keep the sign on a trusted network.</p></div>
 <div class="card"><h2>Time</h2><div class="row">
  <label class="grow">POSIX TZ<input id="c-tz"></label><label class="grow">NTP server<input id="c-ntp"></label></div></div>
 <div class="card"><h2>Falcon Player / xLights</h2><div class="row">
  <label>DDP (4048)<input type="checkbox" id="c-ddp"></label><label>E1.31 (5568)<input type="checkbox" id="c-e131"></label>
  <label>Start universe<input id="c-e131Universe" type="number" style="width:90px"></label>
  <label>Channels / universe<input id="c-e131UniverseSize" type="number" style="width:90px"></label>
  <label>Live timeout ms<input id="c-liveTimeoutMs" type="number" style="width:90px"></label></div>
  <div class="row" style="margin-top:8px">
  <label>FSEQ start channel<input id="c-startChannel" type="number" style="width:100px"></label>
  <label>On threshold (0-255)<input id="c-threshold" type="number" style="width:90px"></label>
  <label>Serpentine rows<input type="checkbox" id="c-serpentine"></label><label>Origin bottom-left<input type="checkbox" id="c-bottomOrigin"></label></div>
  <p class="mute">FPP: add a DDP output to this sign's IP, 3200 pixels (9600 channels). E1.31 is unicast only: 19 universes of 510 channels.</p></div>
 <div class="card"><h2>Display</h2><div class="row"><label>Refresh Hz<input id="c-refreshHz" type="number" style="width:90px"></label></div></div>
 <div class="row"><button class="b" onclick="saveCfg()">Save settings</button><button class="s" onclick="if(confirm('Reboot?'))post('/api/reboot')">Reboot</button>
  <a class="s" href="/update" style="text-decoration:none;padding:8px 12px;border-radius:6px;background:#2a2e35;color:var(--fg)">Firmware update</a></div>
</section>
</main><div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const TABS=[['sign','Sign'],['playlist','Playlist'],['files','Files'],['setup','Setup'],['settings','Settings']];
let FONTS=[],FILES=[],PL=[],CFG={},tab='sign';
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2600)}
async function api(u,o){const r=await fetch(u,o);const t=await r.text();let j;try{j=JSON.parse(t)}catch(e){j=t}
 if(!r.ok){toast((j&&j.error)||('Error '+r.status));throw j}return j}
const post=(u,b)=>api(u,{method:'POST',headers:b?{'Content-Type':'application/json'}:{},body:b?JSON.stringify(b):undefined});
function show(t){tab=t;for(const[k]of TABS){$('t-'+k).classList.toggle('on',k==t);$('n-'+k).classList.toggle('on',k==t)}
 if(t=='files')loadFiles();if(t=='playlist')loadPl();if(t=='settings'||t=='setup')loadCfg()}
$('nav').innerHTML=TABS.map(([k,n])=>`<button id="n-${k}" onclick="show('${k}')">${n}</button>`).join('');
// preview
const pv=$('pv'),cx=pv.getContext('2d'),COL=['#26160f','#ff3b2f','#3ddc5a','#ffae1a'];
async function frame(){if(tab=='sign'&&!document.hidden){try{const t=await (await fetch('/api/frame')).text();const L=t.split('\n');
 cx.fillStyle='#000';cx.fillRect(0,0,1000,80);for(let y=0;y<16;y++){const s=L[y]||'';for(let x=0;x<200;x++){cx.fillStyle=COL[+s[x]||0];cx.beginPath();cx.arc(x*5+2.5,y*5+2.5,2,0,7);cx.fill()}}}catch(e){}}
 setTimeout(frame,350)}
async function status(){try{const s=await api('/api/status');
 $('stat').innerHTML=`${s.hostname} &middot; ${s.ip}${s.ap?' (setup AP)':''}<br>${s.mode}${s.liveSource?' '+s.liveSource:''} &middot; ${s.refreshHz} Hz &middot; heap ${s.heap}`;
 let n='Now: '+(s.now||'');if(s.frames)n+=` (frame ${s.frame}/${s.frames})`;if(s.error)n+=' | last error: '+s.error;$('now').textContent=n;
 if(document.activeElement!=$('bri'))$('bri').value=s.brightness}catch(e){}setTimeout(status,2000)}
let briT;$('bri').oninput=e=>{clearTimeout(briT);briT=setTimeout(()=>post('/api/brightness?value='+e.target.value),150)};
function fillFonts(){document.querySelectorAll('select.fonts').forEach(s=>{if(s.options.length)return;FONTS.forEach((f,i)=>s.add(new Option(f,i)));s.value=s.dataset.v||2})}
function sendMsg(){post('/api/message',{type:'text',text:$('m-text').value,font:+$('m-font').value,color:+$('m-color').value,effect:+$('m-effect').value,duration:+$('m-dur').value,align:1,speed:40}).then(()=>toast('Showing'))}
// playlist
async function loadPl(){const d=await api('/api/playlist');PL=d.items;FONTS=d.fonts;if(!FILES.length)await loadFiles(true);renderPl()}
function opt(list,v){return list.map((n,i)=>`<option value="${i}"${i==v?' selected':''}>${n}</option>`).join('')}
function renderPl(){const c=$('pl');c.innerHTML='';PL.forEach((it,i)=>{const d=document.createElement('div');d.className='item';
 const hd=`<div class="hd"><b>${i+1}. ${it.type=='fseq'?'Sequence':'Text'}</b><button class="s" onclick="mv(${i},-1)">&uarr;</button><button class="s" onclick="mv(${i},1)">&darr;</button><button class="d" onclick="PL.splice(${i},1);renderPl()">Delete</button></div>`;
 if(it.type=='fseq'){d.innerHTML=hd+`<div class="row"><label class="grow">File<select data-k="file">${FILES.map(f=>`<option${f.name==it.file?' selected':''}>${f.name}</option>`).join('')}</select></label>
  <label>Loops (0 = forever)<input data-k="duration" type="number" min="0" style="width:90px"></label></div>`}
 else{d.innerHTML=hd+`<textarea data-k="text"></textarea><div class="row" style="margin-top:6px">
  <label>Font<select data-k="font" data-n>${opt(FONTS,it.font)}</select></label>
  <label>Color<select data-k="color" data-n>${opt(['Red','Green','Orange','Cycle'],it.color)}</select></label>
  <label>Effect<select data-k="effect" data-n>${opt(['Static','Scroll','Blink'],it.effect)}</select></label>
  <label>Align<select data-k="align" data-n>${opt(['Left','Center','Right'],it.align)}</select></label>
  <label>Speed px/s<input data-k="speed" type="number" style="width:80px"></label>
  <label>${it.effect==1?'Passes':'Seconds'}<input data-k="duration" type="number" min="0" style="width:80px"></label></div>`}
 d.querySelectorAll('[data-k]').forEach(el=>{const k=el.dataset.k;if(el.tagName!='SELECT'||k=='file')el.value=it[k]??'';
  el.onchange=()=>{it[k]=(el.type=='number'||el.dataset.n!==undefined)?+el.value:el.value;if(k=='effect')renderPl()}});
 if(it.type=='fseq'&&!it.file&&FILES[0])it.file=FILES[0].name;c.appendChild(d)});
 if(!PL.length)c.innerHTML='<p class="mute">Empty playlist: the sign shows its address.</p>'}
function mv(i,d){const j=i+d;if(j<0||j>=PL.length)return;[PL[i],PL[j]]=[PL[j],PL[i]];renderPl()}
function addItem(t){PL.push(t=='fseq'?{type:'fseq',file:FILES[0]?FILES[0].name:'',duration:1}:{type:'text',text:'Hello',font:2,color:1,effect:1,align:1,speed:40,duration:1});renderPl()}
function savePl(){post('/api/playlist',{items:PL}).then(()=>toast('Playlist saved'))}
// files
async function loadFiles(quiet){const d=await api('/api/files');FILES=d.files;$('fsu').textContent=`(${(d.fsUsed/1024|0)} of ${(d.fsTotal/1024|0)} KB used)`;
 $('files').innerHTML='<tr><th>Name</th><th>Size</th><th>Frames</th><th>Length</th><th></th></tr>'+FILES.map(f=>`<tr><td>${f.name}</td><td>${(f.size/1024|0)} KB</td>
 <td>${f.error?'<span style="color:var(--red)">'+f.error+'</span>':f.frames+' @ '+f.stepMs+' ms'}</td><td>${f.frames?(f.frames*f.stepMs/1000).toFixed(1)+' s':''}</td>
 <td><button class="d" onclick="delFile('${f.name}')">Delete</button></td></tr>`).join('')}
function delFile(n){if(confirm('Delete '+n+'?'))api('/api/files?name='+encodeURIComponent(n),{method:'DELETE'}).then(()=>loadFiles())}
function upload(){const f=$('up').files[0];if(!f)return;const fd=new FormData();fd.append('file',f,f.name);const x=new XMLHttpRequest(),p=$('upp');
 p.style.display='block';x.upload.onprogress=e=>p.value=e.loaded*100/e.total;
 x.onload=()=>{p.style.display='none';let j={};try{j=JSON.parse(x.responseText)}catch(e){}
  toast(j.ok?(j.warning?'Uploaded, but: '+j.warning:`Uploaded ${j.name}: ${j.frames} frames`):'Upload failed: '+(j.error||x.status));loadFiles()};
 x.open('POST','/api/upload');x.send(fd)}
// settings
const BOOL=['reverseX','flipY','swapBands','ddp','e131','serpentine','bottomOrigin'];
async function loadCfg(){CFG=await api('/api/config');for(const k in CFG){const e=$('c-'+k);if(!e)continue;if(BOOL.includes(k))e.checked=CFG[k];else e.value=CFG[k]}}
function readCfg(keys){const o={};for(const k of keys){const e=$('c-'+k);if(!e)continue;o[k]=BOOL.includes(k)?e.checked:(e.type=='number'?+e.value:e.value)}return o}
function saveCfg(){post('/api/config',readCfg(Object.keys(CFG))).then(r=>toast(r.reboot?'Saved, rebooting...':'Saved'))}
function saveMap(){post('/api/config',readCfg(['reverseX','flipY','swapBands'])).then(()=>toast('Mapping applied'))}
$('tests').innerHTML=['ident','red','green','orange','rows','cols','checker','panels'].map(p=>`<button class="s" onclick="post('/api/test?pattern=${p}')">${p}</button>`).join('')+'<button class="b" onclick="post(\'/api/test?pattern=\')">Stop test</button>';
(async()=>{show('sign');try{const d=await api('/api/playlist');FONTS=d.fonts;PL=d.items;fillFonts()}catch(e){}status();frame()})();
</script></body></html>)rawliteral";
