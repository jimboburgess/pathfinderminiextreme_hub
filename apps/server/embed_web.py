from pathlib import Path
import sys
source,out=map(Path,sys.argv[1:])
result=['// Generated fixed assets: no request-controlled filesystem paths.\n']
for name in ('html','js','css'):
    text=(source/('play.'+name)).read_text(encoding='utf-8')
    assert ')PFMEWEB"' not in text
    result.append('static const char play_'+name+'[] = R"PFMEWEB('+text+')PFMEWEB";\n')
out.write_text(''.join(result),encoding='utf-8')

text=(source/'audio.js').read_text(encoding='utf-8')
assert ')PFMEWEB"' not in text
with out.open('a',encoding='utf-8') as target:
    target.write('static const char audio_js[] = R"PFMEWEB('+text+')PFMEWEB";\n')
