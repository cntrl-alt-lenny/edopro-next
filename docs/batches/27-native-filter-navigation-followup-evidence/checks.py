import os,pathlib,subprocess
p=pathlib.Path('ui/build/b27-followup-rerun')
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
commands=[['cmake','-S','ui','-B','ui/build','-G','Ninja','-DCMAKE_BUILD_TYPE=Debug','-DEDOPRO_NEXT_WERROR=ON','-DEDOPRO_NEXT_UI_TESTS=ON'],['cmake','--build','ui/build','--parallel'],['ctest','--test-dir','ui/build','--output-on-failure'],['python3.13','tools/generate_messages.py','--check'],['python3.13','tools/generate_protocol_constants.py','--check'],['python3.13','tools/generate_readme_status.py','--check'],['python3.13','-m','unittest','discover','-s','tests','-v'],['python3.13','tests/test_replay_trace.py','--update'],['git','diff','--exit-code','--','tests/golden'],['python3','tools/fw.py','check'],['git','diff','--check'],['python3.13','tools/check_pr_evidence.py','--file','ui/build/b27-followup-rerun/pr-body.md']]
for i,cmd in enumerate(commands):
 r=subprocess.run(cmd,capture_output=True,text=True)
 (p/('check-'+str(i)+'.txt')).write_text('SOURCE '+source+'\nCOMMAND '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
 print(i,r.returncode,flush=True)
