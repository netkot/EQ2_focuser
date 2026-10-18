static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Focuser</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,sans-serif;
     background:#0d1117;color:#e6edf3;min-height:100vh;padding:14px 12px;}

.hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:14px;}
.hdr h1{font-size:17px;font-weight:700;letter-spacing:-.4px;}
.badge{font-size:11px;font-weight:600;padding:3px 9px;border-radius:20px;margin-left:8px;
       background:#162020;color:#3fb950;border:1px solid #238636;transition:all .3s;}
.badge.mv{background:#2d1117;color:#f85149;border-color:#da3633;
          animation:bk 1.2s ease infinite;}
@keyframes bk{0%,100%{opacity:1}50%{opacity:.5}}
.hdr-ip{font-size:11px;color:#484f58;}
.card{background:#161b22;border:1px solid #30363d;border-radius:14px;
      padding:14px 16px;margin-bottom:10px;}
.lbl{font-size:10px;color:#6e7681;letter-spacing:.5px;text-transform:uppercase;
     margin-bottom:10px;}
.gc{background:#161b22;border:1px solid #30363d;border-radius:14px;
    padding:18px 16px 14px;margin-bottom:10px;text-align:center;}
.gr{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:8px 24px;}
.gw{display:block;flex:0 1 260px;min-width:0;}
.gw svg{width:100%;height:auto;}
#ga{transition:stroke-dasharray .4s ease,stroke .35s ease;}
#gtd{transition:opacity .3s ease;}
.gmt{display:flex;justify-content:space-around;margin-top:12px;}
.gm-l{font-size:10px;color:#6e7681;text-transform:uppercase;
      letter-spacing:.4px;margin-bottom:2px;text-align:center;}
.gm-v{font-size:13px;font-weight:600;text-align:center;}
.gm-v.bl{color:#58a6ff;}
.dw{flex:0 1 260px;min-width:0;}
.dw svg{display:block;width:100%;height:auto;}
#dtg,#dtt,#dta{transition:transform .4s ease,opacity .3s ease;}
#dtr{transition:stroke-dasharray .4s ease;}
#dtal,#dtar{transition:opacity .3s ease;}
#dtc{transition:stroke .35s ease;}
.mr{display:grid;grid-template-columns:repeat(3,1fr) 1.4fr repeat(3,1fr);gap:6px;}
.mb{border:none;border-radius:10px;padding:11px 4px;font-size:12px;font-weight:700;
    cursor:pointer;display:flex;flex-direction:column;align-items:center;gap:2px;
    transition:filter .1s,transform .08s;}
.mb:active{filter:brightness(.72);transform:scale(.95);}
.mb.o{background:#0d2d58;color:#58a6ff;}
.mb.i{background:#2d1117;color:#f85149;}
.mb span{font-size:10px;font-weight:400;opacity:.65;}
.sb{min-width:0;border:none;border-radius:10px;background:#da3633;color:#fff;
    font-size:14px;font-weight:700;padding:11px 4px;cursor:pointer;letter-spacing:.3px;
    transition:filter .1s;}
.sb:active{filter:brightness(.78);}
.go-row{display:flex;gap:8px;}
.go-row input{flex:1;background:#0d1117;border:1px solid #30363d;border-radius:9px;
              color:#e6edf3;font-size:14px;padding:9px 11px;outline:none;
              transition:border-color .2s;}
.go-row input:focus{border-color:#58a6ff;}
.go-row input::placeholder{color:#484f58;}
.go-btn{background:#1f6feb;border:none;border-radius:9px;color:#fff;
        font-size:13px;font-weight:700;padding:9px 16px;cursor:pointer;
        transition:background .15s;}
.go-btn:active{background:#1158c7;}
.sr{display:flex;gap:6px;}
.sp{flex:1;background:#0d1117;border:1px solid #30363d;border-radius:9px;
    color:#6e7681;font-size:12px;font-weight:600;padding:8px 0;cursor:pointer;
    text-align:center;transition:all .15s;}
.sp.a{background:#122d62;border-color:#1f6feb;color:#58a6ff;}
.ag{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
.ac{background:#0d1117;border:1px solid #30363d;border-radius:9px;color:#8b949e;
    font-size:12px;font-weight:600;padding:9px;cursor:pointer;text-align:center;
    transition:all .15s;}
.ac:active{background:#161b22;color:#e6edf3;}
.ac.w{grid-column:1/-1;color:#f0883e;border-color:#5a3a1a;}
.ft{text-align:center;font-size:11px;color:#484f58;margin-top:4px;padding-bottom:8px;}
</style>
</head>
<body>

<div class="hdr">
  <div style="display:flex;align-items:center">
    <h1>Focuser</h1>
    <span class="badge" id="bdg">idle</span>
  </div>
  <span class="hdr-ip" id="hip">—</span>
</div>

<div class="gc">
  <div class="gr">
    <div class="gw">
      <svg viewBox="0 0 220 200" xmlns="http://www.w3.org/2000/svg">
        <circle cx="110" cy="120" r="82"
          fill="none" stroke="#21262d" stroke-width="14" stroke-linecap="round"
          stroke-dasharray="387 515" transform="rotate(135 110 120)"/>
        <circle id="ga" cx="110" cy="120" r="82"
          fill="none" stroke-width="12" stroke-linecap="round"
          transform="rotate(135 110 120)"
          style="stroke:#1f6feb;stroke-dasharray:0 515"/>
        <circle id="gtd" r="5.5" fill="#f0883e"
          style="opacity:0"/>
        <line x1="110" y1="40" x2="110" y2="50"
          stroke="#30363d" stroke-width="2" stroke-linecap="round"/>
        <line x1="30"  y1="120" x2="40"  y2="120"
          stroke="#30363d" stroke-width="2" stroke-linecap="round"/>
        <line x1="190" y1="120" x2="180" y2="120"
          stroke="#30363d" stroke-width="2" stroke-linecap="round"/>
        <text id="gv" x="110" y="107" text-anchor="middle"
              font-size="34" font-weight="700" fill="#e6edf3"
              font-family="-apple-system,sans-serif">0</text>
        <text x="110" y="129" text-anchor="middle"
              font-size="11" fill="#6e7681"
              font-family="-apple-system,sans-serif">steps</text>
        <text x="30" y="188" text-anchor="middle"
              font-size="10" fill="#484f58"
              font-family="-apple-system,sans-serif">0</text>
        <text id="gmaxlbl" x="190" y="188" text-anchor="middle"
              font-size="10" fill="#484f58"
              font-family="-apple-system,sans-serif">9000</text>
      </svg>
    </div>
    <div class="dw">
      <svg viewBox="0 0 216 116" xmlns="http://www.w3.org/2000/svg">
        <defs>
          <linearGradient id="tg" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0" stop-color="#161b22"/>
            <stop offset=".25" stop-color="#6e7681"/>
            <stop offset=".5" stop-color="#e6edf3"/>
            <stop offset=".75" stop-color="#6e7681"/>
            <stop offset="1" stop-color="#161b22"/>
          </linearGradient>
          <linearGradient id="wg" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0" stop-color="#21262d"/>
            <stop offset="1" stop-color="#30363d"/>
          </linearGradient>
        </defs>
        <rect id="dtt" x="54" y="12" width="26" height="64" rx="3"
          fill="none" stroke="#f0883e" stroke-width="1.5" stroke-dasharray="4 3"
          style="opacity:0"/>
        <g id="dtg">
          <rect x="-220" y="24" width="276" height="40" fill="url(#tg)"/>
          <rect id="dtc" x="54" y="12" width="26" height="64" rx="3"
            fill="#0d1117" stroke="#484f58" stroke-width="1.5"/>
        </g>
        <rect x="0" y="0" width="44" height="88" fill="url(#wg)"/>
        <rect x="42" y="16" width="12" height="56" rx="2"
          fill="#0d1117" stroke="#484f58" stroke-width="1.5"/>
        <line x1="54" y1="98" x2="184" y2="98" stroke="#30363d" stroke-width="1"
          stroke-dasharray="3 3"/>
        <path d="M54 92v12M184 92v12" stroke="#484f58" stroke-width="1.5"/>
        <line id="dtr" x1="54" y1="98" x2="184" y2="98" stroke="#da3633" stroke-width="2"
          style="stroke-dasharray:0 999"/>
        <path id="dtal" d="M54 98l8-5v10z" fill="#da3633" style="opacity:0"/>
        <g id="dta">
          <path id="dtar" d="M54 98l-8-5v10z" fill="#da3633" style="opacity:0"/>
          <text id="dtv" x="54" y="90" text-anchor="middle" font-size="10" font-weight="700"
                fill="#f85149" font-family="-apple-system,sans-serif">0</text>
        </g>
        <text x="54" y="114" text-anchor="middle" font-size="10" fill="#6e7681"
              font-family="-apple-system,sans-serif">0</text>
        <text id="dtmax" x="184" y="114" text-anchor="middle" font-size="10" fill="#6e7681"
              font-family="-apple-system,sans-serif">9000</text>
      </svg>
    </div>
  </div>
  <div class="gmt">
    <div>
      <div class="gm-l">Target</div>
      <div class="gm-v bl" id="gtgt">—</div>
    </div>
    <div>
      <div class="gm-l">Remaining</div>
      <div class="gm-v" id="grem">—</div>
    </div>
    <div>
      <div class="gm-l">Range</div>
      <div class="gm-v" id="gpct">—</div>
    </div>
  </div>
</div>

<div class="card">
  <div class="lbl">Move</div>
  <div class="mr">
    <button class="mb i" onclick="step(-1000)">◀<span>1000</span></button>
    <button class="mb i" onclick="step(-100)">◀<span>100</span></button>
    <button class="mb i" onclick="step(-10)">◀<span>10</span></button>
    <button class="sb" onclick="stp()">&#9632; STOP</button>
    <button class="mb o" onclick="step(10)">▶<span>10</span></button>
    <button class="mb o" onclick="step(100)">▶<span>100</span></button>
    <button class="mb o" onclick="step(1000)">▶<span>1000</span></button>
  </div>
</div>

<div class="card">
  <div class="lbl">Go to position</div>
  <div class="go-row">
    <input type="number" id="ap" min="0" placeholder="e.g. 12500">
    <button class="go-btn" onclick="gto()">Go</button>
  </div>
</div>

<div class="card">
  <div class="lbl">Speed</div>
  <div class="sr">
    <button class="sp" id="sp1" onclick="spd(4000,1)">Slow</button>
    <button class="sp a" id="sp2" onclick="spd(3000,2)">Medium</button>
    <button class="sp" id="sp3" onclick="spd(2000,3)">Fast</button>
  </div>
</div>

<div class="card">
  <div class="lbl">Actions</div>
  <div class="ag">
    <button class="ac" onclick="api('/zero')">Set pos = 0</button>
    <button class="ac" onclick="api('/home')">Home (&#8594; 0)</button>
    <button class="ac" onclick="api('/save')">Save EEPROM</button>
    <button class="ac" onclick="api('/reset')">Reset defaults</button>
    <button class="ac w" onclick="cal()">Calibrate zero (drive to IN stop)</button>
  </div>
</div>

<div class="ft" id="ft">—</div>
<div class="ft"><button class="ac" onclick="window.open('/update','_blank')">Open Update Page</button></div>

<script>
var mx=9000,as=2,ARC=387,CI=515,DT=130;
function api(p,q){
  var u=p;
  if(q){u+='?'+Object.keys(q).map(function(k){return k+'='+q[k];}).join('&');}
  fetch(u).catch(function(){});
}
function gto(){api('/moveto',{pos:document.getElementById('ap').value});}
function step(n){api('/step',{steps:n});}
function stp(){api('/stop');}
function cal(){
  if(confirm('Drive the drawtube IN against the mechanical stop and set it as 0?'))api('/calibrate');
}
function spd(us,i){
  api('/setspeed',{us:us}); as=i;
  [1,2,3].forEach(function(j){
    document.getElementById('sp'+j).className='sp'+(j===i?' a':'');
  });
}
function ptOnArc(pct,r){
  var a=(135+Math.min(pct,1)*270)*Math.PI/180;
  return[(110+r*Math.cos(a)).toFixed(2),(120+r*Math.sin(a)).toFixed(2)];
}
function poll(){
  fetch('/status').then(function(r){return r.json();}).then(function(d){
    mx=d.maxstep||9000;
    var pct=d.pos/mx;
    var arc=document.getElementById('ga');
    arc.style.strokeDasharray=(ARC*Math.min(pct,1)).toFixed(2)+' '+CI;
    arc.style.stroke=d.moving?'#f85149':'#1f6feb';
    document.getElementById('gv').textContent=d.pos;
    var pp=ptOnArc(d.target/mx,82);
    var dot=document.getElementById('gtd');
    dot.setAttribute('cx',pp[0]);
    dot.setAttribute('cy',pp[1]);
    dot.style.opacity=d.target!==d.pos?'1':'0';
    document.getElementById('gtgt').textContent=d.target;
    document.getElementById('grem').textContent=d.target!==d.pos?Math.abs(d.target-d.pos):'—';
    document.getElementById('gpct').textContent=(pct*100).toFixed(1)+'%';
    document.getElementById('gmaxlbl').textContent=mx>=1000?(mx/1000).toFixed(0)+'k':mx;
    var cl=function(v){return Math.max(0,Math.min(v,1));};
    var dl=(cl(pct)*DT).toFixed(1);
    document.getElementById('dtg').style.transform='translateX('+dl+'px)';
    document.getElementById('dta').style.transform='translateX('+dl+'px)';
    document.getElementById('dtr').style.strokeDasharray=dl+' 999';
    document.getElementById('dtal').style.opacity=dl>=16?'1':'0';
    document.getElementById('dtar').style.opacity=dl>=16?'1':'0';
    document.getElementById('dtv').textContent=d.pos;
    var tt=document.getElementById('dtt');
    tt.style.transform='translateX('+(cl(d.target/mx)*DT).toFixed(1)+'px)';
    tt.style.opacity=d.target!==d.pos?'1':'0';
    document.getElementById('dtc').style.stroke=d.moving?'#f85149':'#484f58';
    document.getElementById('dtmax').textContent=mx;
    var b=document.getElementById('bdg');
    b.textContent=d.homing?'calibrating':(d.moving?'moving':'idle');
    b.className='badge'+(d.moving?' mv':'');
    document.getElementById('hip').textContent=d.ip;
    document.getElementById('ft').textContent=
      'Temp: '+d.temp.toFixed(1)+'°C  ·  RSSI: '+d.rssi+' dBm';
    var si=[4000,3000,2000].indexOf(d.speed);
    if(si>=0&&si+1!==as){
      as=si+1;
      [1,2,3].forEach(function(j){
        document.getElementById('sp'+j).className='sp'+(j===as?' a':'');
      });
    }
  }).catch(function(){});
}
setInterval(poll,800);
poll();
</script>
</body>
</html>
)rawliteral";
