import os,sys
os.umask(0o027)
os.environ.setdefault('DJANGO_SETTINGS_MODULE','pfme_accounts.settings')
from django.core.management import execute_from_command_line
execute_from_command_line(sys.argv)
