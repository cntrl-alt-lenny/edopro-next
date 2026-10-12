from pathlib import Path
import subprocess,os
p=Path('ui/qml/screens/DeckBuilderScreen.qml');original=p.read_text();out=Path('ui/build/b27-evidence')
try:
 for label,source in [('missing-opening-focus','\n'.join(line for line in original.split('\n') if not line.strip().startswith('onOpened:'))),('missing-list-role',original.replace('                        Accessible.role: Accessible.List\n',''))]:
  p.write_text(source)
  build=subprocess.run(['cmake','--build','ui/build','--target','test_deckbuilder_screen','--parallel'],capture_output=True,text=True)
  (out/(label+'-build.txt')).write_text(build.stdout+build.stderr+'\nEXIT '+str(build.returncode)+'\n')
  row='minimum-forward' if label=='missing-opening-focus' else 'minimum-reverse'
  platform='offscreen' if label=='missing-opening-focus' else 'cocoa'
  r=subprocess.run(['ui/build/tests/test_deckbuilder_screen','filterTabTraversalStaysVisible:'+row],env=dict(os.environ,QT_QPA_PLATFORM=platform),capture_output=True,text=True)
  (out/(label+'.txt')).write_text(r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
  print(label,r.returncode,flush=True)
finally:
 p.write_text(original)
 r=subprocess.run(['cmake','--build','ui/build','--parallel'],capture_output=True,text=True)
 (out/'mutation-restore-build.txt').write_text(r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
 print('restored build',r.returncode,flush=True)
