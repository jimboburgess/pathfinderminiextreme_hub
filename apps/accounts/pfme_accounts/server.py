import os,socket
from pathlib import Path
import uvicorn
def main():
    os.umask(0o027)
    path=Path(os.environ.get('PFME_FRONTEND','/front/gateway.sock'))
    if path.exists():
        if not path.is_socket() or path.stat().st_uid!=os.getuid():raise RuntimeError('Unexpected socket')
        path.unlink()
    sock=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);sock.bind(str(path));os.chown(path,-1,979);path.chmod(0o660)
    sock.listen(32)
    config=uvicorn.Config('pfme_accounts.asgi:application',workers=1,proxy_headers=False,access_log=False,
      log_level='warning',ws_max_size=4096,ws_max_queue=8,ws_per_message_deflate=False,
      timeout_keep_alive=5,limit_concurrency=96,ws_ping_interval=15,ws_ping_timeout=45,h11_max_incomplete_event_size=32768)
    uvicorn.Server(config).run(sockets=[sock])
if __name__=='__main__':main()
