"""One-shot helper inside connector namespace only; no host-network capability."""
import os,subprocess,json
from pathlib import Path
def main():
    nonce=os.environ['PFME_GATE_NONCE']
    if len(nonce)!=64 or any(c not in '0123456789abcdef' for c in nonce):raise RuntimeError('Invalid run nonce')
    # Compose fixes network_mode to service:connector; host networking is forbidden.
    subprocess.run(['/usr/sbin/nft','--check','--file','/app/connector.nft'],check=True)
    subprocess.run(['/usr/sbin/nft','--file','/app/connector.nft'],check=True)
    rules=subprocess.check_output(['/usr/sbin/nft','--json','list','table','inet','pfme_cf_connector'],text=True)
    data=json.loads(rules);chains=[x['chain'] for x in data['nftables'] if 'chain' in x]
    if len(chains)!=3 or any(c.get('policy')!='drop' for c in chains):raise RuntimeError('Filter not active')
    import socket
    for host,port in [('192.168.1.216',22),('192.168.1.216',3000),('192.168.1.216',4533),
                      ('192.168.1.216',2375),('192.168.1.216',8443),('192.168.1.1',80),
                      ('172.17.0.1',2375),('100.99.252.59',22)]:
        try:
            connection=socket.create_connection((host,port),timeout=.3)
        except OSError:continue
        connection.close();raise RuntimeError('Forbidden host/LAN destination reachable; gate stays closed')
    p=Path('/gate/release');fd=os.open(p,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o444)
    with os.fdopen(fd,'w') as f:f.write(nonce);f.flush();os.fsync(f.fileno())
    print('Connector-only filter installed; gate released. No host firewall modified.')
if __name__=='__main__':main()
