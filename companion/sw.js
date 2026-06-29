// Service worker toi gian: cache vo de cai duoc PWA + chay offline
const CACHE = 'vhi-watch-v1';
const ASSETS = [
  './',
  './index.html',
  './manifest.webmanifest',
  './icons/icon-192.png',
  './icons/icon-512.png'
];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(ASSETS)));
  self.skipWaiting();
});

self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener('fetch', (e) => {
  // chi cache cac asset cua app; bo qua BLE (khong qua fetch)
  e.respondWith(
    caches.match(e.request).then((r) => r || fetch(e.request).catch(() => r))
  );
});
