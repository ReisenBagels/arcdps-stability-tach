#include "tracker.h"
#include "settings.h"
#include "arcdps.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
uint64_t g_fake_now = 10000;
enum { STAB = 0, MIGHT = 1, FURY = 2 };
int main(){
  tracker.agentAdded(1,"Me",":me.1234",1,true);
  tracker.agentAdded(2,"Ally",":a.1",1,false);
  tracker.agentAdded(3,"Other",":o.1",2,false);
  for(uint32_t i=1;i<=30;i++) tracker.applyStack(2,STAB,10000,i,2650);   // cap at 25
  auto s=tracker.snapshot(0);
  assert(s.size()==2 && s[0].name=="Me" && s[1].stacks[STAB]==25);
  assert(tracker.snapshot(1).size()==3);
  tracker.applyStack(2,STAB,10000,7,2650);                                 // same id again: no double count
  assert(tracker.snapshot(0)[1].stacks[STAB]==25);
  tracker.removeStackId(2,STAB,7);  assert(tracker.snapshot(0)[1].stacks[STAB]==24);
  tracker.removeStackId(2,STAB,7);  assert(tracker.snapshot(0)[1].stacks[STAB]==24);
  tracker.changeStack(2,STAB,8,500,2650);  assert(tracker.snapshot(0)[1].stacks[STAB]==23);
  tracker.changeStack(2,STAB,9,9000,2650); assert(tracker.snapshot(0)[1].stacks[STAB]==23);
  tracker.removeAll(2,STAB);        assert(tracker.snapshot(0)[1].stacks[STAB]==0);
  // boons are independent
  tracker.applyStack(2,MIGHT,20000,100,0); tracker.applyStack(2,MIGHT,20000,101,0); tracker.applyStack(2,FURY,20000,200,0);
  auto b=tracker.snapshot(0)[1]; assert(b.stacks[MIGHT]==2 && b.stacks[FURY]==1 && b.stacks[STAB]==0);
  tracker.removeStackId(2,MIGHT,100); assert(tracker.snapshot(0)[1].stacks[MIGHT]==1 && tracker.snapshot(0)[1].stacks[FURY]==1);
  tracker.reset(2);                  b=tracker.snapshot(0)[1]; assert(b.stacks[MIGHT]==0 && b.stacks[FURY]==0);
  // stack already over by arrival is ignored; id'd stability drops right at real expiry in timer mode
  tracker.applyStack(1,STAB,2000,50,2650); assert(tracker.snapshot(0)[0].stacks[STAB]==0);
  tracker.applyStack(1,STAB,5000,51,2650); assert(tracker.snapshot(0)[0].stacks[STAB]==1);
  g_fake_now+=2349; assert(tracker.snapshot(0)[0].stacks[STAB]==1);
  g_fake_now+=2;    assert(tracker.snapshot(0)[0].stacks[STAB]==0);
  // duration boon: queued stack must survive well past its duration (does not tick while waiting), until its remove event
  tracker.applyStack(1,FURY,3000,60,0); g_fake_now+=60000;
  assert(tracker.snapshot(0)[0].stacks[FURY]==1);
  tracker.removeStackId(1,FURY,60); assert(tracker.snapshot(0)[0].stacks[FURY]==0);
  // events-only intensity: outlives duration, ends on remove event; 10s net if it never comes
  tracker.applyStack(1,MIGHT,3000,70,0); g_fake_now+=5000; assert(tracker.snapshot(0,true)[0].stacks[MIGHT]==1);
  tracker.removeStackId(1,MIGHT,70); assert(tracker.snapshot(0,true)[0].stacks[MIGHT]==0);
  tracker.applyStack(1,MIGHT,1000,71,0); g_fake_now+=11500; assert(tracker.snapshot(0,true)[0].stacks[MIGHT]==0);
  // id-less snapshots dropped on combat entry
  tracker.applyStack(1,STAB,9000,0,0); tracker.applyStack(1,STAB,9000,61,0); tracker.dropIdless(1);
  assert(tracker.snapshot(0)[0].stacks[STAB]==1);
  tracker.removeAll(1,STAB);
  tracker.applyStack(1,STAB,9000,0,0); tracker.removeStack(0,1,STAB,9000,CBTB_SINGLE); assert(tracker.snapshot(0)[0].stacks[STAB]==0);
  tracker.agentAdded(9,"Me2",":me.1234",1,true); assert(tracker.snapshot(0).size()==2);
  cbtevent e{}; e.pad61=0x78; e.pad62=0x56; e.pad63=0x34; e.pad64=0x12; assert(stackIdOf(&e)==0x12345678);
  // boon table
  assert(boonIndex(1122)==0 && boonIndex(740)==1 && boonIndex(999999)==-1);
  for(int i=0;i<BOON_COUNT;i++) for(int j=i+1;j<BOON_COUNT;j++) assert(kBoons[i].id!=kBoons[j].id);
  // presets
  TachoCfg a; a.applyPreset(1); assert(a.tTicks==2 && a.tRedline && a.cFace[0]>0.9f);
  a.applyPreset(5); assert(a.tType==2 && a.tFill==1);
  a.applyPreset(6); assert(a.tShape==1 && a.tRedline && a.needleLen<1.f && a.themePreset==6);   // GM squarebody
  a.applyPreset(0); TachoCfg def; assert(a.tType==def.tType && a.cFace[0]==def.cFace[0] && a.tNeedle==def.tNeedle && a.tShape==def.tShape);
  std::filesystem::create_directories("addons/arcdps");
  // several tachos persist independently (theme, boons, window, skin)
  Settings m; m.tachos.resize(3);
  m.tachos[0].name="Main"; m.tachos[0].applyPreset(3); m.tachos[0].boonMask=(1<<0)|(1<<1)|(1<<5); m.tachos[0].boonMode=1; m.tachos[0].cFace[1]=0.123f;
  m.tachos[1].name="Bar one"; m.tachos[1].applyPreset(5); m.tachos[1].skinFile="my skin (2).png"; m.tachos[1].winX=321; m.tachos[1].winW=640; m.tachos[1].opacity=0.4f;
  m.tachos[2].name="Truck"; m.tachos[2].applyPreset(6); m.tachos[2].locked=true; m.tachos[2].enabled=false; m.tachos[2].imgOn=false;
  m.timerMode=1; m.showSolo=true; cfg=m; cfg.save();
  cfg=Settings(); cfg.load();
  assert(cfg.tachos.size()==3 && cfg.timerMode==1 && cfg.showSolo);
  assert(cfg.tachos[0].name=="Main" && cfg.tachos[0].tFill==1 && cfg.tachos[0].boonMask==(1|2|32) && cfg.tachos[0].boonMode==1);
  assert(cfg.tachos[0].cFace[1]>0.122f && cfg.tachos[0].cFace[1]<0.124f);
  assert(cfg.tachos[1].tType==2 && cfg.tachos[1].skinFile=="my skin (2).png" && cfg.tachos[1].winX==321 && cfg.tachos[1].opacity>0.39f && cfg.tachos[1].opacity<0.41f);
  assert(cfg.tachos[2].name=="Truck" && cfg.tachos[2].themePreset==6 && cfg.tachos[2].locked && !cfg.tachos[2].enabled && !cfg.tachos[2].imgOn);
  assert(cfg.tachos[0].tType==0 && cfg.tachos[2].tType==0 && cfg.tachos[1].tShape==1);        // no bleed between tachos
  // an ini written by an older version (one tacho under flat keys) still loads
  { std::ofstream f("addons/arcdps/arcdps_stability_tach.ini"); f<<"visible=1\ndemo=0\ntType=2\nfireOn=0\nboonMask=3\nopacity=0.5\nwinX=55\nneedleCol=0.1,0.2,0.3\n"; }
  cfg=Settings(); cfg.load();
  assert(cfg.tachos.size()==1 && cfg.tachos[0].tType==2 && !cfg.tachos[0].fireOn && cfg.tachos[0].boonMask==3 && cfg.tachos[0].winX==55);
  assert(cfg.tachos[0].opacity>0.49f && cfg.tachos[0].opacity<0.51f && cfg.tachos[0].needleCol[1]>0.19f && cfg.tachos[0].needleCol[1]<0.21f);
  // theme sharing: look is exported, window/name/skin are not
  TachoCfg x; x.applyPreset(2); x.boonMask=5; x.name="Mine"; x.winX=999; x.skinFile="x.png";
  std::string txt=x.exportStyle(); assert(txt.find("STT1")==0 && txt.find("winX")==std::string::npos && txt.find("skinFile")==std::string::npos);
  TachoCfg y; y.winX=777; y.name="Other"; assert(y.importStyle(txt));
  assert(y.tNeedle==2 && y.boonMask==5 && y.themePreset==2 && y.winX==777 && y.name=="Other");
  assert(!y.importStyle("garbage") && !y.importStyle("STT1\n"));
  // bad values are clamped
  TachoCfg z; z.tType=99; z.tShape=-3; z.boonMask=0; z.opacity=5.f; z.winW=1.f; z.themePreset=42; z.name=""; z.sanitize();
  assert(z.tType==2 && z.tShape==0 && z.boonMask==1 && z.opacity==1.f && z.winW==160.f && z.themePreset==kPresetCount-1 && z.name=="Tacho");
  puts("all tests passed");
}
