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
#include "Transition.hh"

namespace sta {

using std::max;

TransRiseFall TransRiseFall::rise_("rise", "^", 0);
TransRiseFall TransRiseFall::fall_("fall", "v", 1);
const std::array<TransRiseFall*, 2> TransRiseFall::range_{&rise_, &fall_};
const std::array<int, 2> TransRiseFall::range_index_{rise_.index(), fall_.index()};

TransRiseFall::TransRiseFall(const char *name,
			     const char *short_name,
			     int sdf_triple_index) :
  name_(name),
  short_name_(stringCopy(short_name)),
  sdf_triple_index_(sdf_triple_index)
{
}

TransRiseFall::~TransRiseFall()
{
  stringDelete(short_name_);
}

void
TransRiseFall::setShortName(const char *short_name)
{
  stringDelete(short_name_);
  short_name_ = stringCopy(short_name);
}

TransRiseFall *
TransRiseFall::opposite() const
{
  if (this == &rise_)
    return &fall_;
  else
    return &rise_;
}

TransRiseFall *
TransRiseFall::find(const char *tr_str)
{
  if (stringEq(tr_str, rise_.name()))
    return &rise_;
  else if (stringEq(tr_str, fall_.name()))
    return &fall_;
  else
    return nullptr;
}

TransRiseFall *
TransRiseFall::find(int index)
{
  if (index == rise_.index())
    return &rise_;
  else
    return &fall_;
}

TransRiseFallBoth *
TransRiseFall::asRiseFallBoth()
{
  if (this == &rise_)
    return TransRiseFallBoth::rise();
  else
    return TransRiseFallBoth::fall();
}

const TransRiseFallBoth *
TransRiseFall::asRiseFallBoth() const
{
  if (this == &rise_)
    return TransRiseFallBoth::rise();
  else
    return TransRiseFallBoth::fall();
}

Transition *
TransRiseFall::asTransition() const
{
  if (this == &rise_)
    return Transition::rise();
  else
    return Transition::fall();
}

////////////////////////////////////////////////////////////////

TransRiseFallBoth TransRiseFallBoth::rise_("rise", "^", 0,
					   TransRiseFall::rise(),
					   {TransRiseFall::rise()},
					   {TransRiseFall::riseIndex()});
TransRiseFallBoth TransRiseFallBoth::fall_("fall", "v", 1,
					   TransRiseFall::fall(),
					   {TransRiseFall::fall()},
					   {TransRiseFall::fallIndex()});
TransRiseFallBoth TransRiseFallBoth::rise_fall_("rise_fall", "rf", 2,
						nullptr,
						{TransRiseFall::rise(),
						 TransRiseFall::fall()},
						{TransRiseFall::riseIndex(),
						 TransRiseFall::fallIndex()});

TransRiseFallBoth::TransRiseFallBoth(const char *name,
				     const char *short_name,
				     int sdf_triple_index,
				     TransRiseFall *as_rise_fall,
				     std::vector<TransRiseFall*> range,
				     std::vector<int> range_index) :
  name_(name),
  short_name_(stringCopy(short_name)),
  sdf_triple_index_(sdf_triple_index),
  as_rise_fall_(as_rise_fall),
  range_(range),
  range_index_(range_index)
{
}

TransRiseFallBoth::~TransRiseFallBoth()
{
  stringDelete(short_name_);
}

TransRiseFallBoth *
TransRiseFallBoth::find(const char *tr_str)
{
  if (stringEq(tr_str, rise_.name()))
    return &rise_;
  else if (stringEq(tr_str, fall_.name()))
    return &fall_;
  else if (stringEq(tr_str, rise_fall_.name()))
    return &rise_fall_;
  else
    return nullptr;
}

bool
TransRiseFallBoth::matches(const TransRiseFall *tr) const
{
  return this == &rise_fall_
    || as_rise_fall_ == tr;
}

bool
TransRiseFallBoth::matches(const Transition *tr) const
{
  return this == &rise_fall_
    || (this == &rise_
	&& tr == Transition::rise())
    || (this == &fall_
	&& tr == Transition::fall());
}

void
TransRiseFallBoth::setShortName(const char *short_name)
{
  stringDelete(short_name_);
  short_name_ = stringCopy(short_name);
}

////////////////////////////////////////////////////////////////

TransitionMap Transition::transition_map_;
int Transition::max_index_ = 0;

// Sdf triple order defined on Sdf 3.0 spec, pg 3-17.
Transition Transition::rise_{  "^", "01", TransRiseFall::rise(),  0};
Transition Transition::fall_ { "v", "10", TransRiseFall::fall(),  1};
Transition Transition::tr_0Z_{"0Z", "0Z", TransRiseFall::rise(),  2};
Transition Transition::tr_Z1_{"Z1", "Z1", TransRiseFall::rise(),  3};
Transition Transition::tr_1Z_{"1Z", "1Z", TransRiseFall::fall(),  4};
Transition Transition::tr_Z0_{"Z0", "Z0", TransRiseFall::fall(),  5};
Transition Transition::tr_0X_{"0X", "0X", TransRiseFall::rise(),  6};
Transition Transition::tr_X1_{"X1", "X1", TransRiseFall::rise(),  7};
Transition Transition::tr_1X_{"1X", "1X", TransRiseFall::fall(),  8};
Transition Transition::tr_X0_{"X0", "X0", TransRiseFall::fall(),  9};
Transition Transition::tr_XZ_{"XZ", "XZ",               nullptr, 10};
Transition Transition::tr_ZX_{"ZX", "ZX",               nullptr, 11};
Transition Transition::rise_fall_{"*", "**",               nullptr, -1};

Transition::Transition(const char *name,
		       const char *init_final,
		       TransRiseFall *as_rise_fall,
		       int sdf_triple_index) :
  name_(stringCopy(name)),
  init_final_(init_final),
  as_rise_fall_(as_rise_fall),
  sdf_triple_index_(sdf_triple_index)
{
  transition_map_[name_] = this;
  transition_map_[init_final_] = this;
  max_index_ = max(sdf_triple_index, max_index_);
}

Transition::~Transition()
{
  stringDelete(name_);
}

bool
Transition::matches(const Transition *tr) const
{
  return this == riseFall() || tr == this;
}

Transition *
Transition::find(const char *tr_str)
{
  return transition_map_.findKey(tr_str);
}

const TransRiseFallBoth *
Transition::asRiseFallBoth() const
{
  return reinterpret_cast<const TransRiseFallBoth*>(as_rise_fall_);
}

void
Transition::setName(const char *name)
{
  stringDelete(name_);
  name_ = stringCopy(name);
}

////////////////////////////////////////////////////////////////

TransRiseFallIterator::TransRiseFallIterator(const TransRiseFallBoth *tr)
{
  if (tr == TransRiseFallBoth::riseFall()) {
    index_ = 0;
    index_max_ = TransRiseFall::index_max;
  }
  else {
    index_ = tr->asRiseFall()->index();
    index_max_ = index_;
  }
}

void
TransRiseFallIterator::init()
{
  index_ = 0;
  index_max_ = TransRiseFall::index_max;
}

bool
TransRiseFallIterator::hasNext()
{
  return index_ <= index_max_;
}

TransRiseFall *
TransRiseFallIterator::next()
{
  return (index_++ == 0) ? TransRiseFall::rise() : TransRiseFall::fall();
}

} // namespace
