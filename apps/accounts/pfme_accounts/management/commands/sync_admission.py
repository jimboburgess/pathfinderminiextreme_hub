import json,os
from pathlib import Path
from django.core.management.base import BaseCommand,CommandError
from pfme_accounts.models import AccountProfile
class Command(BaseCommand):
    help='Root-only export of active account IDs to the native read-only admission policy.'
    def handle(self,*args,**options):
        if not hasattr(os,'geteuid') or os.geteuid()!=0:raise CommandError('Run as root:backend-group using the host helper.')
        p=Path(os.environ.get('PFME_POLICY','/policy/admission.json'))
        previous=json.loads(p.read_text())
        users=[{'login':x.login,'principal':x.principal} for x in AccountProfile.objects.filter(user__is_active=True).order_by('user_id')]
        if len(users)>32:raise CommandError('Too many admitted accounts')
        if previous['users']==users:self.stdout.write('Admission unchanged.');return
        temporary=p.with_name('admission.new')
        fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o640)
        try:
            with os.fdopen(fd,'w') as f:
                json.dump({'version':1,'revision':previous['revision']+1,'users':users},f);f.flush();os.fsync(f.fileno())
            temporary.replace(p)
        finally:
            if temporary.exists():temporary.unlink()
        self.stdout.write('Native admission synchronized. Individual account IDs are stable.')
