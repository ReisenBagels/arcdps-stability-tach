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
  // presets + settings round trip
  Settings a; a.applyPreset(1); assert(a.tTicks==2 && a.tRedline && a.cFace[0]>0.9f);
  a.applyPreset(5); assert(a.tType==2 && a.tFill==1);
  a.applyPreset(0); Settings def; assert(a.tType==def.tType && a.cFace[0]==def.cFace[0] && a.tNeedle==def.tNeedle);
  std::filesystem::create_directories("addons/arcdps");
  cfg.applyPreset(3); cfg.boonMask=(1<<0)|(1<<1)|(1<<5); cfg.boonMode=1; cfg.opacity=0.55f; cfg.locked=true; cfg.cFace[1]=0.123f; cfg.save();
  Settings loaded; cfg=loaded; cfg.load();
  assert(cfg.themePreset==3 && cfg.tFill==1 && cfg.boonMask==(1|2|32) && cfg.boonMode==1 && cfg.locked && cfg.opacity>0.54f && cfg.opacity<0.56f);
  assert(cfg.cFace[1]>0.122f && cfg.cFace[1]<0.124f);
  puts("all tests passed");
}
