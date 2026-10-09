import pathlib,subprocess,os,time
p=pathlib.Path('ui/build/b27-followup-rerun');source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
with (p/'shell-foreground.txt').open('w') as f:
 monitor=subprocess.Popen([str(p/'foreground')],stdout=f,stderr=f)
 try:
  for platform in ['offscreen','cocoa']:
   for row in ['minimum-forward','minimum-reverse','default-forward','default-reverse']:
    cmd=[str(p/'probe-build/tests/test_deckbuilder_screen'),'filterTabTraversalStaysVisible:'+row]
    r=subprocess.run(cmd,env=dict(os.environ,QT_QPA_PLATFORM=platform,EDOPRO_DIAG_FULL_SHELL='1',EDOPRO_NAVIGATION_TRACE='1',EDOPRO_FOCUS_CAPTURES=str((p/'captures').resolve())),capture_output=True,text=True)
    (p/('shell-'+platform+'-'+row+'.txt')).write_text('SOURCE '+source+'\nCOMMAND EDOPRO_DIAG_FULL_SHELL=1 QT_QPA_PLATFORM='+platform+' '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
    print('shell',platform,row,r.returncode,flush=True)
  cmd=['ui/build/tests/test_deckbuilder_screen','filterNavigationSkipsUnavailableControls']
  r=subprocess.run(cmd,env=dict(os.environ,QT_QPA_PLATFORM='cocoa',EDOPRO_NAVIGATION_TRACE='1'),capture_output=True,text=True)
  (p/'isolated-cocoa-unavailable.txt').write_text('SOURCE '+source+'\nCOMMAND QT_QPA_PLATFORM=cocoa '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
  print('isolated unavailable',r.returncode,flush=True)
 finally:
  monitor.terminate();monitor.wait()
