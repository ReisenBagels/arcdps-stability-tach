for f in tracker settings gauge main log; do echo "== $f"; g++ -std=c++17 -fsyntax-only -Wall -Wextra -I tests/stubs -I src src/$f.cpp 2>&1 | head -30; done
