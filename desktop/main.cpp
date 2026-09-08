// ./tinyttstools "some text" -- see DesktopMain.h for what this wires up.
// main() can't be a class method, so this stays a one-line shim; it's also
// the ONE place in this whole target allowed to #include DesktopMain.h --
// see that file's ODR note.
#include "DesktopMain.h"

int main(int argc, char** argv) {
  tinyttstools::DesktopMain m;
  return m.run(argc, argv);
}
