// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Machine.hh"
#include "Clock.hh"
#include "DataCheck.hh"

namespace sta {

DataCheck::DataCheck(Pin *from,
		     Pin *to,
		     Clock *clk) :
  from_(from),
  to_(to),
  clk_(clk)
{
}

void
DataCheck::margin(const TransRiseFall *from_tr,
		  const TransRiseFall *to_tr,
		  const SetupHold *setup_hold,
		  // Return values.
		  float &margin,
		  bool &exists) const
{
  return margins_[from_tr->index()].value(to_tr, setup_hold,
					  margin, exists);
}

void
DataCheck::setMargin(const TransRiseFallBoth *from_tr,
		     const TransRiseFallBoth *to_tr,
		     const SetupHoldAll *setup_hold,
		     float margin)
{
  for (auto from_tr_index : from_tr->rangeIndex())
    margins_[from_tr_index].setValue(to_tr, setup_hold, margin);
}

void
DataCheck::removeMargin(const TransRiseFallBoth *from_tr,
			const TransRiseFallBoth *to_tr,
			const SetupHoldAll *setup_hold)
{
  for (auto from_tr_index : from_tr->rangeIndex())
    margins_[from_tr_index].removeValue(to_tr, setup_hold);
}

bool
DataCheck::empty() const
{
  for (auto tr_index : TransRiseFall::rangeIndex()) {
    if (!margins_[tr_index].empty())
      return false;
  }
  return true;
}

void
DataCheck::marginIsOneValue(SetupHold *setup_hold,
			    // Return values.
			    float &value,
			    bool &one_value) const
{
  float value1, value2;
  if (margins_[TransRiseFall::riseIndex()].isOneValue(setup_hold, value1)
      && margins_[TransRiseFall::fallIndex()].isOneValue(setup_hold, value2)
      && value1 == value2) {
    value = value1;
    one_value = true;
  }
  else
    one_value = false;
}

////////////////////////////////////////////////////////////////

DataCheckLess::DataCheckLess(const Network *network) :
  pin_less_(network)
{
}

bool
DataCheckLess::operator()(const DataCheck *check1,
			  const DataCheck *check2) const
{
  const Pin *from1 = check1->from();
  const Pin *from2 = check2->from();
  const Pin *to1 = check1->to();
  const Pin *to2 = check2->to();
  const Clock *clk1 = check1->clk();
  const Clock *clk2 = check2->clk();
  int clk_cmp = clkCmp(clk1, clk2);
  return pin_less_(from1, from2)
    || (from1 == from2
	&& (pin_less_(to1, to2)
	    || (to1 == to2
		&& clk_cmp < 0)));
}

} // namespace
