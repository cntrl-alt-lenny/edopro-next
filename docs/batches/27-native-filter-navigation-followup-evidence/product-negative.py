import os,pathlib,subprocess
p=pathlib.Path('ui/qml/screens/DeckBuilderScreen.qml');original=p.read_text();out=pathlib.Path('ui/build/b27-followup-rerun')
source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
try:
 for label,old in [('opening-focus','                    onOpened: categoryRepeater.itemAt(0).forceActiveFocus(Qt.TabFocusReason)\n'),('category-operation','                                        onToggled: searchResults.setCategorySelected(index, checked)\n')]:
  assert original.count(old)==1
  p.write_text(original.replace(old,''))
  build=subprocess.run(['cmake','--build','ui/build','--target','test_deckbuilder_screen','--parallel'],capture_output=True,text=True)
  (out/('negative-'+label+'-build.txt')).write_text('SOURCE '+source+' plus named mutation\nCOMMAND cmake --build ui/build --target test_deckbuilder_screen --parallel\n'+build.stdout+build.stderr+'\nEXIT '+str(build.returncode)+'\n')
  if build.returncode: raise RuntimeError('mutation build failed')
  cmd=['ui/build/tests/test_deckbuilder_screen','filterTabTraversalStaysVisible:minimum-forward']
  r=subprocess.run(cmd,env=dict(os.environ,QT_QPA_PLATFORM='offscreen',EDOPRO_NAVIGATION_TRACE='1'),capture_output=True,text=True)
  (out/('negative-'+label+'.txt')).write_text('SOURCE '+source+' plus named mutation\nCOMMAND QT_QPA_PLATFORM=offscreen '+' '.join(cmd)+'\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
  print(label,r.returncode,flush=True)
finally:
 p.write_text(original)
 r=subprocess.run(['cmake','--build','ui/build','--parallel'],capture_output=True,text=True)
 (out/'negative-restore-build.txt').write_text('COMMAND cmake --build ui/build --parallel\n'+r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
 print('restore',r.returncode,flush=True)
