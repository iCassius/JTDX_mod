#include "qsohistory.h"

#include <cstdlib>
#include <iostream>

namespace
{
void expect(bool actual, char const * description)
{
  if (!actual) {
    std::cerr << "failed: " << description << '\n';
    std::exit(1);
  }
}
}

int main()
{
  // The legacy AutoSeq selector must be unchanged when no rare-target
  // option is active.
  expect(QsoHistory::autoCallPriorityAllowed(17, 0), "legacy priority 17");
  expect(QsoHistory::autoCallPriorityAllowed(19, 0), "legacy priority 19");
  expect(!QsoHistory::autoCallPriorityAllowed(22, 0), "legacy excludes new DXCC");

  expect(QsoHistory::autoCallPriorityAllowed(22, QsoHistory::AutoCallNewDXCC), "new DXCC");
  expect(QsoHistory::autoCallPriorityAllowed(23, QsoHistory::AutoCallNewDXCC), "new DXCC band");
  expect(!QsoHistory::autoCallPriorityAllowed(20, QsoHistory::AutoCallNewDXCC), "DXCC option excludes band/mode");

  expect(QsoHistory::autoCallPriorityAllowed(20, QsoHistory::AutoCallNewDXCCBandMode), "DXCC new band/mode");
  expect(QsoHistory::autoCallPriorityAllowed(21, QsoHistory::AutoCallNewDXCCBandMode), "DXCC new mode/band");
  expect(!QsoHistory::autoCallPriorityAllowed(22, QsoHistory::AutoCallNewDXCCBandMode), "band/mode excludes new DXCC");

  expect(QsoHistory::autoCallPriorityAllowed(13, QsoHistory::AutoCallNewGrid), "new grid band/mode");
  expect(QsoHistory::autoCallPriorityAllowed(16, QsoHistory::AutoCallNewGrid), "new grid");
  expect(!QsoHistory::autoCallPriorityAllowed(17, QsoHistory::AutoCallNewGrid), "grid option excludes legacy target");

  return 0;
}
