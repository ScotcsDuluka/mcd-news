import http.server
import ssl
import json
import http.client
import urllib.request
import os
import sys
from datetime import datetime

PORT = 443
BIND = '127.0.0.1'

GITHUB_NEWS_URL = 'https://scotcsduluka.github.io/mcd-news/news.json'

# Real server - connect by IP to avoid hosts file loop
REAL_IP = '150.171.110.98'
REAL_HOST = 'launchercontent.mojang.com'

# Local images folder
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IMAGE_DIR = os.path.join(SCRIPT_DIR, 'images')

cached_news = None
cache_time = 0
CACHE_TTL = 300

def get_news():
    global cached_news, cache_time
    now = datetime.now().timestamp()
    if cached_news and (now - cache_time) < CACHE_TTL:
        return cached_news
    try:
        req = urllib.request.Request(GITHUB_NEWS_URL, headers={'User-Agent': 'MCD-NewsServer/1.0'})
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = resp.read()
            news = json.loads(data)
            news['version'] = 1
            cached_news = news
            cache_time = now
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Fetched from GitHub ({len(data)} bytes), set version=1")
            return cached_news
    except Exception as e:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] ERROR fetching from GitHub: {e}")
        if cached_news:
            return cached_news
        return {
            "version": 1,
            "entries": [{
                "title": "MCD Mod Launcher",
                "tag": "news",
                "category": "Minecraft for Windows",
                "date": "2026-06-03",
                "text": "Welcome to Minecraft Dungeons Mod Launcher!",
                "playPageImage": {
                    "title": "mcd_mod_news.png",
                    "url": "/v2/images/mcd_mod_news.png"
                },
                "newsPageImage": {
                    "title": "mcd_mod_news.png",
                    "url": "/v2/images/mcd_mod_news.png",
                    "dimensions": {"width": 772, "height": 350}
                },
                "readMoreLink": "",
                "cardBorder": False,
                "articleBody": "<p>Minecraft Dungeons Mod Launcher is ready!</p>",
                "newsType": ["Java", "Dungeons", "News page", "Bedrock", "Legends"],
                "id": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2",
                "needsTranslation": True
            }]
        }


def proxy_to_real(method, path, request_headers=None):
    """Forward request to real launchercontent.mojang.com via direct IP"""
    try:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE

        conn = http.client.HTTPSConnection(REAL_IP, 443, timeout=15, context=ctx)

        headers = {
            'Host': REAL_HOST,
            'User-Agent': 'MCD-NewsServer/1.0',
            'Accept': '*/*',
        }
        if request_headers:
            for key in ['Accept', 'Accept-Encoding', 'Accept-Language']:
                if key in request_headers:
                    headers[key] = request_headers[key]

        conn.request(method, path, headers=headers)
        resp = conn.getresponse()
        data = resp.read()
        resp_headers = resp.getheaders()
        status = resp.status
        conn.close()

        print(f"[{datetime.now().strftime('%H:%M:%S')}] Proxy: {method} {path} -> {status} ({len(data)} bytes)")
        return status, resp_headers, data
    except Exception as e:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Proxy error for {path}: {e}")
        return 502, [], b'Bad Gateway'


def serve_local_image(path):
    """Try to serve an image from the local images folder"""
    # Extract filename from path like /v2/images/filename.png
    filename = path.split('/')[-1]
    filepath = os.path.join(IMAGE_DIR, filename)
    
    if os.path.exists(filepath):
        ext = os.path.splitext(filename)[1].lower()
        content_types = {
            '.png': 'image/png',
            '.jpg': 'image/jpeg',
            '.jpeg': 'image/jpeg',
            '.gif': 'image/gif',
            '.webp': 'image/webp',
        }
        content_type = content_types.get(ext, 'application/octet-stream')
        
        with open(filepath, 'rb') as f:
            data = f.read()
        
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Local image: {filename} ({len(data)} bytes)")
        return 200, [('Content-Type', content_type), ('Content-Length', len(data)), ('Cache-Control', 'public, max-age=3600')], data
    
    return None


class NewsHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def do_GET(self):
        path = self.path.split('?')[0]
        print(f"[{datetime.now().strftime('%H:%M:%S')}] >>> {self.command} {path}")

        # Intercept news paths
        if path in ('/v2/news.json', '/news.json') or 'dungeonsnews' in path.lower():
            self.serve_news()
        # Intercept image paths - serve local first, then proxy
        elif path.startswith('/v2/images/'):
            result = serve_local_image(path)
            if result:
                status, headers, data = result
                self.send_response(status)
                for key, value in headers:
                    self.send_header(key, value)
                self.end_headers()
                self.wfile.write(data)
            else:
                # No local image, try proxy to real server
                self.proxy_request()
        else:
            self.proxy_request()

    def do_HEAD(self):
        path = self.path.split('?')[0]
        print(f"[{datetime.now().strftime('%H:%M:%S')}] >>> HEAD {path}")

        if path in ('/v2/news.json', '/news.json') or 'dungeonsnews' in path.lower():
            news = get_news()
            data = json.dumps(news).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', len(data))
            self.end_headers()
        elif path.startswith('/v2/images/'):
            result = serve_local_image(path)
            if result:
                status, headers, data = result
                self.send_response(status)
                for key, value in headers:
                    self.send_header(key, value)
                self.end_headers()
            else:
                self.proxy_request(head_only=True)
        else:
            self.proxy_request(head_only=True)

    def serve_news(self):
        news = get_news()
        data = json.dumps(news).encode('utf-8')
        self.send_response(200)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', len(data))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Cache-Control', 'no-cache, no-store')
        self.end_headers()
        self.wfile.write(data)
        print(f"[{datetime.now().strftime('%H:%M:%S')}] >>> SENT custom news ({len(data)} bytes)")

    def proxy_request(self, head_only=False):
        method = 'HEAD' if head_only else 'GET'

        req_headers = {}
        for key in self.headers:
            req_headers[key] = self.headers[key]

        status, resp_headers, data = proxy_to_real(method, self.path, req_headers)

        self.send_response(status)
        skip = {'transfer-encoding', 'connection', 'keep-alive'}
        for key, value in resp_headers:
            if key.lower() not in skip:
                self.send_header(key, value)
        self.end_headers()

        if not head_only:
            self.wfile.write(data)


def main():
    cert_file = os.path.join(SCRIPT_DIR, 'cert.pem')
    key_file = os.path.join(SCRIPT_DIR, 'key.pem')

    if not os.path.exists(cert_file) or not os.path.exists(key_file):
        print("ERROR: SSL certificate not found!")
        print(f"Looking for: {cert_file} and {key_file}")
        print('Generate with:')
        print('  openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes -subj "/CN=launchercontent.mojang.com"')
        sys.exit(1)

    # Create images directory if it doesn't exist
    os.makedirs(IMAGE_DIR, exist_ok=True)

    server = http.server.HTTPServer((BIND, PORT), NewsHandler)
    ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_ctx.load_cert_chain(cert_file, key_file)
    server.socket = ssl_ctx.wrap_socket(server.socket, server_side=True)

    print(f"=== MCD News Server (Full Proxy + Local Images) ===")
    print(f"Listening: https://{BIND}:{PORT}")
    print(f"Intercept: /dungeonsNewsInGame/* and /v2/news.json")
    print(f"Local images: {IMAGE_DIR}")
    print(f"Proxy: all other paths to {REAL_IP}")
    print(f"====================================================\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
        server.server_close()

if __name__ == '__main__':
    main()