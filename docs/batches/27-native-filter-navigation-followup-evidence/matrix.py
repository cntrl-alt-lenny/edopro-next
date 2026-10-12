import os,pathlib,subprocess,time
p=pathlib.Path('ui/build/b27-followup-rerun')
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
captures=p/'captures';captures.mkdir(exist_ok=True)
with (p/'matrix-foreground.txt').open('w') as f:
 monitor=subprocess.Popen([str(p/'foreground')],stdout=f,stderr=f)
 try:
  for platform in ['offscreen','cocoa']:
   for row in ['minimum-forward','minimum-reverse','default-forward','default-reverse','unavailable']:
    name='filterNavigationSkipsUnavailableControls' if row=='unavailable' else 'filterTabTraversalStaysVisible:'+row
    cmd=['ui/build/tests/test_deckbuilder_screen',name]
    r=subprocess.run(cmd,env=dict(os.environ,QT_QPA_PLATFORM=platform,EDOPRO_NAVIGATION_TRACE='1',EDOPRO_FOCUS_CAPTURES=str(captures.resolve())),capture_output=True,text=True)
    (p/('matrix-'+platform+'-'+row+'.txt')).write_text('SOURCE '+source+'\nCOMMAND QT_QPA_PLATFORM='+platform+' EDOPRO_NAVIGATION_TRACE=1 '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
    print(platform,row,r.returncode,flush=True)
 finally:
  monitor.terminate();monitor.wait()
