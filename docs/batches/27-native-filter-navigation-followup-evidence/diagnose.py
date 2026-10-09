import subprocess,os,pathlib,time
p=pathlib.Path('ui/build/b27-followup-rerun')
with (p/'foreground.txt').open('w') as f:
 monitor=subprocess.Popen([str(p/'foreground')],stdout=f,stderr=f)
 try:
  for row in ['minimum-forward','minimum-reverse','default-forward','default-reverse']:
   cmd=['ui/build/tests/test_deckbuilder_screen','filterTabTraversalStaysVisible:'+row]
   start=time.time()
   r=subprocess.run(cmd,env=dict(os.environ,QT_QPA_PLATFORM='cocoa',EDOPRO_NAVIGATION_TRACE='1',QT_LOGGING_RULES='qt.qpa.window=true;qt.qpa.events=true'),capture_output=True,text=True)
   (p/('diagnostic-'+row+'.txt')).write_text('START '+str(start)+'\nCOMMAND '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
   print(row,r.returncode,flush=True)
 finally:
  monitor.terminate(); monitor.wait()
