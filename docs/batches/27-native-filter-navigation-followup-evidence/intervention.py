import subprocess,pathlib,os,time,selectors
p=pathlib.Path('ui/build/b27-followup-rerun')
for row in ['minimum-forward','minimum-reverse','default-forward','default-reverse']:
 with (p/('intervention-foreground-'+row+'.txt')).open('w') as f:
  monitor=subprocess.Popen([str(p/'foreground')],stdout=f,stderr=f)
  peer=None
  lines=[]
  try:
   proc=subprocess.Popen(['ui/build/tests/test_deckbuilder_screen','filterTabTraversalStaysVisible:'+row],env=dict(os.environ,QT_QPA_PLATFORM='cocoa',EDOPRO_NAVIGATION_TRACE='1',QT_LOGGING_RULES='qt.qpa.cocoa.notifications=true'),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
   for line in proc.stdout:
    lines.append(line)
    if peer is None and 'categories' in line and (' Tab ' in line or ' Shift+Tab ' in line):
     lines.append('INTERVENTION '+str(time.time())+' launch separate Cocoa shell\n')
     peer=subprocess.Popen(['ui/build/edopro_next_shell'],env=dict(os.environ,QT_QPA_PLATFORM='cocoa'),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
   result=proc.wait()
   lines.append('EXIT '+str(result)+'\n')
   (p/('intervention-'+row+'.txt')).write_text('COMMAND external launch during first Effects key\n'+''.join(lines))
   print(row,result,flush=True)
  finally:
   if peer: peer.terminate();peer.wait()
   monitor.terminate();monitor.wait()
