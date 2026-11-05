#include "log.h"
#include <cstdio>
int main(){
  Log::setEnabled(true);
  for(int i=0;i<90000;i++) Log::write("line %d padding padding padding padding padding padding padding padding padding", i); // ~8-9 MB
  Log::setEnabled(false);
  Log::write("ignored when disabled");
}
