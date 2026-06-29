#include "web_upload.h"
#include "config.h"
#include "app_state.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

static WebServer          server(WEB_PORT);
static bool               running = false;
static File               uploadFile;
static WallpaperUpdatedCb s_cb = nullptr;

// Trang HTML toi gian de upload tu trinh duyet (fallback khi khong dung app)
static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>VHI Watch - Doi anh nen</title>
<style>body{font-family:sans-serif;max-width:420px;margin:24px auto;padding:0 16px}
canvas{border:1px solid #ccc;width:240px;height:240px}button{padding:10px 16px;font-size:16px}</style>
</head><body>
<h3>Doi anh nen dong ho (240x240)</h3>
<input type=file accept=image/* id=f><br><br>
<canvas id=c width=240 height=240></canvas><br><br>
<button onclick=up()>Gui len dong ho</button>
<p id=st></p>
<script>
const c=document.getElementById('c'),x=c.getContext('2d');let img=null;
document.getElementById('f').onchange=e=>{const r=new FileReader();
 r.onload=()=>{const i=new Image();i.onload=()=>{
  // crop vuong + ve vao 240x240
  const s=Math.min(i.width,i.height);
  x.drawImage(i,(i.width-s)/2,(i.height-s)/2,s,s,0,0,240,240);img=i;};
  i.src=r.result;};r.readAsDataURL(e.target.files[0]);};
function rgb565(d){const o=new Uint8Array(240*240*2);let k=0;
 for(let p=0;p<d.length;p+=4){const r=d[p]>>3,g=d[p+1]>>2,b=d[p+2]>>3;
  const v=(r<<11)|(g<<5)|b;o[k++]=v&0xff;o[k++]=v>>8;}return o;}
function up(){if(!img){alert('Chon anh truoc');return;}
 const d=x.getImageData(0,0,240,240).data;const buf=rgb565(d);
 const fd=new FormData();fd.append('img',new Blob([buf]),'wallpaper.bin');
 document.getElementById('st').textContent='Dang gui...';
 fetch('/upload',{method:'POST',body:fd}).then(r=>r.text())
  .then(t=>document.getElementById('st').textContent=t)
  .catch(e=>document.getElementById('st').textContent='Loi: '+e);}
</script></body></html>
)HTML";

static void handleRoot() {
    server.send_P(200, "text/html", PAGE);
}

// Nhan file upload (multipart) -> ghi vao LittleFS theo tung chunk
static void handleUpload() {
    HTTPUpload &u = server.upload();
    if (u.status == UPLOAD_FILE_START) {
        uploadFile = LittleFS.open(WALLPAPER_PATH, "w");
    } else if (u.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) uploadFile.write(u.buf, u.currentSize);
    } else if (u.status == UPLOAD_FILE_END) {
        if (uploadFile) uploadFile.close();
    }
}

void web_on_wallpaper_updated(WallpaperUpdatedCb cb) { s_cb = cb; }

void web_start() {
    if (running) return;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/upload", HTTP_POST,
        []() {                              // sau khi upload xong
            server.send(200, "text/plain", "OK - Da cap nhat anh nen!");
            if (s_cb) s_cb();
        },
        handleUpload);                      // handler nhan du lieu
    server.begin();

    running        = true;
    g_sys.wifiUp   = true;
    Serial.printf("[WEB] AP: %s  IP: %s\n", WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void web_stop() {
    if (!running) return;
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    running = false;
    g_sys.wifiUp = false;
    Serial.println("[WEB] Da tat AP");
}

bool web_is_running() { return running; }

void web_task() {
    if (running) server.handleClient();
}
