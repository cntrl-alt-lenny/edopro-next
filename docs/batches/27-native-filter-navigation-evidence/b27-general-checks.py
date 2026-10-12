import subprocess,pathlib
p=pathlib.Path('ui/build/b27-evidence');cmds=[['python3.13','tools/generate_messages.py','--check'],['python3.13','tools/generate_protocol_constants.py','--check'],['python3.13','tools/generate_readme_status.py','--check'],['python3.13','-m','unittest','discover','-s','tests','-v'],['python3.13','tests/test_replay_trace.py','--update'],['git','diff','--exit-code','--','tests/golden'],['python3','tools/fw.py','check'],['git','diff','--check']]
for i,cmd in enumerate(cmds):
 r=subprocess.run(cmd,capture_output=True,text=True)
 (p/f'general-{i}.txt').write_text('COMMAND '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
 print(' '.join(cmd),r.returncode,flush=True)
