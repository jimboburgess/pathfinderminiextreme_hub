"""Private read-only status. Mount only status data and its dedicated bearer secret."""
import json,time,hmac,os
from pathlib import Path
from http.server import BaseHTTPRequestHandler,HTTPServer
class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup();self.connection.settimeout(3)
    def log_message(self,*args):pass
    def do_GET(self):
        expected=Path('/run/secrets/status-token').read_text().strip()
        if self.path!='/status' or not hmac.compare_digest(self.headers.get('Authorization',''),'Bearer '+expected):
            self.send_response(403);self.end_headers();return
        try:
            data=json.loads(Path('/status/current.json').read_text())
            data['online']=bool(data.get('backend_online')) and time.time()-data['updated_at']<25
            data['state']='Online' if data['online'] else 'Offline'
        except (OSError,ValueError):data={'online':False,'state':'Offline','active_players':0,'capacity':4,'players':[],'connected_accounts':0,'waiting_usernames':[]}
        data['usernames']=', '.join(x['username'] for x in data['players']) or 'None'
        body=json.dumps(data).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('Content-Length',str(len(body)));self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(body)
if __name__=='__main__':HTTPServer(('0.0.0.0',8080),Handler).serve_forever()
