import subprocess,pathlib,os,time
p=pathlib.Path('ui/build/b27-evidence');captures=pathlib.Path('ui/build/b27-captures').resolve()
for platform in ['offscreen','cocoa']:
 for row in ['minimum-forward','minimum-reverse','default-forward','default-reverse']:
  cmd=['ui/build/tests/test_deckbuilder_screen','filterTabTraversalStaysVisible:'+row]
  r=subprocess.run(cmd,env=dict(os.environ,QT_QPA_PLATFORM=platform,EDOPRO_FOCUS_CAPTURES=str(captures)),capture_output=True,text=True)
  (p/('final-'+platform+'-'+row+'.txt')).write_text('SOURCE 54ed9d8d9ebcd91e0ccafea9496135cf7173e9d1\nCOMMAND QT_QPA_PLATFORM='+platform+' '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
  print(platform,row,r.returncode,flush=True)
 r=subprocess.run(['ui/build/tests/test_deckbuilder_screen','filterNavigationSkipsUnavailableControls'],env=dict(os.environ,QT_QPA_PLATFORM=platform),capture_output=True,text=True)
 (p/('final-skips-'+platform+'.txt')).write_text(r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
 print(platform,'skips',r.returncode,flush=True)
